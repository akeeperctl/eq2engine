//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Debug text drawer system
//////////////////////////////////////////////////////////////////////////////////

#include "core/core_common.h"
#include "core/ConVar.h"
#include "math/Utility.h"
#include "math/Rectangle.h"
#include "DebugOverlay.h"

#include "font/IFontCache.h"

#include "materialsystem1/IMaterialSystem.h"
#include "materialsystem1/MeshBuilder.h"

#ifdef _RETAIL
#define DISABLE_DEBUG_DRAWING
#endif

#define BOXES_DRAW_SUBDIV (64)
#define LINES_DRAW_SUBDIV (128)
#define POLYS_DRAW_SUBDIV (64)
#define GRAPH_MAX_VALUES  400

static CDebugOverlay g_DebugOverlays;
IDebugOverlay* debugoverlay = (IDebugOverlay*)&g_DebugOverlays;

#ifndef DISABLE_DEBUG_DRAWING
static Threading::CEqMutex	s_debugOverlayMutex;

DECLARE_CVAR(r_debugDrawFrameStats, "0", nullptr, CV_ARCHIVE);
DECLARE_CVAR(r_debugDrawGraphs, "0", nullptr, CV_ARCHIVE);
DECLARE_CVAR(r_debugDrawShapes, "0", nullptr, CV_ARCHIVE);
DECLARE_CVAR(r_debugDrawLines, "0", nullptr, CV_ARCHIVE);

ITexturePtr g_pDebugTexture = nullptr;

static void OnShowTextureChanged(ConVar* pVar,char const* pszOldValue)
{
	if (!g_renderAPI)
		return;

	g_pDebugTexture = g_renderAPI->FindTexture( pVar->GetString() );
}

DECLARE_CVAR_CHANGE(r_debugShowTexture, "", OnShowTextureChanged, "input texture name to show texture. To hide view input anything else.", CV_CHEAT);
DECLARE_CVAR(r_debugShowTextureScale, "1.0", nullptr, CV_ARCHIVE);

static void GUIDrawWindow(const AARectangle &rect, const ColorRGBA &color1)
{
	ColorRGBA color2(0.2,0.2,0.2,0.8);

	BlendStateParams blending;
	blending.srcFactor = BLENDFACTOR_SRC_ALPHA;
	blending.dstFactor = BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

	g_matSystem->FindGlobalMaterialVar<MatTextureProxy>(StringToHashConst("basetexture")).Set(nullptr);
	g_matSystem->SetBlendingStates(blending);
	g_matSystem->SetRasterizerStates(CULL_FRONT, FILL_SOLID);
	g_matSystem->SetDepthStates(false,false);

	Vector2D r0[] = { MAKEQUAD(rect.leftTop.x, rect.leftTop.y,rect.leftTop.x, rect.rightBottom.y, -0.5f) };
	Vector2D r1[] = { MAKEQUAD(rect.rightBottom.x, rect.leftTop.y,rect.rightBottom.x, rect.rightBottom.y, -0.5f) };
	Vector2D r2[] = { MAKEQUAD(rect.leftTop.x, rect.rightBottom.y,rect.rightBottom.x, rect.rightBottom.y, -0.5f) };
	Vector2D r3[] = { MAKEQUAD(rect.leftTop.x, rect.leftTop.y,rect.rightBottom.x, rect.leftTop.y, -0.5f) };

	// draw all rectangles with just single draw call
	CMeshBuilder meshBuilder(g_matSystem->GetDynamicMesh());
	RenderDrawCmd drawCmd;
	drawCmd.material = g_matSystem->GetDefaultMaterial();

	meshBuilder.Begin(PRIM_TRIANGLE_STRIP);
		// put main rectangle
		meshBuilder.Color4fv(color1);
		meshBuilder.Quad2(rect.GetLeftBottom(), rect.GetRightBottom(), rect.GetLeftTop(), rect.GetRightTop());

		// put borders
		meshBuilder.Color4fv(color2);
		meshBuilder.Quad2(r0[0], r0[1], r0[2], r0[3]);
		meshBuilder.Quad2(r1[0], r1[1], r1[2], r1[3]);
		meshBuilder.Quad2(r2[0], r2[1], r2[2], r2[3]);
		meshBuilder.Quad2(r3[0], r3[1], r3[2], r3[3]);
	if(meshBuilder.End(drawCmd))
		g_matSystem->Draw(drawCmd);
}

#define BBOX_STRIP_VERTS(min, max) \
	Vector3D(min.x, max.y, max.z),\
	Vector3D(max.x, max.y, max.z),\
	Vector3D(min.x, max.y, min.z),\
	Vector3D(max.x, max.y, min.z),\
	Vector3D(min.x, min.y, min.z),\
	Vector3D(max.x, min.y, min.z),\
	Vector3D(min.x, min.y, max.z),\
	Vector3D(max.x, min.y, max.z),\
	Vector3D(max.x, min.y, max.z),\
	Vector3D(max.x, min.y, min.z),\
	Vector3D(max.x, min.y, min.z),\
	Vector3D(max.x, max.y, min.z),\
	Vector3D(max.x, min.y, max.z),\
	Vector3D(max.x, max.y, max.z),\
	Vector3D(min.x, min.y, max.z),\
	Vector3D(min.x, max.y, max.z),\
	Vector3D(min.x, min.y, min.z),\
	Vector3D(min.x, max.y, min.z)

// TODO: this must be replaced
static void DrawOrientedBox(const Vector3D& position, const Vector3D& mins, const Vector3D& maxs, const Quaternion& quat, const ColorRGBA& color, float fTime = 0.0f)
{
	Vector3D verts[18] = { BBOX_STRIP_VERTS(mins, maxs) };

	// transform them
	for (int i = 0; i < 18; i++)
		verts[i] = position + rotateVector(verts[i], quat);

	Vector3D r, u, f;
	r = rotateVector(vec3_right, quat);
	u = rotateVector(vec3_up, quat);
	f = rotateVector(vec3_forward, quat);

	debugoverlay->Line3D(position + r * mins.x, position + r * maxs.x, ColorRGBA(1, 0, 0, 1), ColorRGBA(1, 0, 0, 1));
	debugoverlay->Line3D(position + u * mins.y, position + u * maxs.y, ColorRGBA(0, 1, 0, 1), ColorRGBA(0, 1, 0, 1));
	debugoverlay->Line3D(position + f * mins.z, position + f * maxs.z, ColorRGBA(0, 0, 1, 1), ColorRGBA(0, 0, 1, 1));

	ColorRGBA polyColor(color);
	polyColor.w *= 0.65f;

	debugoverlay->Polygon3D(verts[0], verts[1], verts[2], polyColor);
	debugoverlay->Polygon3D(verts[2], verts[1], verts[3], polyColor);

	debugoverlay->Polygon3D(verts[2], verts[3], verts[4], polyColor);
	debugoverlay->Polygon3D(verts[4], verts[3], verts[5], polyColor);

	debugoverlay->Polygon3D(verts[4], verts[5], verts[6], polyColor);
	debugoverlay->Polygon3D(verts[6], verts[5], verts[7], polyColor);

	debugoverlay->Polygon3D(verts[10], verts[11], verts[12], polyColor);
	debugoverlay->Polygon3D(verts[12], verts[11], verts[13], polyColor);

	debugoverlay->Polygon3D(verts[12], verts[13], verts[14], polyColor);
	debugoverlay->Polygon3D(verts[14], verts[13], verts[15], polyColor);

	debugoverlay->Polygon3D(verts[14], verts[15], verts[16], polyColor);
	debugoverlay->Polygon3D(verts[16], verts[15], verts[17], polyColor);
}
#endif 

