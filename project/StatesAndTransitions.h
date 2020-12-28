/*=============================================================================*/
// Copyright 2020-2021 Elite Engine
/*=============================================================================*/
// StatesAndTransitions.h: Implementation of the state/transition classes
/*=============================================================================*/
#ifndef ELITE_APPLICATION_FSM_STATES_TRANSITIONS
#define ELITE_APPLICATION_FSM_STATES_TRANSITIONS

#include "SteeringController.h"
#include "EFiniteStateMachine.h"
#include "EBlackboard.h"
#include "IExamInterface.h"

using namespace Elite;
//STATES
class WanderState final : public Elite::FSMState
{
public:
	WanderState() : FSMState() {};
	virtual void OnEnter(Blackboard* pBlackBoard) override
	{
		SteeringController* pSteeringController{ nullptr };
		bool isDataAvailable{ pBlackBoard->GetData("SteeringSelector", pSteeringController) };

		if (!isDataAvailable)
		{
			return;
		}

		pSteeringController->SetToWander();
	}
};

class FleeState final : public Elite::FSMState
{
public:
	FleeState() : FSMState() {};
	virtual void OnEnter(Blackboard* pBlackBoard) override
	{
		SteeringController* pSteeringController{ nullptr };
		bool isDataAvailable{ pBlackBoard->GetData("SteeringSelector", pSteeringController) };

		if (!isDataAvailable)
		{
			return;
		}

		TargetData target{};
		isDataAvailable = pBlackBoard->GetData("Target", target);
		if (!isDataAvailable)
		{
			return;
		}

		//pSteeringController->SetToFlee(target);
		pSteeringController->SetToImperfectFlee(target);
	}

};

class SeekState final : public Elite::FSMState
{
public:
	SeekState() : FSMState() {};
	virtual void OnEnter(Blackboard* pBlackBoard) override
	{
		SteeringController* pSteeringController{ nullptr };
		bool isDataAvailable{ pBlackBoard->GetData("SteeringSelector", pSteeringController) };

		if (!isDataAvailable)
		{
			return;
		}

		TargetData target{};
		isDataAvailable = pBlackBoard->GetData("Target", target);
		if (!isDataAvailable)
		{
			return;
		}

		pSteeringController->SetToSeek(target);
	}
	virtual void Update(Blackboard* pBlackboard, float deltaTime)
	{
		IExamInterface* pInterface{ nullptr };
		pBlackboard->GetData("Interface", pInterface);
		TargetData target{};
		pBlackboard->GetData("Target", target);
		pInterface->Draw_SolidCircle(target.Position, 5, { 1,1 }, { 1,0,0 });
	}

};

//TRANSITIONS

class SeesZombie final : public Elite::FSMTransition
{
public:
	SeesZombie() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		std::vector<EntityInfo> entitiesVect{};
		bool isDataAvailable{pBlackboard->GetData("EntitiesInFOV", entitiesVect)};

		if (!isDataAvailable)
		{
			return false;
		}

		for (const EntityInfo& info : entitiesVect)
		{
			if (info.Type == eEntityType::ENEMY)
			{
				TargetData target{};
				target.Position = info.Location;
				pBlackboard->ChangeData("Target", target);
				return true;					
			}
		}

		return false;
	}
};

class SeesHouse final : public Elite::FSMTransition
{
public:
	SeesHouse() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		std::vector<HouseInfo> housesVect{};
		bool isDataAvailable{ pBlackboard->GetData("HousesInFOV", housesVect) };

		if (!isDataAvailable)
		{
			return false;
		}

		if (!housesVect.empty())
		{
			IExamInterface* pInterface{ nullptr };
			pBlackboard->GetData("Interface", pInterface);
			TargetData target{};
			target.Position = pInterface->NavMesh_GetClosestPathPoint(housesVect[0].Center); //do this continuously in update maybe
			pBlackboard->ChangeData("Target", target);
			return true;
		}

		return false;
	}
};
#endif