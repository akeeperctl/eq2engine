//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2022
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium Engine Audio system
//////////////////////////////////////////////////////////////////////////////////

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include "core/core_common.h"
#include "core/ConVar.h"
#include "core/IDkCore.h"
#include "utils/KeyValues.h"

#include "render/IDebugOverlay.h"

#include "eqAudioSystemAL.h"
#include "source/snd_al_source.h"

using namespace Threading;
static CEqMutex s_audioSysMutex;

static CEqAudioSystemAL s_audioSystemAL;
IEqAudioSystem* g_audioSystem = &s_audioSystemAL;

// this allows us to mix between samples in single source
// and also eliminates few reallocations to just copy to single AL buffer
#define USE_ALSOFT_BUFFER_CALLBACK 1

DECLARE_CVAR(al_report_errors, "0", nullptr, 0);
DECLARE_CVAR(al_break_on_error, "0", nullptr, 0);
DECLARE_CVAR(al_bypass_errors, "0", nullptr, 0);

static bool ALCheckError(const char* op, ...)
{
	const int lastError = alGetError();
	if (lastError != AL_NO_ERROR)
	{
		EqString errString = EqString::Format("code %x", lastError);
		switch (lastError)
		{
		case AL_INVALID_NAME:
			errString = "AL_INVALID_NAME";
			break;
		case AL_INVALID_ENUM:
			errString = "AL_INVALID_ENUM";
			break;
		case AL_INVALID_VALUE:
			errString = "AL_INVALID_VALUE";
			break;
		case AL_INVALID_OPERATION:
			errString = "AL_INVALID_OPERATION";
			break;
		case AL_OUT_OF_MEMORY:
			errString = "AL_OUT_OF_MEMORY";
			break;
		}

		va_list argptr;
		va_start(argptr, op);
		EqString errorMsg = EqString::FormatVa(op, argptr);
		va_end(argptr);

		if (al_break_on_error.GetBool())
		{
			_DEBUG_BREAK;
		}

		if (al_report_errors.GetBool())
			MsgError("*OpenAL* error occured while '%s' (%s)\n", errorMsg.ToCString(), errString.ToCString());

		return al_bypass_errors.GetBool();
	}

	return true;
}

static int GetLoopRegionIdx(int offsetInSamples, int* points, int regionCount)
{
	for (int i = 0; i < regionCount; ++i)
	{
		if (offsetInSamples >= points[i * 2]) //&& offsetInSamples <= points[i*2+1])
			return i;
	}
	return -1;
}

static int WrapAroundSampleOffset(int sampleOffset, const ISoundSource* sample, bool looping)
{
	const int sampleCount = sample->GetSampleCount();

	if (looping)
	{
		int loopPoints[SOUND_SOURCE_MAX_LOOP_REGIONS * 2];
		const int numLoopRegions = sample->GetLoopRegions(loopPoints);

		const int loopRegionIdx = GetLoopRegionIdx(sampleOffset, loopPoints, numLoopRegions);
		const int sampleMin = (loopRegionIdx == -1) ? 0 : loopPoints[loopRegionIdx * 2];
		const int sampleMax = (loopRegionIdx == -1) ? sampleCount : loopPoints[loopRegionIdx * 2 + 1];

		const int sampleRange = sampleMax - sampleMin;
		
		if(sampleRange > 0)
			sampleOffset = sampleMin + ((sampleOffset - sampleMin) % sampleRange);
		else
			sampleOffset = sampleMin;
	}
	else
		sampleOffset = min(sampleOffset, sampleCount);

	return sampleOffset;
}

//---------------------------------------------------------
// AL COMMON

static const char* getALCErrorString(int err)
{
	switch (err)
	{
	case ALC_NO_ERROR:
		return "AL_NO_ERROR";
	case ALC_INVALID_DEVICE:
		return "ALC_INVALID_DEVICE";
	case ALC_INVALID_CONTEXT:
		return "ALC_INVALID_CONTEXT";
	case ALC_INVALID_ENUM:
		return "ALC_INVALID_ENUM";
	case ALC_INVALID_VALUE:
		return "ALC_INVALID_VALUE";
	case ALC_OUT_OF_MEMORY:
		return "ALC_OUT_OF_MEMORY";
	default:
		return "AL_UNKNOWN";
	}
}

static const char* getALErrorString(int err)
{
	switch (err)
	{
	case AL_NO_ERROR:
		return "AL_NO_ERROR";
	case AL_INVALID_NAME:
		return "AL_INVALID_NAME";
	case AL_INVALID_ENUM:
		return "AL_INVALID_ENUM";
	case AL_INVALID_VALUE:
		return "AL_INVALID_VALUE";
	case AL_INVALID_OPERATION:
		return "AL_INVALID_OPERATION";
	case AL_OUT_OF_MEMORY:
		return "AL_OUT_OF_MEMORY";
	default:
		return "AL_UNKNOWN";
	}
}

static bool checkALDeviceForErrors(ALCdevice* dev, const char* stage)
{
	ALCenum alErr = alcGetError(dev);
	if (alErr != AL_NO_ERROR)
	{
		MsgError("%s error: %s\n", stage, getALCErrorString(alErr));
		return false;
	}
	return true;
}

// AL filters functions
static LPALGENFILTERS alGenFilters = nullptr;
static LPALDELETEFILTERS alDeleteFilters = nullptr;
static LPALISFILTER alIsFilter = nullptr;
static LPALFILTERI alFilteri = nullptr;
static LPALFILTERIV alFilteriv = nullptr;
static LPALFILTERF alFilterf = nullptr;
static LPALFILTERFV alFilterfv = nullptr;
static LPALGETFILTERI alGetFilteri = nullptr;
static LPALGETFILTERIV alGetFilteriv = nullptr;
static LPALGETFILTERF alGetFilterf = nullptr;
static LPALGETFILTERFV alGetFilterfv = nullptr;

// AL effects functions
static LPALGENEFFECTS alGenEffects = nullptr;
static LPALDELETEEFFECTS alDeleteEffects = nullptr;
static LPALISEFFECT alIsEffect = nullptr;
static LPALEFFECTI alEffecti = nullptr;
static LPALEFFECTIV alEffectiv = nullptr;
static LPALEFFECTF alEffectf = nullptr;
static LPALEFFECTFV alEffectfv = nullptr;
static LPALGETEFFECTI alGetEffecti = nullptr;
static LPALGETEFFECTIV alGetEffectiv = nullptr;
static LPALGETEFFECTF alGetEffectf = nullptr;
static LPALGETEFFECTFV alGetEffectfv = nullptr;

// AL auxiliary effect functions
static LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots = nullptr;
static LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots = nullptr;
static LPALISAUXILIARYEFFECTSLOT alIsAuxiliaryEffectSlot = nullptr;
static LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti = nullptr;
static LPALAUXILIARYEFFECTSLOTIV alAuxiliaryEffectSlotiv = nullptr;
static LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf = nullptr;
static LPALAUXILIARYEFFECTSLOTFV alAuxiliaryEffectSlotfv = nullptr;
static LPALGETAUXILIARYEFFECTSLOTI alGetAuxiliaryEffectSloti = nullptr;
static LPALGETAUXILIARYEFFECTSLOTIV alGetAuxiliaryEffectSlotiv = nullptr;
static LPALGETAUXILIARYEFFECTSLOTF alGetAuxiliaryEffectSlotf = nullptr;
static LPALGETAUXILIARYEFFECTSLOTFV alGetAuxiliaryEffectSlotfv = nullptr;