void CDebugOverlay::Init(bool hidden)
{
#ifndef DISABLE_DEBUG_DRAWING
	if (!hidden)
	{
		r_debugDrawFrameStats.SetBool(true);
		r_debugDrawGraphs.SetBool(true);
		r_debugDrawShapes.SetBool(true);
		r_debugDrawLines.SetBool(true);
	}
#endif

	m_debugFont = g_fontCache->GetFont("debug", 0);
	m_debugFont2 = g_fontCache->GetFont("default", 0);
}

void CDebugOverlay::Shutdown()
{
#ifndef DISABLE_DEBUG_DRAWING
	g_pDebugTexture = nullptr;
#endif
}

void CDebugOverlay::Text(const ColorRGBA &color, char const *fmt,...)
{
#ifndef DISABLE_DEBUG_DRAWING
	if(!r_debugDrawFrameStats.GetBool())
		return;

	Threading::CScopedMutex m(s_debugOverlayMutex);

	DebugTextNode_t& textNode = m_TextArray.append();
	textNode.color = MColor(color).pack();

	va_list		argptr;
	va_start (argptr,fmt);
	textNode.pszText = EqString::FormatVa(fmt, argptr);
	va_end (argptr);
#endif
}

void CDebugOverlay::Text3D(const Vector3D &origin, float dist, const ColorRGBA &color, const char* text, float fTime, int hashId)
{
#ifndef DISABLE_DEBUG_DRAWING
	if(hashId == 0 && !m_frustum.IsSphereInside(origin, 1.0f))
		return;

	Threading::CScopedMutex m(s_debugOverlayMutex);

	DebugText3DNode_t& textNode = m_Text3DArray.append();
	textNode.color = MColor(color).pack();
	textNode.origin = origin;
	textNode.dist = dist;
	textNode.lifetime = fTime;
	textNode.pszText = text;

	textNode.frameindex = m_frameId;
	textNode.nameHash = hashId;

	if (hashId != 0)
		m_newNames.insert(hashId, m_frameId);
#endif
}

#define MAX_MINICON_MESSAGES 32

void CDebugOverlay::TextFadeOut(int position, const ColorRGBA &color,float fFadeTime, char const *fmt, ...)
{
#ifndef DISABLE_DEBUG_DRAWING
	if(position == 1)
	{
		if(!r_debugDrawFrameStats.GetBool())
			return;
	}

	Threading::CScopedMutex m(s_debugOverlayMutex);

	DebugFadingTextNode_t textNode;
	textNode.color = MColor(color).pack();
	textNode.lifetime = fFadeTime;
	textNode.initialLifetime = fFadeTime;

	va_list		argptr;
	va_start (argptr,fmt);
	textNode.pszText = EqString::FormatVa(fmt, argptr);
	va_end (argptr);
	if(position == 0)
	{
		m_LeftTextFadeArray.append(std::move(textNode));

		if(m_LeftTextFadeArray.getCount() > MAX_MINICON_MESSAGES)
			m_LeftTextFadeArray.popFront();
	}
	else
		m_RightTextFadeArray.append(std::move(textNode));
#endif
}

void CDebugOverlay::Box3D(const Vector3D &mins, const Vector3D &maxs, const ColorRGBA &color, float fTime, int hashId)
{
#ifndef DISABLE_DEBUG_DRAWING
	if(!r_debugDrawShapes.GetBool())
		return;

	if(hashId == 0 && !m_frustum.IsBoxInside(mins,maxs))
		return;

	Threading::CScopedMutex m(s_debugOverlayMutex);

	DebugBoxNode_t& box = m_BoxList.append();

	box.mins = mins;
	box.maxs = maxs;
	box.color = MColor(color).pack();
	box.lifetime = fTime;

	box.frameindex = m_frameId;
	box.nameHash = hashId;

	if (hashId != 0)
		m_newNames.insert(hashId, m_frameId);
#endif
}

void CDebugOverlay::Cylinder3D(const Vector3D& position, float radius, float height, const ColorRGBA& color, float fTime, int hashId)
{
#ifndef DISABLE_DEBUG_DRAWING
	if (!r_debugDrawShapes.GetBool())
		return;

	Vector3D boxSize(radius, height * 0.5f, radius);
	if(hashId == 0 && !m_frustum.IsSphereInside(position, max(radius, height)))
		return;

	Threading::CScopedMutex m(s_debugOverlayMutex);

	DebugCylinderNode_t& cyl = m_CylinderList.append();
	cyl.origin = position;
	cyl.radius = radius;
	cyl.height = height;
	cyl.color = MColor(color).pack();
	cyl.lifetime = fTime;

	cyl.frameindex = m_frameId;
	cyl.nameHash = hashId;

	if (hashId != 0)
		m_newNames.insert(hashId, m_frameId);
#endif
}

void CDebugOverlay::Line3D(const Vector3D &start, const Vector3D &end, const ColorRGBA &color1, const ColorRGBA &color2, float fTime, int hashId)
{
#ifndef DISABLE_DEBUG_DRAWING
	if(!r_debugDrawLines.GetBool())
		return;

	if(hashId == 0 && !m_frustum.IsBoxInside(start,end))
		return;

	Threading::CScopedMutex m(s_debugOverlayMutex);

	DebugLineNode_t& line = m_LineList.append();
	line.start = start;
	line.end = end;
	line.color1 = MColor(color1).pack();
	line.color2 = MColor(color2).pack();
	line.lifetime = fTime;

	line.frameindex = m_frameId;
	line.nameHash = hashId;

	if (hashId != 0)
		m_newNames.insert(hashId, m_frameId);
#endif
}

void CDebugOverlay::OrientedBox3D(const Vector3D& mins, const Vector3D& maxs, const Vector3D& position, const Quaternion& rotation, const ColorRGBA& color, float fTime, int hashId)
{
#ifndef DISABLE_DEBUG_DRAWING
	if(!r_debugDrawShapes.GetBool())
		return;

	if(hashId == 0 && !m_frustum.IsBoxInside(position+mins, position+maxs))
		return;

	Threading::CScopedMutex m(s_debugOverlayMutex);

	DebugOriBoxNode_t& box = m_OrientedBoxList.append();
	box.mins = mins;
	box.maxs = maxs;
	box.position = position;
	box.rotation = rotation;
	box.color = MColor(color).pack();
	box.lifetime = fTime;

	box.frameindex = m_frameId;
	box.nameHash = hashId;

	if (hashId != 0)
		m_newNames.insert(hashId, m_frameId);
#endif
}

