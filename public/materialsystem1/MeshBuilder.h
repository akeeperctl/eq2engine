//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: a mesh builder designed for dynamic meshes (material system)
//////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "IDynamicMesh.h"
#include "renderers/ShaderAPI_defs.h"

class CMeshBuilder
{
public:
	CMeshBuilder(IDynamicMesh* mesh);
	~CMeshBuilder();

	// begins the mesh
	void		Begin(EPrimTopology type);

	// ends building and fills draw command
	bool		End(RenderDrawCmd& drawCmd);

	// ends mesh building
	void		End();

	// position setup
	void		Position3f(float x, float y, float z);
	void		Position3fv(const Vector3D& v);
	void		Position2f(float x, float y);
	void		Position2fv(const Vector2D& v);

	// normal setup
	void		Normal3f(float nx, float ny, float nz);
	void		Normal3fv(const Vector3D& v);

	// texcoord setup
	void		TexCoord2f(float s, float t);
	void		TexCoord2fv(const Vector2D& v);
	void		TexCoord3f(float s, float t, float r);
	void		TexCoord3fv(const Vector3D& v);

	// color setup
	void		Color3f( float r, float g, float b );
	void		Color3fv( const ColorRGB& rgb );
	void		Color4f( float r, float g, float b, float a );
	void		Color4fv( const ColorRGBA& rgba );
	void		Color3(const MColor& rgb);
	void		Color4(const MColor& rgba);

	//
	// complex primitives (also using index buffer)
	//

	// lines
	void		Line2fv(const Vector2D& v1, const Vector2D& v2);
	void		Line3fv(const Vector3D& v1, const Vector3D& v2);

	// Makes 2D triangle
	// to set quad color use Color3*/Color4* operators
	void		Triangle2(	const Vector2D& v1, const Vector2D& v2, const Vector2D& v3);

	// Makes 3D triangle
	// to set quad color use Color3*/Color4* operators
	void		Triangle3(	const Vector3D& v1, const Vector3D& v2, const Vector3D& v3);

	// Makes 2D quad
	// to set quad color use Color3*/Color4* operators
	void		Quad2(	const Vector2D& v_tl, const Vector2D& v_tr, const Vector2D& v_bl, const Vector2D& v_br);

	// Makes 3D quad
	// to set quad color use Color3*/Color4* operators
	void		Quad3(const Vector3D& v1, const Vector3D& v2, const Vector3D& v3, const Vector3D& v4);

	// Makes 2D textured quad
	// to set quad color use Color3*/Color4* operators
	void		TexturedQuad2(	const Vector2D& v_tl, const Vector2D& v_tr, const Vector2D& v_bl, const Vector2D& v_br,
								const Vector2D& t_tl, const Vector2D& t_tr, const Vector2D& t_bl,const Vector2D& t_br);

	// Makes 3D textured quad
	// to set quad color use Color3*/Color4* operators
	void		TexturedQuad3(	const Vector3D& v_tl, const Vector3D& v_tr, const Vector3D& v_bl, const Vector3D& v_br,
								const Vector2D& t_tl, const Vector2D& t_tr, const Vector2D& t_bl,const Vector2D& t_br);

	// advances vertex, no indexing issued
	void		AdvanceVertex();

	// advances index with current index
	int			AdvanceVertexIndex();

	// advances index with custom index
	int			AdvanceVertexIndex(uint16 index);

	IDynamicMesh* GetMesh() const { return m_mesh; }

protected:

	IDynamicMesh*					m_mesh;
	ArrayCRef<VertexFormatDesc>	m_formatDesc;

	void*				m_curVertex;
	int					m_stride;

	struct vertdata_t
	{
		vertdata_t(){
			offset = -1;
			count = 0;
		}

		int8				offset;
		int8				count;
		EVertAttribFormat	format;
		Vector4D			value;
	};

	void				CopyVertData(vertdata_t& vert, bool isNormal = false);
	void				AdvanceVertexPtr();

	vertdata_t			m_position;
	vertdata_t			m_normal;
	vertdata_t			m_texcoord;
	vertdata_t			m_color;

	bool				m_pushedVert;

	bool				m_begun;
};

//----------------------------------------------------------

