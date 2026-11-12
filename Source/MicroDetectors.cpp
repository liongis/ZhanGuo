#include "MicroDetectors.h"
#include "MapTools.h"

using namespace UAlbertaBot;

//探测器
MicroDetectors::MicroDetectors()
	: unitClosestToEnemy(nullptr)
{
}

void MicroDetectors::executeMicro(const BWAPI::Unitset & targets) 
{
	const BWAPI::Unitset & detectorUnits = getUnits();
	int frame = BWAPI::Broodwar->getFrameCount();

	if (detectorUnits.empty())
	{
		return;
	}

	// NOTE targets is a list of nearby enemies.
	// Currently unused. Could use it to avoid enemy fire, among other possibilities.
	//注意目标是附近敌人的名单。
	//当前未使用。可以使用它来避免敌人的炮火, 除其他可能性。

	cloakedUnitMap.clear();
	BWAPI::Unitset cloakedUnits;

	// Find enemy cloaked units.
	// NOTE This code is unused, but it is potentially useful.
	//找到敌方隐形单位。
	//注意此代码未使用, 但可能有用。
	for (const auto enemy : BWAPI::Broodwar->enemies()) {
		for (const auto unit : enemy->getUnits())
		{
			if (unit->getType().hasPermanentCloak() ||     // dark templar, observer
				unit->getType().isCloakable() ||           // wraith, ghost
				unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Arbiter ||
				unit->isBurrowed() ||
				(unit->isVisible() && !unit->isDetected()))
			{
				cloakedUnits.insert(unit);
				cloakedUnitMap[unit] = false;
			}
		}
	}

	for (const auto detectorUnit : detectorUnits)
	{
		if (!detectorUnit) return;

		if (detectorUnit->getType() == BWAPI::UnitTypes::Terran_Science_Vessel) {
			if (detectorUnit->getEnergy() > 150 || detectorUnit->getHitPoints() < 75) {
				BWAPI::Unit target = detectorUnit->getClosestUnit(BWAPI::Filter::GetPlayer == BWAPI::Broodwar->self() &&
					(BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode || BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture), detectorUnit->getType().sightRange());
				if (target) {
					if(Micro::DefensiveMatrix(detectorUnit, target)) continue;
				}
			}

			BWAPI::Unit target = getTarget(detectorUnit, targets);
			if (target && target->getType().getRace() == BWAPI::Races::Zerg) {
				if(Micro::Irradiate(detectorUnit, target)) continue;
			}
		}

		if (keepElude(detectorUnit)) continue;

		if (retreatUnit[detectorUnit] && frame < retreatUnit[detectorUnit]) continue;

		BWAPI::Position mainAttackPoint = InformationManager::Instance().getMainAttackPoint();

		if (detectorUnit->getType() == BWAPI::UnitTypes::Protoss_Arbiter) {
			if (mainAttackPoint.isValid()) {
				Micro::AttackMove(detectorUnit, mainAttackPoint);
				continue;
			}
		}

		BWAPI::UnitCommand currentCommand(detectorUnit->getLastCommand());

		// if we've already told this unit to move to this position, ignore this command
		//如果我们已经告诉这个单位移动到这个位置, 忽略这个命令
		if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit) && (BWAPI::Broodwar->getFrameCount() < detectorUnit->getLastCommandFrame() + 12))
		{
			continue;
		}

		//if (keepSafeDistance(detectorUnit, targets)) continue;

		int closestDist = 100000;

		for (auto enemy : cloakedUnits) {
			int dist = detectorUnit->getDistance(enemy);
			if (dist < closestDist) {
				unitClosestToEnemy = enemy;
				closestDist = dist;
			}
		}

		if (unitClosestToEnemy) {
			cloakedUnits.erase(unitClosestToEnemy);
		}

		// Move the detector toward the squadmate closest to the enemy.
		//将探测器移向离敌人最近的 squadmate。
		if (unitClosestToEnemy  && unitClosestToEnemy->getType() != BWAPI::UnitTypes::Protoss_Observer)
		{
			BWAPI::Position enemyPosition = unitClosestToEnemy->getPosition();
			BWAPI::Position position = MapTools::Instance().getDistancePosition(enemyPosition, detectorUnit->getPosition(), detectorUnit->getType().sightRange() - 32);

			//如果隐形敌人区域没有我方部队
			BWAPI::Unitset ownedUnits = BWAPI::Broodwar->getUnitsInRadius(enemyPosition, 10 * 32, BWAPI::Filter::IsOwned && BWAPI::Filter::CanAttack);
			if (ownedUnits.size() == 0) {
				
				if (mainAttackPoint.isValid()) {
					position = mainAttackPoint;
				}
			}

			/*
			BWAPI::Unit nearEnemy = detectorUnit->getClosestUnit(BWAPI::Filter::IsEnemy && (BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Spore_Colony || BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Photon_Cannon || BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Missile_Turret));
			if (detectorUnit->isUnderAttack() || nearEnemy) {
				if (detectorUnit->getDistance(nearEnemy) < detectorUnit->getType().sightRange() - 1 * 32) {
					position = MapTools::Instance().getDistancePosition(nearEnemy->getPosition(), detectorUnit->getPosition(), detectorUnit->getType().sightRange() + 3 * 32);
				}
			}
			*/
			
			Micro::Move(detectorUnit, position);
			continue;
		}
	}
}

