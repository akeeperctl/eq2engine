//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium OpenGL ShaderAPI
//////////////////////////////////////////////////////////////////////////////////

#include "core/core_common.h"
#include "core/ConVar.h"
#include "core/IConsoleCommands.h"

#include "shaderapigl_def.h"
#include "GLTexture.h"
#include "GLWorker.h"
#include "ShaderAPIGL.h"
#include "imaging/ImageLoader.h"

DECLARE_CVAR(gl_skipTextures, "0", nullptr, CV_CHEAT);

Threading::CEqMutex g_sapi_ProgressiveTextureMutex;

static GLTextureRef_t invalidTexture = {0, IMAGE_TYPE_INVALID};

CGLTexture::CGLTexture()
{
	m_flLod = 0.0f;
	m_glTarget = GL_NONE;
	m_glDepthID = GL_NONE;
	m_texSize = 0;
}

CGLTexture::~CGLTexture()
{
	Release();
}

void CGLTexture::Ref_DeleteObject()
{
	s_renderApi.FreeTexture(this);

	g_glWorker.Execute(__func__, [this]() {
		delete this;
		return 0;
	});
}

void CGLTexture::Release()
{
	ReleaseTextures();
}

void CGLTexture::ReleaseTextures()
{
	{
		Threading::CScopedMutex m(g_sapi_ProgressiveTextureMutex);
		m_progressiveState.clear();
		s_renderApi.m_progressiveTextures.remove(this);
	}

	if (m_glTarget == GL_RENDERBUFFER)
	{
		glDeleteRenderbuffers(1, &m_glDepthID);
		GLCheckError("del tex renderbuffer");
		m_glDepthID = GL_NONE;
	}
	else
	{
		for (int i = 0; i < m_textures.numElem(); i++)
		{
			glDeleteTextures(1, &m_textures[i].glTexID);
			GLCheckError("del tex");
		}
		m_textures.clear();

		if (m_glDepthID != GL_NONE)
		{
			glDeleteTextures(1, &m_glDepthID);
			GLCheckError("del depth");
		}
		m_glDepthID = GL_NONE;
	}
	m_texSize = 0;
	m_glTarget = GL_NONE;
}

