/*=============================================================================*/
// Copyright 2017-2018 Elite Engine
// Authors: Matthieu Delaere, Thomas Goussaert
/*=============================================================================*/
// SteeringBehaviors.h: SteeringBehaviors interface and different implementations
/*=============================================================================*/
#ifndef ELITE_STEERINGBEHAVIORS
#define ELITE_STEERINGBEHAVIORS

//-----------------------------------------------------------------
// Includes & Forward Declarations
//-----------------------------------------------------------------
#include "SteeringHelpers.h"
#include "Exam_HelperStructs.h"

using namespace Elite;

#pragma region **ISTEERINGBEHAVIOR** (BASE)
class ISteeringBehavior
{
public:
	ISteeringBehavior() = default;
	virtual ~ISteeringBehavior() = default;

	virtual SteeringPlugin_Output CalculateSteering(float deltaT, const AgentInfo& agentInfo) = 0;

	//Seek Functions
	void SetTarget(const TargetData& target) { m_Target = target; }

	template<class T, typename std::enable_if<std::is_base_of<ISteeringBehavior, T>::value>::type* = nullptr>
	T* As()
	{ return static_cast<T*>(this); }

protected:
	TargetData m_Target;
};
#pragma endregion

///////////////////////////////////////
//SEEK
//****
class Seek : public ISteeringBehavior
{
public:
	Seek() = default;
	virtual ~Seek() = default;

	//Seek Behaviour
	SteeringPlugin_Output CalculateSteering(float deltaT, const AgentInfo& agentInfo) override;
};

///////////////////////////////////////
//FLEE
//****
class Flee : public ISteeringBehavior
{
public:
	Flee() = default;
	virtual ~Flee() = default;

	//Flee Behaviour
	SteeringPlugin_Output CalculateSteering(float deltaT, const AgentInfo& agentInfo) override;
};

///////////////////////////////////////
//ARRIVE
//****
//class Arrive : public ISteeringBehavior
//{
//public:
//	Arrive() = default;
//	virtual ~Arrive() = default;
//
//	//Arrive Behaviour
//	SteeringOutput CalculateSteering(float deltaT, SteeringAgent* pAgent) override;
//};

///////////////////////////////////////
//FACE
//****
//class Face : public ISteeringBehavior
//{
//public:
//	Face() = default;
//	virtual ~Face() = default;
//
//	//Face Behaviour
//	SteeringOutput CalculateSteering(float deltaT, SteeringAgent* pAgent) override;
//};

//////////////////////////
//WANDER
//******
class Wander : public Seek
{
public:
	Wander() = default;
	virtual ~Wander() = default;

	//Wander Behavior
	SteeringPlugin_Output CalculateSteering(float deltaT, const AgentInfo& agentInfo) override;

	void SetWanderOffset(const float offset) { m_Offset = offset; };
	void SetWanderRadius(const float radius) { m_Radius = radius; };
	void SetMaxAngleChange(const float angleChange) { m_AngleChange = angleChange; };
protected:
	float m_Offset = 6.f; //distance from agent to circle center
	float m_Radius = 4.f;
	float m_AngleChange = ToRadians(45); //max WanderAngle change per frame
	float m_WanderAngle = 0.f;

	void SetTarget(const TargetData* pTarget) {};//no need to set target, hide this function
};
	

//////////////////////////
//EVADE
//******
//class Evade : public ISteeringBehavior
//{
//public:
//	Evade() = default;
//	virtual ~Evade() = default;
//
//	//Evade Behavior
//	SteeringOutput CalculateSteering(float deltaT, SteeringAgent* pAgent) override;
//};

//////////////////////////
//PURSUIT
//******
//class Pursuit : public ISteeringBehavior
//{
//public:
//	Pursuit() = default;
//	virtual ~Pursuit() = default;
//
//	//Pursuit Behavior
//	SteeringOutput CalculateSteering(float deltaT, SteeringAgent* pAgent) override;
//};
#endif