// Choose a target from the set. Never return null!
BWAPI::Unit MicroDetectors::getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	BWAPI::Unitset irradiateTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(irradiateTargets, irradiateTargets.end()),
		[](BWAPI::Unit u){ 
		return 
			!u->isIrradiated() && 
			!u->getType().isBuilding() &&
			u->getType().gasPrice() > 0 &&
			u->getType() != BWAPI::UnitTypes::Zerg_Overlord &&
			u->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			u->getType() != BWAPI::UnitTypes::Zerg_Zergling &&
			u->getType() != BWAPI::UnitTypes::Zerg_Larva;
	});

	for (const auto target : irradiateTargets)
	{
		int priority = getAttackPriority(meleeUnit, target);    // 0..12
		int range = meleeUnit->getDistance(target);             // 0..map size in pixels
		int toGoal = target->getDistance(order.getPosition());  // 0..map size in pixels

		// Let's say that 1 priority step is worth 64 pixels (2 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 2 * 32 * priority - range - toGoal / 2;

		// Adjust for special features.
		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		if (meleeUnit->isInWeaponRange(target))
		{
			if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Ultralisk)
			{
				score += 12 * 32;   // because they're big and awkward
			}
			else
			{
				score += 2 * 32;
			}
		}
		else if (!target->isMoving())
		{
			if (target->isSieged() ||
				target->getOrder() == BWAPI::Orders::Sieging ||
				target->getOrder() == BWAPI::Orders::Unsieging)
			{
				score += 48;
			}
			else
			{
				score += 24;
			}
		}
		else if (target->isBraking())
		{
			score += 16;
		}
		else if (target->getType().topSpeed() >= meleeUnit->getType().topSpeed())
		{
			score -= 2 * 32;
		}

		if (target->isIrradiated())
		{
			score -= 128;
		}

		// Prefer targets that are already hurt.
		if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() == 0)
		{
			score += 32;
		}
		else if (target->getHitPoints() < target->getType().maxHitPoints())
		{
			score += 24;
		}

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	return bestTarget;
}


// get the attack priority of a type
int MicroDetectors::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit target) const
{
	BWAPI::UnitType targetType = target->getType();

	if (attacker->getType().groundWeapon() == BWAPI::WeaponTypes::None) {
		return 0;
	}
	if (attacker->getType() == BWAPI::UnitTypes::Zerg_Overlord) {
		return 0;
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Zerg_Drone)
	{
		return 1;
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Zerg_Hydralisk)
	{
		return 9;
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Zerg_Lurker)
	{
		return 12;
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Zerg_Ultralisk)
	{
		return 12;
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Zerg_Zergling)
	{
		return 5;
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Zerg_Devourer)
	{
		return 12;
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Zerg_Guardian)
	{
		return 12;
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
	{
		return 12;
	}

	// then everything else
	return 0;
}

// NOTE Unused but potentially useful.
//注释未使用但可能有用。
BWAPI::Unit MicroDetectors::closestCloakedUnit(const BWAPI::Unitset & cloakedUnits, BWAPI::Unit detectorUnit)
{
	BWAPI::Unit closestCloaked = nullptr;
	double closestDist = 100000;

	for (const auto unit : cloakedUnits)
	{
		// if we haven't already assigned an detectorUnit to this cloaked unit
		if (!cloakedUnitMap[unit])
		{
			int dist = unit->getDistance(detectorUnit);

			if (dist < closestDist)
			{
				closestCloaked = unit;
				closestDist = dist;
			}
		}
	}

	return closestCloaked;
}