void SetupGLSamplerState(uint texTarget, const SamplerStateParams &sampler, int mipMapCount)
{
	// Set requested wrapping modes
	glTexParameteri(texTarget, GL_TEXTURE_WRAP_S, g_gl_texAddrModes[sampler.addressS]);
	GLCheckError("smp w s");

#ifndef USE_GLES2
	if (texTarget != GL_TEXTURE_1D)
#endif // USE_GLES2
	{
		glTexParameteri(texTarget, GL_TEXTURE_WRAP_T, g_gl_texAddrModes[sampler.addressT]);
		GLCheckError("smp w t");
	}

	if (texTarget == GL_TEXTURE_3D)
	{
		glTexParameteri(texTarget, GL_TEXTURE_WRAP_R, g_gl_texAddrModes[sampler.addressR]);
		GLCheckError("smp w r");
	}

	// Set requested filter modes
	glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, g_gl_texMinFilters[sampler.magFilter]);
	GLCheckError("smp mag");

	glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, g_gl_texMinFilters[sampler.minFilter]);
	GLCheckError("smp min");

	glTexParameteri(texTarget, GL_TEXTURE_MAX_LEVEL, max(mipMapCount - 1, 0));
	GLCheckError("smp mip");

	if(sampler.compareFunc == COMPFUNC_NONE)
	{
		glTexParameteri(texTarget, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		GLCheckError("smp cmpmode");
	}
	else
	{
		glTexParameteri(texTarget, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		GLCheckError("smp cmpmode");

		glTexParameteri(texTarget, GL_TEXTURE_COMPARE_FUNC, g_gl_depthConst[sampler.compareFunc]);
		GLCheckError("smp cmpfunc");
	}

#if GL_ARB_texture_filter_anisotropic
	if (sampler.aniso > 1 && GLAD_GL_ARB_texture_filter_anisotropic)
	{
		glTexParameteri(texTarget, GL_TEXTURE_MAX_ANISOTROPY, sampler.aniso);
		GLCheckError("smp aniso");
	}
#elif GL_EXT_texture_filter_anisotropic
	if (sampler.aniso > 1 && GLAD_GL_EXT_texture_filter_anisotropic)
	{
		glTexParameteri(texTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, sampler.aniso);
		GLCheckError("smp aniso");
	}
#endif
}

GLTextureRef_t CGLTexture::CreateGLTexture(const CImage *img, const SamplerStateParams &sampler, int startMip, int mipCount) const
{
	// EImageType type, ETextureFormat format, int mipCount, int widthMip0, int heightMip0, int depthMip0

	const EImageType type = img->GetImageType();
	const ETextureFormat format = img->GetFormat();

	GLTextureRef_t glTexture;
	glTexture.type = type;

	glGenTextures(1, &glTexture.glTexID);
	if (!GLCheckError("gen tex"))
		return invalidTexture;

	const GLenum glTarget = g_gl_texTargetType[type];
	const GLenum internalFormat = PickGLInternalFormat(format);

	glBindTexture(glTarget, glTexture.glTexID);
	GLCheckError("bind tex");

#ifdef USE_GLES2
	int widthMip0 = img->GetWidth(startMip);
	int heightMip0 = img->GetHeight(startMip);
	int depthMip0 = img->GetDepth(startMip);

	if (type == IMAGE_TYPE_CUBE)
	{
		glTexStorage2D(glTarget, mipCount, internalFormat, widthMip0, heightMip0);
	}
	else if (type == IMAGE_TYPE_3D)
	{
		glTexStorage3D(glTarget, mipCount, internalFormat, widthMip0, heightMip0, depthMip0);
	}
	else if (type == IMAGE_TYPE_2D || type == IMAGE_TYPE_1D)
	{
		glTexStorage2D(glTarget, mipCount, internalFormat, widthMip0, heightMip0);
	}
	else
	{
		ASSERT_FAIL("Invalid texture type!");
	}
	GLCheckError("create tex storage");
#else
	const GLenum srcFormat = g_gl_chanCountTypes[GetChannelCount(format)];
	const GLenum srcType = g_gl_chanTypePerFormat[format];

	if (type == IMAGE_TYPE_CUBE)
	{
		for (int i = 0; i < mipCount; ++i)
		{
			const int width = img->GetWidth(startMip + i);
			const int height = img->GetHeight(startMip + i);

			if (IsCompressedFormat(format))
			{
				for (int j = 0; j < 6; ++j)
				{
					glCompressedTexImage2D(glTarget, i, internalFormat, width, height, 0, img->GetMipMappedSize(startMip + i, 1), nullptr);
					GLCheckError("create tex2D cube %d compr storage level %d", j, i);
				}
			}
			else
			{
				for (int j = 0; j < 6; ++j)
				{
					glTexImage2D(glTarget, i, internalFormat, width, height, 0, srcFormat, srcType, nullptr);
					GLCheckError("create tex storage level %d", i);
				}
			}
		}
	}
	else if (type == IMAGE_TYPE_3D)
	{
		for (int i = 0; i < mipCount; ++i)
		{
			const int width = img->GetWidth(startMip + i);
			const int height = img->GetHeight(startMip + i);
			const int depth = img->GetDepth(startMip + i);

			if (IsCompressedFormat(format))
			{
				glCompressedTexImage3D(glTarget, i, internalFormat, width, height, depth, 0, img->GetMipMappedSize(startMip + i, 1), nullptr);
				GLCheckError("create tex3D compr storage level %d", i);
			}
			else
			{
				glTexImage3D(glTarget, i, internalFormat, width, height, depth, 0, srcFormat, srcType, nullptr);
				GLCheckError("create tex storage level %d", i);
				GLCheckError("create tex storage level %d", i);
			}
		}
	}
	else if (type == IMAGE_TYPE_2D || type == IMAGE_TYPE_1D)
	{
		for (int i = 0; i < mipCount; ++i)
		{
			const int width = img->GetWidth(startMip + i);
			const int height = img->GetHeight(startMip + i);

			if (IsCompressedFormat(format))
			{
				glCompressedTexImage2D(glTarget, i, internalFormat, width, height, 0, img->GetMipMappedSize(startMip + i, 1), nullptr);
				GLCheckError("create tex2D compr storage level %d", i);
			}
			else
			{
				glTexImage2D(glTarget, i, internalFormat, width, height, 0, srcFormat, srcType, nullptr);
				GLCheckError("create tex2D storage level %d", i);
			}
		}
	}
	else
	{
		ASSERT_FAIL("Invalid texture type!");
	}
#endif

	SetupGLSamplerState(glTarget, sampler, mipCount);

	return glTexture;
}

static bool UpdateGLTextureFromImageMipmap(GLTextureRef_t texture, CImage *image, int sourceMipLevel, int targetMipLevel)
{
	const GLenum glTarget = g_gl_texTargetType[texture.type];
	const ETextureFormat format = image->GetFormat();

	const GLenum srcFormat = g_gl_chanCountTypes[GetChannelCount(format)];
	const GLenum srcType = g_gl_chanTypePerFormat[format];

	const GLenum internalFormat = PickGLInternalFormat(format);

	glBindTexture(glTarget, texture.glTexID);
	GLCheckError("bind tex");

	// Upload it all
	ubyte *src = image->GetPixels(sourceMipLevel);

	const int size = image->GetMipMappedSize(sourceMipLevel, 1);

	const int width = image->GetWidth(sourceMipLevel);
	const int height = image->GetHeight(sourceMipLevel);

	if (texture.type == IMAGE_TYPE_3D)
	{
		if (IsCompressedFormat(format))
		{
			glCompressedTexSubImage3D(glTarget, targetMipLevel,
									  0, 0, 0,
									  width, height, image->GetDepth(sourceMipLevel), internalFormat, size, src);
		}
		else
		{
			glTexSubImage3D(glTarget, targetMipLevel, 0, 0, 0,
							width, height, image->GetDepth(sourceMipLevel), srcFormat, srcType, src);
		}
		GLCheckError("tex upload 3d (mip %d)", sourceMipLevel);
	}
	else if (texture.type == IMAGE_TYPE_CUBE)
	{
		const int cubeFaceSize = size / 6;

		for (uint i = 0; i < 6; i++)
		{
			if (IsCompressedFormat(format))
			{
				glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetMipLevel, 0, 0,
										  width, height, internalFormat, cubeFaceSize, src + i * cubeFaceSize);
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetMipLevel, 0, 0,
								width, height, srcFormat, srcType, src + i * cubeFaceSize);
			}
			GLCheckError("tex upload cube (mip %d)", sourceMipLevel);
		}
	}
	else if (texture.type == IMAGE_TYPE_1D || IMAGE_TYPE_2D)
	{
		if (IsCompressedFormat(format))
		{
			glCompressedTexSubImage2D(glTarget, targetMipLevel, 0, 0,
									  width, height, internalFormat, size, src);
		}
		else
		{
			glTexSubImage2D(glTarget, targetMipLevel, 0, 0,
							width, height, srcFormat, srcType, src);
		}

		GLCheckError("tex upload 2d (mip %d)", sourceMipLevel);
	}

	glTexParameteri(glTarget, GL_TEXTURE_BASE_LEVEL, targetMipLevel);

	glBindTexture(glTarget, 0);
	GLCheckError("tex unbind");

	return true;
}

