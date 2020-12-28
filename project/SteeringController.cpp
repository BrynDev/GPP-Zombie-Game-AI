#include "stdafx.h"
#include "SteeringController.h"
#include "SteeringBehaviors.h"
#include "BlendedSteering.h"

SteeringController::SteeringController()
	:m_pWander{new Wander()}
	, m_pFlee{new Flee()}
	, m_pSeek{new Seek()}
	, m_pImperfectFlee{nullptr}
	, m_pCurrentSteering{nullptr}
{
	m_pImperfectFlee = new BlendedSteering({ {m_pFlee, 0.8f}, {m_pWander, 0.2f} });
}

SteeringController::~SteeringController()
{
	delete m_pWander;
	delete m_pFlee;
	delete m_pSeek;
	delete m_pImperfectFlee;
}

SteeringPlugin_Output SteeringController::CalculateSteering(const float deltaTime, const AgentInfo& agentInfo) const
{
	return m_pCurrentSteering->CalculateSteering(deltaTime, agentInfo);
}

void SteeringController::SetToWander()
{
	m_pCurrentSteering = m_pWander;
}

void SteeringController::SetToFlee(const TargetData& target)
{
	m_pCurrentSteering = m_pFlee;
	m_pFlee->SetTarget(target);
}

void SteeringController::SetToImperfectFlee(const TargetData& target)
{
	m_pCurrentSteering = m_pImperfectFlee;
	m_pFlee->SetTarget(target);
}

void SteeringController::SetToSeek(const TargetData& target)
{
	m_pCurrentSteering = m_pSeek;
	m_pSeek->SetTarget(target);
}