inline CMeshBuilder::CMeshBuilder(IDynamicMesh* mesh) :
	m_mesh(nullptr),
	m_curVertex(nullptr),
	m_formatDesc(nullptr),
	m_stride(0),
	m_begun(false)
{
	m_mesh = mesh;

	// get the format offsets
	m_formatDesc = m_mesh->GetVertexFormatDesc();

	int vertexSize = 0;
	for(int i = 0; i < m_formatDesc.numElem(); i++)
	{
		const EVertAttribFormat format = m_formatDesc[i].attribFormat;
		const EVertAttribType type = static_cast<EVertAttribType>(m_formatDesc[i].attribType & VERTEXATTRIB_MASK);
		const int vecCount = m_formatDesc[i].elemCount;
		const int attribSize = vecCount * s_attributeSize[format];

		if(type == VERTEXATTRIB_POSITION)
		{
			m_position.offset = vertexSize;
			m_position.count = vecCount;
			m_position.format = format;
		}
		else if(type == VERTEXATTRIB_NORMAL)
		{
			m_normal.offset = vertexSize;
			m_normal.count = vecCount;
			m_normal.format = format;
		}
		else if(type == VERTEXATTRIB_TEXCOORD)
		{
			m_texcoord.offset = vertexSize;
			m_texcoord.count = vecCount;
			m_texcoord.format = format;
		}
		else if(type == VERTEXATTRIB_COLOR)
		{
			m_color.offset = vertexSize;
			m_color.count = vecCount;
			m_color.format = format;
		}

		// TODO: add Tangent and Binormal
		vertexSize += attribSize;
	}

	m_stride = vertexSize;
}

inline CMeshBuilder::~CMeshBuilder()
{

}

// begins the mesh
inline void CMeshBuilder::Begin(EPrimTopology type)
{
	m_mesh->Reset();
	m_mesh->SetPrimitiveType(type);

	m_position.value = Vector4D(0,0,0,1.0f);
	m_texcoord.value = vec4_zero;
	m_normal.value = Vector4D(0,1,0,0);
	m_color.value = color_white;

	m_curVertex = nullptr;

	m_begun = true;
	m_pushedVert = false;
}

// ends building and renders the mesh
inline bool CMeshBuilder::End(RenderDrawCmd& drawCmd)
{
	const bool success = m_mesh->FillDrawCmd(drawCmd);
	m_begun = false;
	return success;
}

inline void CMeshBuilder::End()
{
	m_begun = false;
}

// position setup
inline void CMeshBuilder::Position3f(float x, float y, float z)
{
	m_position.value.x = x;
	m_position.value.y = y;
	m_position.value.z = z;
	m_position.value.w = 1.0f;
	m_pushedVert =  true;
}

inline void CMeshBuilder::Position3fv(const Vector3D& v)
{
	Position3f(v[0], v[1], v[2]);
}

inline void CMeshBuilder::Position2f(float x, float y)
{
	m_position.value.x = x;
	m_position.value.y = y;
	m_position.value.z = 0.0f;
	m_position.value.w = 1.0f;
	m_pushedVert =  true;
}

inline void CMeshBuilder::Position2fv(const Vector2D& v)
{
	Position2f(v[0], v[1]);
}

// normal setup
inline void CMeshBuilder::Normal3f(float nx, float ny, float nz)
{
	m_normal.value.x = nx;
	m_normal.value.y = ny;
	m_normal.value.z = nz;
	m_normal.value.w = 0.0f;
}

inline void CMeshBuilder::Normal3fv(const Vector3D& v)
{
	Normal3f(v[0], v[1], v[2]);
}

inline void CMeshBuilder::TexCoord2f(float s, float t)
{
	m_texcoord.value.x = s;
	m_texcoord.value.y = t;
	m_texcoord.value.z = 0.0f;
	m_texcoord.value.w = 0.0f;
}

inline void CMeshBuilder::TexCoord2fv(const Vector2D& v)
{
	TexCoord2f(v[0],v[1]);
}

inline void CMeshBuilder::TexCoord3f(float s, float t, float r)
{
	m_texcoord.value.x = s;
	m_texcoord.value.y = t;
	m_texcoord.value.z = r;
	m_texcoord.value.w = 0.0f;
}

