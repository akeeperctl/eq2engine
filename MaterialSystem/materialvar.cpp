//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2015
//////////////////////////////////////////////////////////////////////////////////
// Description: DarkTech materialvar
//////////////////////////////////////////////////////////////////////////////////

#include "DebugInterface.h"
#include "materialvar.h"
#include "utils/strtools.h"

CMatVar::CMatVar()
{
	m_nValue = 0;
	m_vector = Vector4D(0);
	m_pAssignedTexture = NULL;
	m_isDirtyString = 0;
}

// initializes the material var
void CMatVar::Init(const char* pszName,const char* pszValue)
{
	m_pszVarName = pszName;
	m_pszValue	 = pszValue;

	SetString( pszValue );
}

int CMatVar::GetInt() const
{
	return m_nValue;
}

float CMatVar::GetFloat() const
{
	return m_vector.x;
}

const char* CMatVar::GetName() const
{
	return m_pszVarName.c_str();
}
// sets new name
void CMatVar::SetName(const char* szNewName)
{
	m_pszVarName = szNewName;
}

// gives string
const char* CMatVar::GetString()
{
	if(m_isDirtyString > 0)
	{
		switch(m_isDirtyString)
		{
			case 1:	// float
				m_pszValue = varargs("%f",m_vector.x);
				break;
			case 2:	// int
				m_pszValue = varargs("%i",m_nValue);
				break;
			case 3:	// TODO: vector2D
				break;
			case 4:	// TODO: vector3D
				break;
			case 5:	// TODO: vector4D
				break;
		}
		m_isDirtyString = 0;
	}

	return m_pszValue.GetData();
}

// Value setup
void CMatVar::SetString(const char* szValue)
{
	m_pszValue = szValue;

	m_vector.x = (float)atof(m_pszValue.GetData());
	m_nValue  = (int)m_vector.x;
	sscanf(szValue,"[%f %f]",&m_vector.x, &m_vector.y);
	sscanf(szValue,"[%f %f %f]",&m_vector.x, &m_vector.y, &m_vector.z);
	sscanf(szValue,"[%f %f %f %f]",&m_vector.x, &m_vector.y, &m_vector.z, &m_vector.w);
}

void CMatVar::SetFloat(float fValue)
{
	m_vector = Vector4D(fValue,0,0,0);
	m_nValue  = (int)m_vector.x;
	m_isDirtyString = 1;
}

void CMatVar::SetInt(int nValue)
{
	m_nValue = nValue;
	m_vector = Vector4D(nValue,0,0,0);
	m_isDirtyString = 2;
}

void CMatVar::SetVector2(Vector2D &vector)
{
	m_vector.x = vector.x;
	m_vector.y = vector.y;
	m_vector.z = 0.0f;
	m_vector.w = 1.0f;

	m_isDirtyString = 3;
}

void CMatVar::SetVector3(Vector3D &vector)
{
	m_vector.x = vector.x;
	m_vector.y = vector.y;
	m_vector.z = vector.z;
	m_vector.w = 1.0f;

	m_isDirtyString = 4;
}

void CMatVar::SetVector4(Vector4D &vector)
{
	m_vector = vector;
	m_isDirtyString = 5;
}

Vector2D CMatVar::GetVector2() const
{
	return m_vector.xy();
}

Vector3D CMatVar::GetVector3() const
{
	return m_vector.xyz();
}

Vector4D CMatVar::GetVector4() const
{
	return m_vector;
}

// texture pointer
ITexture* CMatVar::GetTexture() const
{
	return m_pAssignedTexture;
}

// assigns texture
void CMatVar::AssignTexture(ITexture* pTexture)
{
	m_pAssignedTexture = pTexture;
}
