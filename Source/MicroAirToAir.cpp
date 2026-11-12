#include "MicroAirToAir.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// The splash air-to-air units: Valkyries, corsairs, devourers.
//空对空单位，瓦格雷，海盗，吞噬者
MicroAirToAir::MicroAirToAir()
{ 
}

void MicroAirToAir::executeMicro(const BWAPI::Unitset & targets) 
{
	assignTargets(targets);
}

void MicroAirToAir::assignTargets(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & airUnits = getUnits();
	int frame = BWAPI::Broodwar->getFrameCount();

	// The set of potential targets.
	BWAPI::Unitset airTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(airTargets, airTargets.end()),
		[](BWAPI::Unit u) {
		return
			u->isFlying() &&
			u->isVisible() &&
			u->isDetected() &&
			!u->isStasised();
	});

    for (const auto airUnit : airUnits)
	{
		if (keepElude(airUnit)) continue;

		if (retreatUnit[airUnit] && frame < retreatUnit[airUnit]) continue;

		//if (keepSafeDistance(airUnit, targets)) continue;

		// Special case for irradiated devourers. Try not to endanger our other units.
		//辐照吞食者的特殊情况。尽量不要危害我们的其他单位。
		if (airUnit->isIrradiated() && airUnit->getType().getRace() == BWAPI::Races::Zerg)
		{
			if (airUnit->getDistance(order.getPosition()) < 300)
			{
				Micro::AttackMove(airUnit, order.getPosition());
			}
			else
			{
				Micro::Move(airUnit, order.getPosition());
			}
			continue;
		}

		if (order.isCombatOrder())
        {
			BWAPI::Unit target = getTarget(airUnit, airTargets);
			if (target)
			{
				// A target was found.
				if (Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(airUnit->getPosition(), airUnit->getTargetPosition(), BWAPI::Colors::Purple);
				}

				if (Config::Micro::KiteWithRangedUnits)
				{
					if (airUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk || 
						airUnit->getType() == BWAPI::UnitTypes::Terran_Vulture || 
						airUnit->getType() == BWAPI::UnitTypes::Terran_Wraith || 
						airUnit->getType() == BWAPI::UnitTypes::Protoss_Scout
						)
					{
						Micro::MutaDanceTarget(airUnit, target);
					}
					else
					{
						Micro::AttackUnit(airUnit, target);
					}
				}
			}
			else
			{
				// No target found. Go to the attack position.
				Micro::AttackMove(airUnit, order.getPosition());
			}
		}
	}
}

// This could return null if no target is worth attacking, but doesn't happen to.
BWAPI::Unit MicroAirToAir::getTarget(BWAPI::Unit airUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	for (const auto target : targets)
	{
		int priority = getAttackPriority(airUnit, target);     // 0..12
		int range    = airUnit->getDistance(target);           // 0..map size in pixels
		int toGoal   = target->getDistance(order.getPosition());  // 0..map size in pixels

		// Let's say that 1 priority step is worth 160 pixels (5 tiles).
		// We care about unit-target range and target-order position distance.
		//比方说, 1 优先级的步骤值160像素(5 块)。
		//我们关心单位目标范围和目标 - 顺序位置距离。
		int score = 5 * 32 * priority - range - toGoal / 2;

		// Adjust for special features.
		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		//调整特殊功能。
		//这可以调整相对的速度和方向, 所以我们不追逐什么, 我们不能赶上。
		if (airUnit->isInWeaponRange(target))
		{
			score += 4 * 32;
		}
		else if (!target->isMoving())
		{
			score += 24;
		}
		else if (target->isBraking())
		{
			score += 16;
		}
		else if (target->getType().topSpeed() >= airUnit->getType().topSpeed())
		{
			score -= 5 * 32;
		}
		
		// Prefer targets that are already hurt.
		if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() == 0)
		{
			score += 32;
		}

		if (target->getHitPoints() < target->getType().maxHitPoints() && target->getHitPoints() > 0 && target->getType().maxHitPoints() > 0)
		{
			int hit = (target->getHitPoints() / target->getType().maxHitPoints());
			if (hit > 0) {
				score += 20 / hit;
			}
		}

		// TODO prefer targets in groups, so they'll all get splashed

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}
	
	return bestTarget;
}

// get the attack priority of a target unit
int MicroAirToAir::getAttackPriority(BWAPI::Unit airUnit, BWAPI::Unit target) 
{
	const BWAPI::UnitType rangedType = airUnit->getType();
	const BWAPI::UnitType targetType = target->getType();

	// Devourers are different from the others.
	if (rangedType == BWAPI::UnitTypes::Zerg_Devourer)
	{
		if (targetType.isBuilding())
		{
			// A lifted building is less important.
			return 1;
		}
		if (targetType == BWAPI::UnitTypes::Zerg_Scourge)
		{
			// Devourers are not good at attacking scourge.
			return 9;
		}

		// Everything else is the same.
		return 10;
	}
	
	// The rest is for valkyries and corsairs.

	// Scourge are dangerous and are the worst.
	if (targetType == BWAPI::UnitTypes::Zerg_Scourge)
	{
		return 10;
	}

	// Threats can attack us back.
	if (UnitUtil::TypeCanAttackAir(targetType))    // includes carriers
	{
		// Enemy unit which is far enough outside its range is lower priority.
		if (airUnit->getDistance(target) > 64 + UnitUtil::GetAttackRange(target, airUnit))
		{
			return 8;
		}
		return 9;
	}

	// Certain other enemies are also bad.
	if (targetType == BWAPI::UnitTypes::Terran_Science_Vessel ||
		targetType == BWAPI::UnitTypes::Terran_Dropship ||
		targetType == BWAPI::UnitTypes::Protoss_Shuttle ||
		targetType == BWAPI::UnitTypes::Zerg_Overlord)
	{
		return 12;
	}

	if (targetType.isWorker()) {
		return 10;
	}

	// Flying buildings are less important than other units.
	if (targetType.isBuilding())
	{
		return 1;
	}

	// Other air units are a little less important.
	return 7;
}
