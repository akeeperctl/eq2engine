extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>
#include <libavcodec/avcodec.h>
}

#include "core/core_common.h"
#include "core/IFileSystem.h"
#include "audio/IEqAudioSystem.h"
#include "materialsystem1/IMaterialSystem.h"

#include "VideoPlayer.h"

using namespace Threading;

enum DecodeState : int
{
	DEC_ERROR = -1,
	DEC_DEQUEUE_PACKET = 0,
	DEC_SEND_PACKET,
	DEC_RECV_FRAME,
	DEC_READY_FRAME,
};

struct VideoPlayerData
{
	AVPacket			packet;
	AVFormatContext*	formatCtx{ nullptr };

	// Video
	struct VideoState
	{
		CEqTimer	timer;
		int64_t		videoOffset{ 0 };
		int64_t		lastVideoPts{ 0 };
		double		presentationDelay{ 0.0f };

		AVFrame*	frame{ nullptr };
		AVPacket*	deqPacket{ nullptr };
		DecodeState state;
		bool		presentFlag{ false };

	} videoState;

	AVStream*		videoStream{ nullptr };
	AVCodecContext* videoCodec{ nullptr };
	SwsContext*		videoSws{ nullptr };
	VPacketQueue		videoPacketQueue;

	// Audio
	struct AudioState
	{
		int64_t		audioOffset{ 0 };
		int64_t		lastAudioPts{ 0 };

		AVFrame*	frame{ nullptr };
		AVPacket*	deqPacket{ nullptr };

		DecodeState state;
	} audioState;

	AVStream*		audioStream{ nullptr };
	AVCodecContext* audioCodec{ nullptr };
	SwrContext*		audioSwr{ nullptr };
	APacketQueue	audioPacketQueue;
	FrameQueue		audioFrameQueue;

	float			clockSpeed{ 1.0f };
	int64_t			clockStartTime{ AV_NOPTS_VALUE };
};

static int CreateCodec(AVCodecContext** _cc, AVStream* stream, AVBufferRef* hwDeviceCtx)
{
	bool failed = false;

	const AVCodec* c = avcodec_find_decoder(stream->codecpar->codec_id);
	if (!c)
		return -1;

	AVCodecContext* cc = avcodec_alloc_context3(c);
	if (cc == nullptr)
		return -1;

	defer{
		if (failed && cc)
			avcodec_free_context(&cc);
	};

	int r = avcodec_parameters_to_context(cc, stream->codecpar);
	if (r != 0)
	{
		failed = true;
		return -1;
	}

	if (hwDeviceCtx)
		cc->hw_device_ctx = av_buffer_ref(hwDeviceCtx);

	r = avcodec_open2(cc, c, nullptr);
	if (r < 0)
	{
		failed = true;
		return -1;
	}

	*_cc = cc;
	return 0;
}

static VideoPlayerData* CreatePlayerData(AVBufferRef* hw_device_context, const char* filename)
{
	VideoPlayerData* player = PPNew VideoPlayerData;

	bool failed = false;
	defer{
		if (!failed || !player)
			return;

		sws_freeContext(player->videoSws);

		if (player->audioCodec)
			avcodec_free_context(&player->audioCodec);

		if (player->audioSwr)
			swr_free(&player->audioSwr);

		if (player->videoCodec)
			avcodec_free_context(&player->videoCodec);

		if (player->formatCtx)
			avformat_close_input(&player->formatCtx);

		SAFE_DELETE(player);
	};

	const EqString fsFilePath = g_fileSystem->FindFilePath(filename);
	if (avformat_open_input(&player->formatCtx, fsFilePath, nullptr, nullptr) != 0)
	{
		MsgError("Failed to open video file %s\n", filename);
		failed = true;
		return nullptr;
	}

	if (avformat_find_stream_info(player->formatCtx, nullptr) < 0)
	{
		MsgError("Failed to find stream info\n");
		failed = true;
		return nullptr;
	}

	AVCodec* c = nullptr;
	for (uint i = 0; i < player->formatCtx->nb_streams; ++i)
	{
		AVStream* stream;
		stream = player->formatCtx->streams[i];

		{
			int r = player->videoStream == nullptr;
			r = r && (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO);
			if (r)
				player->videoStream = stream;
		}

		{
			int r = player->audioStream == nullptr;
			r = r && (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO);
			if (r)
				player->audioStream = stream;
		}

		if (player->audioStream && player->videoStream)
			break;
	}

	if (player->videoStream == nullptr)
	{
		MsgError("No video/audio supported stream found\n");
		failed = true;
		return nullptr;
	}

	// Setup video
	if (CreateCodec(&player->videoCodec, player->videoStream, hw_device_context) != 0)
	{
		failed = true;
		return nullptr;
	}

	const bool isD3D = g_pShaderAPI->GetShaderAPIClass() == SHADERAPI_DIRECT3D9 || g_pShaderAPI->GetShaderAPIClass() == SHADERAPI_DIRECT3D10;

	player->videoSws = sws_getContext(
		player->videoCodec->width, player->videoCodec->height,
		player->videoCodec->pix_fmt,
		player->videoCodec->width, player->videoCodec->height,
		isD3D ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGBA,
		SWS_POINT,
		nullptr,
		nullptr,
		nullptr);

	// Setup audio
	if (player->audioStream)
	{
		if (CreateCodec(&player->audioCodec, player->audioStream, nullptr) != 0)
		{
			failed = true;
			return nullptr;
		}

		player->audioSwr = swr_alloc_set_opts(nullptr,
			AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,
			player->audioStream->codecpar->channel_layout, (enum AVSampleFormat)player->audioStream->codecpar->format, player->audioStream->codecpar->sample_rate,
			0, nullptr);

		if (player->audioSwr == nullptr)
		{
			failed = true;
			return nullptr;
		}

		if (swr_init(player->audioSwr) < 0)
		{
			MsgError("Unable to create SWResample\n");
			failed = true;
			return nullptr;
		}
	}

	return player;
}

