#pragma once
#include "SteeringBehaviors.h"
#include <vector>
#include "Exam_HelperStructs.h"

//****************
//BLENDED STEERING
class BlendedSteering final: public ISteeringBehavior
{
	struct WeightedBehavior
	{
		ISteeringBehavior* pBehavior = nullptr;
		float weight = 0.f;

		WeightedBehavior(ISteeringBehavior* pBehavior, float weight) :
			pBehavior(pBehavior),
			weight(weight)
		{};
	};
public:
	BlendedSteering(std::vector<WeightedBehavior> weightedBehaviors);

	void AddBehaviour(WeightedBehavior weightedBehavior) { m_WeightedBehaviors.push_back(weightedBehavior); }
	SteeringPlugin_Output CalculateSteering(float deltaT, const AgentInfo& agentInfo) override;

private:
	std::vector<WeightedBehavior> m_WeightedBehaviors = {};
};