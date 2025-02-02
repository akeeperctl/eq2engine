//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Direct3D 9 texture class
//////////////////////////////////////////////////////////////////////////////////

#include "core/core_common.h"
#include "core/ConVar.h"
#include "core/IConsoleCommands.h"

#include "imaging/ImageLoader.h"
#include "shaderapid3d9_def.h"
#include "D3D9Texture.h"

#include "ShaderAPID3D9.h"

extern ShaderAPID3D9 s_renderApi;
Threading::CEqMutex g_sapi_ProgressiveTextureMutex;


CD3D9Texture::CD3D9Texture() : CTexture()
{
}

CD3D9Texture::~CD3D9Texture()
{
	Release();
}

void CD3D9Texture::Ref_DeleteObject()
{
	s_renderApi.FreeTexture(this);
	RefCountedObject::Ref_DeleteObject();
}

void CD3D9Texture::Release()
{
	ASSERT_MSG(!m_lockData, "texture was locked");

	ReleaseTextures();
	ReleaseSurfaces();
}

IDirect3DBaseTexture9* CD3D9Texture::CreateD3DTexture(EImageType type, ETextureFormat format, int mipCount, int widthMip0, int heightMip0, int depthMip0) const
{
	IDirect3DDevice9* d3dDevice = s_renderApi.GetD3DDevice();
	IDirect3DBaseTexture9* d3dTexture = nullptr;

	if (IsCompressedFormat(format))
	{
		//FIXME: is that even valid?
		widthMip0 = max(4, widthMip0 & ~3);
		heightMip0 = max(4, heightMip0 & ~3);
	}

	if (type == IMAGE_TYPE_CUBE)
	{
		HRESULT status = d3dDevice->CreateCubeTexture(widthMip0, mipCount, 0,
														g_d3d9_imageFormats[format], (D3DPOOL)m_pool, (LPDIRECT3DCUBETEXTURE9*)&d3dTexture, nullptr);
		if (status != D3D_OK)
		{
			return nullptr;
		}
	} 
	else if (type == IMAGE_TYPE_3D)
	{
		HRESULT status = d3dDevice->CreateVolumeTexture(widthMip0, heightMip0, depthMip0, mipCount, 0,
														g_d3d9_imageFormats[format], (D3DPOOL)m_pool, (LPDIRECT3DVOLUMETEXTURE9*)&d3dTexture, nullptr);
		if (status != D3D_OK)
		{
			return nullptr;
		}
	} 
	else if(type == IMAGE_TYPE_2D || type == IMAGE_TYPE_1D)
	{
		HRESULT status = d3dDevice->CreateTexture(widthMip0, heightMip0, mipCount, 0,
													g_d3d9_imageFormats[format], (D3DPOOL)m_pool, (LPDIRECT3DTEXTURE9*)&d3dTexture, nullptr);
		if (status != D3D_OK)
		{
			return nullptr;
		}
	}
	else
	{
		ASSERT_FAIL("Invalid texture type!");
	}

	return d3dTexture;
}

