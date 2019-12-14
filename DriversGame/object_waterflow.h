//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2019
//////////////////////////////////////////////////////////////////////////////////
// Description: water flow of fire hydrant
//////////////////////////////////////////////////////////////////////////////////

#ifndef OBJECT_WATERFLOW_H
#define OBJECT_WATERFLOW_H

#include "GameObject.h"
#include "state_game.h"

class CObject_WaterFlow : public CGameObject
{
public:
	DECLARE_CLASS(CObject_WaterFlow, CGameObject )

	CObject_WaterFlow( kvkeybase_t* kvdata );
	~CObject_WaterFlow();

	void					OnRemove();
	void					Spawn();

	void					Simulate( float fDt );

	int						ObjType() const		{return GO_DEBRIS;}

protected:
	CEqCollisionObject*		m_ghostObject;
	float					m_lifeTime;
	float					m_force;

	float					m_nextRippleTime;

	Vector3D				m_groundPos;
	Vector3D				m_groundNormal;

	ISoundController*		m_waterFlowSound;
};

#endif // OBJECT_WATERFLOW_H