// AL context functions
static LPALCGETSTRINGISOFT alcGetStringiSOFT = nullptr;
static LPALCRESETDEVICESOFT alcResetDeviceSOFT = nullptr;

// AL buffer
static LPALBUFFERCALLBACKSOFT alBufferCallbackSOFT;

#define AL_LOAD_PROC(T, x)		x = (T)alGetProcAddress(#x)
#define ALC_LOAD_PROC(d, T, x)  x = (T)alcGetProcAddress(d, #x)

//---------------------------------------------------------

void snd_hrtf_changed(ConVar* pVar, char const* pszOldValue)
{
	if(!g_audioSystem)
		return;

	((CEqAudioSystemAL*)g_audioSystem)->UpdateDeviceHRTF();
}

DECLARE_CVAR(snd_device, "0", nullptr, CV_ARCHIVE);
DECLARE_CVAR_CHANGE(snd_hrtf, "0", snd_hrtf_changed, nullptr, CV_ARCHIVE);
DECLARE_CVAR(snd_debug, "0", nullptr, CV_CHEAT);

//---------------------------------------------------------

#define BUFFER_SILENCE_SIZE		128
static const short _silence[BUFFER_SILENCE_SIZE] = { 0 };

//---------------------------------------------------------

CEqAudioSystemAL::CEqAudioSystemAL()
{
}

CEqAudioSystemAL::~CEqAudioSystemAL()
{
}

// init AL context
bool CEqAudioSystemAL::InitContext()
{
	Msg(" \n--------- AudioSystem Init --------- \n");

	// Init openAL
	Array<const char*> tempListChars(PP_SL);

	// check devices list
	const char* devices = (char*)alcGetString(nullptr, ALC_DEVICE_SPECIFIER);

	// go through device list (each device terminated with a single NULL, list terminated with double NULL)
	while ((*devices) != '\0')
	{
		tempListChars.append(devices);

		Msg("found sound device: %s\n", devices);
		devices += strlen(devices) + 1;
	}

	if (snd_device.GetInt() >= tempListChars.numElem())
	{
		MsgWarning("snd_device: Invalid audio device selected, reset to 0\n");
		snd_device.SetInt(0);
	}

	Msg("Audio device: %s\n", tempListChars[snd_device.GetInt()]);
	m_dev = alcOpenDevice((ALCchar*)tempListChars[snd_device.GetInt()]);

	if (!m_dev)
	{
		checkALDeviceForErrors(nullptr, "alcOpenDevice");
		return false;
	}

	if (alcIsExtensionPresent(m_dev, "ALC_SOFT_HRTF"))
	{
		ALC_LOAD_PROC(m_dev, LPALCGETSTRINGISOFT, alcGetStringiSOFT);
		ALC_LOAD_PROC(m_dev, LPALCRESETDEVICESOFT, alcResetDeviceSOFT);

		DevMsg(DEVMSG_SOUND, "Enumerate HRTF modes:\n");

		ALCint numHrtf;
		alcGetIntegerv(m_dev, ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &numHrtf);

		for (int i = 0; i < numHrtf; i++)
		{
			const ALCchar* name = alcGetStringiSOFT(m_dev, ALC_HRTF_SPECIFIER_SOFT, i);
			DevMsg(DEVMSG_SOUND, "    %d: %s\n", i+1, name);
		}
	}
	else
	{
		MsgInfo("EqAudio: HRTF is NOT supported.\n");
	}

	// configure context
	ContextParamsList al_context_params;
	GetContextParams(al_context_params);

	m_ctx = alcCreateContext(m_dev, al_context_params.ptr());
	if (!checkALDeviceForErrors(m_dev, "alcCreateContext"))
		return false;

	alcMakeContextCurrent(m_ctx);
	if (!checkALDeviceForErrors(m_dev, "alcMakeContextCurrent"))
		return false;

	// check HRTF state
	{
		ALCint hrtfState;
		alcGetIntegerv(m_dev, ALC_HRTF_SOFT, 1, &hrtfState);
		if (hrtfState)
		{
			const ALchar* name = alcGetString(m_dev, ALC_HRTF_SPECIFIER_SOFT);
			MsgInfo("EqAudio: HRTF enabled, using %s\n", name);
		}
	}

	// buffer callback is required for multi-source mixing
#if USE_ALSOFT_BUFFER_CALLBACK
	if (alIsExtensionPresent("AL_SOFT_callback_buffer"))
	{
		AL_LOAD_PROC(LPALBUFFERCALLBACKSOFT, alBufferCallbackSOFT);
	}
	else
		ErrorMsg("AL_SOFT_callback_buffer is not supported, OpenAL-soft needs to be updated\n");
#endif


	return true;
}

void CEqAudioSystemAL::GetContextParams(ContextParamsList& paramsList) const
{
	const int frequency[] = { ALC_FREQUENCY, 44100 };
	const int effectSlots[] = { ALC_MAX_AUXILIARY_SENDS, EQSND_EFFECT_SLOTS };
	const int hrtfOn[] = { 
		ALC_HRTF_SOFT, snd_hrtf.GetBool(),
		ALC_HRTF_ID_SOFT, snd_hrtf.GetInt()-1,
	};

	paramsList.append(frequency, elementsOf(frequency));
	paramsList.append(effectSlots, elementsOf(effectSlots));
	paramsList.append(hrtfOn, elementsOf(hrtfOn));

	// must be always last
	const int terminator[] = {0, 0};
	paramsList.append(terminator, elementsOf(terminator));
}

void CEqAudioSystemAL::UpdateDeviceHRTF()
{
	if(!m_ctx || !alcResetDeviceSOFT)
		return;

	ContextParamsList al_context_params;
	GetContextParams(al_context_params);

	alcResetDeviceSOFT(m_dev, al_context_params.ptr());
}

// destroy AL context
void CEqAudioSystemAL::DestroyContext()
{
	// destroy context
	alcMakeContextCurrent(nullptr);
	alcDestroyContext(m_ctx);
	alcCloseDevice(m_dev);
}

// Initializes context and voices
void CEqAudioSystemAL::Init()
{
	// init OpenAL
	if (!InitContext())
		return;

	m_mixerChannels.setNum(EQSND_MIXER_CHANNELS);
	InitEffects();

	m_noSound = false;

	// set other properties
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
}