static bool UpdateGLTextureFromImage(GLTextureRef_t texture, CImage *image, int startMipLevel)
{
	const int numMipMaps = image->GetMipMapCount();
	int mipMapLevel = numMipMaps - 1;

	while (mipMapLevel >= startMipLevel)
	{
		const int lockBoxLevel = mipMapLevel - startMipLevel;

		UpdateGLTextureFromImageMipmap(texture, image, mipMapLevel, lockBoxLevel);
		--mipMapLevel;
	}

	return true;
}

// initializes texture from image array of images
bool CGLTexture::Init(const SamplerStateParams &sampler, const ArrayCRef<CRefPtr<CImage>> images, int flags)
{
	// FIXME: only release if pool, flags, format and size is different
	Release();

	m_samplerState = sampler;
	m_samplerState.aniso = max(s_renderApi.GetCaps().maxTextureAnisotropicLevel, sampler.aniso);
	m_iFlags = flags;

	HOOK_TO_CVAR(r_loadmiplevel);

	for (int i = 0; i < images.numElem(); i++)
	{
		if (images[i]->IsCube())
			m_iFlags |= TEXFLAG_CUBEMAP;
	}

	m_glTarget = g_gl_texTargetType[images[0]->GetImageType()];

	const int quality = (m_iFlags & TEXFLAG_NOQUALITYLOD) ? 0 : r_loadmiplevel->GetInt();
	if (s_renderApi.m_progressiveTextureFrequency > 0)
		m_progressiveState.reserve(images.numElem());
	m_textures.reserve(images.numElem());

	for (int i = 0; i < images.numElem(); i++)
	{
		const CRefPtr<CImage> &img = images[i];

		if ((m_iFlags & TEXFLAG_CUBEMAP) && !img->IsCube())
		{
			CrashMsg("TEXFLAG_CUBEMAP set - every texture in set must be cubemap, %s is not a cubemap\n", m_szTexName.ToCString());
		}

		const EImageType imgType = img->GetImageType();
		const ETextureFormat imgFmt = img->GetFormat();
		const int imgMipCount = img->GetMipMapCount();
		const bool imgHasMipMaps = (imgMipCount > 1);

		const int mipStart = imgHasMipMaps ? min(quality, imgMipCount - 1) : 0;
		const int mipCount = max(imgMipCount - quality, 1);

		const int texWidth = img->GetWidth(mipStart);
		const int texHeight = img->GetHeight(mipStart);
		const int texDepth = img->GetDepth(mipStart);

		if (gl_skipTextures.GetBool())
		{
			m_textures.append(invalidTexture);
			continue;
		}

		enum TexLoadResult
		{
			TEXLOAD_ERROR = -1,
			TEXLOAD_DONE = 0,
			TEXLOAD_NEED_ADDITIONAL_LEVELS
		};

		// for progressive levels
		const int numMipMaps = img->GetMipMapCount();
		int mipMapLevel = numMipMaps - 1;

		GLTextureRef_t texture;

		const int result = g_glWorker.WaitForExecute(__func__, [&]()
													 {
			// Generate a texture
			texture = CreateGLTexture(img, m_samplerState, mipStart, mipCount);

			if(texture.glTexID == GL_NONE)
				return TEXLOAD_ERROR;

			if ((m_iFlags & TEXFLAG_PROGRESSIVE_LODS) && s_renderApi.m_progressiveTextureFrequency > 0)
			{
				int transferredSize = 0;
				do
				{
					const int size = img->GetMipMappedSize(mipMapLevel, 1);
					const int lockBoxLevel = mipMapLevel - mipStart;

					UpdateGLTextureFromImageMipmap(texture, img, mipMapLevel, lockBoxLevel);
					transferredSize += size;

					if (transferredSize > TEXTURE_TRANSFER_RATE_THRESHOLD)
						return TEXLOAD_NEED_ADDITIONAL_LEVELS;

					--mipMapLevel;
					if (mipMapLevel < 0)
						break;

				} while (true);
			}
			else if (!UpdateGLTextureFromImage(texture, img, mipStart))
			{
				glBindTexture(m_glTarget, 0);
				GLCheckError("tex unbind");

				glDeleteTextures(1, &texture.glTexID);
				GLCheckError("del tex");

				return TEXLOAD_ERROR;
			}
			return TEXLOAD_DONE; });

		if (result == TEXLOAD_ERROR)
		{
			MsgError("Error - cannot upload texture %s data\n", m_szTexName.ToCString());
			continue;
		}

		m_textures.append(texture);

		const int lockBoxLevel = mipMapLevel - mipStart;
		if (lockBoxLevel > 1 && result == TEXLOAD_NEED_ADDITIONAL_LEVELS)
		{
			LodState &state = m_progressiveState.append();
			state.idx = i;
			state.lockBoxLevel = lockBoxLevel - 1;
			state.mipMapLevel = mipMapLevel - 1;
			state.image = img;
		}

		// FIXME: check for differences?
		m_mipCount = max(m_mipCount, mipCount);
		m_iWidth = max(m_iWidth, texWidth);
		m_iHeight = max(m_iHeight, texHeight);
		m_iDepth = max(m_iDepth, texDepth);
		m_iFormat = imgFmt;

		m_texSize += img->GetMipMappedSize(mipStart);
	}

	// hey you have concurrency errors if this assert hits!
	ASSERT_MSG(images.numElem() == m_textures.numElem(), "%s - %d images at input while %d textures created", m_szTexName.ToCString(), images.numElem(), m_textures.numElem());

	if (m_progressiveState.numElem())
	{
		m_progressiveFrameDelay = min(s_renderApi.m_progressiveTextureFrequency, 255);
		Threading::CScopedMutex m(g_sapi_ProgressiveTextureMutex);
		s_renderApi.m_progressiveTextures.insert(this);
	}

	m_numAnimatedTextureFrames = m_textures.numElem();

	return true;
}