static void UpdateD3DTextureFromImageMipmap(IDirect3DBaseTexture9* texture, CImage* image, int sourceMipLevel, int targetMipLevel, DWORD lockFlags)
{
	ubyte* src = image->GetPixels(sourceMipLevel);
	ASSERT(src);
	const int size = image->GetMipMappedSize(sourceMipLevel, 1);

	switch (texture->GetType())
	{
		case D3DRTYPE_VOLUMETEXTURE:
		{
			IDirect3DVolumeTexture9* texture3D = (IDirect3DVolumeTexture9*)texture;
			D3DLOCKED_BOX box;
			if (texture3D->LockBox(targetMipLevel, &box, nullptr, lockFlags) == D3D_OK)
			{
				memcpy(box.pBits, src, size);
				texture3D->UnlockBox(targetMipLevel);
			}
			break;
		}
		case D3DRTYPE_CUBETEXTURE:
		{
			IDirect3DCubeTexture9* cubeTexture = (IDirect3DCubeTexture9*)texture;
			const int cubeFaceSize = size / 6;

			D3DLOCKED_RECT rect;
			for (int i = 0; i < 6; i++)
			{
				if (cubeTexture->LockRect((D3DCUBEMAP_FACES)i, targetMipLevel, &rect, nullptr, lockFlags) == D3D_OK)
				{
					memcpy(rect.pBits, src, cubeFaceSize);
					cubeTexture->UnlockRect((D3DCUBEMAP_FACES)i, targetMipLevel);
				}
				src += cubeFaceSize;
			}
			break;
		}
		case D3DRTYPE_TEXTURE:
		{
			IDirect3DTexture9* texture2D = (IDirect3DTexture9*)texture;
			D3DLOCKED_RECT rect;

			if (texture2D->LockRect(targetMipLevel, &rect, nullptr, lockFlags) == D3D_OK)
			{
				memcpy(rect.pBits, src, size);
				texture2D->UnlockRect(targetMipLevel);
			}
			break;
		}
		default:
		{
			ASSERT_FAIL("Invalid resource type passed to UpdateD3DTextureFromImage");
		}
	}
}

static bool UpdateD3DTextureFromImage(IDirect3DBaseTexture9* texture, CImage* image, int startMipLevel)
{
	const DWORD lockFlags = D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK;

	const int numMipMaps = image->GetMipMapCount();
	int mipMapLevel = numMipMaps-1;

	while (mipMapLevel >= startMipLevel)
	{
		const int lockBoxLevel = mipMapLevel - startMipLevel;

		UpdateD3DTextureFromImageMipmap(texture, image, mipMapLevel, lockBoxLevel, lockFlags);
		--mipMapLevel;
	}
	
	return true;
}

// initializes texture from image array of images
bool CD3D9Texture::Init(const SamplerStateParams& sampler, const ArrayCRef<CRefPtr<CImage>> images, int flags)
{
	// FIXME: only release if pool, flags, format and size is different
	Release();

	m_samplerState = sampler;
	m_samplerState.aniso = max(s_renderApi.GetCaps().maxTextureAnisotropicLevel, sampler.aniso);
	m_iFlags = flags;
	m_pool = D3DPOOL_MANAGED;

	HOOK_TO_CVAR(r_loadmiplevel);

	for (int i = 0; i < images.numElem(); i++)
	{
		if (images[i]->IsCube())
			m_iFlags |= TEXFLAG_CUBEMAP;
	}

	const int quality = (m_iFlags & TEXFLAG_NOQUALITYLOD) ? 0 : r_loadmiplevel->GetInt();

	if(s_renderApi.m_progressiveTextureFrequency > 0)
		m_progressiveState.reserve(images.numElem());
	m_textures.reserve(images.numElem());

	for (int i = 0; i < images.numElem(); i++)
	{
		const CRefPtr<CImage>& img = images[i];

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

		IDirect3DBaseTexture9* d3dTexture = CreateD3DTexture(imgType, imgFmt, mipCount, texWidth, texHeight, texDepth );

		if (!d3dTexture)
		{
			MsgError("D3D9 ERROR: failed to create texture for image %s\n", img->GetName());
			continue;
		}

		if ((m_iFlags & TEXFLAG_PROGRESSIVE_LODS) && s_renderApi.m_progressiveTextureFrequency > 0)
		{
			// start with uploading only first LOD
			const DWORD lockFlags = D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK;

			const int numMipMaps = img->GetMipMapCount();
			int mipMapLevel = numMipMaps - 1;

			int transferredSize = 0;
			do
			{
				const int size = img->GetMipMappedSize(mipMapLevel, 1);
				const int lockBoxLevel = mipMapLevel - mipStart;

				UpdateD3DTextureFromImageMipmap(d3dTexture, img, mipMapLevel, lockBoxLevel, lockFlags);
				d3dTexture->SetLOD(lockBoxLevel);
				
				transferredSize += size;

				if (transferredSize > TEXTURE_TRANSFER_RATE_THRESHOLD)
				{
					if (lockBoxLevel > 1)
					{
						LodState& state = m_progressiveState.append();
						state.idx = i;
						state.lockBoxLevel = lockBoxLevel - 1;
						state.mipMapLevel = mipMapLevel - 1;
						state.image = img;
					}
					break;
				}

				--mipMapLevel;
				if (mipMapLevel < 0)
					break;

			} while (true);
		}
		else
		{
			// upload all LODs
			UpdateD3DTextureFromImage(d3dTexture, img, mipStart);
		}

		d3dTexture->PreLoad();

		// FIXME: check for differences?
		m_mipCount = max(m_mipCount, mipCount);
		m_iWidth = max(m_iWidth, texWidth);
		m_iHeight = max(m_iHeight, texHeight);
		m_iDepth = max(m_iDepth, texDepth);
		m_iFormat = imgFmt;

		m_texSize += img->GetMipMappedSize(mipStart);
		m_textures.append(d3dTexture);
	}

	// hey you have concurrency errors if this assert hits!
	ASSERT_MSG(images.numElem() == m_textures.numElem(), "%s - %d images at input while %d textures created", m_szTexName.ToCString(), images.numElem(), m_textures.numElem());

	if(m_progressiveState.numElem())
	{
		m_progressiveFrameDelay = min(s_renderApi.m_progressiveTextureFrequency, 255);
		
		Threading::CScopedMutex m(g_sapi_ProgressiveTextureMutex);
		s_renderApi.m_progressiveTextures.insert(this);
	}

	m_numAnimatedTextureFrames = m_textures.numElem();

	return true;
}

