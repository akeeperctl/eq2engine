//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2015
//////////////////////////////////////////////////////////////////////////////////
// Description: Unlit Shader with fog support
//////////////////////////////////////////////////////////////////////////////////

#include "BaseShader.h"

class CHDRBlurFilter : public CBaseShader
{
public:
	CHDRBlurFilter()
	{
		m_pBaseTexture = NULL;

		SHADER_PASS(Unlit) = NULL;
	}

	void InitTextures()
	{
		// parse material variables
		SHADER_PARAM_RENDERTARGET_FIND(BaseTexture, m_pBaseTexture);

		// set texture setup
		SetParameterFunctor(SHADERPARAM_BASETEXTURE, &CHDRBlurFilter::SetupBaseTexture0);
	}

	bool InitShaders()
	{
		if(SHADER_PASS(Unlit))
			return true;

		bool brightTest = 0;
		SHADER_PARAM_BOOL(BrightTest, brightTest);

		// begin shader definitions
		SHADERDEFINES_BEGIN;

		SHADER_DECLARE_SIMPLE_DEFINITION(brightTest, "BRIGHTNESS_TEST");

		// compile without fog
		SHADER_FIND_OR_COMPILE(Unlit, "HDR_BlurFilter");

		m_depthtest = false;
		m_depthwrite = false;

		return true;
	}

	void SetupShader()
	{
		if(IsError())
			return;

		SHADER_BIND_PASS_SIMPLE(Unlit);
	}


	void SetupConstants()
	{
		if(IsError())
			return;

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
		ITexture* pSetupTexture = materials->GetConfiguration().wireframeMode ? materials->GetWhiteTexture() : m_pBaseTexture;

		//g_pShaderAPI->CopyFramebufferToTexture( m_pBaseTexture );

		g_pShaderAPI->SetTexture(pSetupTexture, "BaseTextureSampler", 0);
	}

	const char* GetName()
	{
		return "HDRBlurFilter";
	}

	ITexture*	GetBaseTexture(int stage)
	{
		return m_pBaseTexture;
	}

	ITexture*	GetBumpTexture(int stage)
	{
		return NULL;
	}

	// returns main shader program
	IShaderProgram*	GetProgram()
	{
		return SHADER_PASS(Unlit);
	}
private:

	ITexture*			m_pBaseTexture;

	SHADER_DECLARE_PASS(Unlit);
};

DEFINE_SHADER(HDRBlurFilter, CHDRBlurFilter)