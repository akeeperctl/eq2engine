//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2014
//////////////////////////////////////////////////////////////////////////////////
// Description: ShaderAPI render state base
//////////////////////////////////////////////////////////////////////////////////

#pragma once

enum RenderStateType_e
{
	RENDERSTATE_DEPTHSTENCIL = 0,
	RENDERSTATE_BLENDING,
	RENDERSTATE_RASTERIZER,
	RENDERSTATE_SAMPLER,

	RENDERSTATE_COUNT,
};

class IRenderState : public RefCountedObject<IRenderState, RefCountedKeepPolicy>
{
public:
	// type of render state
	virtual RenderStateType_e	GetType() const = 0;
	virtual	const void*			GetDescPtr() const = 0;
};