void CDebugOverlay::Sphere3D(const Vector3D& position, float radius, const ColorRGBA &color, float fTime, int hashId)
{
#ifndef DISABLE_DEBUG_DRAWING
	if(!r_debugDrawShapes.GetBool())
		return;

	if(hashId == 0 && !m_frustum.IsSphereInside(position, radius))
		return;

	Threading::CScopedMutex m(s_debugOverlayMutex);

	DebugSphereNode_t& sphere = m_SphereList.append();
	sphere.origin = position;
	sphere.radius = radius;
	sphere.color = MColor(color).pack();
	sphere.lifetime = fTime;

	sphere.frameindex = m_frameId;
	sphere.nameHash = hashId;

	if (hashId != 0)
		m_newNames.insert(hashId, m_frameId);
#endif
}

void CDebugOverlay::Polygon3D(const Vector3D &v0, const Vector3D &v1,const Vector3D &v2, const Vector4D &color, float fTime, int hashId)
{
#ifndef DISABLE_DEBUG_DRAWING
	if(hashId == 0 && !m_frustum.IsTriangleInside(v0,v1,v2))
		return;

	Threading::CScopedMutex m(s_debugOverlayMutex);

	DebugPolyNode_t& poly = m_polygons.append();
	poly.v0 = v0;
	poly.v1 = v1;
	poly.v2 = v2;

	poly.color = MColor(color).pack();
	poly.lifetime = fTime;

	poly.frameindex = m_frameId;
	poly.nameHash = hashId;

	if (hashId != 0)
		m_newNames.insert(hashId, m_frameId);
#endif
}

void CDebugOverlay::Draw2DFunc(const OnDebugDrawFn& func, float fTime, int hashId)
{
#ifndef DISABLE_DEBUG_DRAWING
	Threading::CScopedMutex m(s_debugOverlayMutex);

	DebugDrawFunc_t& fn = m_draw2DFuncs.append();
	fn.func = func;
	fn.lifetime = fTime;

	fn.frameindex = m_frameId;
	fn.nameHash = hashId;

	if (hashId != 0)
		m_newNames.insert(hashId, m_frameId);
#endif
}

void CDebugOverlay::Draw3DFunc(const OnDebugDrawFn& func, float fTime, int hashId)
{
#ifndef DISABLE_DEBUG_DRAWING
	Threading::CScopedMutex m(s_debugOverlayMutex);

	DebugDrawFunc_t& fn = m_draw3DFuncs.append();
	fn.func = func;
	fn.lifetime = fTime;

	fn.frameindex = m_frameId;
	fn.nameHash = hashId;

	if (hashId != 0)
		m_newNames.insert(hashId, m_frameId);
#endif
}

#ifndef DISABLE_DEBUG_DRAWING
static void DrawLineArray(Array<DebugLineNode_t>& lines, float frametime)
{
	if(!lines.numElem())
		return;

	g_matSystem->FindGlobalMaterialVar<MatTextureProxy>(StringToHashConst("basetexture")).Set(nullptr);

	g_matSystem->SetBlendingStates(BLENDFACTOR_SRC_ALPHA, BLENDFACTOR_ONE_MINUS_SRC_ALPHA,BLENDFUNC_ADD);
	g_matSystem->SetRasterizerStates(CULL_NONE,FILL_SOLID);
	g_matSystem->SetDepthStates(true, false);

	RenderDrawCmd drawCmd;
	drawCmd.material = g_matSystem->GetDefaultMaterial();

	CMeshBuilder meshBuilder(g_matSystem->GetDynamicMesh());
	meshBuilder.Begin(PRIM_LINES);

		for(int i = 0; i < lines.numElem(); i++)
		{
			DebugLineNode_t& line = lines[i];

			meshBuilder.Color4(line.color1);
			meshBuilder.Position3fv(line.start);

			meshBuilder.AdvanceVertex();

			meshBuilder.Color4(line.color2);
			meshBuilder.Position3fv(line.end);

			meshBuilder.AdvanceVertex();

			line.lifetime -= frametime;
		}

	if(meshBuilder.End(drawCmd))
		g_matSystem->Draw(drawCmd);
}

static void DrawOrientedBoxArray(Array<DebugOriBoxNode_t>& boxes, float frametime)
{
	if (!boxes.numElem())
		return;

	g_matSystem->FindGlobalMaterialVar<MatTextureProxy>(StringToHashConst("basetexture")).Set(nullptr);

	g_matSystem->SetBlendingStates(BLENDFACTOR_SRC_ALPHA, BLENDFACTOR_ONE_MINUS_SRC_ALPHA, BLENDFUNC_ADD);
	g_matSystem->SetRasterizerStates(CULL_NONE, FILL_SOLID);
	g_matSystem->SetDepthStates(true, false);

	g_matSystem->Apply();

	RenderDrawCmd drawCmd;
	drawCmd.material = g_matSystem->GetDefaultMaterial();

	CMeshBuilder meshBuilder(g_matSystem->GetDynamicMesh());
	meshBuilder.Begin(PRIM_LINES);

	for (int i = 0; i < boxes.numElem(); i++)
	{
		DebugOriBoxNode_t& node = boxes[i];

		meshBuilder.Color4(node.color);

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.mins.x, node.maxs.y, node.mins.z), node.rotation),
			node.position + rotateVector(Vector3D(node.mins.x, node.maxs.y, node.maxs.z), node.rotation));

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.maxs.x, node.maxs.y, node.maxs.z), node.rotation),
			node.position + rotateVector(Vector3D(node.maxs.x, node.maxs.y, node.mins.z), node.rotation));

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.maxs.x, node.mins.y, node.mins.z), node.rotation),
			node.position + rotateVector(Vector3D(node.maxs.x, node.mins.y, node.maxs.z), node.rotation));

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.mins.x, node.mins.y, node.maxs.z), node.rotation),
			node.position + rotateVector(Vector3D(node.mins.x, node.mins.y, node.mins.z), node.rotation));

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.mins.x, node.mins.y, node.maxs.z), node.rotation),
			node.position + rotateVector(Vector3D(node.mins.x, node.maxs.y, node.maxs.z), node.rotation));

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.maxs.x, node.mins.y, node.maxs.z), node.rotation),
			node.position + rotateVector(Vector3D(node.maxs.x, node.maxs.y, node.maxs.z), node.rotation));

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.mins.x, node.mins.y, node.mins.z), node.rotation),
			node.position + rotateVector(Vector3D(node.mins.x, node.maxs.y, node.mins.z), node.rotation));

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.maxs.x, node.mins.y, node.mins.z), node.rotation),
			node.position + rotateVector(Vector3D(node.maxs.x, node.maxs.y, node.mins.z), node.rotation));

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.mins.x, node.maxs.y, node.mins.z), node.rotation),
			node.position + rotateVector(Vector3D(node.maxs.x, node.maxs.y, node.mins.z), node.rotation));

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.mins.x, node.maxs.y, node.maxs.z), node.rotation),
			node.position + rotateVector(Vector3D(node.maxs.x, node.maxs.y, node.maxs.z), node.rotation));

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.mins.x, node.mins.y, node.mins.z), node.rotation),
			node.position + rotateVector(Vector3D(node.maxs.x, node.mins.y, node.mins.z), node.rotation));

		meshBuilder.Line3fv(node.position + rotateVector(Vector3D(node.mins.x, node.mins.y, node.maxs.z), node.rotation),
			node.position + rotateVector(Vector3D(node.maxs.x, node.mins.y, node.maxs.z), node.rotation));

		node.lifetime -= frametime;

		if ((i % BOXES_DRAW_SUBDIV) == 0)
		{
			if (meshBuilder.End(drawCmd))
				g_matSystem->Draw(drawCmd);
			meshBuilder.Begin(PRIM_LINES);
		}
	}

	if (meshBuilder.End(drawCmd))
		g_matSystem->Draw(drawCmd);
}

