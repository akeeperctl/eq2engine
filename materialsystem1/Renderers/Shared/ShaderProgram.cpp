//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2022
//////////////////////////////////////////////////////////////////////////////////
// Description: Shader program base
//////////////////////////////////////////////////////////////////////////////////

#include "core/core_common.h"
#include "ShaderProgram.h"

const char* CShaderProgram::GetName() const
{ 
	return m_szName.ToCString();
}

void CShaderProgram::SetName(const char* pszName)
{
	m_szName = pszName;
	m_nameHash = StringToHash(pszName);
}