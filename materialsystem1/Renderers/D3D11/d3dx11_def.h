//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2023
//////////////////////////////////////////////////////////////////////////////////
// Description: Constant types for Equilibrium renderer
//////////////////////////////////////////////////////////////////////////////////

#pragma once

#define SAFE_RELEASE(p) { if (p) { p->Release(); p = nullptr; } }

static const D3D10_BLEND blendingConsts[] = {
	D3D10_BLEND_ZERO,
	D3D10_BLEND_ONE,
	D3D10_BLEND_SRC_COLOR,
	D3D10_BLEND_INV_SRC_COLOR,
	D3D10_BLEND_DEST_COLOR,
	D3D10_BLEND_INV_DEST_COLOR,
	D3D10_BLEND_SRC_ALPHA,
	D3D10_BLEND_INV_SRC_ALPHA,
	D3D10_BLEND_DEST_ALPHA,
	D3D10_BLEND_INV_DEST_ALPHA,
	D3D10_BLEND_SRC_ALPHA_SAT,
};

static const D3D10_BLEND blendingConstsAlpha[] = {
	D3D10_BLEND_ZERO,
	D3D10_BLEND_ONE,
	D3D10_BLEND_SRC_ALPHA,
	D3D10_BLEND_INV_DEST_ALPHA,
	D3D10_BLEND_DEST_ALPHA,
	D3D10_BLEND_INV_DEST_ALPHA,
	D3D10_BLEND_SRC_ALPHA,
	D3D10_BLEND_INV_SRC_ALPHA,
	D3D10_BLEND_DEST_ALPHA,
	D3D10_BLEND_INV_DEST_ALPHA,
	D3D10_BLEND_SRC_ALPHA_SAT,
};

static const D3D10_BLEND_OP blendingModes[] = {
	D3D10_BLEND_OP_ADD,
	D3D10_BLEND_OP_SUBTRACT,
	D3D10_BLEND_OP_REV_SUBTRACT,
	D3D10_BLEND_OP_MIN,
	D3D10_BLEND_OP_MAX,
};

static const D3D10_COMPARISON_FUNC comparisonConst[] = {
	D3D10_COMPARISON_NEVER,
	D3D10_COMPARISON_LESS,
	D3D10_COMPARISON_EQUAL,
	D3D10_COMPARISON_LESS_EQUAL,
	D3D10_COMPARISON_GREATER,
	D3D10_COMPARISON_NOT_EQUAL,
	D3D10_COMPARISON_GREATER_EQUAL,
	D3D10_COMPARISON_ALWAYS,
};

static const D3D10_STENCIL_OP stencilConst[] = {
	D3D10_STENCIL_OP_KEEP,
	D3D10_STENCIL_OP_ZERO,
	D3D10_STENCIL_OP_REPLACE,
	D3D10_STENCIL_OP_INVERT,
	D3D10_STENCIL_OP_INCR,
	D3D10_STENCIL_OP_DECR,
	D3D10_STENCIL_OP_INCR_SAT,
	D3D10_STENCIL_OP_DECR_SAT,
};

static const D3D10_CULL_MODE cullConst[] = {
	D3D10_CULL_NONE,
	D3D10_CULL_BACK,
	D3D10_CULL_FRONT,
};

static const D3D10_FILL_MODE fillConst[] = {
	D3D10_FILL_SOLID,
	D3D10_FILL_WIREFRAME,
	D3D10_FILL_WIREFRAME,
};

