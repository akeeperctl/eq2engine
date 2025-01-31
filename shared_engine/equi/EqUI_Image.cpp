//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: EGUI label
//////////////////////////////////////////////////////////////////////////////////

#include "core/core_common.h"
#include "utils/KeyValues.h"
#include "utils/TextureAtlas.h"
#include "EqUI_Image.h"

#include "EqUI_Manager.h"

#include "materialsystem1/IMaterialSystem.h"
#include "materialsystem1/MeshBuilder.h"

namespace equi
{

Image::Image() : IUIControl()
{
	m_color = color_white;
	m_imageFlags = 0;
}

Image::~Image()
{
}

void Image::InitFromKeyValues( KVSection* sec, bool noClear )
{
	BaseClass::InitFromKeyValues(sec, noClear);

	m_color = KV_GetVector4D(sec->FindSection("color"), 0, m_color);

	bool flipX = KV_GetValueBool(sec->FindSection("flipx"), 0, (m_imageFlags & FLIP_X) > 0);
	bool flipY = KV_GetValueBool(sec->FindSection("flipy"), 0, (m_imageFlags & FLIP_Y) > 0);

	m_imageFlags = (flipX ? FLIP_X : 0) | (flipY ? FLIP_Y : 0);

	KVSection* pathBase = sec->FindSection("path");
	if (pathBase)
	{
		const char* materialPath = KV_GetValueString(pathBase, 0, "ui/default");
		SetMaterial(materialPath);
		m_atlasRegion.leftTop = Vector2D(0.0f);
		m_atlasRegion.rightBottom = Vector2D(1.0f);
	}
	else
	{
		pathBase = sec->FindSection("atlas");
	}

	if (pathBase)
	{
		const char* atlasPath = KV_GetValueString(pathBase, 0, "");

		SetMaterial(atlasPath);

		if (m_material->GetAtlas())
		{
			AtlasEntry* entry = m_material->GetAtlas()->FindEntry(KV_GetValueString(pathBase, 1));
			if (entry)
				m_atlasRegion = entry->rect;
		}
	}
	else
	{
		MsgError("EqUI error: image '%s' missing 'path' or 'atlas' property\n", m_name.ToCString());
	}
}

void Image::SetMaterial(const char* materialName)
{
	m_material = g_matSystem->GetMaterial(materialName);
	m_material->LoadShaderAndTextures();
}

void Image::SetColor(const ColorRGBA &color)
{
	m_color = color;
}

const ColorRGBA& Image::GetColor() const
{
	return m_color;
}

void Image::DrawSelf( const IAARectangle& rect, bool scissorOn)
{
	

	AARectangle atlasRect = m_atlasRegion;
	if (m_imageFlags & FLIP_X)
		atlasRect.FlipX();

	if (m_imageFlags & FLIP_Y)
		atlasRect.FlipY();

	// draw all rectangles with just single draw call
	CMeshBuilder meshBuilder(g_matSystem->GetDynamicMesh());
	RenderDrawCmd drawCmd;
	drawCmd.material = m_material;
	meshBuilder.Begin(PRIM_TRIANGLE_STRIP);
		//meshBuilder.Color4fv(m_color);
		meshBuilder.TexturedQuad2(rect.GetLeftBottom(), rect.GetRightBottom(), rect.GetLeftTop(), rect.GetRightTop(), 
			atlasRect.GetLeftBottom(), atlasRect.GetRightBottom(), atlasRect.GetLeftTop(), atlasRect.GetRightTop());
	if(meshBuilder.End(drawCmd))
	{
		g_matSystem->SetAmbientColor(m_color);
		g_matSystem->Draw(drawCmd);
	}
}

};

DECLARE_EQUI_CONTROL(image, Image)