void CEqAudioSystemAL::InitEffects()
{
	AL_LOAD_PROC(LPALGENFILTERS, alGenFilters);
	AL_LOAD_PROC(LPALDELETEFILTERS, alDeleteFilters);
	AL_LOAD_PROC(LPALISFILTER, alIsFilter);
	AL_LOAD_PROC(LPALFILTERI, alFilteri);
	AL_LOAD_PROC(LPALFILTERIV, alFilteriv);
	AL_LOAD_PROC(LPALFILTERF, alFilterf);
	AL_LOAD_PROC(LPALFILTERFV, alFilterfv);
	AL_LOAD_PROC(LPALGETFILTERI, alGetFilteri);
	AL_LOAD_PROC(LPALGETFILTERIV, alGetFilteriv);
	AL_LOAD_PROC(LPALGETFILTERF, alGetFilterf);
	AL_LOAD_PROC(LPALGETFILTERFV, alGetFilterfv);

	if (!alcIsExtensionPresent(m_dev, ALC_EXT_EFX_NAME))
	{
		MsgWarning("Sound effects are NOT supported!\n");
		return;
	}

	AL_LOAD_PROC(LPALGENEFFECTS, alGenEffects);
	AL_LOAD_PROC(LPALDELETEEFFECTS, alDeleteEffects);
	AL_LOAD_PROC(LPALISEFFECT, alIsEffect);
	AL_LOAD_PROC(LPALEFFECTI, alEffecti);
	AL_LOAD_PROC(LPALEFFECTIV, alEffectiv);
	AL_LOAD_PROC(LPALEFFECTF, alEffectf);
	AL_LOAD_PROC(LPALEFFECTFV, alEffectfv);
	AL_LOAD_PROC(LPALGETEFFECTI, alGetEffecti);
	AL_LOAD_PROC(LPALGETEFFECTIV, alGetEffectiv);
	AL_LOAD_PROC(LPALGETEFFECTF, alGetEffectf);
	AL_LOAD_PROC(LPALGETEFFECTFV, alGetEffectfv);

	AL_LOAD_PROC(LPALGENAUXILIARYEFFECTSLOTS, alGenAuxiliaryEffectSlots);
	AL_LOAD_PROC(LPALDELETEAUXILIARYEFFECTSLOTS, alDeleteAuxiliaryEffectSlots);
	AL_LOAD_PROC(LPALISAUXILIARYEFFECTSLOT, alIsAuxiliaryEffectSlot);
	AL_LOAD_PROC(LPALAUXILIARYEFFECTSLOTI, alAuxiliaryEffectSloti);
	AL_LOAD_PROC(LPALAUXILIARYEFFECTSLOTIV, alAuxiliaryEffectSlotiv);
	AL_LOAD_PROC(LPALAUXILIARYEFFECTSLOTF, alAuxiliaryEffectSlotf);
	AL_LOAD_PROC(LPALAUXILIARYEFFECTSLOTFV, alAuxiliaryEffectSlotfv);
	AL_LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTI, alGetAuxiliaryEffectSloti);
	AL_LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTIV, alGetAuxiliaryEffectSlotiv);
	AL_LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTF, alGetAuxiliaryEffectSlotf);
	AL_LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTFV, alGetAuxiliaryEffectSlotfv);

	int maxEffectSlots = 0;
	alcGetIntegerv(m_dev, ALC_MAX_AUXILIARY_SENDS, 1, &maxEffectSlots);
	m_effectSlots.setNum(maxEffectSlots);

	alGenAuxiliaryEffectSlots(maxEffectSlots, m_effectSlots.ptr());

	//
	// Load effect presets from file
	//
	KVSection* soundSettings = g_eqCore->GetConfig()->FindSection("Sound");

	const char* effectFilePath = soundSettings ? KV_GetValueString(soundSettings->FindSection("EFXScript"), 0, nullptr) : nullptr;

	if (effectFilePath == nullptr)
	{
		MsgError("InitEFX: EQCONFIG missing Sound:EFXScript !\n");
		return;
	}

	KeyValues kv;
	if (!kv.LoadFromFile(effectFilePath))
	{
		MsgError("InitEFX: Can't init EFX from '%s'\n", effectFilePath);
		return;
	}

	for (int i = 0; i < kv.GetRootSection()->keys.numElem(); i++)
	{
		KVSection* pEffectSection = kv.GetRootSection()->keys[i];
		const int nameHash = StringToHash(pEffectSection->name, true);

		sndEffect_t effect;
		strcpy(effect.name, pEffectSection->name);

		KVSection* pPair = pEffectSection->FindSection("type");

		if (pPair)
		{
			if (!CreateALEffect(KV_GetValueString(pPair), pEffectSection, effect))
			{
				MsgError("SOUND: Cannot create effect '%s' with type %s!\n", effect.name, KV_GetValueString(pPair));
				continue;
			}
		}
		else
		{
			MsgError("SOUND: Effect '%s' doesn't have type!\n", effect.name);
			continue;
		}

		DevMsg(DEVMSG_SOUND, "registering sound effect '%s'\n", effect.name);

		m_effects.insert(nameHash, effect);
	}
}

bool CEqAudioSystemAL::CreateALEffect(const char* pszName, KVSection* pSection, sndEffect_t& effect)
{
#define PARAM_VALUE(type, name, str_name)  AL_##type##_##name, clamp(KV_GetValueFloat(pSection->FindSection(str_name), 0, AL_##type##_DEFAULT_##name), AL_##type##_MIN_##name, AL_##type##_MAX_##name)


	if (!stricmp(pszName, "reverb"))
	{
		alGenEffects(1, &effect.nAlEffect);
		if (!ALCheckError("gen buffers"))
			return false;

		alEffecti(effect.nAlEffect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);

		alEffectf(effect.nAlEffect, PARAM_VALUE(REVERB, GAIN, "gain"));
		alEffectf(effect.nAlEffect, PARAM_VALUE(REVERB, GAINHF, "gain_hf"));

		alEffectf(effect.nAlEffect, PARAM_VALUE(REVERB, DECAY_TIME, "decay_time"));
		alEffectf(effect.nAlEffect, PARAM_VALUE(REVERB, DECAY_HFRATIO, "decay_hf"));
		alEffectf(effect.nAlEffect, PARAM_VALUE(REVERB, REFLECTIONS_DELAY, "reflection_delay"));
		alEffectf(effect.nAlEffect, PARAM_VALUE(REVERB, REFLECTIONS_GAIN, "reflection_gain"));
		alEffectf(effect.nAlEffect, PARAM_VALUE(REVERB, DIFFUSION, "diffusion"));
		alEffectf(effect.nAlEffect, PARAM_VALUE(REVERB, DENSITY, "density"));
		alEffectf(effect.nAlEffect, PARAM_VALUE(REVERB, AIR_ABSORPTION_GAINHF, "airabsorption_gain"));

		return true;
	}
	else if (!stricmp(pszName, "echo"))
	{
		alGenEffects(1, &effect.nAlEffect);
		if (!ALCheckError("gen buffers"))
			return false;

		alEffecti(effect.nAlEffect, AL_EFFECT_TYPE, AL_EFFECT_ECHO);

		return true;
	}

#undef PARAM_VALUE

	return false;
}

void CEqAudioSystemAL::DestroyEffects()
{
	for (auto it = m_effects.begin(); !it.atEnd(); ++it)
		alDeleteEffects(1, &it.value().nAlEffect);

	alDeleteAuxiliaryEffectSlots(m_effectSlots.numElem(), m_effectSlots.ptr());
	m_effectSlots.clear(true);
	m_effects.clear(true);
}

// Destroys context and vocies
void CEqAudioSystemAL::Shutdown()
{
	StopAllSounds();
	DestroyEffects();

	// clear voices
	m_sources.clear(true);

	// delete sample sources
	m_samples.clear(true);

	// destroy context
	DestroyContext();

	m_noSound = true;
}

