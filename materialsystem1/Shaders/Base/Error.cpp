//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Error Shader
//////////////////////////////////////////////////////////////////////////////////

#include "core/core_common.h"
#include "IMaterialSystem.h"
#include "BaseShader.h"

BEGIN_SHADER_CLASS(Error)

	SHADER_INIT_PARAMS()
	{
		SHADER_PASS(Unlit) = nullptr;
	}

	SHADER_INIT_TEXTURES()
	{
		SHADER_PARAM_TEXTURE(BaseTexture, m_baseTexture)

		// set texture setup
		SetParameterFunctor(SHADERPARAM_BASETEXTURE, &CErrorShader::SetupBaseTexture0);
		SetParameterFunctor(SHADERPARAM_COLOR, &CErrorShader::SetColorModulation);
	}

	SHADER_INIT_RHI()
	{
		// begin shader definitions
		SHADERDEFINES_BEGIN;

		// compile without fog
		SHADER_FIND_OR_COMPILE(Unlit, "BaseUnlit");

		return true;
	}

	SHADER_SETUP_STAGE()
	{
		SHADER_BIND_PASS_SIMPLE(Unlit);
	}

	SHADER_SETUP_CONSTANTS()
	{
		SetupDefaultParameter(SHADERPARAM_TRANSFORM);

		SetupDefaultParameter(SHADERPARAM_BASETEXTURE);

		SetupDefaultParameter(SHADERPARAM_ALPHASETUP);
		SetupDefaultParameter(SHADERPARAM_DEPTHSETUP);
		SetupDefaultParameter(SHADERPARAM_RASTERSETUP);
		SetupDefaultParameter(SHADERPARAM_COLOR);
	}

	void SetColorModulation()
	{
		g_pShaderAPI->SetShaderConstantVector4D("AmbientColor", materials->GetAmbientColor());
	}

	void SetupBaseTexture0()
	{
		const ITexturePtr& setupTexture = materials->GetConfiguration().wireframeMode ? materials->GetWhiteTexture() : m_baseTexture.Get();
		g_pShaderAPI->SetTexture("BaseTextureSampler", setupTexture);
	}

	const ITexturePtr& GetBaseTexture(int stage)  const {return m_baseTexture.Get();}

	MatTextureProxy	m_baseTexture;

	SHADER_DECLARE_PASS(Unlit);

END_SHADER_CLASS