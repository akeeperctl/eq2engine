//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Direct3D 9 texture class
//////////////////////////////////////////////////////////////////////////////////

#include "core/core_common.h"
#include "imaging/ImageLoader.h"
#include "shaderapid3d9_def.h"
#include "D3D9Texture.h"

#include "ShaderAPID3D9.h"

extern ShaderAPID3DX9 s_shaderApi;

CD3D9Texture::CD3D9Texture() : CTexture()
{
	m_nLockLevel = 0;
	m_bIsLocked = false;
	m_pLockSurface = nullptr;
	m_texSize = 0;
	m_dummyDepth = nullptr;
}

CD3D9Texture::~CD3D9Texture()
{
	Release();
}

void CD3D9Texture::Release()
{
	ReleaseTextures();
	ReleaseSurfaces();
}

void CD3D9Texture::ReleaseTextures()
{
	for (int i = 0; i < textures.numElem(); i++)
		textures[i]->Release();

	textures.clear();
}

void CD3D9Texture::ReleaseSurfaces()
{
	for (int i = 0; i < surfaces.numElem(); i++)
		surfaces[i]->Release();

	surfaces.clear();

	if (m_dummyDepth)
		m_dummyDepth->Release();
}

LPDIRECT3DBASETEXTURE9 CD3D9Texture::GetCurrentTexture()
{
	if(m_nAnimatedTextureFrame > textures.numElem()-1)
		return nullptr;

	return textures[m_nAnimatedTextureFrame];
}