CRefPtr<IEqAudioSource> CEqAudioSystemAL::CreateSource()
{
	CScopedMutex m(s_audioSysMutex);
	const int index = m_sources.append(CRefPtr_new(CEqAudioSourceAL, this));

	return static_cast<CRefPtr<IEqAudioSource>>(m_sources[index]);
}

void CEqAudioSystemAL::DestroySource(IEqAudioSource* source)
{
	if (!source)
		return;

	CEqAudioSourceAL* src = (CEqAudioSourceAL*)source;

	src->m_releaseOnStop = true;
	src->m_forceStop = true;
}

void CEqAudioSystemAL::StopAllSounds(int chanId /*= -1*/)
{
	// suspend all sources
	for (int i = 0; i < m_sources.numElem(); i++)
	{
		CEqAudioSourceAL* source = m_sources[i].Ptr();
		if (chanId == -1 || source->m_channel == chanId)
		{
			source->m_forceStop = true;
		}
	}
}

void CEqAudioSystemAL::PauseAllSounds(int chanId /*= -1*/)
{
	IEqAudioSource::Params param;
	param.set_state(IEqAudioSource::PAUSED);

	// suspend all sources
	for (int i = 0; i < m_sources.numElem(); i++)
	{
		CEqAudioSourceAL* source = m_sources[i].Ptr();
		if (chanId == -1 || source->m_channel == chanId)
			source->UpdateParams(param);
	}
}

void CEqAudioSystemAL::ResumeAllSounds(int chanId /*= -1*/)
{
	IEqAudioSource::Params param;
	param.set_state(IEqAudioSource::PLAYING);

	// suspend all sources
	for (int i = 0; i < m_sources.numElem(); i++)
	{
		CEqAudioSourceAL* source = m_sources[i].Ptr();
		if (chanId == -1 || source->m_channel == chanId)
			source->UpdateParams(param);
	}
}

void CEqAudioSystemAL::ResetMixer(int chanId)
{
	if (!m_mixerChannels.inRange(chanId))
		return;

	m_mixerChannels[chanId] = MixerChannel_t();
}

void CEqAudioSystemAL::SetChannelVolume(int chanType, float value)
{
	if (!m_mixerChannels.inRange(chanType))
		return;

	MixerChannel_t& channel = m_mixerChannels[chanType];
	channel.volume = value;
	channel.updateFlags |= IEqAudioSource::UPDATE_VOLUME;
}

void CEqAudioSystemAL::SetChannelPitch(int chanType, float value)
{
	if (!m_mixerChannels.inRange(chanType))
		return;

	MixerChannel_t& channel = m_mixerChannels[chanType];
	channel.pitch = value;
	channel.updateFlags |= IEqAudioSource::UPDATE_PITCH;
}

// loads sample source data
ISoundSourcePtr CEqAudioSystemAL::GetSample(const char* filename)
{
	{
		const int nameHash = StringToHash(filename, true);
		CScopedMutex m(s_audioSysMutex);
		auto it = m_samples.find(nameHash);
		if (!it.atEnd())
			return ISoundSourcePtr(*it);
	}

	ISoundSourcePtr sampleSource = ISoundSource::CreateSound(filename);

	if (sampleSource)
	{
		const ISoundSource::Format& fmt = sampleSource->GetFormat();

		if (fmt.dataFormat != ISoundSource::FORMAT_PCM || fmt.bitwidth > 16)	// not PCM or 32 bit
		{
			MsgWarning("Sound '%s' has unsupported format!\n", filename);
			return nullptr;
		}
		else if (fmt.channels > 2)
		{
			MsgWarning("Sound '%s' has unsupported channel count (%d)!\n", filename, fmt.channels);
			return nullptr;
		}

		if (!alBufferCallbackSOFT && !sampleSource->IsStreaming())
		{
			// Set memory to OpenAL and destroy original source (as it's not needed anymore)
			sampleSource = ISoundSourcePtr(CRefPtr_new(CSoundSource_OpenALCache, sampleSource));
		}

		AddSample(sampleSource);
	}

	return sampleSource;
}

void CEqAudioSystemAL::AddSample(ISoundSource* sample)
{
	const int nameHash = sample->GetNameHash();

	{
		CScopedMutex m(s_audioSysMutex);
		auto it = m_samples.find(nameHash);
		ASSERT_MSG(it.atEnd(), "Audio sample '%s' is already registered\n", sample->GetFilename());
	}

	CScopedMutex m(s_audioSysMutex);
	m_samples.insert(nameHash, sample);
}

void CEqAudioSystemAL::OnSampleDeleted(ISoundSource* sampleSource)
{
	if (!sampleSource)
		return;

	// stop voices using that sample
	SuspendSourcesWithSample(sampleSource);

	DevMsg(DEVMSG_SOUND, "freeing sample %s\n", sampleSource->GetFilename());

	// remove from list
	{
		CScopedMutex m(s_audioSysMutex);
		m_samples.remove(sampleSource->GetNameHash());
	}
}

// finds the effect. May return EFFECTID_INVALID
audioEffectId_t CEqAudioSystemAL::FindEffect(const char* name) const
{
	const int nameHash = StringToHash(name, true);
	auto it = m_effects.find(nameHash);

	if (!it.atEnd())
		return it.value().nAlEffect;

	return EFFECT_ID_NONE;
}

// sets the new effect
void CEqAudioSystemAL::SetEffect(int slot, audioEffectId_t effect)
{
	// used directly
	alAuxiliaryEffectSloti(m_effectSlots[slot], AL_EFFECTSLOT_EFFECT, effect);
}

int	CEqAudioSystemAL::GetEffectSlotCount() const
{
	return m_effectSlots.numElem();
}

//-----------------------------------------------

void CEqAudioSystemAL::SuspendSourcesWithSample(ISoundSource* sample)
{
	for (int i = 0; i < m_sources.numElem(); i++)
	{
		CEqAudioSourceAL* src = m_sources[i].Ptr();

		for (int j = 0; j < src->m_streams.numElem(); ++j)
		{
			if (src->m_streams[j].sample == sample)
			{
				// sadly, entire sound source has to be stopped
				src->Release();
				break;
			}
		}
	}
}

// updates all channels
void CEqAudioSystemAL::BeginUpdate()
{
	ASSERT(m_begunUpdate == false);
	m_begunUpdate = true;
	alcSuspendContext(m_ctx);
}

