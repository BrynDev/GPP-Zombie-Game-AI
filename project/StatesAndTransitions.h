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
		pBlackboard->GetData("Target", target);

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
		pBlackboard->GetData("Target", target);
		
		pSteeringController->SetToSeek(target);
	}
	virtual void Update(Blackboard* pBlackboard, float deltaTime) override
	{
		IExamInterface* pInterface{ nullptr };
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return;
		}

		TargetData target{};
		pBlackboard->GetData("Target", target);
		
		//check if agent has arrived
		const Elite::Vector2 agentPos{ pInterface->Agent_GetInfo().Position };
		const float nearbyRange{ 1.f };
		if (Elite::DistanceSquared(target.Position, agentPos) <= nearbyRange * nearbyRange)
		{
			HouseInfo targetHouseInfo{};
			pBlackboard->GetData("TargetHouse", targetHouseInfo);
			//now that agent has arrived at the goal, go to the next closest navmesh point to the house
			//this will repeat, making the agent get closer and closer to the entrance, until a transition returns true and this state is exited 
			//(likely because the agent got inside the house)
			TargetData nextTarget{};
			nextTarget.Position = pInterface->NavMesh_GetClosestPathPoint(targetHouseInfo.Center);
			if (nextTarget.Position != target.Position)
			{
				SteeringController* pSteeringController{ nullptr };
				isDataAvailable = pBlackboard->GetData("SteeringController", pSteeringController);
				if (!isDataAvailable)
				{
					return;
				}

				pSteeringController->SetToSeek(nextTarget);
				//mark the previous position as the house entry point, so the agent can exit this way later
				pBlackboard->ChangeData("HouseEntryPoint", target.Position);
				pBlackboard->ChangeData("Target", nextTarget);
			}
		}
	}

	virtual void OnExit(Blackboard* pBlackboard) override
	{
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return;
		}

		pBlackboard->ChangeData("HouseEntryPoint", pInterface->Agent_GetInfo().Position);
	}
	
};

class GrabItemState final : public FSMState
{
public:
	GrabItemState() : FSMState() {};
	virtual void OnEnter(Blackboard* pBlackboard) override
	{
		EntityInfo entityInfo{};
		pBlackboard->GetData("TargetItem", entityInfo);
		
		SteeringController* pSteeringController;
		bool isDataAvailable = pBlackboard->GetData("SteeringController", pSteeringController);
		if (!isDataAvailable)
		{
			return;
		}

		TargetData seekTarget{};
		seekTarget.Position = entityInfo.Location;
		pSteeringController->SetToSeek(seekTarget);
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
		if (Elite::DistanceSquared(targetItem.Location, agentPos) <= nearbyRange * nearbyRange)
		{
			EvaluateItem(targetItem, pBlackboard, pInterface);
		}
	}
private:
	void EvaluateItem(const EntityInfo& newItemEntityInfo, Blackboard* pBlackboard ,IExamInterface* const pInterface) const
	{	
		ItemInfo newItem{};
		if (!pInterface->Item_GetInfo(newItemEntityInfo, newItem))
		{
			//trying to pick up an invalid item
			return;
		}

		eItemType newItemType{ newItem.Type };
		if (newItemType == eItemType::GARBAGE)
		{
			//don't pick up garbage
			pInterface->Item_Destroy(newItemEntityInfo);
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
					//if this is a pistol, mark it so the agent can use it
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
			if (invItem.Type == newItemType)
			{
				switch (newItemType)
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

		
		HouseInfo houseInfo{};
		pBlackboard->GetData("TargetHouse", houseInfo);

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
		pBlackboard->GetData("HouseEntryPoint", houseEntryPoint);

		TargetData target{};
		target.Position = houseEntryPoint;
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
		pBlackboard->GetData("TargetEnemy", targetEnemy);

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

		EnemyInfo targetEnemy{};
		pBlackboard->GetData("TargetEnemy", targetEnemy);
		AgentInfo agentInfo{ pInterface->Agent_GetInfo() };

		//check if agent is aiming at the zombie
		//Face behavior stops rotating when facing the target, so if the angular velocity is 0 that means the agent is facing the target
		if (Elite::AreEqual(agentInfo.AngularVelocity, 0.f))
		{			
			IExamInterface* pInterface{};
			bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
			if (!isDataAvailable)
			{
				return;
			}

			//shoot
			int weaponIdx{};
			pBlackboard->GetData("WeaponInventoryIndex", weaponIdx);
			pInterface->Inventory_UseItem(weaponIdx);
			//decrement shoot counter
			int nrTimesToShoot{};
			pBlackboard->GetData("NrTimesToShoot", nrTimesToShoot);
			pBlackboard->ChangeData("NrTimesToShoot", nrTimesToShoot - 1);
		}
	}

	virtual void OnExit(Blackboard* pBlackboard) override
	{
		pBlackboard->ChangeData("AutoOrient", true);

		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return;
		}

		int weaponIdx{};
		pBlackboard->GetData("WeaponInventoryIndex", weaponIdx);

		//check if this gun has any more ammo
		ItemInfo weaponInfo{};
		pInterface->Inventory_GetItem(weaponIdx, weaponInfo);
		if (pInterface->Weapon_GetAmmo(weaponInfo) <= 0)
		{
			//weapon is empty, throw it away
			pInterface->Inventory_RemoveItem(weaponIdx);

			//now we need to check if the agent has another gun to use
			//check all inventory items
			const unsigned int invCapacity{ pInterface->Inventory_GetCapacity() };
			for (unsigned int i{ 0 }; i < invCapacity; ++i)
			{
				ItemInfo invItem{};

				if (!pInterface->Inventory_GetItem(i, invItem))
				{
					//if this slot is empty, continue
					continue;
				}

				if (invItem.Type == eItemType::PISTOL)
				{
					//if the agent has a weapon, change weapon index data
					pBlackboard->ChangeData("WeaponInventoryIndex", int(i));
					return;
				}
			}
			//if the agent has no other weapon, mark this in the weapon inventory index
			pBlackboard->ChangeData("WeaponInventoryIndex", -1);
		}
	}
};

class GoToWorldCenterState final : public FSMState
{
public:
	GoToWorldCenterState() : FSMState() {};
	virtual void OnEnter(Blackboard* pBlackboard) override
	{
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return;
		}