GLTextureRef_t &CGLTexture::GetCurrentTexture()
{
	static GLTextureRef_t nulltex = {0, IMAGE_TYPE_INVALID};

	if (!m_textures.inRange(m_nAnimatedTextureFrame))
		return nulltex;

	return m_textures[m_nAnimatedTextureFrame];
}

EProgressiveStatus CGLTexture::StepProgressiveLod()
{
	if (!m_textures.numElem())
		return PROGRESSIVE_STATUS_COMPLETED;

	if (m_progressiveFrameDelay > 0)
	{
		--m_progressiveFrameDelay;
		return PROGRESSIVE_STATUS_WAIT_MORE_FRAMES;
	}

	{
		Threading::CScopedMutex m(g_sapi_ProgressiveTextureMutex);
		for (int i = 0; i < m_progressiveState.numElem(); ++i)
		{
			LodState &state = m_progressiveState[i];
			GLTextureRef_t &texture = m_textures[state.idx];

			g_glWorker.WaitForExecute("StepProgressiveTextures", [&]() {
				UpdateGLTextureFromImageMipmap(texture, state.image, state.mipMapLevel, state.lockBoxLevel);

				--state.lockBoxLevel;
				--state.mipMapLevel;

				if (state.lockBoxLevel < 0)
				{
					m_progressiveState.fastRemoveIndex(i);
					--i;
				}
				return 0;
			});
		}
	}

	if (!m_progressiveState.numElem())
		return PROGRESSIVE_STATUS_COMPLETED;

	m_progressiveFrameDelay = min(s_renderApi.m_progressiveTextureFrequency, 255);
	return PROGRESSIVE_STATUS_DID_UPLOAD;
}

