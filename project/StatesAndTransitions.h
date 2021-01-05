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
	virtual void OnEnter(Blackboard* pBlackboard) override
	{
		SteeringController* pSteeringController{ nullptr };
		bool isDataAvailable{ pBlackboard->GetData("SteeringController", pSteeringController) };

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
	virtual void OnEnter(Blackboard* pBlackboard) override
	{
		SteeringController* pSteeringController{ nullptr };
		bool isDataAvailable{ pBlackboard->GetData("SteeringController", pSteeringController) };

		if (!isDataAvailable)
		{
			return;
		}

		TargetData target{};
		isDataAvailable = pBlackboard->GetData("Target", target);
		if (!isDataAvailable)
		{
			return;
		}

		IExamInterface* pInterface{};
		isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return;
		}

		pSteeringController->SetToImperfectFlee(target);
	}
};

class EnterHouseState final : public Elite::FSMState
{
public:
	EnterHouseState() : FSMState() {};
	virtual void OnEnter(Blackboard* pBlackboard) override
	{
		SteeringController* pSteeringController{ nullptr };
		bool isDataAvailable{ pBlackboard->GetData("SteeringController", pSteeringController) };

		if (!isDataAvailable)
		{
			return;
		}

		TargetData target{};
		isDataAvailable = pBlackboard->GetData("Target", target);
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
		
		//check if agent has arrived
		const Elite::Vector2 agentPos{ pInterface->Agent_GetInfo().Position };
		const float nearbyRange{ 0.5f };
		if (Elite::Distance(target.Position, agentPos) <= nearbyRange)
		{
			SteeringController* pSteeringController{ nullptr };
			pBlackboard->GetData("SteeringController", pSteeringController);

			//get house info
			HouseInfo targetHouseInfo{};
			pBlackboard->GetData("TargetHouse", targetHouseInfo);
			//now that agent has arrived at the goal, go to the next closest navmesh point to the house
			//this will repeat, making the agent get closer and closer to the entrance, until a transition returns true and this state is exited 
			//(likely because the agent got inside the house)
			TargetData nextTarget{};
			nextTarget.Position = pInterface->NavMesh_GetClosestPathPoint(targetHouseInfo.Center);
			if (nextTarget.Position != target.Position)
			{
				pSteeringController->SetToSeek(nextTarget);
				//mark the previous position as the house entry point, so the agent can exit this way later
				pBlackboard->ChangeData("HouseEntryPoint", target.Position);
				pBlackboard->ChangeData("Target", nextTarget);
			}
		}
	}
};

class GrabItemState final : public FSMState
{
public:
	GrabItemState() : FSMState() {};
	virtual void OnEnter(Blackboard* pBlackboard) override
	{
		EntityInfo entityInfo{};
		bool isDataAvailable{ pBlackboard->GetData("TargetItem", entityInfo) };
		if (!isDataAvailable)
		{
			return;
		}

		SteeringController* pSteeringController;
		isDataAvailable = pBlackboard->GetData("SteeringController", pSteeringController);
		if (!isDataAvailable)
		{
			return;
		}

		TargetData seekTarget{};
		seekTarget.Position = entityInfo.Location;
		pSteeringController->SetToSeek(seekTarget);

		IExamInterface* pInterface{};
		isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return;
		}
	}

	virtual void Update(Blackboard* pBlackboard, float deltaTime)
	{
		IExamInterface* pInterface{ nullptr };
		pBlackboard->GetData("Interface", pInterface);
		
		EntityInfo targetItem{};
		pBlackboard->GetData("TargetItem", targetItem);
	
		//pInterface->Draw_SolidCircle(target.Position, 5, { 1,1 }, { 1,0,0 });
		const Elite::Vector2 agentPos{ pInterface->Agent_GetInfo().Position };
		const float nearbyRange{ 1.0f };
		//check if agent has arrived
		if (Elite::Distance(targetItem.Location, agentPos) <= nearbyRange)
		{		
			EvaluateItem(targetItem, pInterface);
			//destroy the ground item after evaluating it
			pInterface->Item_Destroy(targetItem);
		}
	}
private:
	void EvaluateItem(const EntityInfo& newItemEntityInfo, IExamInterface* const pInterface) const
	{	
		ItemInfo newItem{};
		pInterface->Item_GetInfo(newItemEntityInfo, newItem);

		if (newItem.Type == eItemType::GARBAGE)
		{
			//don't pick up garbage
			return;
		}
		//check inventory
		for (unsigned int i{ 0 }; i < pInterface->Inventory_GetCapacity(); ++i)
		{
			ItemInfo inventoryItem{};
			//if there's an empty inventory slot, pick up the item
			if (!pInterface->Inventory_GetItem(i, inventoryItem))
			{
				pInterface->Item_Grab(newItemEntityInfo, newItem);
				pInterface->Inventory_AddItem(i, newItem);
			}
			else
			{
				eItemType newItemType{ newItem.Type };
				
			}
		}

	
	}
};

class SearchCurrentHouseState final : public FSMState
{
public:
	SearchCurrentHouseState() : FSMState() {};
	virtual void OnEnter(Blackboard* pBlackboard) override
	{
		SteeringController* pSteeringController{};
		bool isDataAvailable = pBlackboard->GetData("SteeringController", pSteeringController);
		if (!isDataAvailable)
		{
			return;
		}

		TargetData lastTarget{};
		isDataAvailable = pBlackboard->GetData("Target", lastTarget);
		if (!isDataAvailable)
		{
			return;
		}

		HouseInfo houseInfo{};
		isDataAvailable = pBlackboard->GetData("TargetHouse", houseInfo);
		if (!isDataAvailable)
		{
			return;
		}

		//move to house center
		TargetData houseCenter{};
		houseCenter.Position = houseInfo.Center;
		pSteeringController->SetToSeek(houseCenter);
	}
};