static void DrawBoxArray(Array<DebugBoxNode_t>& boxes, float frametime)
{
	if(!boxes.numElem())
		return;

	g_matSystem->FindGlobalMaterialVar<MatTextureProxy>(StringToHashConst("basetexture")).Set(nullptr);

	g_matSystem->SetBlendingStates(BLENDFACTOR_SRC_ALPHA, BLENDFACTOR_ONE_MINUS_SRC_ALPHA,BLENDFUNC_ADD);
	g_matSystem->SetRasterizerStates(CULL_NONE,FILL_SOLID);
	g_matSystem->SetDepthStates(true,false);

	g_matSystem->Apply();

	RenderDrawCmd drawCmd;
	drawCmd.material = g_matSystem->GetDefaultMaterial();

	CMeshBuilder meshBuilder(g_matSystem->GetDynamicMesh());
	meshBuilder.Begin(PRIM_LINES);

		for(int i = 0; i < boxes.numElem(); i++)
		{
			DebugBoxNode_t& node = boxes[i];

			meshBuilder.Color4(node.color);

			meshBuilder.Line3fv(Vector3D(node.mins.x, node.maxs.y, node.mins.z),
								Vector3D(node.mins.x, node.maxs.y, node.maxs.z));

			meshBuilder.Line3fv(Vector3D(node.maxs.x, node.maxs.y, node.maxs.z),
								Vector3D(node.maxs.x, node.maxs.y, node.mins.z));

			meshBuilder.Line3fv(Vector3D(node.maxs.x, node.mins.y, node.mins.z),
								Vector3D(node.maxs.x, node.mins.y, node.maxs.z));

			meshBuilder.Line3fv(Vector3D(node.mins.x, node.mins.y, node.maxs.z),
								Vector3D(node.mins.x, node.mins.y, node.mins.z));

			meshBuilder.Line3fv(Vector3D(node.mins.x, node.mins.y, node.maxs.z),
								Vector3D(node.mins.x, node.maxs.y, node.maxs.z));

			meshBuilder.Line3fv(Vector3D(node.maxs.x, node.mins.y, node.maxs.z),
								Vector3D(node.maxs.x, node.maxs.y, node.maxs.z));

			meshBuilder.Line3fv(Vector3D(node.mins.x, node.mins.y, node.mins.z),
								Vector3D(node.mins.x, node.maxs.y, node.mins.z));

			meshBuilder.Line3fv(Vector3D(node.maxs.x, node.mins.y, node.mins.z),
								Vector3D(node.maxs.x, node.maxs.y, node.mins.z));

			meshBuilder.Line3fv(Vector3D(node.mins.x, node.maxs.y, node.mins.z),
								Vector3D(node.maxs.x, node.maxs.y, node.mins.z));

			meshBuilder.Line3fv(Vector3D(node.mins.x, node.maxs.y, node.maxs.z),
								Vector3D(node.maxs.x, node.maxs.y, node.maxs.z));

			meshBuilder.Line3fv(Vector3D(node.mins.x, node.mins.y, node.mins.z),
								Vector3D(node.maxs.x, node.mins.y, node.mins.z));

			meshBuilder.Line3fv(Vector3D(node.mins.x, node.mins.y, node.maxs.z),
								Vector3D(node.maxs.x, node.mins.y, node.maxs.z));

			node.lifetime -= frametime;

			if((i % BOXES_DRAW_SUBDIV) == 0)
			{
				if (meshBuilder.End(drawCmd))
					g_matSystem->Draw(drawCmd);
				meshBuilder.Begin(PRIM_LINES);
			}
		}

	if (meshBuilder.End(drawCmd))
		g_matSystem->Draw(drawCmd);
}

static void DrawCylinder(CMeshBuilder& meshBuilder, DebugCylinderNode_t& cylinder, float frametime)
{
	static const int NUM_SEG = 8;
	static float dir[NUM_SEG * 2];
	static bool init = false;
	if (!init)
	{
		init = true;
		for (int i = 0; i < NUM_SEG; ++i)
		{
			const float a = (float)i / (float)NUM_SEG * M_PI_F * 2;
			dir[i * 2] = cosf(a);
			dir[i * 2 + 1] = sinf(a);
		}
	}

	Vector3D min, max;
	min = cylinder.origin + Vector3D(-cylinder.radius, -cylinder.height * 0.5f, -cylinder.radius);
	max = cylinder.origin + Vector3D(cylinder.radius, cylinder.height * 0.5f, cylinder.radius);

	const float cx = (max.x + min.x) / 2;
	const float cz = (max.z + min.z) / 2;
	const float rx = (max.x - min.x) / 2;
	const float rz = (max.z - min.z) / 2;

	meshBuilder.Color4(cylinder.color);

	for (int i = 0, j = NUM_SEG - 1; i < NUM_SEG; j = i++)
	{
		meshBuilder.Line3fv(
			Vector3D(cx + dir[j * 2 + 0] * rx, min.y, cz + dir[j * 2 + 1] * rz), 
			Vector3D(cx + dir[i * 2 + 0] * rx, min.y, cz + dir[i * 2 + 1] * rz));

		meshBuilder.Line3fv(
			Vector3D(cx + dir[j * 2 + 0] * rx, max.y, cz + dir[j * 2 + 1] * rz),
			Vector3D(cx + dir[i * 2 + 0] * rx, max.y, cz + dir[i * 2 + 1] * rz));
	}

	for (int i = 0; i < NUM_SEG; i += NUM_SEG / 4)
	{
		meshBuilder.Line3fv(
			Vector3D(cx + dir[i * 2 + 0] * rx, min.y, cz + dir[i * 2 + 1] * rz),
			Vector3D(cx + dir[i * 2 + 0] * rx, max.y, cz + dir[i * 2 + 1] * rz));
	}

	cylinder.lifetime -= frametime;
}