static DXGI_FORMAT formats[] = {
	DXGI_FORMAT_UNKNOWN,

	DXGI_FORMAT_R8_UNORM,
	DXGI_FORMAT_R8G8_UNORM,
	DXGI_FORMAT_R8G8B8A8_UNORM, // RGB8 not directly supported, threat as RGBA8
	DXGI_FORMAT_R8G8B8A8_UNORM,

	DXGI_FORMAT_R16_UNORM,
	DXGI_FORMAT_R16G16_UNORM,
	DXGI_FORMAT_UNKNOWN, // RGB16 not directly supported
	DXGI_FORMAT_R16G16B16A16_UNORM,

	DXGI_FORMAT_R8_SNORM,
	DXGI_FORMAT_R8G8_SNORM,
	DXGI_FORMAT_UNKNOWN, // RGB8S not directly supported
	DXGI_FORMAT_R8G8B8A8_SNORM,

	DXGI_FORMAT_R16_SNORM,
	DXGI_FORMAT_R16G16_SNORM,
	DXGI_FORMAT_UNKNOWN, // RGB16S not directly supported
	DXGI_FORMAT_R16G16B16A16_SNORM,

	DXGI_FORMAT_R16_FLOAT,
	DXGI_FORMAT_R16G16_FLOAT,
	DXGI_FORMAT_UNKNOWN, // RGB16F not directly supported
	DXGI_FORMAT_R16G16B16A16_FLOAT,

	DXGI_FORMAT_R32_FLOAT,
	DXGI_FORMAT_R32G32_FLOAT,
	DXGI_FORMAT_R32G32B32_FLOAT,
	DXGI_FORMAT_R32G32B32A32_FLOAT,

	DXGI_FORMAT_R16_SINT,
	DXGI_FORMAT_R16G16_SINT,
	DXGI_FORMAT_UNKNOWN, // RGB16I not directly supported
	DXGI_FORMAT_R16G16B16A16_SINT,

	DXGI_FORMAT_R32_SINT,
	DXGI_FORMAT_R32G32_SINT,
	DXGI_FORMAT_R32G32B32_SINT,
	DXGI_FORMAT_R32G32B32A32_SINT,

	DXGI_FORMAT_R16_UINT,
	DXGI_FORMAT_R16G16_UINT,
	DXGI_FORMAT_UNKNOWN, // RGB16UI not directly supported
	DXGI_FORMAT_R16G16B16A16_UINT,

	DXGI_FORMAT_R32_UINT,
	DXGI_FORMAT_R32G32_UINT,
	DXGI_FORMAT_R32G32B32_UINT,
	DXGI_FORMAT_R32G32B32A32_UINT,

	DXGI_FORMAT_UNKNOWN, // RGBE8 not directly supported
	DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
	DXGI_FORMAT_R11G11B10_FLOAT,
	DXGI_FORMAT_B5G6R5_UNORM,
	DXGI_FORMAT_UNKNOWN, // RGBA4 not directly supported
	DXGI_FORMAT_R10G10B10A2_UNORM,

	DXGI_FORMAT_D16_UNORM,
	DXGI_FORMAT_D24_UNORM_S8_UINT,
	DXGI_FORMAT_D24_UNORM_S8_UINT,
	DXGI_FORMAT_D32_FLOAT,

	DXGI_FORMAT_BC1_UNORM,
	DXGI_FORMAT_BC2_UNORM,
	DXGI_FORMAT_BC3_UNORM,
	DXGI_FORMAT_BC4_UNORM,
	DXGI_FORMAT_BC5_UNORM,

	// TODO: more BC

	// mobile compressed formats are unsupported
	DXGI_FORMAT_UNKNOWN,
	DXGI_FORMAT_UNKNOWN,
	DXGI_FORMAT_UNKNOWN,
	DXGI_FORMAT_UNKNOWN,
	DXGI_FORMAT_UNKNOWN,
	DXGI_FORMAT_UNKNOWN,
	DXGI_FORMAT_UNKNOWN,
	DXGI_FORMAT_UNKNOWN
};

static const DXGI_FORMAT d3ddecltypes[][4] = {
	DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
	DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_UNKNOWN,         DXGI_FORMAT_R16G16B16A16_FLOAT,
	DXGI_FORMAT_R8_UNORM,  DXGI_FORMAT_R8G8_UNORM,   DXGI_FORMAT_UNKNOWN,         DXGI_FORMAT_R8G8B8A8_UNORM,
};
/*
static const char *semantics[] = {
	nullptr,
	"Position",
	"Texcoord",
	"Normal",
	"Tangent",
	"Binormal",
};
*/

static const D3D10_USAGE g_d3d9_bufferUsages[] = {
	D3D10_USAGE_DEFAULT,
	D3D10_USAGE_IMMUTABLE,
	D3D10_USAGE_DYNAMIC,
};

const D3D10_PRIMITIVE_TOPOLOGY d3dPrim[] = {
	D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED, // Triangle fans not supported
	D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
	D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED, // Quads not supported
	D3D10_PRIMITIVE_TOPOLOGY_LINELIST,
	D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP,
	D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED, // Line loops not supported
	D3D10_PRIMITIVE_TOPOLOGY_POINTLIST,
};

const D3D10_FILTER d3dFilterType[] = {
	D3D10_FILTER_MIN_MAG_MIP_POINT,
	D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT,
	D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT,
	D3D10_FILTER_MIN_MAG_MIP_LINEAR,
	D3D10_FILTER_ANISOTROPIC,
	D3D10_FILTER_ANISOTROPIC,
};

const D3D10_TEXTURE_ADDRESS_MODE d3dAddressMode[] = {
	D3D10_TEXTURE_ADDRESS_WRAP,
	D3D10_TEXTURE_ADDRESS_CLAMP,
	D3D10_TEXTURE_ADDRESS_MIRROR,
};