void CEqAudioSystemAL::EndUpdate()
{
	PROF_EVENT("AudioSystemAL EndUpdate");
	ASSERT(m_begunUpdate);

	for (int i = 0; i < m_sources.numElem(); i++)
	{
		CEqAudioSourceAL* src = m_sources[i].Ptr();

		if (src->m_forceStop)
		{
			src->Release();
			src->m_forceStop = false;
		}

		if (!src->DoUpdate())
		{
			if (src->m_releaseOnStop)
			{
				CScopedMutex m(s_audioSysMutex);
				m_sources.fastRemoveIndex(i);
				i--;
			}
		}
	}

	// setup orientation parameters
	const float orient[] = { m_listener.orientF.x,  m_listener.orientF.y,  m_listener.orientF.z, -m_listener.orientU.x, -m_listener.orientU.y, -m_listener.orientU.z };

	alListenerfv(AL_POSITION, m_listener.position);
	alListenerfv(AL_VELOCITY, m_listener.velocity);
	alListenerfv(AL_ORIENTATION, orient);

	alcProcessContext(m_ctx);

	for (int i = 0; i < m_mixerChannels.numElem(); ++i)
		m_mixerChannels[i].updateFlags = 0;

	if (snd_debug.GetBool())
	{
		uint sampleMem = 0;
		for (auto it = m_samples.begin(); !it.atEnd(); ++it)
		{
			const ISoundSource* sample = *it;
			if (sample->IsStreaming())
				continue;

			const ISoundSource::Format& fmt = sample->GetFormat();
			const int sampleUnit = (fmt.bitwidth >> 3);
			const int sampleSize = sampleUnit * fmt.channels;

			sampleMem += sample->GetSampleCount() * sampleSize;
		}

		uint playing = 0;
		for (int i = 0; i < m_sources.numElem(); i++)
		{
			CEqAudioSourceAL* src = m_sources[i].Ptr();
			playing += (src->GetState() == IEqAudioSource::PLAYING);
		}

		debugoverlay->Text(color_white, "-----SOUND STATISTICS-----");
		debugoverlay->Text(color_white, "  sources: %d, (%d allocated)", playing, m_sources.numElem());
		debugoverlay->Text(color_white, "  samples: %d, mem: %d kbytes (non-streamed)", m_samples.size(), sampleMem / 1024);
	}

	m_begunUpdate = false;
}

void CEqAudioSystemAL::SetMasterVolume(float value)
{
	alListenerf(AL_GAIN, value);
}

// sets listener properties
void CEqAudioSystemAL::SetListener(const Vector3D& position,
	const Vector3D& velocity,
	const Vector3D& forwardVec,
	const Vector3D& upVec)
{
	m_listener.position = position;
	m_listener.velocity = velocity;
	m_listener.orientF = forwardVec;
	m_listener.orientU = upVec;
}

// gets listener properties
const Vector3D& CEqAudioSystemAL::GetListenerPosition() const
{
	return m_listener.position;
}

//----------------------------------------------------------------------------------------------
// Sound source
//----------------------------------------------------------------------------------------------

CEqAudioSourceAL::CEqAudioSourceAL(CEqAudioSystemAL* owner) 
	: m_owner(owner)
{
}

CEqAudioSourceAL::~CEqAudioSourceAL()
{
	Release();
}

