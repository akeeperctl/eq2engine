//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium material
//////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "materialsystem1/IMaterial.h"

class IMaterialProxy;
class IShaderAPI;
struct KVSection;

class CMaterial : public IMaterial
{
public:
	friend class			CBaseShader;
	friend class			CMaterialSystem;
	friend class			CEqMatSystemThreadedLoader;

							// constructor, destructor
							CMaterial(const char* materialName, bool loadFromDisk);
							~CMaterial();

	void					Ref_DeleteObject();

	const char*				GetName() const { return m_szMaterialName.GetData(); }
	const char*				GetShaderName() const;
	CTextureAtlas*			GetAtlas() const;

	int						GetState() const {return m_state;}
	bool					IsError() const {return (m_state == MATERIAL_LOAD_ERROR);}
	int						GetFlags() const;

// init + shutdown

	// initializes material from file
	void					Init(IShaderAPI* renderAPI);
	
	// initializes material from keyvalues
	void					Init(IShaderAPI* renderAPI, KVSection* shader_root);

	void					Cleanup(bool dropVars = true, bool dropShader = true);

	bool					LoadShaderAndTextures();
	void					WaitForLoading() const;

// material var operations
	MatVarProxyUnk			FindMaterialVar(const char* pszVarName) const;
	MatVarProxyUnk			GetMaterialVar(const char* pszVarName, const char* defaultValue);

// render-time operations
	void					UpdateProxy(float fDt);					
	const ITexturePtr&		GetBaseTexture(int stage = 0);

	void					Setup(IShaderAPI* renderAPI, uint paramMask);
private:

	void					InitVars(KVSection* kvs, const char* renderAPIName);
	MatVarData&				VarAt(int idx) const;

	void					InitShader(IShaderAPI* renderAPI);
	void					InitMaterialVars(KVSection* kvs, const char* prefix = nullptr);
	void					InitMaterialProxy(KVSection* kvs);

protected:
	bool					DoLoadShaderAndTextures();

	EqString				m_szMaterialName;
	EqString				m_szShaderName;

	MaterialVarBlock		m_vars;
	Array<IMaterialProxy*>	m_proxies{ PP_SL };

	CTextureAtlas*			m_atlas{ nullptr };
	IMaterialSystemShader*	m_shader{ nullptr };

	int						m_state{ MATERIAL_LOAD_ERROR };	// FIXME: may be interlocked?
	int						m_nameHash{ 0 };

	uint					m_frameBound{ 0 };
	bool					m_loadFromDisk{ false };
};