static void FreePlayerData(VideoPlayerData** player)
{
	if (!player)
		return;

	VideoPlayerData* p = *player;
	if (!p)
		return;

	avformat_close_input(&p->formatCtx);

	if (p->videoStream)
	{
		avcodec_free_context(&p->videoCodec);
		sws_freeContext(p->videoSws);

		if (p->videoState.deqPacket)
			av_packet_free(&p->videoState.deqPacket);

		if (p->videoState.frame)
			av_frame_free(&p->videoState.frame);
	}

	if (p->audioStream)
	{
		avcodec_free_context(&p->audioCodec);
		swr_free(&p->audioSwr);

		if (p->audioState.deqPacket)
			av_packet_free(&p->audioState.deqPacket);

		if (p->audioState.frame)
			av_frame_free(&p->audioState.frame);

		p->audioStream = nullptr;
	}

	SAFE_DELETE(*player);
}

static bool PlayerDemuxStep(VideoPlayerData* player)
{
	if (player->videoPacketQueue.getCount() >= AV_PACKET_VIDEO_CAPACITY)
		return true;

	if (player->audioPacketQueue.getCount() >= AV_PACKET_AUDIO_CAPACITY)
		return true;

	VideoPlayerData::VideoState& videoState = player->videoState;
	VideoPlayerData::AudioState& audioState = player->audioState;

	AVPacket& packet = player->packet;

	bool isError = false;

	switch (av_read_frame(player->formatCtx, &packet))
	{
	case 0:
		if (player->videoStream && (packet.stream_index == player->videoStream->index))
		{
			AVPacket* videoPacket = av_packet_clone(&packet);
			videoPacket->pts += videoState.videoOffset;
			videoPacket->dts += videoState.videoOffset;
			videoState.lastVideoPts = videoPacket->pts;
			
			player->videoPacketQueue.append(videoPacket);
		}
		else if (player->audioStream && (packet.stream_index == player->audioStream->index))
		{
			AVPacket* audioPacket = av_packet_clone(&packet);
			audioPacket->pts += audioState.audioOffset;
			audioPacket->dts += audioState.audioOffset;
			audioState.lastAudioPts = audioPacket->pts;
			
			player->audioPacketQueue.append(audioPacket);
		}
		break;
	case AVERROR_EOF:
	{
		// restart
		if (av_seek_frame(player->formatCtx, player->videoStream->index, player->videoStream->start_time, AVSEEK_FLAG_BACKWARD) < 0)
		{
			DevMsg(DEVMSG_CORE, "Failed av_seek_frame\n");
			isError = true;
		}

		if (player->videoStream)
			videoState.videoOffset = videoState.lastVideoPts;

		if (player->audioStream)
			audioState.audioOffset = audioState.lastAudioPts;

		break;
	}
	default:
		DevMsg(DEVMSG_CORE, "Failed av_read_frame\n");
		isError = true;
	}

	// don't forget, otherwise memleak
	av_packet_unref(&packet);

	return !isError;
}

static double clock_seconds(int64_t start_time)
{
	return (av_gettime() - start_time) / 1000000.;
}

static double pts_seconds(AVFrame* frame, AVStream* stream)
{
	return frame->pts * av_q2d(stream->time_base);
}