inline void CMeshBuilder::TexCoord3fv(const Vector3D& v)
{
	TexCoord3f(v[0], v[1], v[2]);
}

// color setup
inline void CMeshBuilder::Color3f( float r, float g, float b )
{
	m_color.value.x = r;
	m_color.value.y = g;
	m_color.value.z = b;
	m_color.value.w = 1.0f;
}

inline void CMeshBuilder::Color3fv( const ColorRGB& rgb )
{
	Color3f(rgb.x, rgb.y, rgb.z);
}

inline void CMeshBuilder::Color4f( float r, float g, float b, float a )
{
	m_color.value.x = r;
	m_color.value.y = g;
	m_color.value.z = b;
	m_color.value.w = a;
}

inline void CMeshBuilder::Color4fv( const ColorRGBA& rgba )
{
	Color4f(rgba.x, rgba.y, rgba.z, rgba.w);
}

inline void CMeshBuilder::Color3(const MColor& rgb)
{
	Color3f(rgb.r, rgb.g, rgb.b);
}

inline void CMeshBuilder::Color4(const MColor& rgba)
{
	Color4f(rgba.r, rgba.g, rgba.b, rgba.a);
}


// advances vertex
inline void CMeshBuilder::AdvanceVertex()
{
	if(!m_begun)
		return;

	if(!m_pushedVert)
		return;

	m_pushedVert = false;

	if(m_mesh->AllocateGeom(1, 0, &m_curVertex, nullptr, false) == -1)
		return;

	CopyVertData(m_position);
	CopyVertData(m_texcoord);
	CopyVertData(m_normal, true);
	CopyVertData(m_color);
}

// advances index with current index
inline int CMeshBuilder::AdvanceVertexIndex()
{
	if(!m_begun)
		return -1;

	if(!m_pushedVert)
		return -1;

	m_pushedVert = false;

	uint16* inputIdx;

	int curVertex = m_mesh->AllocateGeom(1, 1, &m_curVertex, &inputIdx, false);
	if(curVertex == -1)
		return -1;

	CopyVertData(m_position);
	CopyVertData(m_texcoord);
	CopyVertData(m_normal, true);
	CopyVertData(m_color);

	*inputIdx = curVertex;
	return curVertex;
}

// advances index with custom index
inline int CMeshBuilder::AdvanceVertexIndex(uint16 index)
{
	if(!m_begun)
		return -1;

	if(index == 0xFFFF)
	{
		m_mesh->AddStripBreak();
		return -1;
	}

	uint16* inputIdx;

	int curVertex = m_mesh->AllocateGeom(m_pushedVert ? 1 : 0, 1, &m_curVertex, &inputIdx, false);
	if(curVertex == -1)
	{
		m_pushedVert = false;
		return -1;
	}

	if(m_pushedVert)
	{
		CopyVertData(m_position);
		CopyVertData(m_texcoord);
		CopyVertData(m_normal, true);
		CopyVertData(m_color);

		m_pushedVert = false;
	}

	*inputIdx = index;
	return curVertex;
}

inline void CMeshBuilder::AdvanceVertexPtr()
{
	if(!m_curVertex) // first it needs to be allocated
		return;

	if(!m_pushedVert)
		return;

	m_pushedVert = false;

	CopyVertData(m_position);
	CopyVertData(m_texcoord);
	CopyVertData(m_normal, true);
	CopyVertData(m_color);

	m_curVertex = (ubyte*)m_curVertex + m_stride;
}

//-------------------------------------------------------------------------------
// Complex primitives
//-------------------------------------------------------------------------------

// lines
inline void CMeshBuilder::Line2fv(const Vector2D& v1, const Vector2D& v2)
{
	Position2fv(v1); AdvanceVertex();
	Position2fv(v2); AdvanceVertex();
}

inline void CMeshBuilder::Line3fv(const Vector3D& v1, const Vector3D& v2)
{
	Position3fv(v1); AdvanceVertex();
	Position3fv(v2); AdvanceVertex();
}