static void DrawCylinderArray(Array<DebugCylinderNode_t>& cylArray, float frametime)
{
	g_matSystem->FindGlobalMaterialVar<MatTextureProxy>(StringToHashConst("basetexture")).Set(nullptr);

	g_matSystem->SetBlendingStates(BLENDFACTOR_SRC_ALPHA, BLENDFACTOR_ONE_MINUS_SRC_ALPHA, BLENDFUNC_ADD);
	g_matSystem->SetRasterizerStates(CULL_NONE, FILL_SOLID);
	g_matSystem->SetDepthStates(true, false);

	CMeshBuilder meshBuilder(g_matSystem->GetDynamicMesh());

	RenderDrawCmd drawCmd;
	drawCmd.material = g_matSystem->GetDefaultMaterial();

	meshBuilder.Begin(PRIM_LINES);

	for (int i = 0; i < cylArray.numElem(); ++i)
	{
		DrawCylinder(meshBuilder, cylArray[i], frametime);

		if ((i % BOXES_DRAW_SUBDIV) == 0)
		{
			if (meshBuilder.End(drawCmd))
				g_matSystem->Draw(drawCmd);
			meshBuilder.Begin(PRIM_LINES);
		}
	}

	if (meshBuilder.End(drawCmd))
		g_matSystem->Draw(drawCmd);
}

static void DrawGraph(debugGraphBucket_t* graph, int position, IEqFont* pFont, float frame_time)
{
	const float GRAPH_HEIGHT = 100;
	const float GRAPH_Y_OFFSET = 50;

	float x_pos = 15;
	float y_pos = GRAPH_Y_OFFSET + GRAPH_HEIGHT + position*110;

	Vector2D last_point(-1);

	Vertex2D lines[] =
	{
		Vertex2D(Vector2D(x_pos, y_pos), vec2_zero),
		Vertex2D(Vector2D(x_pos, y_pos - GRAPH_HEIGHT), vec2_zero),

		Vertex2D(Vector2D(x_pos, y_pos), vec2_zero),
		Vertex2D(Vector2D(x_pos+400, y_pos), vec2_zero),

		Vertex2D(Vector2D(x_pos, y_pos - GRAPH_HEIGHT *0.75f), vec2_zero),
		Vertex2D(Vector2D(x_pos+32, y_pos - GRAPH_HEIGHT *0.75f), vec2_zero),

		Vertex2D(Vector2D(x_pos, y_pos - GRAPH_HEIGHT *0.50f), vec2_zero),
		Vertex2D(Vector2D(x_pos+32, y_pos - GRAPH_HEIGHT *0.50f), vec2_zero),

		Vertex2D(Vector2D(x_pos, y_pos - GRAPH_HEIGHT *0.25f), vec2_zero),
		Vertex2D(Vector2D(x_pos+32, y_pos - GRAPH_HEIGHT *0.25f), vec2_zero),
	};

	eqFontStyleParam_t textStl;
	textStl.styleFlag = TEXT_STYLE_SHADOW | TEXT_STYLE_FROM_CAP;

	pFont->RenderText(graph->name, Vector2D(x_pos + 5, y_pos - GRAPH_HEIGHT - 16), textStl);

	pFont->RenderText("0", Vector2D(x_pos + 5, y_pos), textStl);
	pFont->RenderText(EqString::Format("%.2f", graph->maxValue).ToCString(), Vector2D(x_pos + 5, y_pos - GRAPH_HEIGHT), textStl);

	BlendStateParams blending;
	blending.srcFactor = BLENDFACTOR_SRC_ALPHA;
	blending.dstFactor = BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

	g_matSystem->DrawDefaultUP(PRIM_LINES, ArrayCRef(lines), nullptr, color_white, &blending);

	pFont->RenderText(EqString::Format("%.2f", (graph->maxValue*0.75f)).ToCString(), Vector2D(x_pos + 5, y_pos - GRAPH_HEIGHT *0.75f), textStl);
	pFont->RenderText(EqString::Format("%.2f", (graph->maxValue*0.50f)).ToCString(), Vector2D(x_pos + 5, y_pos - GRAPH_HEIGHT *0.50f), textStl);
	pFont->RenderText(EqString::Format("%.2f", (graph->maxValue*0.25f)).ToCString(), Vector2D(x_pos + 5, y_pos - GRAPH_HEIGHT *0.25f), textStl);

	int value_id = 0;

	Vertex2D graph_line_verts[GRAPH_MAX_VALUES*2];
	int num_line_verts = 0;

	float graph_max_value = 1.0f;

	int length = graph->values.numElem();
	while(length-- > 0)
	{
		//if(graph->points.getCurrent() > graph->fMaxValue)
		//	graph->fMaxValue = graph->points.getCurrent();

		const int graphIdx = (graph->cursor + length) % graph->values.numElem();
		debugGraphBucket_t::graphVal_t& graphVal = graph->values[graphIdx];

		// get a value of it.
		float value = clamp(graphVal.value, 0.0f, graph->maxValue);

		if(graphVal.value > graph_max_value )
			graph_max_value = graphVal.value;

		value /= graph->maxValue;

		value *= GRAPH_HEIGHT;

		Vector2D point(x_pos + GRAPH_MAX_VALUES - value_id, y_pos - value);

		if(value_id > 0 && num_line_verts < GRAPH_MAX_VALUES*2)
		{
			graph_line_verts[num_line_verts].position = last_point;
			graph_line_verts[num_line_verts].color = graphVal.color;

			num_line_verts++;

			graph_line_verts[num_line_verts].position = point;
			graph_line_verts[num_line_verts].color = graphVal.color;

			num_line_verts++;
		}

		value_id++;

		last_point = point;
	}

	if(graph->dynamic)
		graph->maxValue = graph_max_value;

	g_matSystem->DrawDefaultUP(PRIM_LINES, ArrayCRef(graph_line_verts, num_line_verts), nullptr, color_white);

	graph->remainingTime -= frame_time;

	if(graph->remainingTime <= 0)
		graph->remainingTime = 0.0f;

}