void CD3D9Texture::ReleaseTextures()
{
	{
		Threading::CScopedMutex m(g_sapi_ProgressiveTextureMutex);
		s_renderApi.m_progressiveTextures.remove(this);
		m_progressiveState.clear(true);
	}

	for (int i = 0; i < m_textures.numElem(); i++)
		m_textures[i]->Release();

	m_textures.clear();
	m_texSize = 0;
}

void CD3D9Texture::ReleaseSurfaces()
{
	for (int i = 0; i < m_surfaces.numElem(); i++)
		m_surfaces[i]->Release();

	m_surfaces.clear();

	if (m_dummyDepth)
		m_dummyDepth->Release();
	m_dummyDepth = nullptr;
}

void CD3D9Texture::ReleaseForRestoration()
{
	// TODO
}

void CD3D9Texture::Restore()
{
	// TODO
}

LPDIRECT3DBASETEXTURE9 CD3D9Texture::GetCurrentTexture()
{
	if (!m_textures.inRange(m_nAnimatedTextureFrame))
		return nullptr;

	return m_textures[m_nAnimatedTextureFrame];
}

EProgressiveStatus CD3D9Texture::StepProgressiveLod()
{
	if (!m_textures.numElem())
		return PROGRESSIVE_STATUS_COMPLETED;

	if(m_progressiveFrameDelay > 0)
	{
		--m_progressiveFrameDelay;
		return PROGRESSIVE_STATUS_WAIT_MORE_FRAMES;
	}

	{
		Threading::CScopedMutex m(g_sapi_ProgressiveTextureMutex);
		for (int i = 0; i < m_progressiveState.numElem(); ++i)
		{
			LodState& state = m_progressiveState[i];
			IDirect3DBaseTexture9* texture = m_textures[state.idx];

			const DWORD lockFlags = D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK;
			UpdateD3DTextureFromImageMipmap(texture, state.image, state.mipMapLevel, state.lockBoxLevel, lockFlags);

			texture->SetLOD(state.lockBoxLevel);
			--state.lockBoxLevel;
			--state.mipMapLevel;

			if (state.lockBoxLevel < 0)
			{
				m_progressiveState.fastRemoveIndex(i);
				--i;
			}
		}
	}

	if (!m_progressiveState.numElem())
		return PROGRESSIVE_STATUS_COMPLETED;

	m_progressiveFrameDelay = min(s_renderApi.m_progressiveTextureFrequency, 255);

	return PROGRESSIVE_STATUS_WAIT_MORE_FRAMES;
}