// Makes 2D triangle
// to set quad color use Color3*/Color4* operators
inline void CMeshBuilder::Triangle2( const Vector2D& v1, const Vector2D& v2, const Vector2D& v3 )
{
	//ER_PrimitiveType primType = m_mesh->GetPrimitiveType();
	uint16* indices = nullptr;

	int startIndex = m_mesh->AllocateGeom(3, 3, &m_curVertex, &indices);

	if(startIndex == -1)
		return;

	Position2fv(v1);
	AdvanceVertexPtr();

	Position2fv(v2);
	AdvanceVertexPtr();

	Position2fv(v3);
	AdvanceVertexPtr();

	indices[0] = startIndex;
	indices[1] = startIndex+1;
	indices[2] = startIndex+2;
}

// Makes 3D triangle
// to set quad color use Color3*/Color4* operators
inline void CMeshBuilder::Triangle3(const Vector3D& v1, const Vector3D& v2, const Vector3D& v3)
{
	//ER_PrimitiveType primType = m_mesh->GetPrimitiveType();
	uint16* indices = nullptr;

	int startIndex = m_mesh->AllocateGeom(3, 3, &m_curVertex, &indices);

	if (startIndex == -1)
		return;

	Position3fv(v1);
	AdvanceVertexPtr();

	Position3fv(v2);
	AdvanceVertexPtr();

	Position3fv(v3);
	AdvanceVertexPtr();

	indices[0] = startIndex;
	indices[1] = startIndex + 1;
	indices[2] = startIndex + 2;
}

// Makes 2D quad
// to set quad color use Color3*/Color4* operators
inline void CMeshBuilder::Quad2(const Vector2D& v_tl, const Vector2D& v_tr, const Vector2D& v_bl, const Vector2D& v_br)
{
	EPrimTopology primType = m_mesh->GetPrimitiveType();
	uint16* indices = nullptr;

	int quadIndices = (primType == PRIM_TRIANGLES) ? 6 : 4;

	int startIndex = m_mesh->AllocateGeom(4, quadIndices, &m_curVertex, &indices);

	if(startIndex == -1)
		return;

	// top left 0
	Position2fv(v_tl);
	AdvanceVertexPtr();

	// top right 1
	Position2fv(v_tr);
	AdvanceVertexPtr();

	// bottom left 2
	Position2fv(v_bl);
	AdvanceVertexPtr();

	// bottom right 3
	Position2fv(v_br);
	AdvanceVertexPtr();

	// make indices working
	if(primType == PRIM_TRIANGLES)
	{
		indices[0] = startIndex;
		indices[1] = startIndex+1;
		indices[2] = startIndex+2;

		indices[3] = startIndex+2;
		indices[4] = startIndex+1;
		indices[5] = startIndex+3;
	}
	else // more linear
	{
		indices[0] = startIndex;
		indices[1] = startIndex+1;
		indices[2] = startIndex+2;
		indices[3] = startIndex+3;
	}
}

// Makes textured quad
// to set quad color use Color3*/Color4* operators
inline void CMeshBuilder::TexturedQuad2(const Vector2D& v_tl, const Vector2D& v_tr, const Vector2D& v_bl, const Vector2D& v_br,
										const Vector2D& t_tl, const Vector2D& t_tr, const Vector2D& t_bl,const Vector2D& t_br)
{
	EPrimTopology primType = m_mesh->GetPrimitiveType();
	uint16* indices = nullptr;

	int quadIndices = (primType == PRIM_TRIANGLES) ? 6 : 4;

	int startIndex = m_mesh->AllocateGeom(4, quadIndices, &m_curVertex, &indices);

	if(startIndex == -1)
		return;

	// top left 0
	Position2fv(v_tl);
	TexCoord2fv(t_tl);
	AdvanceVertexPtr();

	// top right 1
	Position2fv(v_tr);
	TexCoord2fv(t_tr);
	AdvanceVertexPtr();

	// bottom left 2
	Position2fv(v_bl);
	TexCoord2fv(t_bl);
	AdvanceVertexPtr();

	// bottom right 3
	Position2fv(v_br);
	TexCoord2fv(t_br);
	AdvanceVertexPtr();

	// make indices working
	if(primType == PRIM_TRIANGLES)
	{
		indices[0] = startIndex;
		indices[1] = startIndex+1;
		indices[2] = startIndex+2;

		indices[3] = startIndex+2;
		indices[4] = startIndex+1;
		indices[5] = startIndex+3;
	}
	else // more linear
	{
		indices[0] = startIndex;
		indices[1] = startIndex+1;
		indices[2] = startIndex+2;
		indices[3] = startIndex+3;
	}
}