static void DrawPolygons(Array<DebugPolyNode_t>& polygons, float frameTime)
{
	if(!polygons.numElem())
		return;

	g_matSystem->FindGlobalMaterialVar<MatTextureProxy>(StringToHashConst("basetexture")).Set(nullptr);

	g_matSystem->SetBlendingStates(BLENDFACTOR_SRC_ALPHA, BLENDFACTOR_ONE_MINUS_SRC_ALPHA,BLENDFUNC_ADD);
	g_matSystem->SetRasterizerStates(CULL_BACK,FILL_SOLID);
	g_matSystem->SetDepthStates(true, true);

	RenderDrawCmd drawCmd;
	drawCmd.material = g_matSystem->GetDefaultMaterial();

	CMeshBuilder meshBuilder(g_matSystem->GetDynamicMesh());
	meshBuilder.Begin(PRIM_TRIANGLES);

		for(int i = 0; i < polygons.numElem(); i++)
		{
			meshBuilder.Color4(polygons[i].color);

			meshBuilder.Position3fv(polygons[i].v0);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(polygons[i].v1);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(polygons[i].v2);
			meshBuilder.AdvanceVertex();

			polygons[i].lifetime -= frameTime;

			if((i % POLYS_DRAW_SUBDIV) == 0)
			{
				if (meshBuilder.End(drawCmd))
					g_matSystem->Draw(drawCmd);
				meshBuilder.Begin(PRIM_TRIANGLES);
			}
		}

	if (meshBuilder.End(drawCmd))
		g_matSystem->Draw(drawCmd);

	meshBuilder.Begin(PRIM_LINES);
		for(int i = 0; i < polygons.numElem(); i++)
		{
			meshBuilder.Color4(polygons[i].color);

			meshBuilder.Position3fv(polygons[i].v0);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(polygons[i].v1);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(polygons[i].v1);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(polygons[i].v2);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(polygons[i].v2);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(polygons[i].v0);
			meshBuilder.AdvanceVertex();

			if((i % LINES_DRAW_SUBDIV) == 0)
			{
				if (meshBuilder.End(drawCmd))
					g_matSystem->Draw(drawCmd);
				meshBuilder.Begin(PRIM_LINES);
			}
		}
	if (meshBuilder.End(drawCmd))
		g_matSystem->Draw(drawCmd);
}

static Vector3D v3sphere(float theta, float phi)
{
	return Vector3D(
		cos(theta) * cos(phi),
		sin(theta) * cos(phi),
		sin(phi));
}

// use PRIM_LINES
static void DrawSphereWireframe(CMeshBuilder& meshBuilder, DebugSphereNode_t& sphere, int sides)
{
	if (sphere.radius <= 0)
		return;

	meshBuilder.Color4(sphere.color);

	sides = sides + (sides % 2);

	const double oneBySides = 1.0 / sides;

	{
		for (int i = 0; i < sides; i++)
		{
			const double ds = sin((i * 2 * M_PI_D) * oneBySides);
			const double dc = cos((i * 2 * M_PI_D) * oneBySides);

			meshBuilder.Position3f(
						static_cast<float>(sphere.origin.x + sphere.radius * dc),
						static_cast<float>(sphere.origin.y + sphere.radius * ds),
						sphere.origin.z
						);

			meshBuilder.AdvanceVertex();
		}
	}

	{
		for (int i = 0; i < sides; i++)
		{
			const double ds = sin((i * 2 * M_PI_D) * oneBySides);
			const double dc = cos((i * 2 * M_PI_D) * oneBySides);

			meshBuilder.Position3f(
						static_cast<float>(sphere.origin.x + sphere.radius * dc),
						sphere.origin.y,
						static_cast<float>(sphere.origin.z + sphere.radius * ds)
						);

			meshBuilder.AdvanceVertex();
		}
	}

	{
		for (int i = 0; i < sides; i++)
		{
			const double ds = sin((i * 2 * M_PI_D) * oneBySides);
			const double dc = cos((i * 2 * M_PI_D) * oneBySides);

			meshBuilder.Position3f(
						sphere.origin.x,
						static_cast<float>(sphere.origin.y + sphere.radius * dc),
						static_cast<float>(sphere.origin.z + sphere.radius * ds)
						);

			meshBuilder.AdvanceVertex();
		}
	}
}

// use PRIM_TRIANGLES
static void DrawSphereFilled(CMeshBuilder& meshBuilder, DebugSphereNode_t& sphere, int sides)
{
	if (sphere.radius <= 0)
		return;

	const float dt = M_PI_D *2.0f / float(sides);
	const float dp = M_PI_D / float(sides);

	meshBuilder.Color4(sphere.color);

	for (int i = 0; i <= sides - 1; i++)
	{
		for (int j = 0; j <= sides - 2; j++)
		{
			const double t = i * dt;
			const double p = (j * dp) - (M_PI_D * 0.5f);

			{
				Vector3D v(sphere.origin + (v3sphere(t, p) * sphere.radius));
				meshBuilder.Position3fv(v);
				meshBuilder.AdvanceVertex();
			}

			{
				Vector3D v(sphere.origin + (v3sphere(t, p + dp) * sphere.radius));
				meshBuilder.Position3fv(v);
				meshBuilder.AdvanceVertex();
			}

			{
				Vector3D v(sphere.origin + (v3sphere(t + dt, p + dp) * sphere.radius));
				meshBuilder.Position3fv(v);
				meshBuilder.AdvanceVertex();
			}

			{
				Vector3D v(sphere.origin + (v3sphere(t, p) * sphere.radius));
				meshBuilder.Position3fv(v);
				meshBuilder.AdvanceVertex();
			}

			{
				Vector3D v(sphere.origin + (v3sphere(t + dt, p + dp) * sphere.radius));
				meshBuilder.Position3fv(v);
				meshBuilder.AdvanceVertex();
			}

			{
				Vector3D v(sphere.origin + (v3sphere(t + dt, p) * sphere.radius));
				meshBuilder.Position3fv(v);
				meshBuilder.AdvanceVertex();
			}
		}
	}

	{
		const double p = (sides - 1) * dp - (M_PI_D * 0.5f);
		for (int i = 0; i <= sides - 1; i++)
		{
			const double t = i * dt;

			{
				Vector3D v(sphere.origin + (v3sphere(t, p) * sphere.radius));
				meshBuilder.Position3fv(v);
				meshBuilder.AdvanceVertex();
			}

			{
				Vector3D v(sphere.origin + (v3sphere(t + dt, p + dp) * sphere.radius));
				meshBuilder.Position3fv(v);
				meshBuilder.AdvanceVertex();
			}

			{
				Vector3D v(sphere.origin + (v3sphere(t + dt, p) * sphere.radius));
				meshBuilder.Position3fv(v);
				meshBuilder.AdvanceVertex();
			}
		}
	}
}

static void DrawSphereArray(Array<DebugSphereNode_t>& spheres, float frameTime)
{
	if(!spheres.numElem())
		return;

	g_matSystem->FindGlobalMaterialVar<MatTextureProxy>(StringToHashConst("basetexture")).Set(nullptr);

	g_matSystem->SetBlendingStates(BLENDFACTOR_SRC_ALPHA, BLENDFACTOR_ONE_MINUS_SRC_ALPHA,BLENDFUNC_ADD);
	g_matSystem->SetRasterizerStates(CULL_BACK,FILL_SOLID);
	g_matSystem->SetDepthStates(true, true);

	RenderDrawCmd drawCmd;
	drawCmd.material = g_matSystem->GetDefaultMaterial();

	CMeshBuilder meshBuilder(g_matSystem->GetDynamicMesh());
	meshBuilder.Begin(PRIM_LINES);

	for (int i = 0; i < spheres.numElem(); i++)
	{
		DrawSphereWireframe(meshBuilder, spheres[i], 20);
		spheres[i].lifetime -= frameTime;
	}

	if (meshBuilder.End(drawCmd))
		g_matSystem->Draw(drawCmd);
}

