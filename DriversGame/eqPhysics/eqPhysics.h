//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium fixed point 3D physics engine
//
//	FEATURES:
//				Fixed point object positions
//				Best works with fixed timestep
//				Simple dynamics build from ground up
//				Raycasting along with sweep collision test
//				Using Bullet Collision Library for fast collision detection
//
//////////////////////////////////////////////////////////////////////////////////

/*
DONE:
		- Rigid body dynamics
		- Collision detecion
		- Collision shapes
		- Static object optimization
		- Dynamic objects optimization
		- Line test for dynamic objects
TODO:
		- Multithreaded integration, collision detection and collision response
		- Multithreaded line test (test bunch of lines)
		- Sweep test
		- Constraints (car doors, hoods, other)
*/

#ifndef EQPHYSICS_H
#define EQPHYSICS_H

#include "math/FVector.h"
#include "math/BoundingBox.h"
#include "utils/DkList.h"
#include "utils/eqthread.h"

#include "btBulletCollisionCommon.h"

#include "eqCollision_ObjectGrid.h"

// max world size is +/-32768
#define EQPHYS_MAX_WORLDSIZE	32767.0f

class CEqRigidBody;
class CEqCollisionObject;

enum ECollPairFlag
{
	COLLPAIRFLAG_NO_SOUND				= (1 << 0),
	COLLPAIRFLAG_OBJECTA_STATIC			= (1 << 1),

	COLLPAIRFLAG_OBJECTA_NO_RESPONSE	= (1 << 2),
	COLLPAIRFLAG_OBJECTB_NO_RESPONSE	= (1 << 3),

	COLLPAIRFLAG_NO_RESPONSE = (COLLPAIRFLAG_OBJECTA_NO_RESPONSE | COLLPAIRFLAG_OBJECTB_NO_RESPONSE),

	// other flags
	COLLPAIRFLAG_USER_PROCESSED			= (1 << 16),		// special flag for user needs
	COLLPAIRFLAG_USER_PROCESSED2		= (1 << 17),		// special flag for user needs
	COLLPAIRFLAG_USER_PROCESSED3		= (1 << 18),		// special flag for user needs
	COLLPAIRFLAG_USER_PROCESSED4		= (1 << 19),		// special flag for user needs
};

#define COLLISION_MASK_ALL 0xFFFFFFFF

struct CollisionData_t
{
	CollisionData_t()
	{
		fract = 1.0f;
		hitobject = NULL;
		materialIndex = -1;
	}

	FVector3D			position;			// position in world
	Vector3D			normal;

	CEqCollisionObject*	hitobject;

	float				fract;				// collision depth (if RayTest or SweepTest - factor between start[Transform] and end[Transform])
	int					materialIndex;
};

struct ContactPair_t
{
	CEqCollisionObject* GetOppositeTo(CEqCollisionObject* obj) const;

	Vector3D			normal;
	FVector3D			position;

	CEqCollisionObject*	bodyA;
	CEqCollisionObject*	bodyB;

	float				restitutionA;
	float				frictionA;

	float				dt;
	float				depth;

	int					flags;
};

struct CollisionPairData_t
{
	CollisionPairData_t()
	{
		flags = 0;
	}

	CEqCollisionObject* GetOppositeTo(CEqCollisionObject* obj) const;

	FVector3D			position;			// position in world
	Vector3D			normal;

	CEqCollisionObject*	bodyA;
	CEqCollisionObject*	bodyB;

	float				fract;				// collision depth (if RayTest or SweepTest - factor between start[Transform] and end[Transform])
	float				appliedImpulse;
	float				impactVelocity;
	int					flags;
};

//---------------------------------------------------------------------------------
// Equilibrium physics step
//---------------------------------------------------------------------------------

enum EPhysFilterType
{
	EQPHYS_FILTER_TYPE_EXCLUDE = 0,		// excludes objects
	EQPHYS_FILTER_TYPE_INCLUDE_ONLY,		// includes only objects
};

enum EPhysFilterFlags
{
	EQPHYS_FILTER_FLAG_STATICOBJECTS	= (1 << 0),	// filters only static objects
	EQPHYS_FILTER_FLAG_DYNAMICOBJECTS	= (1 << 1), // filters only dynamic objects
	EQPHYS_FILTER_FLAG_CHECKUSERDATA	= (1 << 2), // filter uses userdata comparison instead of objects

	EQPHYS_FILTER_FLAG_DISALLOW_STATIC	= (1 << 3),
	EQPHYS_FILTER_FLAG_DISALLOW_DYNAMIC	= (1 << 4),