static void PlayerVideoDecodeStep(VideoPlayerData* player, ITexturePtr texture)
{
	if (!player->videoStream)
		return;

	VideoPlayerData::VideoState& videoState = player->videoState;
	DecodeState& state = player->videoState.state;

	if (state == DEC_DEQUEUE_PACKET)
	{
		if (player->videoPacketQueue.getCount() == 0)
			return;

		// must delete old packet as it appears to be processed already
		av_packet_free(&videoState.deqPacket);
		videoState.deqPacket = player->videoPacketQueue.popFront();

		state = DEC_SEND_PACKET;
	}
	else if (state == DEC_SEND_PACKET)
	{
		const int r = avcodec_send_packet(player->videoCodec, videoState.deqPacket);
		if (r == 0)
		{
			state = DEC_DEQUEUE_PACKET;
			return;
		}

		if (r != AVERROR(EAGAIN))
		{
			state = DEC_ERROR;
			return;
		}
		state = DEC_RECV_FRAME;
	}
	else if (state == DEC_RECV_FRAME)
	{
		const int r = avcodec_receive_frame(player->videoCodec, videoState.frame);
		if (r == AVERROR(EAGAIN))
		{
			state = DEC_SEND_PACKET;
			return;
		}

		if (r != 0)
		{
			state = DEC_ERROR;
			return;
		}

		videoState.frame->pts = videoState.frame->pts / player->clockSpeed;

		const double clock = clock_seconds(player->clockStartTime);
		const double pts_secs = pts_seconds(videoState.frame, player->videoStream);
		const double delta = (pts_secs - clock);

		if (delta < -0.01f)
		{
			state = DEC_RECV_FRAME;
			return;
		}

		videoState.presentationDelay = delta;
		
		state = DEC_READY_FRAME;
	}
	else if (state == DEC_READY_FRAME)
	{
		// wait extra amount of time
		if (videoState.presentationDelay > videoState.timer.GetTime())
			return;

		videoState.timer.GetTime(true);
		state = DEC_RECV_FRAME;

		if (!videoState.presentFlag)
			return;
		videoState.presentFlag = false;

		// time to update renderer texture
		ITexture::LockInOutData writeTo(TEXLOCK_DISCARD);
		if (texture->Lock(writeTo))
		{
			uint8_t* data[1] = {
				(ubyte*)writeTo.lockData
			};
			const int stride[1] = {
				(int)player->videoCodec->width * 4
			};
			sws_scale(player->videoSws, videoState.frame->data, videoState.frame->linesize, 0, videoState.frame->height, data, stride);
			texture->Unlock();
		}
	}
}

static void PlayerAudioDecodeStep(VideoPlayerData* player, FrameQueue& frameQueue)
{
	if (!player->audioStream)
		return;

	VideoPlayerData::AudioState& audioState = player->audioState;
	DecodeState& state = player->audioState.state;

	if (state == DEC_DEQUEUE_PACKET)
	{
		if (player->audioPacketQueue.getCount() == 0)
			return;

		av_packet_free(&audioState.deqPacket);
		audioState.deqPacket = player->audioPacketQueue.popFront();

		state = DEC_SEND_PACKET;
	}
	else if (state == DEC_SEND_PACKET)
	{
		int r = avcodec_send_packet(player->audioCodec, audioState.deqPacket);
		if (r == 0)
		{
			state = DEC_DEQUEUE_PACKET;
			return;
		}

		if (r != AVERROR(EAGAIN))
		{
			state = DEC_ERROR;
			return;
		}
		state = DEC_RECV_FRAME;
	}
	else if (state == DEC_RECV_FRAME)
	{
		int r = avcodec_receive_frame(player->audioCodec, audioState.frame);
		if (r == AVERROR(EAGAIN))
		{
			state = DEC_SEND_PACKET;
			return;
		}

		if (r != 0)
		{
			state = DEC_ERROR;
			return;
		}

		state = DEC_RECV_FRAME;
		
		AVFrame* convFrame = av_frame_alloc();
		convFrame->channels = 2;
		convFrame->channel_layout = AV_CH_LAYOUT_STEREO;
		convFrame->format = AV_SAMPLE_FMT_S16;
		convFrame->sample_rate = 44100;
		convFrame->pts = static_cast<double>(audioState.frame->pts) / player->clockSpeed;
		if (swr_convert_frame(player->audioSwr, convFrame, audioState.frame) == 0)
		{
			if(frameQueue.getCount() < AV_PACKET_AUDIO_CAPACITY)
				frameQueue.append(convFrame);
		}
		else
			av_frame_free(&convFrame);
	}
}

static bool StartPlayback(VideoPlayerData* player)
{
	if (!player)
		return false;

	if (player->videoStream)
	{
		av_init_packet(&player->packet);

		{
			VideoPlayerData::VideoState& videoState = player->videoState;
			videoState.frame = av_frame_alloc();
			videoState.state = DEC_DEQUEUE_PACKET;
			videoState.deqPacket = nullptr;
			videoState.timer.GetTime(true);
		}

		{
			VideoPlayerData::AudioState& audioState = player->audioState;
			audioState.frame = av_frame_alloc();
			audioState.state = DEC_DEQUEUE_PACKET;
			audioState.deqPacket = nullptr;
		}
	}

	return true;
}

