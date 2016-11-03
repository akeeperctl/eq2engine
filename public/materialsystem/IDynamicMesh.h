//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2017
//////////////////////////////////////////////////////////////////////////////////
// Description: dynamic mesh interface
//////////////////////////////////////////////////////////////////////////////////

#ifndef IDYNAMICMESH_H
#define IDYNAMICMESH_H

#include "materialsystem/renderers/IShaderAPI.h"

// the standard dynamic mesh vertex
// please don't change unless you recompile the materialsystem dll
struct StdDynMeshVertex_t
{
	TVec4D<float>	position;
	TVec4D<half>	texCoord;
	TVec4D<half>	normal;		// FIXME: can be compressed
	TVec4D<half>	color;		// FIXME: can be compressed
};

assert_sizeof(StdDynMeshVertex_t, 40); // 1.1 MB of VRAM needed for this

static VertexFormatDesc_t g_standardVertexFormatDesc[] = {
	0, 4, VERTEXTYPE_VERTEX,	ATTRIBUTEFORMAT_FLOAT,
	0, 4, VERTEXTYPE_TEXCOORD,	ATTRIBUTEFORMAT_HALF,
	0, 4, VERTEXTYPE_NORMAL,	ATTRIBUTEFORMAT_HALF,
	0, 4, VERTEXTYPE_COLOR,		ATTRIBUTEFORMAT_HALF,
};

//
// The dynamic mesh interface
//
class IDynamicMesh
{
public:
	virtual ~IDynamicMesh() {}

	// sets the primitive type (chooses the way how to allocate geometry parts)
	virtual void			SetPrimitiveType( PrimitiveType_e primType ) = 0;
	virtual PrimitiveType_e	GetPrimitiveType() const = 0;

	// returns a pointer to vertex format description
	virtual void			GetVertexFormatDesc(VertexFormatDesc_t** desc, int& numAttribs) = 0;

	// allocates geometry chunk. Returns the start index. Will return -1 if failed
	// addStripBreak is for PRIM_TRIANGLE_STRIP. Set it false to work with current strip
	//
	// note that if you use materials->GetDynamicMesh() you should pass the StdDynMeshVertex_t vertices
	//
	// FIXME: subdivide on streams???
	virtual int				AllocateGeom( int nVertices, int nIndices, void** verts, uint16** indices, bool addStripBreak = true ) = 0;

	// uploads buffers and renders the mesh. Note that you has been set material and adjusted RTs
	virtual void			Render() = 0;

	// resets the dynamic mesh
	virtual void			Reset() = 0;
};

#endif // IDYNAMICMESH_H