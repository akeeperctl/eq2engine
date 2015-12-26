//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2015
//////////////////////////////////////////////////////////////////////////////////
// Description: traffic car controller AI
//////////////////////////////////////////////////////////////////////////////////

#ifndef AITRAFFICCARCONTROLLER_H
#define AITRAFFICCARCONTROLLER_H

#include "car.h"
#include "level.h"
#include "EventFSM.h"

#include "utils/DkList.h"

#define AI_TRACE_CONTENTS (OBJECTCONTENTS_SOLID_OBJECTS | OBJECTCONTENTS_OBJECT | OBJECTCONTENTS_VEHICLE)

// junction details - holds already found roads. 
// Using this AI selects straight and turns on repeater indicator
struct junctionDetail_t
{
	junctionDetail_t()
	{
		allowedMovement = 0;
		selectedStraight = 0;
	}

	DkList<straight_t>	foundStraights;
	roadJunction_t		junc;
	int					allowedMovement;		// flags, 1 & 2 depending on traffic light
	int					selectedStraight;
};

//
// Little helper
//
class CTimedRelay
{
public:
	CTimedRelay() : m_time(0), m_delay(0) {}

	void Set( float time, float delay = 0.0f )
	{
		m_time = time;
		m_delay = delay;
	}

	void SetIfNot( float time, float delay = 0.0f )
	{
		if(GetTotalTime() > 0)
			return;

		m_time = time;
		m_delay = delay;
	}

	bool IsOn()
	{
		return (m_delay <= 0.0f && m_time > 0.0f);
	}

	float GetTotalTime()
	{
		return m_delay + m_time;
	}

	float GetRemainingTime()
	{
		return m_time;
	}

	void Update(float fDt)
	{
		if (m_delay <= 0.0f)
		{
			m_delay = 0.0f;
			if (m_time > 0.0f)
				m_time -= fDt;
		}
		else
			m_delay -= fDt;
	}

protected:

	float m_delay;
	float m_time;
};

//-----------------------------------------------------------------------------------------------

struct signalSeq_t;

class CAITrafficCar :	public CFSM_Base,
						public CCar
{
public:
	DECLARE_CLASS(CAITrafficCar, CCar);

	CAITrafficCar( carConfigEntry_t* carConfig );
	~CAITrafficCar();

	virtual void		InitAI(CLevelRegion* reg, levroadcell_t* cell);

	virtual void		Spawn();
	virtual void		OnCarCollisionEvent(const CollisionPairData_t& pair, CGameObject* hitBy);
	
	int					ObjType() const { return GO_CAR_AI; }
	virtual bool		IsPursuer() const {return false;}

	void				SignalRandomSequence( float delayBeforeStart );
	void				SignalNoSequence( float time, float delayBeforeStart );

protected:
	virtual void		OnPrePhysicsFrame( float fDt );

	// task
	void				SearchJunctionAndStraight();
	void				SwitchLane();
	void				ChangeRoad( const straight_t& road );

	// states
	int					SearchForRoad( float fDt, EStateTransition transition );
	virtual int			TrafficDrive( float fDt, EStateTransition transition );

	int					Event_TrafficLight( float fDt, EStateTransition transition );
	int					Event_FrontObjHasMoved( float fDt, EStateTransition transition );

	int					BrakeToTheLine( float fDt, EStateTransition transition );
	int					BrakeToObject( float fDt, EStateTransition transition );

	virtual int			DeadState( float fDt, EStateTransition transition )			{return 0;}

	//------------------------------------------------

	float				m_speedModifier;
	bool				m_hasDamage;
	bool				m_frameSkip;

	straight_t			m_straights[2];
	IVector2D			m_currEnd;

	junctionDetail_t	m_nextJuncDetails;

	float				m_prevFract;

	bool				m_switchedLane;

	float				m_refreshTime;

	CTimedRelay			m_hornTime;

	float				m_thinkTime;
	float				m_nextSwitchLaneTime;
	float				m_laneSwitchTimeout;

	bool				m_emergencyEscape;
	float				m_emergencyEscapeTime;
	float				m_emergencyEscapeSteer;

	signalSeq_t*		m_signalSeq;
	int					m_signalSeqFrame;
};

#endif // AITRAFFICCONTROLLER_H