//---------------------------------------------------

CVideoAudioSource::CVideoAudioSource()
{
	m_format.channels = 2;
	m_format.frequency = 44100;
	m_format.bitwidth = 16;
	m_format.dataFormat = 1;
}

AVFrame* popFrame(FrameQueue& queue)
{
	return queue.popFront();
}

int CVideoAudioSource::GetSamples(void* out, int samplesToRead, int startOffset, bool loop) const
{
	const int sampleSize = m_format.channels * (m_format.bitwidth >> 3);

	if (m_frameQueue.getCount() < 1)
	{
		// we don't have frames yet, return 1 because we need a warmup from video system
		return 1;
	}

	const int requestedSamples = samplesToRead;
	int numSamplesRead = 0;
	int numFramesProcessed = 0;
	while (samplesToRead > 0)
	{
		if (m_frameQueue.getCount() == 0)
			break;

		AVFrame* frame = m_frameQueue.front();
		const int frameSamples = frame->nb_samples - frame->height;
		const int paintSamples = min(samplesToRead, frameSamples);

		memcpy((ubyte*)out + numSamplesRead * sampleSize,
			frame->data[0] + frame->height * sampleSize,
			paintSamples * sampleSize);

		numSamplesRead += paintSamples;
		samplesToRead -= paintSamples;
		++numFramesProcessed;

		if (paintSamples == frameSamples)
		{
			popFrame(*const_cast<FrameQueue*>(&m_frameQueue));
			av_frame_free(&frame);
		}
		else
		{
			// keep the frame but we keep read cursor
			frame->height += paintSamples;
		}
	}

	if (numSamplesRead < samplesToRead)
		DevMsg(DEVMSG_CORE, "CVideoAudioSource::GetSamples underpaint - %d of %d\n", numSamplesRead, samplesToRead);

	return numSamplesRead;
}

int	CVideoAudioSource::GetSampleCount() const
{
	return INT_MAX;
}

//---------------------------------------------------

CVideoPlayer::CVideoPlayer()
{
	// due to this thing is pushed to audio cache, it must not be deleted
	m_audioSrc.Ref_Grab();
}

CVideoPlayer::~CVideoPlayer()
{
	Shutdown();
}

int	CVideoPlayer::Run()
{
	if (!m_player)
		return 0;

	m_player->clockStartTime = av_gettime();

	while (true)
	{
		YieldCurrentThread();

		if (m_pendingQuit)
			break;

		if (!PlayerDemuxStep(m_player))
			break;

		PlayerVideoDecodeStep(m_player, m_texture);
		PlayerAudioDecodeStep(m_player, m_audioSrc.m_frameQueue);
	}

	return 0;
}

bool CVideoPlayer::Init(const char* pathToVideo)
{
	m_player = CreatePlayerData(nullptr, pathToVideo);
	if(m_player)
	{
		const AVCodecContext* codec = m_player->videoCodec;

		if (m_player->videoStream)
		{
			m_texture = g_pShaderAPI->CreateProceduralTexture(pathToVideo, FORMAT_RGBA8, codec->width, codec->height, 1, 1, TEXFILTER_LINEAR);
		}

		if (m_player->audioStream)
		{
			m_audioSrc.SetFilename(pathToVideo);
			g_audioSystem->AddSample(&m_audioSrc);
		}
	}

	return m_player != nullptr;
}

void CVideoPlayer::Shutdown()
{
	Stop();
	FreePlayerData(&m_player);
	g_audioSystem->OnSampleDeleted(&m_audioSrc);
	m_texture = nullptr;
}

void CVideoPlayer::Start()
{
	if (!StartPlayback(m_player))
		return;
	StartThread("vidPlayer");
}

void CVideoPlayer::Stop()
{
	if (!m_player)
		return;
	VideoPlayerData* player = m_player;

	m_pendingQuit = true;
	WaitForThread();

	for (auto item = player->videoPacketQueue.begin(); item; item = item->next)
		av_packet_free(&item->value);

	for (auto item = player->audioPacketQueue.begin(); item; item = item->next)
		av_packet_free(&item->value);

	player->videoPacketQueue.clear();
	player->audioPacketQueue.clear();

	m_pendingQuit = false;
}

void CVideoPlayer::Present()
{
	if (!m_player)
		return;

	m_player->videoState.presentFlag = true;
}

void CVideoPlayer::SetTimeScale(float value)
{
	if(m_player)
		m_player->clockSpeed = value;
}

ITexturePtr CVideoPlayer::GetImage() const
{
	return m_texture;
}