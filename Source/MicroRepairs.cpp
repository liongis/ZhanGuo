#include "MicroRepairs.h"
#include "UnitUtil.h"
#include "WorkerManager.h"

using namespace UAlbertaBot;

// Note: Melee units are ground units only. Scourge is a "ranged" unit.
//注: 近战单位仅为地面单位。自杀机是一个 "远程" 单位。
MicroRepairs::MicroRepairs() 
{ 
}

void MicroRepairs::executeMicro(const BWAPI::Unitset & targets) 
{
	assignTargets(targets);
}

void MicroRepairs::assignTargets(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & meleeUnits = getUnits();

	BWAPI::Unitset meleeUnitTargets;
	for (const auto target : targets) 
	{
		if (target->isVisible() &&
			target->isDetected() &&
			!target->isFlying() &&
			target->getPosition().isValid() &&
			target->getType() != BWAPI::UnitTypes::Zerg_Larva && 
			target->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!target->isStasised() &&
			!target->isUnderDisruptionWeb())             // melee unit can't attack under dweb
		{
			meleeUnitTargets.insert(target);
		}
	}

	for (const auto meleeUnit : meleeUnits)
	{
		if (meleeUnit->isBurrowed())
		{
			// For now, it would burrow only if irradiated. Leave it.
			continue;
		}
		
		if (order.isCombatOrder())
        {
			if (unstickStuckUnit(meleeUnit))
			{
				continue;
			}

			// run away if we meet the retreat criterion
            if (meleeUnitShouldRetreat(meleeUnit, targets))
            {
				BWAPI::Unit shieldBattery = InformationManager::Instance().nearestShieldBattery(meleeUnit->getPosition());
				if (shieldBattery &&
					meleeUnit->getDistance(shieldBattery) < 400 &&
					shieldBattery->getEnergy() >= 10)
				{
					useShieldBattery(meleeUnit, shieldBattery);
				}
				else
				{
					BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
					Micro::Move(meleeUnit, fleeTo);
				}
            }
			else if (meleeUnitTargets.empty())
			{
				// There are no targets. Move to the order position if not already close.
				if (meleeUnit->getDistance(order.getPosition()) > 6 * 32)
				{
					Micro::Move(meleeUnit, order.getPosition());
				}
			}
			else
			{
				// There are targets. Pick the best one and attack it.
				// NOTE We *always* choose a target. We can't decide none are worth it and bypass them.
				//      This causes a lot of needless distraction. :-(
				BWAPI::Unit target = getTarget(meleeUnit, meleeUnitTargets);
				//Micro::AttackUnit(meleeUnit, target);
			}
		}

		if (Config::Debug::DrawUnitTargetInfo && meleeUnit->getTarget())
		{
			BWAPI::Broodwar->drawLineMap(meleeUnit->getPosition(), meleeUnit->getTargetPosition(), Config::Debug::ColorLineTarget);
		}
	}
}

void MicroRepairs::update(const BWAPI::Position & center) {
	const BWAPI::Unitset & repairs = getUnits();
	//return;

	// create a set of all medic targets
	BWAPI::Unitset repairTargets;
	BWAPI::Unitset targets = BWAPI::Broodwar->getUnitsInRadius(center, 10 * 32, BWAPI::Filter::IsOwned);

	//for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	for (const auto unit : targets)
	{
		if (unit->isCompleted() && unit->getHitPoints() <= unit->getType().maxHitPoints() * 0.8 && unit->getType().isMechanical())
		{
			repairTargets.insert(unit);
		}
	}

	//if (repairTargets.size() == 0) return;

	//BWAPI::Unitset availableRepairs(repairs);
	//if (availableRepairs.size() == 0) return;

	int closestRepairDist = 99999;
	BWAPI::Unit target = nullptr;

	for (const auto repair : repairs)
	{

		if (meleeUnitShouldRetreat(repair, targets)) continue;

		target = getTarget(repair, repairTargets);

		// if we found a medic, send it to heal the target
		if (target)
		{
			//closestRepair->useTech(BWAPI::TechTypes::Healing, target);
			Micro::Repair(repair, target);

			//repairs.erase(repair);
			repairTargets.erase(target);
		}
		// otherwise we didn't find a medic which means they're all in use so break
		else
		{
			//Micro::AttackMove(repair, center);
		}
	}

	// the remaining medics should head toward the middle of the squad
	/*
	for (const auto Repair : availableRepairs)
	{
		Micro::AttackMove(Repair, center);
	}
	*/
}