class ExitCurrentHouseState final : public FSMState
{
public:
	ExitCurrentHouseState() : FSMState() {};
	virtual void OnEnter(Blackboard* pBlackboard) override
	{
		SteeringController* pSteeringController{};
		bool isDataAvailable = pBlackboard->GetData("SteeringController", pSteeringController);
		if (!isDataAvailable)
		{
			return;
		}

		Elite::Vector2 houseEntryPoint{};
		isDataAvailable = pBlackboard->GetData("HouseEntryPoint", houseEntryPoint);
		if (!isDataAvailable)
		{
			return;
		}

		TargetData target{};
		target.Position = houseEntryPoint;
		//pBlackboard->GetData("Target", target);
		pSteeringController->SetToSeek(target);
	}
	virtual void OnExit(Blackboard* pBlackboard) override
	{
		//set the target to the house entry point
		//when the agent leaves the house and transitions to the flee state,
		//it will run away from the house before wandering
		//this makes it so the agent doesn't stick near the house it just looted

		TargetData target{};
		Elite::Vector2 houseEntryPoint{};
		pBlackboard->GetData("HouseEntryPoint", houseEntryPoint);
		target.Position = houseEntryPoint;
		pBlackboard->ChangeData("Target", target);
	}
};

//TRANSITIONS

class SeesZombieTransition final : public Elite::FSMTransition
{
public:
	SeesZombieTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard, const AgentInfo& agentInfo) const override
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

class SeesHouseTransition final : public Elite::FSMTransition
{
public:
	SeesHouseTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard, const AgentInfo& agentInfo) const override
	{
		std::vector<HouseInfo> housesVect{};
		bool isDataAvailable{ pBlackboard->GetData("HousesInFOV", housesVect) };

		if (!isDataAvailable)
		{
			return false;
		}

		if (!housesVect.empty())
		{
			//make sure not to enter the same house twice in a row
			HouseInfo prevHouse{};
			pBlackboard->GetData("TargetHouse", prevHouse);
			if (housesVect[0].Center == prevHouse.Center)
			{
				return false;
			}

			IExamInterface* pInterface{ nullptr };
			pBlackboard->GetData("Interface", pInterface);
			//get house info
			HouseInfo targetHouseInfo{};
			targetHouseInfo.Center = housesVect[0].Center;
			targetHouseInfo.Size = housesVect[0].Size;
			//set initial seek target
			TargetData target{};
			target.Position = pInterface->NavMesh_GetClosestPathPoint(targetHouseInfo.Center);
			pBlackboard->ChangeData("Target", target);
			//set house info
			pBlackboard->ChangeData("TargetHouse", targetHouseInfo);
			return true;
		}

		return false;
	}
};

class SeesItemTransition final : public Elite::FSMTransition
{
public:
	SeesItemTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard, const AgentInfo& agentInfo) const override
	{
		std::vector<EntityInfo> entityVect{};
		bool isDataAvailable{ pBlackboard->GetData("EntitiesInFOV", entityVect) };

		if (!isDataAvailable)
		{
			return false;
		}

		for (int i{ 0 }; i < int(entityVect.size()); ++i)
		{
			EntityInfo currentInfo{ entityVect[i] };
			if (currentInfo.Type == eEntityType::ITEM)
			{
				pBlackboard->ChangeData("TargetItem", currentInfo);
				return true;
			}
		}

		return false;
	}
};

class FinishedFleeingTransition final : public FSMTransition
{
public:
	FinishedFleeingTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard, const AgentInfo& agentInfo) const override
	{
		TargetData fleeTarget{};
		bool isDataAvailable = pBlackboard->GetData("Target", fleeTarget);
		if (!isDataAvailable)
		{
			return false;
		}

		IExamInterface* pInterface;
		isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}

		const float requiredDistance{ 40.f };
		if (Distance(fleeTarget.Position, agentInfo.Position) >= requiredDistance)
		{
			return true;
		}
		return false;

	}
};

class IsInsideHouseTransition final : public FSMTransition
{
public:
	IsInsideHouseTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard, const AgentInfo& agentInfo) const override
	{
		return agentInfo.IsInHouse;
	}
};

class IsNotInsideHouseTransition final : public FSMTransition
{
public:
	IsNotInsideHouseTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard, const AgentInfo& agentInfo) const override
	{
		return !agentInfo.IsInHouse;
	}
};

class FinishedSearchingHouseTransition final : public FSMTransition
{
public:
	FinishedSearchingHouseTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard, const AgentInfo& agentInfo) const override
	{
		HouseInfo targetHouse{};
		bool isDataAvailable = pBlackboard->GetData("TargetHouse", targetHouse);
		if (!isDataAvailable)
		{
			return false;
		}

		const float nearbyRange{2.f};
		if (Distance(targetHouse.Center, agentInfo.Position) <= nearbyRange)
		{
			return true;
		}
		return false;
	}
};

class HasGrabbedItemTransition final : public FSMTransition
{
public:
	HasGrabbedItemTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard, const AgentInfo& agentInfo) const override
	{
		EntityInfo targetItem{};
		bool isDataAvailable = pBlackboard->GetData("TargetItem", targetItem);
		if (!isDataAvailable)
		{
			return false;
		}

		const float nearbyRange{ 1.f };
		if (Distance(targetItem.Location, agentInfo.Position) <= nearbyRange)
		{
			return true;
		}
		return false;
	}
};

#endif