// Updates channel with user parameters
void CEqAudioSourceAL::UpdateParams(const Params& params, int overrideUpdateFlags)
{
	int mask = overrideUpdateFlags == -1 ? params.updateFlags : overrideUpdateFlags;

	// apply update flags from mixer
	if (mask & UPDATE_CHANNEL)
	{
		m_channel = params.channel;
		mask &= ~UPDATE_CHANNEL;
	}

	CEqAudioSystemAL::MixerChannel_t mixChannel;

	const int channel = m_channel;
	if (m_owner->m_mixerChannels.inRange(channel))
		mixChannel = m_owner->m_mixerChannels[channel];

	mask |= mixChannel.updateFlags;

	if (mask == 0)
		return;

	const ALuint thisSource = m_source;

	// is that source needs setup again?
	if (thisSource == 0)
		return;

	ALuint qbuffer;
	int numQueued;

	if (mask & UPDATE_POSITION)
		alSourcefv(thisSource, AL_POSITION, params.position);

	if (mask & UPDATE_VELOCITY)
		alSourcefv(thisSource, AL_VELOCITY, params.velocity);

	if (mask & UPDATE_DIRECTION)
		alSourcefv(thisSource, AL_DIRECTION, params.direction);

	if (mask & UPDATE_CONE_ANGLES)
	{
		alSourcef(thisSource, AL_CONE_INNER_ANGLE, params.coneAngles.x);
		alSourcef(thisSource, AL_CONE_OUTER_ANGLE, params.coneAngles.y);
	}

	if(params.updateFlags & UPDATE_VOLUME)
		m_volume = params.volume;

	if (params.updateFlags & UPDATE_PITCH)
		m_pitch = params.pitch;

	if (mask & UPDATE_VOLUME)
	{
		alSourcef(thisSource, AL_GAIN, m_volume.x * mixChannel.volume);
		alSourcef(thisSource, AL_CONE_OUTER_GAIN, m_volume.y);
		alSourcef(thisSource, AL_CONE_OUTER_GAINHF, m_volume.z);
	}

	if (mask & UPDATE_PITCH)
		alSourcef(thisSource, AL_PITCH, m_pitch * mixChannel.pitch);

	if (mask & UPDATE_REF_DIST)
		alSourcef(thisSource, AL_REFERENCE_DISTANCE, params.referenceDistance);

	if (mask & UPDATE_AIRABSORPTION)
		alSourcef(thisSource, AL_AIR_ABSORPTION_FACTOR, params.airAbsorption);

	if (mask & UPDATE_ROLLOFF)
		alSourcei(thisSource, AL_ROLLOFF_FACTOR, params.rolloff);

	if (mask & UPDATE_EFFECTSLOT)
	{
		if (params.effectSlot < 0)
			alSource3i(thisSource, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
		else
			alSource3i(thisSource, AL_AUXILIARY_SEND_FILTER, m_owner->m_effectSlots[params.effectSlot], 0, AL_FILTER_NULL);
	}

	if (mask & UPDATE_BANDPASS)
	{
		if (!m_filter)
		{
			alGenFilters(1, &m_filter);
			ALCheckError("gen buffers");

			alFilteri(m_filter, AL_FILTER_TYPE, AL_FILTER_BANDPASS);
			alFilterf(m_filter, AL_BANDPASS_GAIN, 1.0f);
		}

		alFilterf(m_filter, AL_BANDPASS_GAINLF, params.bandPass.x);
		alFilterf(m_filter, AL_BANDPASS_GAINHF, params.bandPass.y);

		alSourcei(thisSource, AL_DIRECT_FILTER, m_filter);
	}

	if (mask & UPDATE_RELATIVE)
	{
		const int tempValue = params.relative == true ? AL_TRUE : AL_FALSE;
		alSourcei(thisSource, AL_SOURCE_RELATIVE, tempValue);

		// temporary enable direct channels on relative sources (as spec says it will be only for non-mono sources)
		alSourcei(thisSource, AL_DIRECT_CHANNELS_SOFT, tempValue);
	}

	const bool isStreaming = IsStreamed();

	if (mask & UPDATE_LOOPING)
	{
		m_looping = params.looping;

		if (!isStreaming)
			alSourcei(thisSource, AL_LOOPING, m_looping ? AL_TRUE : AL_FALSE);
	}

	if (mask & UPDATE_DO_REWIND)
	{
		for (int i = 0; i < m_streams.numElem(); ++i)
			m_streams[i].curPos = 0;

#ifdef USE_ALSOFT_BUFFER_CALLBACK
		if(!alBufferCallbackSOFT && !isStreaming)
#else
		if (!isStreaming)
#endif
		{
			alSourceRewind(thisSource);
		}
	}

	if (mask & UPDATE_RELEASE_ON_STOP)
		m_releaseOnStop = params.releaseOnStop;

	// change state
	if (mask & UPDATE_STATE)
	{
		if (params.state == STOPPED)
		{
			alSourceStop(thisSource);
		}
		else if (params.state == PAUSED)
		{
			// HACK: make source armed
			if (m_state != PLAYING)
				alSourcePlay(thisSource);

			alSourcePause(thisSource);
		}
		else if (params.state == PLAYING)
		{
			// re-queue stream buffers
			if (isStreaming)
			{
				alSourceStop(thisSource);

				// first dequeue buffers
				numQueued = 0;
				alGetSourcei(thisSource, AL_BUFFERS_QUEUED, &numQueued);

				while (numQueued--)
					alSourceUnqueueBuffers(thisSource, 1, &qbuffer);

				for (int i = 0; i < EQSND_STREAM_BUFFER_COUNT; i++)
				{
					if (!QueueStreamChannel(m_buffers[i]))
						break; // too short
				}
				ALCheckError("queue buffers");
			}

			alSourcePlay(thisSource);
		}

		m_state = params.state;
	}

	ALCheckError("source update");
}

void CEqAudioSourceAL::SetSamplePlaybackPosition(int sourceIdx, float seconds)
{
	if (sourceIdx == -1)
	{
		for (int i = 0; i < m_streams.numElem(); ++i)
		{
			const ISoundSource::Format& fmt = m_streams[i].sample->GetFormat();
			m_streams[i].curPos = WrapAroundSampleOffset(seconds * fmt.frequency, m_streams[i].sample, m_looping);
		}
		return;
	}

	if (!m_streams.inRange(sourceIdx))
		return;
	const ISoundSource::Format& fmt = m_streams[sourceIdx].sample->GetFormat();
	m_streams[sourceIdx].curPos = WrapAroundSampleOffset(seconds * fmt.frequency, m_streams[sourceIdx].sample, m_looping);
}

float CEqAudioSourceAL::GetSamplePlaybackPosition(int sourceIdx)
{
	if (m_streams.inRange(sourceIdx))
	{
		const ISoundSource::Format& fmt = m_streams[sourceIdx].sample->GetFormat();
		return m_streams[sourceIdx].curPos / fmt.frequency;
	}
	return 0.0f;
}

void CEqAudioSourceAL::SetSampleVolume(int sourceIdx, float volume)
{
	if (sourceIdx == -1)
	{
		for (int i = 0; i < m_streams.numElem(); ++i)
			m_streams[i].volume = volume;

		return;
	}

	if(m_streams.inRange(sourceIdx))
		m_streams[sourceIdx].volume = volume;
}

float CEqAudioSourceAL::GetSampleVolume(int sourceIdx)
{
	if (m_streams.inRange(sourceIdx))
		return m_streams[sourceIdx].volume;
	return 0.0f;
}

int	CEqAudioSourceAL::GetSampleCount() const
{
	return m_streams.numElem();
}

void CEqAudioSourceAL::GetParams(Params& params) const
{
	const ALuint thisSource = m_source;

	int sourceState;
	int tempValue;

	if (thisSource == AL_NONE)
		return;

	params.channel = m_channel;

	const bool isStreaming = IsStreamed();

	// get current state of alSource
	alGetSourcefv(thisSource, AL_POSITION, params.position);
	alGetSourcefv(thisSource, AL_VELOCITY, params.velocity);
	params.volume = m_volume;
	params.pitch = m_pitch;
	alGetSourcef(thisSource, AL_REFERENCE_DISTANCE, &params.referenceDistance);
	alGetSourcef(thisSource, AL_ROLLOFF_FACTOR, &params.rolloff);
	alGetSourcef(thisSource, AL_AIR_ABSORPTION_FACTOR, &params.airAbsorption);

	params.looping = m_looping;

	alGetSourcei(thisSource, AL_SOURCE_RELATIVE, &tempValue);
	params.relative = (tempValue == AL_TRUE);

	if (m_filter != AL_NONE)
	{
		alGetFilterf(m_filter, AL_BANDPASS_GAINLF, &params.bandPass.x);
		alGetFilterf(m_filter, AL_BANDPASS_GAINLF, &params.bandPass.y);
	}

	if (isStreaming)
	{
		// continuous; use channel state
		params.state = m_state;
	}
	else
	{
		alGetSourcei(thisSource, AL_SOURCE_STATE, &sourceState);

		// use AL state
		if (sourceState == AL_INITIAL || sourceState == AL_STOPPED)
			params.state = STOPPED;
		else if (sourceState == AL_PLAYING)
			params.state = PLAYING;
		else if (sourceState == AL_PAUSED)
			params.state = PAUSED;
	}

	params.releaseOnStop = m_releaseOnStop;
}

void CEqAudioSourceAL::Setup(int chanId, const ISoundSource* sample, UpdateCallback fnCallback /*= nullptr*/)
{
	Release();
	InitSource();

	m_callback = fnCallback;
	m_channel = chanId;
	m_releaseOnStop = !m_callback;

	SetupSample(sample);
}

void CEqAudioSourceAL::Setup(int chanId, ArrayCRef<const ISoundSource*> samples, UpdateCallback fnCallback /*= nullptr*/)
{
	Release();
	InitSource();

	m_callback = fnCallback;
	m_channel = chanId;
	m_releaseOnStop = !m_callback;

	SetupSamples(samples);
}

bool CEqAudioSourceAL::IsStreamed() const
{
	return m_streams.numElem() > 0 ? m_streams[0].sample->IsStreaming() : false;
}

bool CEqAudioSourceAL::InitSource()
{
	ALuint source;

	// initialize source
	alGenSources(1, &source);
	if (!ALCheckError("gen source"))
	{
		m_source = source;
		return false;
	}

	alSourcei(source, AL_LOOPING, AL_FALSE);
	alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
	alSourcei(source, AL_AUXILIARY_SEND_FILTER_GAIN_AUTO, AL_TRUE);
	alSourcef(source, AL_MIN_GAIN, 0.0f);
	alSourcef(source, AL_MAX_GAIN, 2.0f);
	alSourcef(source, AL_DOPPLER_FACTOR, 1.0f);
	alSourcef(source, AL_MAX_DISTANCE, F_INFINITY);

	// TODO: enable AL_SOURCE_SPATIALIZE_SOFT for non-relative sources by default
	// TODO: add support mixing for different sources using AL_SOFT_callback_buffer ext

	m_source = source;

	// initialize buffers
	alGenBuffers(EQSND_STREAM_BUFFER_COUNT, m_buffers);
	ALCheckError("gen stream buffer");

	return true;
}

void CEqAudioSourceAL::Release()
{
	m_callback = nullptr;
	m_channel = -1;
	m_state = STOPPED;

	if (m_source != AL_NONE)
	{
		EmptyBuffers();

		alDeleteBuffers(EQSND_STREAM_BUFFER_COUNT, m_buffers);
		alDeleteSources(1, &m_source);

		m_source = AL_NONE;
		m_streams.clear();
	}

	if (m_filter != AL_NONE)
	{
		alDeleteFilters(1, &m_filter);
		m_filter = AL_NONE;
	}
}

// updates channel (in cycle)
bool CEqAudioSourceAL::DoUpdate()
{
	if (!alIsSource(m_source))
	{
		// force destroy invalid source
		Release();
		m_releaseOnStop = true;
		return false;
	}

	// process user callback
	if (m_callback)
	{
		Params params;
		GetParams(params);

		m_callback(this, params);

		// update channel parameters
		UpdateParams(params);
	}
	else
	{
		// monitor mixer state
		const int channel = m_channel;
		if (m_owner->m_mixerChannels.inRange(channel))
		{
			const CEqAudioSystemAL::MixerChannel_t& mixChannel = m_owner->m_mixerChannels[channel];
			if (mixChannel.updateFlags)
			{
				Params params;
				GetParams(params);
				UpdateParams(params);
			}
		}
	}

	if (!m_streams.numElem())
	{
		return (m_releaseOnStop == false);
	}

	const bool isStreaming = IsStreamed();

	// get source state again
	int sourceState = AL_STOPPED;
	alGetSourcei(m_source, AL_SOURCE_STATE, &sourceState);

	if (isStreaming)
	{
		// always disable internal looping for streams
		alSourcei(m_source, AL_LOOPING, AL_FALSE);

		// update buffers
		if (m_state == PLAYING)
		{
			int	processedBuffers;
			alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processedBuffers);

			while (processedBuffers--)
			{
				// dequeue and get buffer
				ALuint buffer;
				alSourceUnqueueBuffers(m_source, 1, &buffer);

				if (!QueueStreamChannel(buffer))
				{
					m_state = STOPPED;
					break;
				}
			}

			if (sourceState != AL_PLAYING)
				alSourcePlay(m_source);
		}
	}
	else
	{
		if (sourceState == AL_INITIAL || sourceState == AL_STOPPED)
			m_state = STOPPED;
		else if (sourceState == AL_PLAYING)
			m_state = PLAYING;
		else if (sourceState == AL_PAUSED)
			m_state = PAUSED;
	}

	// release channel if stopped
	if (m_releaseOnStop && m_state == STOPPED)
	{
		// drop voice
		Release();
	}

	return true;
}