	EQPHYS_FILTER_FLAG_FORCE_RAYCAST	= (1 << 5), // for raycasting - ignores COLLOBJ_NO_RAYCAST flags
};

//----------------------------------------------

struct eqPhysSurfParam_t
{
	EqString	name;

	int			id;

	float		restitution;
	float		friction;

	float		tirefriction;
	float		tirefriction_traction;

	char		word;
};

//----------------------------------------------

#define MAX_COLLISION_FILTER_OBJECTS 8

struct eqPhysCollisionFilter
{
	eqPhysCollisionFilter()
	{
		type = EQPHYS_FILTER_TYPE_EXCLUDE;
		flags = 0;
		numObjects = 0;
	}

	eqPhysCollisionFilter(CEqRigidBody* obj)
	{
		objectPtrs[0] = obj;
		numObjects = 1;

		type = EQPHYS_FILTER_TYPE_EXCLUDE;
		flags = EQPHYS_FILTER_FLAG_DYNAMICOBJECTS;
	}

	eqPhysCollisionFilter(CEqRigidBody** obj, int cnt)
	{
		int cpcnt = sizeof(CEqRigidBody*)*cnt;
		cpcnt = min(cpcnt, MAX_COLLISION_FILTER_OBJECTS);

		memcpy(objectPtrs, obj, cpcnt);
		numObjects = cpcnt;

		type = EQPHYS_FILTER_TYPE_EXCLUDE;
		flags = EQPHYS_FILTER_FLAG_DYNAMICOBJECTS;
	}

	void AddObject(void* ptr)
	{
		if (ptr && numObjects < MAX_COLLISION_FILTER_OBJECTS)
			objectPtrs[numObjects++] = ptr;
	}

	bool HasObject(void* ptr)
	{
		for (int i = 0; i < numObjects; i++)
		{
			if (objectPtrs[i] == ptr)
				return true;
		}

		return false;
	}

	void*	objectPtrs[MAX_COLLISION_FILTER_OBJECTS];

	int		type;
	int		flags;
	int		numObjects;
};

//--------------------------------------------------------------------------------------------------------------

typedef bool (CEqPhysics::*fnSingleObjectLineCollisionCheck)(	CEqCollisionObject* object,
																const FVector3D& start,
																const FVector3D& end,
																const BoundingBox& raybox,
																CollisionData_t& coll,
																int rayMask,
																eqPhysCollisionFilter* filterParams,
																void* args);


//--------------------------------------------------------------------------------------------------------------

typedef void (*FNSIMULATECALLBACK)(float fDt, int iterNum);

class IEqPhysicsConstraint;
class IEqPhysicsController;

class CEqPhysics
{
	struct sweptTestParams_t
	{
		Quaternion			rotation;
		btCollisionShape*	shape;
	};

public:
	CEqPhysics();
	~CEqPhysics();

	void							InitWorld();										///< initializes world
	void							InitGrid();											///< initializes broadphase grid

	void							DestroyWorld();										///< destroys world
	void							DestroyGrid();											///< destroys broadphase grid

	eqPhysSurfParam_t*				FindSurfaceParam(const char* name);
	eqPhysSurfParam_t*				GetSurfaceParamByID(int id);

	void							AddToMoveableList( CEqRigidBody* body );			///< adds object to moveable list

	void							AddToWorld( CEqRigidBody* body, bool moveable = true );	///< adds to processing
	bool							RemoveFromWorld( CEqRigidBody* body );				///< removes body from world, not deleting
	void							DestroyBody( CEqRigidBody* body );					///< destroys body

	void							SetupBodyOnCell( CEqCollisionObject* body );		///< only rigid body and ghost objects

	void							AddGhostObject( CEqCollisionObject* object );		///< adds ghost object
	void							DestroyGhostObject( CEqCollisionObject* object );	///< removes ghost object

	void							AddStaticObject( CEqCollisionObject* object );		///< adds collision object as static body
	void							RemoveStaticObject( CEqCollisionObject* object );	///< removes static object
	void							DestroyStaticObject( CEqCollisionObject* object );	///< destroys static object

    bool                            IsValidStaticObject( CEqCollisionObject* obj ) const;
	bool                            IsValidBody( CEqCollisionObject* body ) const;

	void							AddConstraint( IEqPhysicsConstraint* constraint );		///< adds constraint to the world
	void							RemoveConstraint( IEqPhysicsConstraint* constraint );	///< removes constraint from the world