// locks texture for modifications, etc
bool CGLTexture::Lock(LockInOutData& data)
{
	ASSERT_MSG(!m_lockData, "CGLTexture: already locked");

	if (m_lockData)
		return false;

	if (m_textures.numElem() > 1)
	{
		ASSERT_FAIL("Couldn't handle locking of animated texture! Please tell to programmer!");
		return false;
	}

	if (IsCompressedFormat(m_iFormat))
	{
		ASSERT_FAIL("Compressed textures aren't lockable!");
		return false;
	}

	if (data.flags & TEXLOCK_REGION_BOX)
	{
		ASSERT_FAIL("CGLTexture - does not support locking 3D texture yet");
		return false;
	}

	int sizeToLock = 0;
	int lockOffset = 0;
	int lockPitch = 0;
	switch (m_glTarget)
	{
	case GL_TEXTURE_3D:
	{
		// COULD BE INVALID! I'VE NEVER TESTED THAT

		IBoundingBox box = (data.flags & TEXLOCK_REGION_BOX) ? data.region.box : IBoundingBox(0, 0, 0, GetWidth(), GetHeight(), GetDepth());
		const IVector3D size = box.GetSize();

		sizeToLock = size.x * size.y * size.y;
		lockOffset = box.minPoint.x * box.minPoint.y * box.minPoint.z;
		lockPitch = size.x;

		break;
	}
	case GL_TEXTURE_CUBE_MAP:
	case GL_TEXTURE_2D:
	{
		const IAARectangle lockRect = (data.flags & TEXLOCK_REGION_RECT) ? data.region.rectangle : IAARectangle(0, 0, GetWidth(), GetHeight());
		const IVector2D size = lockRect.GetSize();
		sizeToLock = size.x * size.y;
		lockOffset = lockRect.leftTop.x * lockRect.leftTop.y;
		lockPitch = lockRect.GetSize().x;
		break;
	}
	}

	const int lockByteCount = GetBytesPerPixel(m_iFormat) * sizeToLock;

	// allocate memory for lock data
	data.lockData = (ubyte*)PPAlloc(lockByteCount);
	data.lockPitch = lockPitch * GetBytesPerPixel(m_iFormat);

#ifdef USE_GLES2
	// Always need to discard data from GLES :(
#else
	if (!(data.flags & TEXLOCK_DISCARD))
	{
		g_glWorker.WaitForExecute("LockTexGetData", [&]() {
			const GLenum srcFormat = g_gl_chanCountTypes[GetChannelCount(m_iFormat)];
			const GLenum srcType = g_gl_chanTypePerFormat[m_iFormat];

			glBindTexture(m_glTarget, m_textures[0].glTexID);

			switch (m_glTarget)
			{
				case GL_TEXTURE_3D:
				{
					ASSERT_FAIL("CGLTexture - does not support locking 3D texture yet for reading");
					break;
				}
				case GL_TEXTURE_CUBE_MAP:
				{
					const GLenum cubeTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + data.cubeFaceIdx;
					glGetTexImage(cubeTarget, data.level, srcFormat, srcType, data.lockData);
					GLCheckError("lock read tex image");
					break;
				}
				case GL_TEXTURE_2D:
				{
					glGetTexImage(m_glTarget, data.level, srcFormat, srcType, data.lockData);
					GLCheckError("lock read tex image");
					break;
				}
			}
			glBindTexture(m_glTarget, 0);
			return 0;
		});
	}
#endif // USE_GLES2
	m_lockData = &data;

	return m_lockData && *m_lockData;
}