#endif // DISABLE_DEBUG_DRAWING

void CDebugOverlay::SetMatrices( const Matrix4x4 &proj, const Matrix4x4 &view )
{
	m_projMat = proj;
	m_viewMat = view;

	Matrix4x4 viewProj = m_projMat*m_viewMat;
	m_frustum.LoadAsFrustum(viewProj);
}

void CDebugOverlay::Draw(int winWide, int winTall, float timescale)
{
	m_frameTime = m_timer.GetTime(true) * timescale;

#ifndef DISABLE_DEBUG_DRAWING
	g_renderAPI->SetViewport(0, 0, winWide, winTall);
	g_matSystem->SetMatrix(MATRIXMODE_PROJECTION, m_projMat);
	g_matSystem->SetMatrix(MATRIXMODE_VIEW, m_viewMat);
	g_matSystem->SetMatrix(MATRIXMODE_WORLD, identity4);
	g_matSystem->SetAmbientColor(1.0f);

	CleanOverlays();

	// draw custom stuff
	{
		Threading::CScopedMutex m(s_debugOverlayMutex);

		for (int i = 0; i < m_draw3DFuncs.numElem(); i++)
		{
			if (!m_draw3DFuncs[i].func())
			{
				m_draw3DFuncs.fastRemoveIndex(i);
				--i;
				continue;
			}
			m_draw3DFuncs[i].lifetime -= m_frameTime;
		}
	}

	// may need a reset again
	g_renderAPI->SetViewport(0, 0, winWide, winTall);
	g_matSystem->SetMatrix(MATRIXMODE_PROJECTION, m_projMat);
	g_matSystem->SetMatrix(MATRIXMODE_VIEW, m_viewMat);
	g_matSystem->SetMatrix(MATRIXMODE_WORLD, identity4);
	g_matSystem->SetAmbientColor(1.0f);

	// draw all of 3d stuff
	{
		Threading::CScopedMutex m(s_debugOverlayMutex);
		DrawBoxArray(m_BoxList, m_frameTime);
	}

	{
		Threading::CScopedMutex m(s_debugOverlayMutex);
		DrawCylinderArray(m_CylinderList, m_frameTime);
	}

	{
		Threading::CScopedMutex m(s_debugOverlayMutex);
		DrawOrientedBoxArray(m_OrientedBoxList, m_frameTime);
	}

	{
		Threading::CScopedMutex m(s_debugOverlayMutex);
		DrawSphereArray(m_SphereList, m_frameTime);
	}

	{
		Threading::CScopedMutex m(s_debugOverlayMutex);
		DrawLineArray(m_LineList, m_frameTime);
	}

	{
		Threading::CScopedMutex m(s_debugOverlayMutex);
		DrawPolygons(m_polygons, m_frameTime);
	}

	// now rendering 2D stuff
	g_matSystem->Setup2D(winWide, winTall);

	const Vector2D drawFadedTextBoxPosition = Vector2D(15,45);
	const Vector2D drawTextBoxPosition = Vector2D(15,45);

	int idx = 0;

	{
		Threading::CScopedMutex m(s_debugOverlayMutex);

		using TextIterator = List< DebugFadingTextNode_t>::Iterator;
		Array<TextIterator> deletedNodes(PP_SL);
		for(auto it = m_LeftTextFadeArray.begin(); !it.atEnd(); ++it)
		{
			DebugFadingTextNode_t& current = *it;
			if (current.lifetime < 0.0f)
			{
				deletedNodes.append(it);
				continue;
			}

			MColor curColor(current.color);
				
			if (current.initialLifetime > 0.05f)
				curColor.a = clamp(current.lifetime, 0.0f, 1.0f);
			else
				curColor.a = 1.0f;

			eqFontStyleParam_t textStl;
			textStl.styleFlag = TEXT_STYLE_SHADOW | TEXT_STYLE_FROM_CAP;
			textStl.textColor = curColor;

			Vector2D textPos = drawFadedTextBoxPosition + Vector2D(0, (idx * m_debugFont->GetLineHeight(textStl)));

			m_debugFont->RenderText(current.pszText.GetData(), textPos, textStl);

			idx++;

			current.lifetime -= m_frameTime;

		}

		for (TextIterator it : deletedNodes)
			m_LeftTextFadeArray.remove(it);
	}

	eqFontStyleParam_t textStl;
	textStl.styleFlag = TEXT_STYLE_SHADOW | TEXT_STYLE_FROM_CAP;

	{
		Threading::CScopedMutex m(s_debugOverlayMutex);

		for (int i = 0; i < m_Text3DArray.numElem(); i++)
		{
			DebugText3DNode_t& current = m_Text3DArray[i];

			Vector3D screen(0);

			bool beh = PointToScreen(current.origin, screen, m_projMat * m_viewMat, Vector2D(winWide, winTall));

			bool visible = true;

			if (current.dist > 0)
				visible = (screen.z < current.dist);

			current.lifetime -= m_frameTime;

			if (!beh && visible)
			{
				textStl.textColor = MColor(current.color);
				m_debugFont2->RenderText(current.pszText.GetData(), screen.xy(), textStl);
			}
		}
	}

	if(r_debugDrawFrameStats.GetBool())
	{
		{
			Threading::CScopedMutex m(s_debugOverlayMutex);
			if (m_TextArray.numElem())
			{
				GUIDrawWindow(AARectangle(drawTextBoxPosition.x, drawTextBoxPosition.y, drawTextBoxPosition.x + 380, drawTextBoxPosition.y + (m_TextArray.numElem() * m_debugFont->GetLineHeight(textStl))), ColorRGBA(0.5f, 0.5f, 0.5f, 0.5f));

				for (int i = 0; i < m_TextArray.numElem(); i++)
				{
					DebugTextNode_t& current = m_TextArray[i];

					textStl.textColor = MColor(current.color);

					Vector2D textPos(drawTextBoxPosition.x, drawTextBoxPosition.y + (i * m_debugFont->GetLineHeight(textStl)));

					m_debugFont->RenderText(current.pszText.GetData(), textPos, textStl);
				}
			}
			m_TextArray.clear();
		}

		eqFontStyleParam_t rTextFadeStyle;
		rTextFadeStyle.styleFlag = TEXT_STYLE_SHADOW | TEXT_STYLE_FROM_CAP;
		rTextFadeStyle.align = TEXT_ALIGN_RIGHT;

		{
			Threading::CScopedMutex m(s_debugOverlayMutex);
			for (int i = 0; i < m_RightTextFadeArray.numElem(); i++)
			{
				DebugFadingTextNode_t& current = m_RightTextFadeArray[i];

				MColor curColor(current.color);

				if (current.initialLifetime > 0.05f)
					curColor.a = clamp(current.lifetime, 0.0f, 1.0f);
				else
					curColor.a = 1.0f;

				rTextFadeStyle.textColor = curColor;

				float textLen = m_debugFont->GetStringWidth(current.pszText.ToCString(), textStl);
				Vector2D textPos(winWide - (textLen * m_debugFont->GetLineHeight(textStl)), 45 + (i * m_debugFont->GetLineHeight(textStl)));

				m_debugFont->RenderText(current.pszText.GetData(), textPos, rTextFadeStyle);

				current.lifetime -= m_frameTime;
			}
		}
	}

	if(r_debugDrawGraphs.GetBool())
	{
		Threading::CScopedMutex m(s_debugOverlayMutex);

		for(int i = 0; i < m_graphbuckets.numElem(); i++)
			DrawGraph( m_graphbuckets[i], i, m_debugFont, m_frameTime);

		m_graphbuckets.clear(false);
	}

	// draw custom stuff
	{
		Threading::CScopedMutex m(s_debugOverlayMutex);

		for (int i = 0; i < m_draw2DFuncs.numElem(); i++)
		{
			if (!m_draw2DFuncs[i].func())
			{
				m_draw2DFuncs.fastRemoveIndex(i);
				--i;
				continue;
			}
			m_draw2DFuncs[i].lifetime -= m_frameTime;
		}
	}

	// more universal thing
	if( g_pDebugTexture )
	{
		g_matSystem->Setup2D( winWide, winTall );

		float w, h;
		w = (float)g_pDebugTexture->GetWidth()*r_debugShowTextureScale.GetFloat();
		h = (float)g_pDebugTexture->GetHeight()*r_debugShowTextureScale.GetFloat();

		if(h > winTall)
		{
			float fac = (float)winTall/h;

			w *= fac;
			h *= fac;
		}

		Vertex2D light_depth[] = { MAKETEXQUAD(0, 0, w, h, 0) };
		g_matSystem->DrawDefaultUP(PRIM_TRIANGLE_STRIP, ArrayCRef(light_depth), g_pDebugTexture);

		eqFontStyleParam_t textStl;
		textStl.styleFlag = TEXT_STYLE_SHADOW | TEXT_STYLE_FROM_CAP;

		m_debugFont2->RenderText(EqString::Format("%dx%d (frame %d)\n%s\nrefcnt %d", g_pDebugTexture->GetWidth(), g_pDebugTexture->GetHeight(), g_pDebugTexture->GetAnimationFrame(), g_pDebugTexture->GetName(), g_pDebugTexture->Ref_Count()), Vector2D(10, 10), textStl);
	}

	++m_frameId;
#endif
}

