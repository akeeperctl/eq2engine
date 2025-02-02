//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: DakrTech scene renderer renderable
//////////////////////////////////////////////////////////////////////////////////

#include "core/core_common.h"
#include "RenderList.h"
#include "BaseRenderableObject.h"

#define MIN_OBJECT_RENDERLIST_MEMSIZE 64

CRenderList::CRenderList()
	: m_objectList(PP_SL, MIN_OBJECT_RENDERLIST_MEMSIZE)
	, m_viewDistance(PP_SL, MIN_OBJECT_RENDERLIST_MEMSIZE)
{

}

CRenderList::~CRenderList()
{
	
}

void CRenderList::AddRenderable(Renderable* pObject)
{
	m_objectList.append(pObject);
	m_viewDistance.append(0.0f);
}

int CRenderList::GetRenderableCount()
{
	return m_objectList.numElem();
}

CRenderList::Renderable* CRenderList::GetRenderable(int id)
{
	return m_objectList[id];
}

void CRenderList::Append(CRenderList* pAnotherList)
{
	const int num = pAnotherList->GetRenderableCount();
	for(int i = 0; i < num; i++)
		m_objectList.append(pAnotherList->GetRenderable(i));
}

void CRenderList::Render(int renderFlags, void* userdata)
{
	RenderInfo rinfo;
	rinfo.renderFlags = renderFlags;
	rinfo.userData = userdata;
	for (int i = 0; i < m_objectList.numElem(); ++i)
	{
		rinfo.distance = m_viewDistance[i];
		m_objectList[i]->Render(rinfo);
	}
}

void CRenderList::Clear()
{
	m_objectList.clear(false);
	m_viewDistance.clear(false);
}

void CRenderList::SortByDistanceFrom(const Vector3D& origin, bool reverse)
{
	m_viewDistance.setNum(m_objectList.numElem(), false);

	// pre-compute object distances
	for(int i = 0; i < m_objectList.numElem(); ++i)
	{
		Renderable* renderable = m_objectList[i];
		const BoundingBox& bbox = renderable->GetBoundingBox();

		// clamp point in bbox
		if(!bbox.Contains(origin))
			m_viewDistance[i] = lengthSqr(origin - bbox.ClampPoint(origin));
		else
			m_viewDistance[i] = lengthSqr(origin - bbox.GetCenter());
	}

	if (reverse)
	{
		// furthest to closest (for transparency)
		quickSortIdx(m_objectList, [this](int a, int b) {
			return m_viewDistance[b] - m_viewDistance[a];
		});
	}
	else
	{
		// closest to furthest
		quickSortIdx(m_objectList, [this](int a, int b) {
			return m_viewDistance[a] - m_viewDistance[b];
		});
	}
}