	void							AddController( IEqPhysicsController* controller );		///< adds controller to the world
	void							RemoveController( IEqPhysicsController* controller );	///< removes controller from the world
	void							DestroyController( IEqPhysicsController* controlelr );	///< destroys controller

	//< Performs a line test in the world
	bool							TestLineCollision(	const FVector3D& start,
														const FVector3D& end,
														CollisionData_t& coll,
														int rayMask = COLLISION_MASK_ALL, eqPhysCollisionFilter* filterParams = NULL);

	///< Pushes convex in the world for closest collision
	bool							TestConvexSweepCollision(	btCollisionShape* shape,
																const Quaternion& rotation,
																const FVector3D& start,
																const FVector3D& end,
																CollisionData_t& coll,
																int rayMask = COLLISION_MASK_ALL, eqPhysCollisionFilter* filterParams = NULL);
	///< Performs a line test for a single object.
	///< start, end are world coordinates
	bool							TestLineSingleObject(	CEqCollisionObject* object,
															const FVector3D& start,
															const FVector3D& end,
															const BoundingBox& raybox,
															CollisionData_t& coll,
															int rayMask,
															eqPhysCollisionFilter* filterParams,
															void* args = NULL);

	// Pushes a convex sweep for closest collision for a single object.
	///< start, end are world coordinates
	bool							TestConvexSweepSingleObject(CEqCollisionObject* object,
																const FVector3D& start,
																const FVector3D& end,
																const BoundingBox& raybox,
																CollisionData_t& coll,
																int rayMask,
																eqPhysCollisionFilter* filterParams,
																void* args = NULL);

	///< draws physics bounding boxes
	void							DebugDrawBodies(int mode);

	///< Simulates physics
	void							SimulateStep( float deltaTime, int iteration, FNSIMULATECALLBACK preIntegrFunc);	///< simulates physics

	static void						PerformCollisionDetectionJob(void* thisPhys, int i);

	//------------------------------------------------------

	//------------------------------------------------------

	///< Integrates single body without collision detection
	void							IntegrateSingle(CEqRigidBody* body);

	///< detects body collisions
	void							DetectCollisionsSingle(CEqRigidBody* body);

	///< processes contact pairs
	void							ProcessContactPair(ContactPair_t& pair);

	// checks collision (made especially for rays, but could be used in other situations)
	bool							CheckAllowContactTest(eqPhysCollisionFilter* filterParams,CEqCollisionObject* object);

	void							SetDebugRaycast(bool enable) {m_debugRaycast = enable;}

	void							DetectBodyCollisions(CEqRigidBody* bodyA, CEqRigidBody* bodyB, float fDt);
	void							DetectStaticVsBodyCollision(CEqCollisionObject* staticObj, CEqRigidBody* bodyB, float fDt);

	//static void						CellCollisionDetectionJob(void* data, int iter);

protected:

	///< tests line versus some objects
	template <typename F>
	bool							TestLineCollisionOnCell(int y, int x,
															const FVector3D& start,
															const FVector3D& end,
															const BoundingBox& rayBox,
															CollisionData_t& coll,
															int rayMask,
															eqPhysCollisionFilter* filterParams,
															F func,
															void* args = NULL);

	///< Performs collision tests in broadphase grid
	template <typename F>
	void							InternalTestLineCollisionCells(	int y1, int x1, int y2, int x2,
																	const FVector3D& start,
																	const FVector3D& end,
																	const BoundingBox& rayBox,
																	CollisionData_t& coll,
																	int rayMask,
																	eqPhysCollisionFilter* filterParams,
																	F func,
																	void* args = NULL);

	CEqCollisionBroadphaseGrid		m_grid;

protected:

	DkList<eqPhysSurfParam_t*>		m_physSurfaceParams;

	DkList<CEqRigidBody*>			m_moveable;

	DkList<CEqRigidBody*>			m_dynObjects;
	DkList<CEqCollisionObject*>		m_staticObjects;

	DkList<CEqCollisionObject*>		m_ghostObjects;

	DkList<IEqPhysicsConstraint*>	m_constraints;
	DkList<IEqPhysicsController*>	m_controllers;

	btDispatcherInfo				m_dispatchInfo;
	btCollisionWorld*				m_collisionWorld;
	btCollisionConfiguration*		m_collConfig;
	btCollisionDispatcher*			m_collDispatcher;

	Threading::CEqMutex&			m_mutex;

	int								m_numRayQueries;

	float							m_fDt;

	bool							m_debugRaycast;
};

#endif // EQPHYSICS_H
