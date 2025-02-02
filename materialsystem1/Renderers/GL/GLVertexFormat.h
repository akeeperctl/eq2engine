//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium OpenGL ShaderAPI
//////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "renderers/IVertexFormat.h"

class CVertexFormatGL : public IVertexFormat
{
	friend class		ShaderAPIGL;
public:
	CVertexFormatGL(const char* name, const VertexFormatDesc* desc, int numAttribs);

	const char*			GetName() const { return m_name.ToCString(); }

	int					GetVertexSize(int stream) const;
	ArrayCRef<VertexFormatDesc>	GetFormatDesc() const;

protected:
	// Vertex attribute descriptor
	struct eqGLVertAttrDesc_t
	{
		int					streamId;
		int					sizeInBytes;

		EVertAttribFormat	attribFormat;
		int					offsetInBytes;
	};

	int							m_streamStride[MAX_VERTEXSTREAM];
	EqString					m_name;
	Array<VertexFormatDesc>	m_vertexDesc{ PP_SL };

	eqGLVertAttrDesc_t			m_genericAttribs[MAX_GL_GENERIC_ATTRIB];

#ifndef GL_NO_DEPRECATED_ATTRIBUTES
	eqGLVertAttrDesc_t			m_hTexCoord[MAX_TEXCOORD_ATTRIB];
	eqGLVertAttrDesc_t			m_hVertex;
	eqGLVertAttrDesc_t			m_hNormal;
	eqGLVertAttrDesc_t			m_hColor;
#endif // GL_NO_DEPRECATED_ATTRIBUTES

};