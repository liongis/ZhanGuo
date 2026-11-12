#include "MicroDarkTemplar.h"
#include "UnitUtil.h"
#include "WorkerManager.h"

using namespace UAlbertaBot;

// Note: Melee units are ground units only. Scourge is a "ranged" unit.
//注: 近战单位仅为地面单位。自杀机是一个 "远程" 单位。
MicroDarkTemplar::MicroDarkTemplar() 
{ 
}

void MicroDarkTemplar::executeMicro(const BWAPI::Unitset & targets) 
{
	assignTargets(targets);
}

void MicroDarkTemplar::assignTargets(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & meleeUnits = getUnits();
	int frame = BWAPI::Broodwar->getFrameCount();

	BWAPI::Unitset meleeUnitTargets;

	for (const auto target : targets) 
	{
		if (target->isVisible() &&
			target->isDetected() &&
			!target->isFlying() &&
			target->getPosition().isValid() &&
			target->getType() != BWAPI::UnitTypes::Zerg_Larva && 
			//target->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!target->isStasised() &&
			!target->isUnderDisruptionWeb())             // melee unit can't attack under dweb
		{
			meleeUnitTargets.insert(target);
		}
	}

	for (const auto meleeUnit : meleeUnits)
	{
		if (retreatUnit[meleeUnit] && frame < retreatUnit[meleeUnit]) continue;

		BWAPI::Unit target = meleeUnit->getOrderTarget();

		if (target && target->exists()) {
			if (target->getType().isBuilding() && !target->canAttack()) {
				target = getTarget(meleeUnit, targets);
			}
		}
		else {
			target = getTarget(meleeUnit, targets);
		}

		BWAPI::Unit nearEnemy = meleeUnit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack && !BWAPI::Filter::IsFlying, meleeUnit->getType().sightRange());
		if (meleeUnit->getDistance(target) > meleeUnit->getDistance(nearEnemy)) {
			target = nearEnemy;
		}

		if (order.isCombatOrder())
		{
			//BWAPI::Unit target = getTarget(meleeUnit, meleeUnitTargets);

			if (target && target->exists()) {
				if (isShouldRetreat(meleeUnit, targets) && meleeUnit->getDistance(target) < 2 * 32) {
					BWAPI::Position fleePosition = getFleePosition(meleeUnit->getPosition(), target->getPosition(), 1 * 32); //getFleePosition(meleeUnit, target);
					Micro::RightClick(meleeUnit, fleePosition);
					continue;
				}

				Micro::AttackUnit(meleeUnit, target);

				// run away if we meet the retreat criterion
				//如果我们符合撤退标准就逃走吧
				/*
				if (meleeUnitShouldRetreat(meleeUnit, targets))
				{
					BWAPI::Unit shieldBattery = InformationManager::Instance().nearestShieldBattery(meleeUnit->getPosition());
					if (shieldBattery &&
						meleeUnit->getDistance(shieldBattery) < 400 &&
						shieldBattery->getEnergy() >= 10)
					{
						useShieldBattery(meleeUnit, shieldBattery);
						continue;
					}
					else
					{
						BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());

						if (meleeUnit->getDistance(fleeTo) < 4 * 32) {
							if (meleeUnit->getType().isWorker()) {
								removeUnit(meleeUnit);
								WorkerManager::Instance().finishedWithWorker(meleeUnit);
								return;
							}
						}
						else
						{
							Micro::Move(meleeUnit, fleeTo);
						}
						continue;
					}
				}
				*/
			}
			else if (meleeUnitTargets.empty())
			{
				// There are no targets. Move to the order position if not already close.
				if (meleeUnit->getDistance(order.getPosition()) > 3 * 32)
				{
					Micro::Move(meleeUnit, order.getPosition());
				}
			}
		}
		else {
			if (meleeUnit->getDistance(order.getPosition()) > meleeUnit->getType().sightRange())
			{
				Micro::AttackMove(meleeUnit, order.getPosition());
			}
		}

		if (Config::Debug::DrawUnitTargetInfo && meleeUnit->getTarget())
		{
			BWAPI::Broodwar->drawLineMap(meleeUnit->getPosition(), meleeUnit->getTargetPosition(), Config::Debug::ColorLineTarget);
		}
	}
}

