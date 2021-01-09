#pragma once
#include "Exam_HelperStructs.h"
#include "SteeringHelpers.h"

class ISteeringBehavior;
class Wander;
class Flee;
class Seek;
class Face;
class BlendedSteering;

class SteeringController
{
public:
	SteeringController();
	~SteeringController();

	void SetToWander();
	void SetToFlee(const TargetData& target);
	void SetToImperfectFlee(const TargetData& target);
	void SetToSeek(const TargetData& target);
	void SetToFace(const TargetData& target);
	SteeringPlugin_Output CalculateSteering(const float deltaTime, const AgentInfo& agentInfo) const;

	SteeringController(const SteeringController& other) = delete;
	SteeringController& operator=(const SteeringController& rhs) = delete;
	SteeringController(SteeringController&& other) = delete;
	SteeringController& operator=(SteeringController&& rhs) = delete;
private:
	Wander* m_pWander;
	Flee* m_pFlee;
	Seek* m_pSeek;
	Face* m_pFace;
	BlendedSteering* m_pImperfectFlee;
	ISteeringBehavior* m_pCurrentSteering;
};

