//Precompiled Header [ALWAYS ON TOP IN CPP]
#include "stdafx.h"

//Includes
#include "SteeringBehaviors.h"

//SEEK
//****
SteeringPlugin_Output Seek::CalculateSteering(float deltaT, const AgentInfo& agentInfo)
{
	SteeringPlugin_Output steering{};

	Elite::Vector2 vectToTarget{ m_Target.Position - agentInfo.Position }; //vector from agent to target
	vectToTarget.Normalize();
	steering.LinearVelocity = agentInfo.MaxLinearSpeed * vectToTarget; //rescale vect to max speed

	return steering;
}

//FLEE
//****
SteeringPlugin_Output Flee::CalculateSteering(float deltaT, const AgentInfo& agentInfo)
{
	SteeringPlugin_Output steering{};
	//const float fleeRange{ 10.f }; //agent flees from target while in this range

	Elite::Vector2 vectToTarget{ m_Target.Position - agentInfo.Position }; //vector from agent to target
	vectToTarget.Normalize();
	steering.LinearVelocity = -agentInfo.MaxLinearSpeed * vectToTarget; //rescale vect to max speed
	
	return steering;
}

//FACE
//*****

SteeringPlugin_Output Face::CalculateSteering(float deltaT, const AgentInfo& agentInfo)
{
	SteeringPlugin_Output steering{};
	
	Elite::Vector2 vectToTarget{ m_Target.Position - agentInfo.Position }; //vector from agent to target

	float angleToTarget{ atan2f(vectToTarget.x, -vectToTarget.y) }; //get angle between target and x axis
	float currentRotation{ agentInfo.Orientation };

	//check if agent is facing the target, if so then stop rotating
	const float epsilon{ 0.1f };
	if (currentRotation + epsilon > angleToTarget && currentRotation - epsilon < angleToTarget)
	{
		steering.AngularVelocity = 0;
	}
	else
	{
		//set angular velocity correctly
		//1 if true, -1 if false
		steering.AngularVelocity = (int(angleToTarget > currentRotation) * 2 - 1) * agentInfo.MaxAngularSpeed;
	}

	return steering;
}

//WANDER (base> SEEK)
//******
SteeringPlugin_Output Wander::CalculateSteering(float deltaT, const AgentInfo& agentInfo)
{
	const Elite::Vector2 directionVect{ agentInfo.LinearVelocity.GetNormalized() };
	const Elite::Vector2 circleCenter{ agentInfo.Position + directionVect * m_Offset };
	
	m_WanderAngle += (randomFloat(m_AngleChange) - randomFloat(m_AngleChange)); //slightly change wander angle
	
	//place target point on the circle
	Elite::Vector2 targetPoint{ circleCenter };
	targetPoint.x += m_Radius * cos(m_WanderAngle);
	targetPoint.y += m_Radius * sin(m_WanderAngle);

	//set target and go
	m_Target.Position = targetPoint;
	return Seek::CalculateSteering(deltaT, agentInfo);
	return SteeringPlugin_Output{};
}