bool CDebugOverlay::CheckNodeLifetime(DebugNodeBase& node)
{
	// don't touch newly added nodes
	if (node.frameindex == m_frameId)
		return true;

	// expired.
	if (node.lifetime < 0.0f)
		return false;

	if (node.nameHash == 0)
		return true;

	// check if it's been replaced
	auto found = m_newNames.find(node.nameHash);
	if (found.atEnd())
		return true;

	// check node time stamp, we allow to put different objects under similar name in one frame
	return found.value() == node.frameindex;
}

void CDebugOverlay::CleanOverlays()
{
#ifndef DISABLE_DEBUG_DRAWING
	Threading::CScopedMutex m(s_debugOverlayMutex);

	for (int i = 0; i < m_draw2DFuncs.numElem(); i++)
	{
		if (!CheckNodeLifetime(m_draw2DFuncs[i]))
		{
			m_draw2DFuncs.fastRemoveIndex(i);
			i--;
		}
	}

	for (int i = 0; i < m_draw3DFuncs.numElem(); i++)
	{
		if (!CheckNodeLifetime(m_draw3DFuncs[i]))
		{
			m_draw3DFuncs.fastRemoveIndex(i);
			i--;
		}
	}

	for (int i = 0; i < m_RightTextFadeArray.numElem(); i++)
	{
		if (m_RightTextFadeArray[i].lifetime <= 0)
		{
			m_RightTextFadeArray.fastRemoveIndex(i);
			i--;
		}
	}

	for (int i = 0; i < m_Text3DArray.numElem(); i++)
	{
		if(!CheckNodeLifetime(m_Text3DArray[i]))
		{
			m_Text3DArray.fastRemoveIndex(i);
			i--;
		}
	}

	for (int i = 0; i < m_LineList.numElem(); i++)
	{
		if(!CheckNodeLifetime(m_LineList[i]))
		{
			m_LineList.fastRemoveIndex(i);
			i--;
		}
	}

	for (int i = 0; i < m_BoxList.numElem(); i++)
	{
		if(!CheckNodeLifetime(m_BoxList[i]))
		{
			m_BoxList.fastRemoveIndex(i);
			i--;
		}
	}

	for (int i = 0; i < m_CylinderList.numElem(); i++)
	{
		if (!CheckNodeLifetime(m_CylinderList[i]))
		{
			m_CylinderList.fastRemoveIndex(i);
			i--;
		}
	}

	for (int i = 0; i < m_OrientedBoxList.numElem(); i++)
	{
		if (!CheckNodeLifetime(m_OrientedBoxList[i]))
		{
			m_OrientedBoxList.fastRemoveIndex(i);
			i--;
		}
	}

	for (int i = 0; i < m_SphereList.numElem(); i++)
	{
		if(!CheckNodeLifetime(m_SphereList[i]))
		{
			m_SphereList.fastRemoveIndex(i);
			i--;
		}
	}

	for (int i = 0; i < m_polygons.numElem(); i++)
	{
		if(!CheckNodeLifetime(m_polygons[i]))
		{
			m_polygons.fastRemoveIndex(i);
			i--;
		}
	}

#endif
}

void CDebugOverlay::Graph_DrawBucket(debugGraphBucket_t* pBucket)
{
#ifndef DISABLE_DEBUG_DRAWING
	if (!r_debugDrawGraphs.GetBool())
		return;

	Threading::CScopedMutex m(s_debugOverlayMutex);

	m_graphbuckets.addUnique(pBucket);
#endif
}

void CDebugOverlay::Graph_AddValue(debugGraphBucket_t* bucket, float value)
{
#ifndef DISABLE_DEBUG_DRAWING
	if(!r_debugDrawGraphs.GetBool())
		return;

	Threading::CScopedMutex m(s_debugOverlayMutex);

	if(bucket->remainingTime <= 0)
	{
		int index;
		if (bucket->values.numElem() < GRAPH_MAX_VALUES)
		{
			index = bucket->values.append(debugGraphBucket_t::graphVal_t{ value, MColor(bucket->color).pack() });
		}
		else
		{
			index = bucket->cursor;
			bucket->values[index] = debugGraphBucket_t::graphVal_t{ value, MColor(bucket->color).pack() };
		}
		index++;
		bucket->cursor = index % GRAPH_MAX_VALUES;

		bucket->remainingTime = bucket->updateTime;
	}
#endif
}

IEqFont* CDebugOverlay::GetFont()
{
	return m_debugFont;
}