// locks texture for modifications, etc
void CD3D9Texture::Lock(LockData* pLockData, Rectangle_t* pRect, bool bDiscard, bool bReadOnly, int nLevel, int nCubeFaceId)
{
	ASSERT_MSG(!m_bIsLocked, "CD3D9Texture: already locked");
	
	if(m_bIsLocked)
		return;

	if(textures.numElem() > 1)
		ASSERT(!"Couldn't handle locking of animated texture! Please tell to programmer!");

	ASSERT( !m_bIsLocked );

	m_nLockLevel = nLevel;
	m_nLockCube = nCubeFaceId;
	m_bIsLocked = true;

	D3DLOCKED_RECT rect;

	RECT lock_rect;
	if(pRect)
	{
		lock_rect.top = pRect->vleftTop.y;
		lock_rect.left = pRect->vleftTop.x;
		lock_rect.right = pRect->vrightBottom.x;
		lock_rect.bottom = pRect->vrightBottom.y;
	}

	if(m_pool != D3DPOOL_DEFAULT)
		bDiscard = false;

	DWORD lock_flags = (bDiscard ? D3DLOCK_DISCARD : 0) | (bReadOnly ? D3DLOCK_READONLY : 0);

	// FIXME: LOck just like in the CreateD3DTextureFromImage

	// try lock surface if exist
	if( surfaces.numElem() )
	{
		ASSERT(nCubeFaceId < surfaces.numElem());

		if(m_pool != D3DPOOL_DEFAULT)
		{
			HRESULT r = surfaces[nCubeFaceId]->LockRect(&rect, (pRect ? &lock_rect : nullptr), lock_flags);

			if(r == D3D_OK)
			{
				// set lock data params
				pLockData->pData = (ubyte*)rect.pBits;
				pLockData->nPitch = rect.Pitch;
			}
			else
			{
				ASSERT(!"Couldn't lock surface for texture!");
			}
		}
		else if(bReadOnly)
		{
			IDirect3DDevice9* pDev = s_shaderApi.m_pD3DDevice;

			if (pDev->CreateOffscreenPlainSurface(m_iWidth, m_iHeight, formats[m_iFormat], D3DPOOL_SYSTEMMEM, &m_pLockSurface, nullptr) == D3D_OK)
			{
				HRESULT r = s_shaderApi.m_pD3DDevice->GetRenderTargetData(surfaces[nCubeFaceId], m_pLockSurface);

				if(r != D3D_OK)
					ASSERT(!"Couldn't lock surface: failed to copy surface to m_pLockSurface!");

				r = m_pLockSurface->LockRect(&rect, (pRect ? &lock_rect : nullptr), lock_flags);

				if(r == D3D_OK)
				{
					// set lock data params
					pLockData->pData = (ubyte*)rect.pBits;
					pLockData->nPitch = rect.Pitch;
				}
				else
				{
					ASSERT(!"Couldn't lock surface for texture!");
					m_pLockSurface->Release();
					m_pLockSurface = nullptr;
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
		HRESULT r;

		if(m_iFlags & TEXFLAG_CUBEMAP)
		{
			r = ((IDirect3DCubeTexture9*) textures[0])->LockRect((D3DCUBEMAP_FACES)m_nLockCube, 0, &rect, (pRect ? &lock_rect : nullptr), lock_flags);
		}
		else
		{
			r = ((IDirect3DTexture9*) textures[0])->LockRect(nLevel, &rect, (pRect ? &lock_rect : nullptr), lock_flags);
		}

		if(r == D3D_OK)
		{
			// set lock data params
			pLockData->pData = (ubyte*)rect.pBits;
			pLockData->nPitch = rect.Pitch;
		}
		else
		{
			if(r == D3DERR_WASSTILLDRAWING)
				ASSERT(!"Please unbind lockable texture!");

			ASSERT(!"Couldn't lock texture!");
		}
	}
}
	
// unlocks texture for modifications, etc
void CD3D9Texture::Unlock()
{
	if(textures.numElem() > 1)
		ASSERT(!"Couldn't handle locking of animated texture! Please tell to programmer!");

	ASSERT( m_bIsLocked );

	if( surfaces.numElem() )
	{
		if(m_pool != D3DPOOL_DEFAULT)
		{
			surfaces[0]->UnlockRect();
		}
		else
		{
			m_pLockSurface->UnlockRect();
			m_pLockSurface->Release();
			m_pLockSurface = nullptr;
		}
	}
	else
	{
		if(m_iFlags & TEXFLAG_CUBEMAP)
			((IDirect3DCubeTexture9*) textures[0])->UnlockRect( (D3DCUBEMAP_FACES)m_nLockCube, m_nLockLevel );
		else
			((IDirect3DTexture9*) textures[0])->UnlockRect( m_nLockLevel );
	}
		

	m_bIsLocked = false;
}

bool UpdateD3DTextureFromImage(IDirect3DBaseTexture9* texture, CImage* image, int startMipLevel, bool convert)
{
	bool isAcceptableImageType =
		(texture->GetType() == D3DRTYPE_VOLUMETEXTURE && image->Is3D()) ||
		(texture->GetType() == D3DRTYPE_CUBETEXTURE && image->IsCube()) ||
		(texture->GetType() == D3DRTYPE_TEXTURE && (image->Is2D() || image->Is1D()));

	ASSERT(isAcceptableImageType);

	if (!isAcceptableImageType)
		return false;

	const ETextureFormat nFormat = image->GetFormat();

	if (convert)
	{
		if (nFormat == FORMAT_RGB8 || nFormat == FORMAT_RGBA8)
			image->SwapChannels(0, 2); // convert to BGR

		// Convert if needed and upload datas
		if (nFormat == FORMAT_RGB8) // as the D3DFMT_X8R8G8B8 used
			image->Convert(FORMAT_RGBA8);
	}
	
	int mipMapLevel = startMipLevel;

	const DWORD lockFlags = D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK;

	ubyte *src;
	while ((src = image->GetPixels(mipMapLevel)) != nullptr)
	{
		const int size = image->GetMipMappedSize(mipMapLevel, 1);
		const int lockBoxLevel = mipMapLevel - startMipLevel;

		if (texture->GetType() == D3DRTYPE_VOLUMETEXTURE)
		{
			D3DLOCKED_BOX box;
			if (((IDirect3DVolumeTexture9*)texture)->LockBox(lockBoxLevel, &box, nullptr, lockFlags) == D3D_OK)
			{
				memcpy(box.pBits, src, size);
				((IDirect3DVolumeTexture9*)texture)->UnlockBox(lockBoxLevel);
			}
		}
		else if (texture->GetType() == D3DRTYPE_CUBETEXTURE)
		{
			const int cubeFaceSize = size / 6;

			D3DLOCKED_RECT rect;
			for (int i = 0; i < 6; i++)
			{
				if (((IDirect3DCubeTexture9*)texture)->LockRect((D3DCUBEMAP_FACES)i, lockBoxLevel, &rect, nullptr, lockFlags) == D3D_OK)
				{
					memcpy(rect.pBits, src, cubeFaceSize);
					((IDirect3DCubeTexture9*)texture)->UnlockRect((D3DCUBEMAP_FACES)i, lockBoxLevel);
				}
				src += cubeFaceSize;
			}
		}
		else
		{
			D3DLOCKED_RECT rect;

			if (((IDirect3DTexture9*)texture)->LockRect(lockBoxLevel, &rect, nullptr, lockFlags) == D3D_OK)
			{
				memcpy(rect.pBits, src, size);
				((IDirect3DTexture9*)texture)->UnlockRect(lockBoxLevel);
			}
		}

		mipMapLevel++;
	}
	
	return true;
}