static ALenum GetSoundSourceFormatAsALEnum(const ISoundSource::Format& fmt)
{
	ALenum alFormat;

	if (fmt.bitwidth == 8)
		alFormat = fmt.channels == 2 ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
	else if (fmt.bitwidth == 16)
		alFormat = fmt.channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
	else
		alFormat = AL_FORMAT_MONO16;

	return alFormat;
}

static ALsizei AL_APIENTRY SoundSourceSampleDataCallback(void* userPtr, void* data, ALsizei size)
{
	CEqAudioSourceAL* audioSrc = reinterpret_cast<CEqAudioSourceAL*>(userPtr);
	return audioSrc->GetSampleBuffer(data, size);
}

// mix 8 bit into 16 bit mono sound
static int MixMono8(float volume, const uint8* in, int numInSamples, short* out, int numOutSamples)
{
	const int maxSamples = min(numInSamples, numOutSamples);
	for (int i = 0; i < maxSamples; ++i)
	{
		const float src_val = ((short)in[i] * 256 - SHRT_MAX) * volume;
		const int result = (((SHRT_MAX - out[i]) * src_val) / SHRT_MAX) + out[i];
		out[i] = clamp(result, SHRT_MIN, SHRT_MAX);
	}

	return maxSamples;
}

// mix 8 bit stereo into 16 bit stereo
static int MixStereo8(float volume, const uint8* in, int numInSamples, short* out, int numOutSamples)
{
	const int maxSamples = 2 * min(numInSamples, numOutSamples);
	for (int i = 0; i < maxSamples; ++i)
	{
		const float src_val = ((short)in[i] * 256 - SHRT_MAX) * volume;
		const int result = (((SHRT_MAX - out[i]) * src_val) / SHRT_MAX) + out[i];
		out[i] = clamp(result, SHRT_MIN, SHRT_MAX);
	}

	return maxSamples;
}

// mix 16 bit mono into 16 bit mono sound
static int MixMono16(float volume, const short* in, int numInSamples, short* out, int numOutSamples)
{
	const int maxSamples = min(numInSamples, numOutSamples);
	for (int i = 0; i < maxSamples; ++i)
	{
		const float src_val = in[i] * volume;
		const int result = (((SHRT_MAX - out[i]) * src_val) / SHRT_MAX) + out[i];
		out[i] = clamp(result, SHRT_MIN, SHRT_MAX);
	}

	return maxSamples;
}

// mix 16 bit stereo into 16 bit stereo
static int MixStereo16(float volume, const short* in, int numInSamples, short* out, int numOutSamples)
{
	const int maxSamples = 2 * min(numInSamples, numOutSamples);
	for (int i = 0; i < maxSamples; ++i)
	{
		const float src_val = in[i] * volume;
		const int result = (((SHRT_MAX - out[i]) * src_val) / SHRT_MAX) + out[i];
		out[i] = clamp(result, SHRT_MIN, SHRT_MAX);
	}

	return maxSamples;
}

ALsizei CEqAudioSourceAL::GetSampleBuffer(void* data, ALsizei size)
{
	const bool looping = m_looping;

	if (m_streams.numElem() == 1)
	{
		SourceStream& mainStream = GetSourceStream();
		ISoundSource* sample = mainStream.sample;

		int loopPoints[SOUND_SOURCE_MAX_LOOP_REGIONS * 2];
		const int numLoopRegions = sample->GetLoopRegions(loopPoints);

		const ISoundSource::Format& fmt = sample->GetFormat();
		const int sampleUnit = (fmt.bitwidth >> 3);
		const int sampleSize = sampleUnit * fmt.channels;

		const int streamPos = mainStream.curPos;
		const int samplesRead = sample->GetSamples(data, size / sampleSize, streamPos, looping);

		mainStream.curPos = WrapAroundSampleOffset(streamPos + samplesRead, sample, looping);

		// clever, if numRead < size then source will be stopped automatically!
		return samplesRead * sampleSize;
	}

	// TODO: perform sample rate conversion

	// silence before mix
	memset(data, 0, size);
	
	// We are mixing always into 16 bit no matter what
	const int sizeOfChannels = sizeof(short) * m_bufferChannels;
	const int numSamplesToRead = size / sizeOfChannels;
	int numRead = 0;

	// we can mix up to 8 samples simultaneously
	for(int i = 0; i < m_streams.numElem(); ++i)
	{
		ISoundSource* sample = m_streams[i].sample;
		const float sampleVolume = min(m_streams[i].volume, 1.0f);

		int loopPoints[SOUND_SOURCE_MAX_LOOP_REGIONS * 2];
		const int numLoopRegions = sample->GetLoopRegions(loopPoints);

		if (sampleVolume <= 0.0f)
		{
			// update playback progress still but don't mix
			m_streams[i].curPos = WrapAroundSampleOffset(m_streams[i].curPos + numSamplesToRead, sample, looping);
			numRead = max(numRead, numSamplesToRead);
			continue;
		}

		const ISoundSource::Format& fmt = sample->GetFormat();
		const int sampleUnit = (fmt.bitwidth >> 3);
		const int sampleSize = sampleUnit;
		const int sampleCount = sample->GetSampleCount();

		const int streamPos = m_streams[i].curPos;
		int samplesRead = 0;
		if (sampleUnit == sizeof(uint8))
		{
			uint8* tmpSamples = (uint8*)stackalloc(numSamplesToRead * sampleSize * fmt.channels);
			samplesRead = sample->GetSamples(tmpSamples, numSamplesToRead, streamPos, looping);

			if(fmt.channels == 1)
				MixMono8(sampleVolume, tmpSamples, samplesRead, (int16*)data, size);
			else if(fmt.channels == 2)
				MixStereo8(sampleVolume, tmpSamples, samplesRead, (int16*)data, size);
		}
		else if (sampleUnit == sizeof(uint16))
		{
			int16* tmpSamples = (int16*)stackalloc(numSamplesToRead * sampleSize * fmt.channels);
			samplesRead = sample->GetSamples(tmpSamples, numSamplesToRead, streamPos, looping);

			if (fmt.channels == 1)
				MixMono16(sampleVolume, tmpSamples, samplesRead, (int16*)data, size);
			else if (fmt.channels == 2)
				MixStereo16(sampleVolume, tmpSamples, samplesRead, (int16*)data, size);
		}
		
		m_streams[i].curPos = WrapAroundSampleOffset(streamPos + samplesRead, sample, looping);
		numRead = max(numRead, samplesRead);
	}

	return numRead * sizeOfChannels;
}