// Choose a target from the set. Never return null!
BWAPI::Unit MicroDarkTemplar::getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;

	for (const auto target : targets)
	{
		if (InformationManager::Instance().isDukou) {
			BWAPI::Unit nearEnemy = meleeUnit->getClosestUnit(BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsFlying, meleeUnit->getType().sightRange());
			if (nearEnemy){
				return nearEnemy;
			}
		}

		int priority = getAttackPriority(meleeUnit, target);    // 0..12
		int range = meleeUnit->getDistance(target);             // 0..map size in pixels
		int toGoal = target->getDistance(order.getPosition());  // 0..map size in pixels

		// Let's say that 1 priority step is worth 64 pixels (2 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 20 * 32 * priority - range - toGoal / 2;

		if (range > 12 * 32) {
			continue;
		}

		if (BWAPI::Broodwar->getFrameCount() > 6 * 60 * 24) {
			score += (toGoal - range) * 12;
		}

		//敌人周围是否有反隐形单位
		BWAPI::Unitset unCloakUnit = target->getUnitsInRadius(meleeUnit->getType().sightRange(), BWAPI::Filter::IsEnemy && BWAPI::Filter::IsVisible &&
			(BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Overlord || BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Observer || BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Photon_Cannon || BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Science_Vessel || BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Missile_Turret));

		if (unCloakUnit.size() > 0) {
			score -= 40 * 32;
		}

		// Adjust for special features.
		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		if (meleeUnit->isInWeaponRange(target))
		{
			if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Ultralisk)//大牛
			{
				score += 12 * 32;   // because they're big and awkward
			}
			else
			{
				score += 10 * 32;
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
			score -= 10 * 32;
		}

		if (target->isUnderStorm())
		{
			score -= 20 * 32;
		}

		BWAPI::Position enemyChokePoint = InformationManager::Instance().getEnemyChokePoint();
		if (enemyChokePoint && enemyChokePoint.x != 0 && enemyChokePoint.y != 0) {
			//score += 10 * 32 - target->getDistance(enemyChokePoint);
			if (target->getType().isBuilding() && target->getType() != BWAPI::UnitTypes::Terran_Bunker) {
				score += 30 * 32 - target->getDistance(enemyChokePoint);
			}
		}

		if (!target->canAttack() && (target->getType() != BWAPI::UnitTypes::Terran_Bunker || target->getType() != BWAPI::UnitTypes::Zerg_Creep_Colony) && target->getDistance(enemyChokePoint) > 6 * 32) {
			score -= 20 * 32;
		}

		if (BWAPI::Broodwar->getFrameCount() < 6 * 60 * 24) {
			if (target->canAttack()) {
				score += 10 * 24;
			}
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

		/*
		if (InformationManager::Instance().getAttackTargets(target) > target->getHitPoints() + target->getShields()) {
			score -= 20 * 12;
			//score = 0;
		}
		*/

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	if (bestTarget) {
		InformationManager::Instance().setAttackTargets(bestTarget, BWAPI::Broodwar->getDamageFrom(meleeUnit->getType(), bestTarget->getType()));
	}

	return bestTarget;
}

// get the attack priority of a type
int MicroDarkTemplar::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit target) const
{
	BWAPI::UnitType targetType = target->getType();

	// Exceptions for dark templar.
	if (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
	{
		return 8;
	}
	
	if ((targetType == BWAPI::UnitTypes::Terran_Missile_Turret || targetType == BWAPI::UnitTypes::Terran_Comsat_Station) &&
		!target->isCompleted())
	{
		return 12;
	}

	if (targetType.isWorker())
	{
		return 10;
	}

	// Short circuit: Enemy unit which is far enough outside its range is lower priority than a worker.
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
		return 11;
	}

	// Medics and ordinary combat units. Include workers that are doing stuff.
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

	// next priority is bored workers and turrets
	if (targetType.isWorker() || targetType == BWAPI::UnitTypes::Terran_Missile_Turret)
	{
		return 7;
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
bool MicroDarkTemplar::meleeUnitShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
    // terran don't regen so it doesn't make sense to retreat
	if (meleeUnit->getType().isWorker() && meleeUnit->getHitPoints() < meleeUnit->getType().maxHitPoints() * 0.5 && meleeUnit->isUnderAttack()) {
		return true;
	}

    if (meleeUnit->getType().getRace() == BWAPI::Races::Terran)
    {
        return false;
    }

    // we don't want to retreat the melee unit if its shields or hit points are above the threshold set in the config file
    // set those values to zero if you never want the unit to retreat from combat individually
    if (meleeUnit->getShields() > Config::Micro::RetreatMeleeUnitShields || meleeUnit->getHitPoints() > Config::Micro::RetreatMeleeUnitHP)
    {
        return false;
    }

    // if there is a ranged enemy unit within attack range of this melee unit then we shouldn't bother retreating since it could fire and kill it anyway
    for (auto & unit : targets)
    {
        int groundWeaponRange = unit->getType().groundWeapon().maxRange();
        if (groundWeaponRange >= 64 && unit->getDistance(meleeUnit) < groundWeaponRange)
        {
            return false;
        }
    }

	// A broodling should not retreat since it is on a timer and regeneration does it no good.
	if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Broodling)
	{
		return false;
	}

	return true;
}