		SteeringController* pSteering{};
		isDataAvailable = pBlackboard->GetData("SteeringController", pSteering);
		if (!isDataAvailable)
		{
			return;
		}

		
		WorldInfo worldInfo{ pInterface->World_GetInfo() };
		TargetData target{};
		target.Position = worldInfo.Center;
		pSteering->SetToSeek(target);
	}
};


class FleePurgeZoneState final : public FSMState
{
public:
	FleePurgeZoneState() : FSMState() {};
	virtual void OnEnter(Blackboard* pBlackboard) override
	{
		SteeringController* pSteeringController{ nullptr };
		bool isDataAvailable{ pBlackboard->GetData("SteeringController", pSteeringController) };

		if (!isDataAvailable)
		{
			return;
		}

		PurgeZoneInfo targetZone{};
		pBlackboard->GetData("TargetPurgeZone", targetZone);

		TargetData target{};
		target.Position = targetZone.Center;
		pSteeringController->SetToImperfectFlee(target);
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
		pBlackboard->GetData("EntitiesInFOV", entitiesVect);

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
		pBlackboard->GetData("EntitiesInFOV", entitiesVect);

		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		//check if the agent has a usable weapon
		//weapon index of -1 means that the agent doesn't have a weapon
		int weaponIdx{};
		pBlackboard->GetData("WeaponInventoryIndex", weaponIdx);
		
		if (!isDataAvailable || weaponIdx == -1)
		{
			return false;
		}

		ItemInfo weaponInfo{};
		bool wasWeaponFound = pInterface->Inventory_GetItem(weaponIdx, weaponInfo);
		if (!wasWeaponFound || !(weaponInfo.Type == eItemType::PISTOL))
		{
			if (!ResetWeaponIndex(pBlackboard, pInterface))
			{
				//there actually is no weapon in the inventory
				return false;
			}
		}

		for (const EntityInfo& info : entitiesVect)
		{
			if (info.Type == eEntityType::ENEMY)
			{			
				EnemyInfo enemyInfo{};			
				pInterface->Enemy_GetInfo(info, enemyInfo);
				//only shoot when the enemy is within a certain range (to improve accuracy)
				const float nearbyRange{ pInterface->Agent_GetInfo().FOV_Range / 2.f };
				if (Elite::DistanceSquared(enemyInfo.Location, pInterface->Agent_GetInfo().Position) >= (nearbyRange * nearbyRange))
				{
					continue;
				}
				//check if the weapon has enough ammo to kill this zombie			
				if (enemyInfo.Health <= pInterface->Weapon_GetAmmo(weaponInfo))
				{
					pBlackboard->ChangeData("TargetEnemy", enemyInfo);
					pBlackboard->ChangeData("NrTimesToShoot", enemyInfo.Health);
					return true;
				}			
			}
		}

		return false;
	}
private:
	//this function gets called if no valid weapon is found when there should have been one
	//"recalibrates" the weapon index
	bool ResetWeaponIndex(Blackboard* pBlackboard, IExamInterface* pInterface) const
	{		
		//check all inventory items
		const unsigned int invCapacity{ pInterface->Inventory_GetCapacity() };
		for (unsigned int i{ 0 }; i < invCapacity; ++i)
		{
			ItemInfo invItem{};
			if (!pInterface->Inventory_GetItem(i, invItem))
			{
				//this slot is empty
				continue;
			}

			if (invItem.Type == eItemType::PISTOL)
			{
				//the proper index for a weapon was found
				pBlackboard->ChangeData("WeaponInventoryIndex", int(i));
				return true;
			}
		}
		//the weapon index was set by mistake at one point since there actually is no weapon in the inventory
		//set the index properly as invalid
		pBlackboard->ChangeData("WeaponInventoryIndex", -1);
		return false;
	}
};