void CEqAudioSourceAL::SetupSample(const ISoundSource* sample)
{
	ASSERT_MSG(sample, "SetupSample - No samples");

	// setup voice defaults
	SourceStream& stream = m_streams.append();
	stream.sample = const_cast<ISoundSource*>(sample);

	if (!sample->IsStreaming())
	{
#if USE_ALSOFT_BUFFER_CALLBACK
		if (alBufferCallbackSOFT)
		{
			// set the callback on AL buffer
			// alBufferData will reset this to NULL for us
			const ISoundSource::Format& fmt = sample->GetFormat();
			ALenum alFormat = GetSoundSourceFormatAsALEnum(fmt);

			alBufferCallbackSOFT(m_buffers[0], alFormat, fmt.frequency, SoundSourceSampleDataCallback, this);
			alSourcei(m_source, AL_BUFFER, m_buffers[0]);
		}
		else
#endif
		{
			CSoundSource_OpenALCache* alSource = (CSoundSource_OpenALCache*)sample;
			alSourcei(m_source, AL_BUFFER, alSource->m_alBuffer);
		}
	}
}

void CEqAudioSourceAL::SetupSamples(ArrayCRef<const ISoundSource*> samples)
{
#if !USE_ALSOFT_BUFFER_CALLBACK
	ASSERT_MSG(samples.numElem() > 1, "SetupSample - USE_ALSOFT_BUFFER_CALLBACK required to setup more than one samples");
#endif

	ASSERT_MSG(samples.numElem() > 0, "SetupSample - No samples");
	ASSERT_MSG(samples.numElem() < EQSND_SAMPLE_COUNT, "SetupSamples - exceeding EQSND_SAMPLE_COUNT (%d), required %d", EQSND_SAMPLE_COUNT, samples.numElem());

	// set the callback on AL buffer
	// alBufferData will reset this to NULL for us
	const ISoundSource::Format& fmt = samples.front()->GetFormat();
	ALenum alFormat = GetSoundSourceFormatAsALEnum(fmt);

	// create streams
	for (int i = 0; i < samples.numElem(); ++i)
	{
		SourceStream& stream = m_streams.append();
		stream.sample = const_cast<ISoundSource*>(samples[i]);
	}

	m_bufferChannels = fmt.channels;

	if (!m_streams.front().sample->IsStreaming())
	{
#if USE_ALSOFT_BUFFER_CALLBACK // all this possible because of this
		if (samples.numElem() > 1)
		{
			int channels = 1;

			// validate each sample
			for (int i = 0; i < m_streams.numElem(); ++i)
			{
				ISoundSource* sample = m_streams[i].sample;
				channels = max(sample->GetFormat().channels, channels);

				if (sample->IsStreaming())
				{
					m_streams[i].sample = nullptr;
					ASSERT_FAIL("Streaming is not yet supported with multi-sample feature");
					continue;
				}

				// extra validations?
			}

			// For multi-sample sound we need to specify the best format to work with if they are different
			// So we're mixing always into 16 bit no matter what
			if (channels == 1)
				alFormat = AL_FORMAT_MONO16;
			else if (channels == 2)
				alFormat = AL_FORMAT_STEREO16;

			m_bufferChannels = channels;
		}

		if (alBufferCallbackSOFT)
		{
			alBufferCallbackSOFT(m_buffers[0], alFormat, fmt.frequency, SoundSourceSampleDataCallback, this);
			alSourcei(m_source, AL_BUFFER, m_buffers[0]);
		}
		else
#endif
		{
			CSoundSource_OpenALCache* alSource = (CSoundSource_OpenALCache*)samples[0];
			alSourcei(m_source, AL_BUFFER, alSource->m_alBuffer);
		}
	}

}

bool CEqAudioSourceAL::QueueStreamChannel(ALuint buffer)
{
	static ubyte pcmBuffer[EQSND_STREAM_BUFFER_SIZE];

	SourceStream& mainStream = GetSourceStream();
	ISoundSource* sample = mainStream.sample;
	const int streamPos = mainStream.curPos;

	const ISoundSource::Format& fmt = sample->GetFormat();
	ALenum alFormat = GetSoundSourceFormatAsALEnum(fmt);
	const int sampleSize = (fmt.bitwidth >> 3) * fmt.channels;

	// read sample data and update AL buffers
	const int numRead = sample->GetSamples(pcmBuffer, EQSND_STREAM_BUFFER_SIZE / sampleSize, streamPos, m_looping);

	if (numRead > 0)
	{
		mainStream.curPos = WrapAroundSampleOffset(streamPos + numRead, sample, m_looping);

		// upload to specific buffer
		alBufferData(buffer, alFormat, pcmBuffer, numRead * sampleSize, fmt.frequency);

		// queue after uploading
		alSourceQueueBuffers(m_source, 1, &buffer);
	}

	return numRead > 0;
}

// dequeues buffers
void CEqAudioSourceAL::EmptyBuffers()
{
	// stop source
	alSourceStop(m_source);

	int sourceType;
	alGetSourcei(m_source, AL_SOURCE_TYPE, &sourceType);
	if (sourceType == AL_STREAMING)
	{
		// dequeue buffers
		int numQueued = 0;
		alGetSourcei(m_source, AL_BUFFERS_QUEUED, &numQueued);

		ALuint qbuffer;
		while (numQueued--)
			alSourceUnqueueBuffers(m_source, 1, &qbuffer);

		// make silent buffer (this also removes callback)
		for (int i = 0; i < EQSND_STREAM_BUFFER_COUNT; i++)
			alBufferData(m_buffers[i], AL_FORMAT_MONO16, (short*)_silence, BUFFER_SILENCE_SIZE, 8000);
	}
	else
	{
		alSourcei(m_source, AL_BUFFER, 0);
	}
}