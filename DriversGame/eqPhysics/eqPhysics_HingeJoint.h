//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2017
//////////////////////////////////////////////////////////////////////////////////
// Description: Physics hinge joint
//////////////////////////////////////////////////////////////////////////////////

#ifndef EQPHYSICS_HINGEJOINT_H
#define EQPHYSICS_HINGEJOINT_H

#include "eqPhysics_Controller.h"

#include "eqPhysics_PointConstraint.h"
#include "eqPhysics_MaxDistConstraint.h"

class CEqRigidBody;

const float MAX_HINGE_ANGLE_LIMIT = 150.0f;

class CEqPhysicsHingeJoint : public IEqPhysicsController
{
public:
	CEqPhysicsHingeJoint();
	~CEqPhysicsHingeJoint();

	void Init(	CEqRigidBody* body0, CEqRigidBody* body1, 
				const Vector3D & hingeAxis, 
				const FVector3D & hingePosRel0,
				const float hingeHalfWidth,
				const float hingeFwdAngle,	// angles are in radians
				const float hingeBckAngle,	// angles are in radians
				const float sidewaysSlack,
				const float damping = -1.0f);

	void				SetEnabled(bool enable);

    void				Break();		/// Just remove the limit constraint
    void				Restore();		/// Just enable the limit constraint

    bool				IsBroken() const				{return m_broken;}
    const FVector3D&	GetHingePosRel0() const			{ return m_hingePosRel0; }

    /// We can be asked to apply an extra torque to body0 (and
    /// opposite to body1) each time step.
    void				SetExtraTorque(float torque)	{m_extraTorque = torque;}

	void				Update(float dt);

protected:
	void				AddedToWorld( CEqPhysics* physics );
	void				RemovedFromWorld( CEqPhysics* physics );

	Vector3D			m_hingeAxis;
	FVector3D			m_hingePosRel0;
	CEqRigidBody*		m_body0;
	CEqRigidBody*		m_body1;

	bool				m_usingLimit;
	bool				m_hingeEnabled;
	bool				m_broken;
	float				m_damping;
	float				m_extraTorque; // allow extra torque applied per update

	CEqPhysicsPointConstraint		m_midPointConstraint;
	CEqPhysicsMaxDistConstraint		m_sidePointConstraints[2];
	CEqPhysicsMaxDistConstraint		m_maxDistanceConstraint;
};

#endif // EQPHYSICS_HINGEJOINT_H