// Makes textured quad
// to set quad color use Color3*/Color4* operators
inline void CMeshBuilder::Quad3(const Vector3D& v1, const Vector3D& v2, const Vector3D& v3, const Vector3D& v4)
{
	EPrimTopology primType = m_mesh->GetPrimitiveType();
	uint16* indices = nullptr;

	int quadIndices = (primType == PRIM_TRIANGLES) ? 6 : 4;

	int startIndex = m_mesh->AllocateGeom(4, quadIndices, &m_curVertex, &indices);

	if (startIndex == -1)
		return;

	// top left 0
	Position3fv(v1);
	AdvanceVertexPtr();

	// top right 1
	Position3fv(v2);
	AdvanceVertexPtr();

	// bottom left 2
	Position3fv(v3);
	AdvanceVertexPtr();

	// bottom right 3
	Position3fv(v4);
	AdvanceVertexPtr();

	// make indices working
	if (primType == PRIM_TRIANGLES)
	{
		indices[0] = startIndex;
		indices[1] = startIndex + 1;
		indices[2] = startIndex + 2;

		indices[3] = startIndex + 2;
		indices[4] = startIndex + 1;
		indices[5] = startIndex + 3;
	}
	else // more linear
	{
		indices[0] = startIndex;
		indices[1] = startIndex + 1;
		indices[2] = startIndex + 2;
		indices[3] = startIndex + 3;
	}
}

// Makes textured quad
// to set quad color use Color3*/Color4* operators
inline void CMeshBuilder::TexturedQuad3(const Vector3D& v1, const Vector3D& v2, const Vector3D& v3, const Vector3D& v4,
										const Vector2D& t1, const Vector2D& t2, const Vector2D& t3,const Vector2D& t4)
{
	EPrimTopology primType = m_mesh->GetPrimitiveType();
	uint16* indices = nullptr;

	int quadIndices = (primType == PRIM_TRIANGLES) ? 6 : 4;

	int startIndex = m_mesh->AllocateGeom(4, quadIndices, &m_curVertex, &indices);

	if(startIndex == -1)
		return;

	// top left 0
	Position3fv(v1);
	TexCoord2fv(t1);
	AdvanceVertexPtr();

	// top right 1
	Position3fv(v2);
	TexCoord2fv(t2);
	AdvanceVertexPtr();

	// bottom left 2
	Position3fv(v3);
	TexCoord2fv(t3);
	AdvanceVertexPtr();

	// bottom right 3
	Position3fv(v4);
	TexCoord2fv(t4);
	AdvanceVertexPtr();

	// make indices working
	if(primType == PRIM_TRIANGLES)
	{
		indices[0] = startIndex;
		indices[1] = startIndex+1;
		indices[2] = startIndex+2;

		indices[3] = startIndex+2;
		indices[4] = startIndex+1;
		indices[5] = startIndex+3;
	}
	else // more linear
	{
		indices[0] = startIndex;
		indices[1] = startIndex+1;
		indices[2] = startIndex+2;
		indices[3] = startIndex+3;
	}
}

inline void CMeshBuilder::CopyVertData(vertdata_t& vert, bool isNormal)
{
	if(!vert.count)
		return;

	ubyte* dest = (ubyte*)m_curVertex + vert.offset;

	int size = min((int)vert.count, 4) * s_attributeSize[vert.format];

	switch(vert.format)
	{
		case ATTRIBUTEFORMAT_FLOAT:
		{
			memcpy(dest, &vert.value, size);
			break;
		}
		case ATTRIBUTEFORMAT_HALF:
		{
			TVec4D<half> val(vert.value);
			memcpy(dest, &val, size);
			break;
		}
		case ATTRIBUTEFORMAT_UBYTE:
		{
			Vector4D vnorm = (isNormal ? (vert.value * 0.5f + 0.5f) : vert.value);
			TVec4D<ubyte> val(clamp(vnorm, Vector4D(0.0f), Vector4D(1.0f)) * 255);
			memcpy(dest, &val, size);

			break;
		}
	}
}
