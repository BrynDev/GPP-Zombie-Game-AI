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
	virtual void Update(Blackboard* pBlackboard, float deltaTime) override
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

	virtual void OnExit(Blackboard* pBlackboard) override
	{
		IExamInterface* pInterface{ nullptr };
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return;
		}

		EntityInfo targetItem{};
		pBlackboard->GetData("TargetItem", targetItem);

		AgentInfo agentInfo{ pInterface->Agent_GetInfo() };
		
		
		const Elite::Vector2 agentPos{ agentInfo.Position };
		const float nearbyRange{ 1.0f };
		//check if agent has arrived
		if (Elite::Distance(targetItem.Location, agentPos) <= nearbyRange)
		{
			EvaluateItem(targetItem, pBlackboard, pInterface);
			//destroy the ground item after evaluating it
			pInterface->Item_Destroy(targetItem);
		}
	}
private:
	void EvaluateItem(const EntityInfo& newItemEntityInfo, Blackboard* pBlackboard ,IExamInterface* const pInterface) const
	{	
		ItemInfo newItem{};
		pInterface->Item_GetInfo(newItemEntityInfo, newItem);
		eItemType newItemType{ newItem.Type };
		if (newItemType == eItemType::GARBAGE)
		{
			//don't pick up garbage
			return;
		}

		const unsigned int invCapacity{ pInterface->Inventory_GetCapacity() };
		std::vector<ItemInfo> invItems{};
		invItems.reserve(invCapacity);
		invItems.resize(invCapacity);
		//check inventory
		for (unsigned int i{ 0 }; i < invCapacity; ++i)
		{
			ItemInfo inventoryItem{};
			//if there's an empty inventory slot, pick up the item
			if (!pInterface->Inventory_GetItem(i, inventoryItem))
			{
				pInterface->Item_Grab(newItemEntityInfo, newItem);
				pInterface->Inventory_AddItem(i, newItem);
				if (newItem.Type == eItemType::PISTOL)
				{
					pBlackboard->ChangeData("WeaponInventoryIndex", int(i));
				}
				return;
			}
			else
			{
				//record the item type in this slot
				invItems[i] = inventoryItem;
			}
		}

		//if we've reached this point, there are no empty inventory slots
		//we need to decide if we should drop or use another item before grabbing this one
		for (unsigned int i{ 0 }; i < invCapacity; ++i)
		{
			ItemInfo invItem{ invItems[i] };
			if (invItem.Type == newItem.Type)
			{
				switch (newItem.Type)
				{
				case eItemType::FOOD:
					if (pInterface->Food_GetEnergy(newItem) >= pInterface->Food_GetEnergy(invItem))
					{
						pInterface->Inventory_UseItem(i);
						pInterface->Inventory_RemoveItem(i);
						pInterface->Item_Grab(newItemEntityInfo, newItem);
						pInterface->Inventory_AddItem(i, newItem);
					}
					break;
				case eItemType::MEDKIT:
					if (pInterface->Medkit_GetHealth(newItem) >= pInterface->Medkit_GetHealth(invItem))
					{
						pInterface->Inventory_UseItem(i);
						pInterface->Inventory_RemoveItem(i);
						pInterface->Item_Grab(newItemEntityInfo, newItem);
						pInterface->Inventory_AddItem(i, newItem);
					}
					break;
				case eItemType::PISTOL:
					if (pInterface->Weapon_GetAmmo(newItem) > pInterface->Weapon_GetAmmo(invItem))
					{
						pInterface->Inventory_RemoveItem(i);
						pInterface->Item_Grab(newItemEntityInfo, newItem);
						pInterface->Inventory_AddItem(i, newItem);
						pBlackboard->ChangeData("WeaponInventoryIndex", int(i));
					}
					break;
				default:
					break;
				}
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

class KillZombieState final : public FSMState
{
public:
	KillZombieState() : FSMState() {};
	virtual void OnEnter(Blackboard* pBlackboard) override
	{
		SteeringController* pSteeringController{};
		bool isDataAvailable = pBlackboard->GetData("SteeringController", pSteeringController);
		if (!isDataAvailable)
		{
			return;
		}

		EnemyInfo targetEnemy{};
		isDataAvailable = pBlackboard->GetData("TargetEnemy", targetEnemy);
		if (!isDataAvailable)
		{
			return;
		}

		TargetData target{};
		target.Position = targetEnemy.Location;
		pSteeringController->SetToFace(target);
		pBlackboard->ChangeData("AutoOrient", false);
	}

	virtual void Update(Blackboard* pBlackboard, float deltaTime) override
	{
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return;
		}

		if (Elite::AreEqual(pInterface->Agent_GetInfo().AngularVelocity, 0.f))
		{
			IExamInterface* pInterface{};
			bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
			if (!isDataAvailable)
			{
				return;
			}

			/*int weaponIdx{};
			pBlackboard->GetData("WeaponInventoryIndex", weaponIdx);
			pInterface->Inventory_UseItem(weaponIdx);*/
		}
	}

	virtual void OnExit(Blackboard* pBlackboard) override
	{
		pBlackboard->ChangeData("AutoOrient", true);
	}
};

//TRANSITIONS

class SeesZombieTransition final : public Elite::FSMTransition
{
public:
	SeesZombieTransition() : FSMTransition() {};
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

class CanKillZombieTransition final : public Elite::FSMTransition
{
public:
	CanKillZombieTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		std::vector<EntityInfo> entitiesVect{};
		bool isDataAvailable{ pBlackboard->GetData("EntitiesInFOV", entitiesVect) };

		if (!isDataAvailable)
		{
			return false;
		}

		IExamInterface* pInterface{};
		isDataAvailable = pBlackboard->GetData("Interface", pInterface);

		for (const EntityInfo& info : entitiesVect)
		{
			if (info.Type == eEntityType::ENEMY)
			{
				//check if the agent has a usable weapon
			//weapon index of -1 means that agent doesn't have a weapon
				int weaponIdx{};
				isDataAvailable = pBlackboard->GetData("WeaponInventoryIndex", weaponIdx);
				if (!isDataAvailable || weaponIdx == -1)
				{
					return false;
				}
				TargetData target{};
				target.Position = info.Location;
				pBlackboard->ChangeData("Target", target);

				EnemyInfo enemyInfo{};
				pInterface->Enemy_GetInfo(info, enemyInfo);
				//check if the weapon has enough ammo to kill this zombie
				ItemInfo weaponInfo{};
				pInterface->Inventory_GetItem(weaponIdx, weaponInfo);
				//also don't bother trying to kill heavy zombies
				if (enemyInfo.Type != eEnemyType::ZOMBIE_HEAVY && enemyInfo.Health <= pInterface->Weapon_GetAmmo(weaponInfo))
				{
					pBlackboard->ChangeData("TargetEnemy", enemyInfo);
					return true;
				}			
			}
		}

		return false;
	}
};

class HasKilledZombieTransition final : public Elite::FSMTransition
{
public:
	HasKilledZombieTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		EnemyInfo targetEnemy{};
		pBlackboard->GetData("TargetEnemy", targetEnemy);
		//TODO
		return false;
	}
};

class SeesHouseTransition final : public Elite::FSMTransition
{
public:
	SeesHouseTransition() : FSMTransition() {};
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
	virtual bool ToTransition(Blackboard* pBlackboard) const override
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
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		TargetData fleeTarget{};
		pBlackboard->GetData("Target", fleeTarget);

		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}

		const float requiredDistance{ 40.f };
		if (Distance(fleeTarget.Position, pInterface->Agent_GetInfo().Position) >= requiredDistance)
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
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}

		return pInterface->Agent_GetInfo().IsInHouse;
	}
};

class IsNotInsideHouseTransition final : public FSMTransition
{
public:
	IsNotInsideHouseTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}

		return !pInterface->Agent_GetInfo().IsInHouse;
	}
};

class FinishedSearchingHouseTransition final : public FSMTransition
{
public:
	FinishedSearchingHouseTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}

		HouseInfo targetHouse{};
		pBlackboard->GetData("TargetHouse", targetHouse);
		
		const float nearbyRange{1.f};
		if (Distance(targetHouse.Center, pInterface->Agent_GetInfo().Position) <= nearbyRange)
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
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		EntityInfo targetItem{};
		pBlackboard->GetData("TargetItem", targetItem);

		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}
		
		AgentInfo agentInfo{ pInterface->Agent_GetInfo() };
		
		const float nearbyRange{ 1.f };
		if (Distance(targetItem.Location, agentInfo.Position) <= nearbyRange)
		{
			return true;
		}
		return false;
	}
};

#endif