// Choose a target from the set. Never return null!
BWAPI::Unit MicroRepairs::getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	for (const auto target : targets)
	{
		int priority = getRepairPriority(meleeUnit, target);    // 0..12
		int range = meleeUnit->getDistance(target);             // 0..map size in pixels
		int toGoal = target->getDistance(order.getPosition());  // 0..map size in pixels

		// Let's say that 1 priority step is worth 64 pixels (2 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 2 * 32 * priority - range - toGoal/2;

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	return bestTarget;
}

// get the attack priority of a type
int MicroRepairs::getRepairPriority(BWAPI::Unit attacker, BWAPI::Unit target) const
{
	BWAPI::UnitType targetType = target->getType();

	if (!target->isMoving()) {
		return 8;
	}

	if (targetType == BWAPI::UnitTypes::Terran_Bunker) {
		return 10;
	}

	if (targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
		return 10;
	}

	if (targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) {
		return 12;
	}

	if (targetType == BWAPI::UnitTypes::Terran_SCV) {
		return 8;
	}

	if (targetType == BWAPI::UnitTypes::Terran_Vulture) {
		return 10;
	}

	if (targetType == BWAPI::UnitTypes::Terran_Missile_Turret) {
		return 10;
	}

	return 1;
}

// get the attack priority of a type
int MicroRepairs::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit target) const
{
	BWAPI::UnitType targetType = target->getType();

	// Exceptions for dark templar.
	if (attacker->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar)
	{
		if (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
		{
			return 10;
		}
		if ((targetType == BWAPI::UnitTypes::Terran_Missile_Turret || targetType == BWAPI::UnitTypes::Terran_Comsat_Station) &&
			(BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) == 0))
		{
			return 9;
		}
		if (targetType.isWorker())
		{
			return 8;
		}
	}

	// Short circuit: Enemy unit which is far enough outside its range is lower priority than a Repair.
	int enemyRange = UnitUtil::GetAttackRange(target, attacker);
	if (enemyRange &&
		!targetType.isWorker() &&
		attacker->getDistance(target) > 32 + enemyRange)
	{
		return 8;
	}
	// Short circuit: Units before bunkers!
	if (targetType == BWAPI::UnitTypes::Terran_Bunker)
	{
		return 10;
	}
	// Medics and ordinary combat units. Include Repairs that are doing stuff.
	if (targetType == BWAPI::UnitTypes::Terran_Medic ||
		targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Reaver)
	{
		return 12;
	}
	if (targetType.groundWeapon() != BWAPI::WeaponTypes::None && !targetType.isWorker())
	{
		return 12;
	}
	if (targetType.isWorker() && (target->isRepairing() || target->isConstructing() || unitNearChokepoint(target)))
	{
		return 12;
	}
	// next priority is bored Repairs and turrets
	if (targetType.isWorker() || targetType == BWAPI::UnitTypes::Terran_Missile_Turret)
	{
		return 9;
	}
    // Buildings come under attack during free time, so they can be split into more levels.
	if (targetType == BWAPI::UnitTypes::Zerg_Spire || targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
	{
		return 6;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool ||
		targetType.isResourceDepot() ||
		targetType == BWAPI::UnitTypes::Protoss_Templar_Archives ||
		targetType.isSpellcaster())
	{
		return 5;
	}
	// Short circuit: Addons other than a completed comsat are worth almost nothing.
	// TODO should also check that it is attached
	if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->isCompleted()))
	{
		return 1;
	}
	// anything with a cost
	if (targetType.gasPrice() > 0 || targetType.mineralPrice() > 0)
	{
		return 3;
	}
	
	// then everything else
	return 1;
}

// Retreat hurt units to allow them to regenerate health (zerg) or shields (protoss).
//撤退伤害单位允许他们再生健康 (虫族) 或盾牌 (神族)。
bool MicroRepairs::meleeUnitShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	if ((meleeUnit->isUnderAttack() && meleeUnit->getHitPoints() < meleeUnit->getType().maxHitPoints() * 0.6) || meleeUnit->getHitPoints() < meleeUnit->getType().maxHitPoints() * 0.4) {
		BWAPI::Unit minerals = WorkerManager::Instance().findEscapeMinerals(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
		if (minerals) {
			//Micro::RightClick(meleeUnit, minerals);
			meleeUnit->rightClick(minerals);
			if (meleeUnit->getType().isWorker()) {
				WorkerManager::Instance().finishedWithWorker(meleeUnit);
			}
			
			return true;
		}
	}

	return false;
}
