//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium material
//////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "materialsystem1/IMaterial.h"

class CMatVar;
class IMaterialProxy;
struct KVSection;

class CMaterial : public IMaterial
{
public:
	friend class			CBaseShader;
	friend class			CMaterialSystem;
	friend class			CEqMatSystemThreadedLoader;

							// constructor, destructor
							CMaterial();
							~CMaterial();

	const char*				GetName() const {return m_szMaterialName.GetData();}
	const char*				GetShaderName() const;
	CTextureAtlas*			GetAtlas() const;

	int						GetState() const {return m_state;}
	bool					IsError() const {return (m_state == MATERIAL_LOAD_ERROR);}
	int						GetFlags() const;

// init + shutdown

	// initializes material from file
	void					Init(const char* matFileName);
	
	// initializes material from keyvalues
	void					Init(const char* materialName, KVSection* shader_root);

	void					Cleanup(bool dropVars = true, bool dropShader = true);

	bool					LoadShaderAndTextures();
	void					WaitForLoading() const;

// material var operations
	IMatVar*				FindMaterialVar(const char* pszVarName) const;
	IMatVar*				GetMaterialVar(const char* pszVarName, const char* defaultParam);
	IMatVar*				CreateMaterialVar(const char* pszVarName, const char* defaultParam);
	void					RemoveMaterialVar(IMatVar* pVar);

// render-time operations
	void					UpdateProxy(float fDt);					
	ITexture*				GetBaseTexture(int stage = 0);

	void					Setup(uint paramMask);
private:

	void					InitVars(KVSection* kvs);

	void					InitShader();
	void					InitMaterialVars(KVSection* kvs);
	void					InitMaterialProxy(KVSection* kvs);

protected:
	bool					DoLoadShaderAndTextures();

	EqString				m_szMaterialName;
	EqString				m_szShaderName;

	Array<CMatVar*>			m_variables{ PP_SL };
	Array<IMaterialProxy*>	m_proxies{ PP_SL };

	CTextureAtlas*			m_atlas;

	IMaterialSystemShader*	m_shader;
	int						m_state;	// FIXME: may be interlocked?
	int						m_nameHash;

	uint					m_frameBound;
	bool					m_loadFromDisk;
};