// locks texture for modifications, etc
bool CD3D9Texture::Lock(LockInOutData& data)
{
	ASSERT_MSG(!m_lockData, "CD3D9Texture: already locked");
	
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

	if (m_pool != D3DPOOL_DEFAULT)
		data.flags &= ~TEXLOCK_DISCARD;

	const bool readOnly = (data.flags & TEXLOCK_READONLY);
	const DWORD lockFlags = ((data.flags & TEXLOCK_DISCARD) ? D3DLOCK_DISCARD : 0)
						  | (readOnly ? D3DLOCK_READONLY : 0);

	// try lock surface if exist
	if(m_surfaces.numElem() )
	{
		ASSERT(data.cubeFaceIdx < m_surfaces.numElem());

		// TODO: 3D surfaces?

		if(m_pool != D3DPOOL_DEFAULT)
		{
			const RECT lockRect = (data.flags & TEXLOCK_REGION_RECT) ? IRectangleToD3DRECT(data.region.rectangle) : RECT {};

			D3DLOCKED_RECT rect;
			HRESULT result = m_surfaces[data.cubeFaceIdx]->LockRect(&rect, ((data.flags & TEXLOCK_REGION_RECT) ? &lockRect : nullptr), lockFlags);

			if(result == D3D_OK)
			{
				// set lock data params
				data.lockData = (ubyte*)rect.pBits;
				data.lockPitch = rect.Pitch;
				m_lockData = &data;
			}
			else
			{
				ASSERT(!"Couldn't lock surface for texture!");
			}
		}
		else if(readOnly)
		{
			IDirect3DDevice9* d3dDevice = s_renderApi.GetD3DDevice();

			if (d3dDevice->CreateOffscreenPlainSurface(m_iWidth, m_iHeight, g_d3d9_imageFormats[m_iFormat], D3DPOOL_SYSTEMMEM, &m_lockSurface, nullptr) == D3D_OK)
			{
				HRESULT result = d3dDevice->GetRenderTargetData(m_surfaces[data.cubeFaceIdx], m_lockSurface);

				if(result != D3D_OK)
					ASSERT(!"Couldn't lock surface: failed to copy surface to m_lockSurface!");

				const RECT lockRect = (data.flags & TEXLOCK_REGION_RECT) ? IRectangleToD3DRECT(data.region.rectangle) : RECT{};

				D3DLOCKED_RECT rect;
				result = m_lockSurface->LockRect(&rect, ((data.flags & TEXLOCK_REGION_RECT) ? &lockRect : nullptr), lockFlags);

				if(result == D3D_OK)
				{
					// set lock data params
					data.lockData = (ubyte*)rect.pBits;
					data.lockPitch = rect.Pitch;
					m_lockData = &data;
				}
				else
				{
					ASSERT(!"Couldn't lock surface for texture!");
					m_lockSurface->Release();
					m_lockSurface = nullptr;
				}
			}
			else
				ASSERT(!"Couldn't lock surface: CreateOffscreenPlainSurface fails!");
		}
		else
			ASSERT(!"Couldn't lock: Rendertargets are read-only!");
	}
	else // lock texture data
	{
		IDirect3DBaseTexture9* texture = m_textures[0];
		HRESULT result = D3DERR_INVALIDCALL;

		switch (texture->GetType())
		{
			case D3DRTYPE_VOLUMETEXTURE:
			{
				IDirect3DVolumeTexture9* texture3D = (IDirect3DVolumeTexture9*)texture;

				const D3DBOX lockBox = (data.flags & TEXLOCK_REGION_BOX) ? IBoundingBoxToD3DBOX(data.region.box) : D3DBOX{};

				D3DLOCKED_BOX box;
				result = texture3D->LockBox(data.level, &box, ((data.flags & TEXLOCK_REGION_BOX) ? &lockBox : nullptr), lockFlags);

				if (result == D3D_OK)
				{
					data.lockData = (ubyte*)box.pBits;
					data.lockPitch = box.RowPitch;
					m_lockData = &data;
				}
				break;
			}
			case D3DRTYPE_CUBETEXTURE:
			{
				IDirect3DCubeTexture9* cubeTexture = (IDirect3DCubeTexture9*)texture;

				const RECT lockRect = (data.flags & TEXLOCK_REGION_RECT) ? IRectangleToD3DRECT(data.region.rectangle) : RECT{};

				D3DLOCKED_RECT rect;
				result = cubeTexture->LockRect((D3DCUBEMAP_FACES)data.cubeFaceIdx, data.level, &rect, ((data.flags & TEXLOCK_REGION_RECT) ? &lockRect : nullptr), lockFlags);

				if (result == D3D_OK)
				{
					data.lockData = (ubyte*)rect.pBits;
					data.lockPitch = rect.Pitch;
					m_lockData = &data;
				}
				break;
			}
			case D3DRTYPE_TEXTURE:
			{
				IDirect3DTexture9* texture2D = (IDirect3DTexture9*)texture;

				const RECT lockRect = (data.flags & TEXLOCK_REGION_RECT) ? IRectangleToD3DRECT(data.region.rectangle) : RECT{};

				D3DLOCKED_RECT rect;
				result = texture2D->LockRect(data.level, &rect, ((data.flags & TEXLOCK_REGION_RECT) ? &lockRect : nullptr), lockFlags);

				if (result == D3D_OK)
				{
					data.lockData = (ubyte*)rect.pBits;
					data.lockPitch = rect.Pitch;
					m_lockData = &data;
				}
				break;
			}
			default:
			{
				ASSERT_FAIL("Invalid resource type");
			}
		}

		if(result != D3D_OK)
		{
			if (result == D3DERR_WASSTILLDRAWING)
			{
				ASSERT_FAIL("Please unbind lockable texture!");
			}
			else
			{
				ASSERT_FAIL("Couldn't lock texture!");
			}
		}
	}

	return m_lockData && *m_lockData;
}
	