// unlocks texture for modifications, etc
void CGLTexture::Unlock()
{
	if (!m_lockData)
		return;

	ASSERT(m_lockData->lockData != nullptr);

	LockInOutData &data = *m_lockData;

	if (!(data.flags & TEXLOCK_READONLY))
	{
		GLenum srcFormat = g_gl_chanCountTypes[GetChannelCount(m_iFormat)];
		GLenum srcType = g_gl_chanTypePerFormat[m_iFormat];
		GLenum internalFormat = PickGLInternalFormat(m_iFormat);

		const int targetOrCubeTarget = (m_glTarget == GL_TEXTURE_CUBE_MAP) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + data.cubeFaceIdx : m_glTarget;

		g_glWorker.WaitForExecute("UnlockTex", [&]() {
			glBindTexture(m_glTarget, m_textures[0].glTexID);
			GLCheckError("bind texture");

			switch (m_glTarget)
			{
				case GL_TEXTURE_3D:
				{
					IBoundingBox box = (data.flags & TEXLOCK_REGION_BOX) ? data.region.box : IBoundingBox(0, 0, 0, GetWidth(), GetHeight(), GetDepth());
					const IVector3D boxSize = box.GetSize();
					glTexSubImage3D(m_glTarget, data.level, box.minPoint.x, box.minPoint.y, box.minPoint.z, boxSize.x, boxSize.y, boxSize.z, srcFormat, srcType, data.lockData);
					GLCheckError("unlock upload tex image");
					break;
				}
				case GL_TEXTURE_CUBE_MAP:
				{
					const GLenum cubeTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + data.cubeFaceIdx;
					const IAARectangle lockRect = (data.flags & TEXLOCK_REGION_RECT) ? data.region.rectangle : IAARectangle(0, 0, GetWidth(), GetHeight());
					const IVector2D size = lockRect.GetSize();

					glTexSubImage2D(cubeTarget, data.level, lockRect.leftTop.x, lockRect.leftTop.y, size.x, size.y, srcFormat, srcType, data.lockData);
					GLCheckError("unlock upload tex image");
				}
				case GL_TEXTURE_2D:
				{
					const IAARectangle lockRect = (data.flags & TEXLOCK_REGION_RECT) ? data.region.rectangle : IAARectangle(0, 0, GetWidth(), GetHeight());
					const IVector2D size = lockRect.GetSize();

					glTexSubImage2D(m_glTarget, data.level, lockRect.leftTop.x, lockRect.leftTop.y, size.x, size.y, srcFormat, srcType, data.lockData);
					GLCheckError("unlock upload tex image");
					break;
				}
			}

			glBindTexture(m_glTarget, 0);
			return 0;
		});

	}

	PPFree(data.lockData);
	data.lockData = nullptr;

	m_lockData = nullptr;
}