class HasKilledZombieTransition final : public Elite::FSMTransition
{
public:
	HasKilledZombieTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		//check if agent has finished shooting
		int nrTimesToShoot{};
		pBlackboard->GetData("NrTimesToShoot", nrTimesToShoot);
		return (nrTimesToShoot == 0);
	}
};

class SeesHouseTransition final : public Elite::FSMTransition
{
public:
	SeesHouseTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		std::vector<HouseInfo> housesVect{};
		pBlackboard->GetData("HousesInFOV", housesVect);

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
			bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
			if (!isDataAvailable)
			{
				return false;
			}
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
		pBlackboard->GetData("EntitiesInFOV", entityVect);

		const int size{ int(entityVect.size()) };
		for (int i{ 0 }; i < size; ++i)
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
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}

		TargetData fleeTarget{};
		pBlackboard->GetData("Target", fleeTarget);

		const float requiredDistance{ 40.f };
		if (DistanceSquared(fleeTarget.Position, pInterface->Agent_GetInfo().Position) >= requiredDistance * requiredDistance)
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
		if (DistanceSquared(targetHouse.Center, pInterface->Agent_GetInfo().Position) <= nearbyRange * nearbyRange)
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
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}

		EntityInfo targetItem{};
		pBlackboard->GetData("TargetItem", targetItem);	
		AgentInfo agentInfo{ pInterface->Agent_GetInfo() };
		
		const float nearbyRange{ 1.f };
		if (DistanceSquared(targetItem.Location, agentInfo.Position) <= nearbyRange * nearbyRange)
		{
			return true;
		}
		return false;
	}
};

class HasLeftWorldTransition final : public Elite::FSMTransition
{
public:
	HasLeftWorldTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}

		//check if agent is outside world
		WorldInfo worldInfo{ pInterface->World_GetInfo() };
		Elite::Vector2 agentPos{ pInterface->Agent_GetInfo().Position };
		const float worldSize{ 190.f }; //use a separate value because the given world dimensions are too big, agent gets lost easily
		bool isOutsideWorldX{ agentPos.x < worldInfo.Center.x - worldSize || agentPos.x > worldInfo.Center.x + worldSize };
		bool isOutsideWorldY{ agentPos.y < worldInfo.Center.y - worldSize || agentPos.y > worldInfo.Center.y + worldSize };
		if (isOutsideWorldX || isOutsideWorldY)
		{		
			return true;
			
		}

		return false;		
	}
};

class IsAtWorldCenterTransition final : public Elite::FSMTransition
{
public:
	IsAtWorldCenterTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}

		WorldInfo worldInfo{ pInterface->World_GetInfo() };
		Elite::Vector2 agentPos{ pInterface->Agent_GetInfo().Position };

		const float nearbyRange{ 10.f };
		if (Elite::DistanceSquared(worldInfo.Center, agentPos) <= nearbyRange * nearbyRange)
		{
			return true;
		}
		return false;
	}
};

class SeesPurgeZoneTransition final : public Elite::FSMTransition
{
public:
	SeesPurgeZoneTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}

		std::vector<EntityInfo> entityVect{};
		pBlackboard->GetData("EntitiesInFOV", entityVect);

		if (entityVect.empty())
		{
			return false;
		}

		const int size{ int(entityVect.size()) };
		for (int i{ 0 }; i < size; ++i)
		{
			EntityInfo currentInfo{ entityVect[i] };
			if (currentInfo.Type == eEntityType::PURGEZONE)
			{
				PurgeZoneInfo zoneInfo{};
				pInterface->PurgeZone_GetInfo(currentInfo, zoneInfo);
				pBlackboard->ChangeData("TargetPurgeZone", zoneInfo);
				return true;
			}
		}

		return false;
	}
	
};

class HasLeftPurgeZoneTransition final : public Elite::FSMTransition
{
public:
	HasLeftPurgeZoneTransition() : FSMTransition() {};
	virtual bool ToTransition(Blackboard* pBlackboard) const override
	{
		IExamInterface* pInterface{};
		bool isDataAvailable = pBlackboard->GetData("Interface", pInterface);
		if (!isDataAvailable)
		{
			return false;
		}
		
		PurgeZoneInfo zoneInfo{};
		if (!pBlackboard->GetData("TargetPurgeZone", zoneInfo))
		{
			return false;
		}
		Elite::Vector2 agentPos{pInterface->Agent_GetInfo().Position};

		const float extraBufferDistance{ 5.f };
		if (Elite::DistanceSquared(zoneInfo.Center, agentPos) > (zoneInfo.Radius * zoneInfo.Radius) + 5.f)
		{
			return true;
		}
		
		return false;
	}
};
#endif