// unlocks texture for modifications, etc
void CD3D9Texture::Unlock()
{
	if (!m_lockData)
		return;

	ASSERT(m_lockData->lockData != nullptr);

	if(m_surfaces.numElem() )
	{
		if(m_pool != D3DPOOL_DEFAULT)
		{
			m_surfaces[0]->UnlockRect();
		}
		else
		{
			m_lockSurface->UnlockRect();
			m_lockSurface->Release();
			m_lockSurface = nullptr;
		}
	}
	else
	{
		IDirect3DBaseTexture9* texture = m_textures[0];
		switch (texture->GetType())
		{
			case D3DRTYPE_VOLUMETEXTURE:
			{
				IDirect3DVolumeTexture9* texture3D = (IDirect3DVolumeTexture9*)texture;
				texture3D->UnlockBox(m_lockData->level);
				break;
			}
			case D3DRTYPE_CUBETEXTURE:
			{
				IDirect3DCubeTexture9* cubeTexture = (IDirect3DCubeTexture9*)texture;
				cubeTexture->UnlockRect((D3DCUBEMAP_FACES)m_lockData->cubeFaceIdx, m_lockData->level);
				break;
			}
			case D3DRTYPE_TEXTURE:
			{
				IDirect3DTexture9* texture2D = (IDirect3DTexture9*)texture;
				texture2D->UnlockRect(m_lockData->level);
				break;
			}
			default:
			{
				ASSERT_FAIL("Invalid resource type");
			}
		}
	}

	m_lockData->lockData = nullptr;
	m_lockData = nullptr;
}