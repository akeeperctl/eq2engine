//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: DX9 Shader program for ShaderAPID3DX9
//////////////////////////////////////////////////////////////////////////////////

#ifndef D3D9SHADERPROGRAM_H
#define D3D9SHADERPROGRAM_H

#include "renderers/IShaderProgram.h"
#include "renderers/ShaderAPI_defs.h"
#include "ds/eqstring.h"
#include <d3d9.h>

#define MAX_CONSTANT_NAMELEN 64

typedef struct DX9ShaderConstant 
{
	char	name[MAX_CONSTANT_NAMELEN];
	int64	hash;

	int		vsReg;
	int		psReg;

	int		constFlags;
}DX9ShaderConstant_t;

typedef struct DX9Sampler_s
{
	char	name[64]{ 0 };

	uint	index{ 0 };
	uint	gsIndex{ 0 };
	uint	vsIndex{ 0 };
}DX9Sampler_t;

struct ID3DXConstantTable;

class CD3D9ShaderProgram : public IShaderProgram
{
public:
	friend class			ShaderAPID3DX9;

							CD3D9ShaderProgram();
							~CD3D9ShaderProgram();

	const char*				GetName() const;
	int						GetNameHash() const { return m_nameHash; }
	void					SetName(const char* pszName);

	int						GetConstantsNum() const;
	int						GetSamplersNum() const;

protected:
	EqString				m_szName;
	int						m_nameHash;

	LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
	LPDIRECT3DPIXELSHADER9  m_pPixelShader;
	ID3DXConstantTable*		m_pVSConstants;
	ID3DXConstantTable*		m_pPSConstants;

	DX9ShaderConstant_t*	m_pConstants;
	DX9Sampler_t*			m_pSamplers;

	int						m_numConstants;
	int						m_numSamplers;
};

#endif //D3D9SHADERPROGRAM_H
