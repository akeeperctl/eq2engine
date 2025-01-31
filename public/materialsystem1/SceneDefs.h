//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Shared scene parameter definitions
//////////////////////////////////////////////////////////////////////////////////

#pragma once

// fog parameters
struct FogInfo
{
	Vector3D	viewPos;
	Vector3D	fogColor;
	float		fogdensity;
	float		fognear;
	float		fogfar;
	bool		enableFog{ false };
};
