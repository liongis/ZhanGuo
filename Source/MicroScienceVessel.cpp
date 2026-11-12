#include "MicroScienceVessel.h"
#include "MapTools.h"
#include "MapPosition.h"

using namespace UAlbertaBot;

//探测器
MicroScienceVessel::MicroScienceVessel()
	: unitClosestToEnemy(nullptr)
{
}

void MicroScienceVessel::executeMicro(const BWAPI::Unitset & targets)
{
	const BWAPI::Unitset & detectorUnits = getUnits();

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
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType().hasPermanentCloak() ||     // dark templar, observer
			unit->getType().isCloakable() ||           // wraith, ghost
			unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
			unit->isBurrowed() ||
			(unit->isVisible() && !unit->isDetected()))
		{
			cloakedUnits.insert(unit);
			cloakedUnitMap[unit] = false;
		}
	}

	for (const auto detectorUnit : detectorUnits)
	{
		int dist = detectorUnit->getDistance(order.getPosition());

		if (keepSafeDistance(detectorUnit, targets)) continue;

		if (detectorUnit->isIdle() && dist < 4 * 32) {
			BWAPI::Position position = MapPosition::Instance().getRandomPosition(detectorUnit->getPosition(), 2 * 32);
			Micro::Move(detectorUnit, position);
		}

		if (detectorUnit->getType() == BWAPI::UnitTypes::Terran_Science_Vessel) {
			if (detectorUnit->getEnergy() > 150 || detectorUnit->getHitPoints() < 75) {
				BWAPI::Unit target = detectorUnit->getClosestUnit(BWAPI::Filter::GetPlayer == BWAPI::Broodwar->self() &&
					(BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode || BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture), detectorUnit->getType().sightRange());
				if (target) {
					Micro::DefensiveMatrix(detectorUnit, target);
					continue;
				}
			}

			if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg) {
				BWAPI::Unit target = getIrradiateTarget(detectorUnit, targets);
				if (target) {
					Micro::Irradiate(detectorUnit, target);
					continue;
				}
			}

			if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss) {
				BWAPI::Unit target = getEmpTarget(detectorUnit, targets);
				if (target) {
					Micro::EmpShockwave(detectorUnit, target->getPosition());
					continue;
				}
			}
		}

		BWAPI::UnitCommand currentCommand(detectorUnit->getLastCommand());

		// if we've already told this unit to move to this position, ignore this command
		//如果我们已经告诉这个单位移动到这个位置, 忽略这个命令
		if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit) && (BWAPI::Broodwar->getFrameCount() < detectorUnit->getLastCommandFrame() + 12))
		{
			continue;
		}

		int closestDist = 100000;

		for (auto enemy : cloakedUnits) {
			int dist = detectorUnit->getDistance(enemy);
			if (dist < closestDist) {
				unitClosestToEnemy = enemy;
				closestDist = dist;
			}
		}

		// Move the detector toward the squadmate closest to the enemy.
		//将探测器移向离敌人最近的 squadmate。
		if (unitClosestToEnemy && unitClosestToEnemy->getPosition().isValid() && unitClosestToEnemy->getType() != BWAPI::UnitTypes::Protoss_Observer)
		{
			BWAPI::Position pos(unitClosestToEnemy->getPosition());
			if (unitClosestToEnemy->getPlayer() != BWAPI::Broodwar->self()) {
				BWAPI::Position pos = MapTools::Instance().getDistancePosition(unitClosestToEnemy->getPosition(), detectorUnit->getPosition(), detectorUnit->getType().sightRange() - 3 * 32);
			}
			Micro::Move(detectorUnit, pos);
			continue;
		}

		if (dist > 3 * 32) {
			Micro::Move(detectorUnit, order.getPosition());
		} else {
			BWAPI::Position position = getFleePosition(order.getPosition(), detectorUnit->getPosition(), 6 * 32);
			Micro::Move(detectorUnit, position);
		}
	}
}


// Choose a target from the set. Never return null!
BWAPI::Unit MicroScienceVessel::getEmpTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	BWAPI::Unitset empTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(empTargets, empTargets.end()),
		[](BWAPI::Unit u){
		return
			u->getShields() > 10 &&
			!u->getType().isWorker() &&
			!u->getType().isBuilding();
	});

	for (const auto target : empTargets)
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
			score += 12 * 32;   // because they're big and awkward
		}

		if (target->getEnergy() > 72) {
			score += 24 * 32;
		}

		BWAPI::Unitset largeEnemy = target->getUnitsInRadius(6 * 32, BWAPI::Filter::IsEnemy);
		score += 20 * largeEnemy.size();

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	return bestTarget;
}

// Choose a target from the set. Never return null!
BWAPI::Unit MicroScienceVessel::getIrradiateTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	BWAPI::Unitset irradiateTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(irradiateTargets, irradiateTargets.end()),
		[](BWAPI::Unit u){
		return
			!u->isIrradiated() &&
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
		if (!target->isMoving())
		{
			if (target->isBurrowed() ||
				target->getOrder() == BWAPI::Orders::Burrowed ||
				target->getOrder() == BWAPI::Orders::Unburrowing)
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
			score -= 10 * 32;
		}

		// Prefer targets that are already hurt.
		if (target->getHitPoints() < target->getType().maxHitPoints())
		{
			score -= 10 * 32;
		}

		BWAPI::Unitset largeEnemy = target->getUnitsInRadius(2 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack);
		score += 20 * largeEnemy.size();

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	return bestTarget;
}


// get the attack priority of a type
int MicroScienceVessel::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit target) const
{
	BWAPI::UnitType targetType = target->getType();

	if (attacker->getShields() < 10){
		return 0;
	}

	if (attacker->getType() == BWAPI::UnitTypes::Protoss_Arbiter) {
		return 12;
	}

	if (attacker->getType() == BWAPI::UnitTypes::Protoss_High_Templar) {
		return 12;
	}

	if (attacker->getType() == BWAPI::UnitTypes::Protoss_Dragoon) {
		return 10;
	}

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
BWAPI::Unit MicroScienceVessel::closestCloakedUnit(const BWAPI::Unitset & cloakedUnits, BWAPI::Unit detectorUnit)
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