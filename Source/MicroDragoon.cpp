#include "MicroDragoon.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

//龙骑微操
MicroDragoon::MicroDragoon()
{ 
}

void MicroDragoon::executeMicro(const BWAPI::Unitset & targets) 
{
	assignTargets(targets);
}

void MicroDragoon::assignTargets(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & rangedUnits = getUnits();
	int frame = BWAPI::Broodwar->getFrameCount();

	// The set of potential targets.
	BWAPI::Unitset rangedUnitTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(rangedUnitTargets, rangedUnitTargets.end()),
		[](BWAPI::Unit u) {
		return
			u->isVisible() &&
			u->isDetected() &&
			u->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			//u->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!u->isStasised();
	});

    for (const auto rangedUnit : rangedUnits)
	{
		// Carriers stay at home until they have enough interceptors to be useful,
		// or retreat toward home to rebuild them if they run low.
		// On attack-move so that they're not helpless, but that can cause problems too....
		// Potentially useful for other units.
		// NOTE Regrouping can cause the carriers to move away from home.
		//承运人呆在家里, 直到他们有足够的拦截器是有用的,
		//或撤退到家园重建他们, 如果他们运行低。
		//攻击 - 移动, 使他们不是无奈, 但也可能导致问题......。
		//对其他单位可能有用。
		//注意重组可能导致运营商离开家。
		/*
		if (stayHomeUntilReady(rangedUnit))
		{
			BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
			Micro::AttackMove(rangedUnit, fleeTo);
			continue;
		}
		*/

		//if (keepSafeDistance(rangedUnit, targets)) continue;

		//if (unstickStuckUnit(rangedUnit)) continue;
		if (retreatUnit[rangedUnit] && frame < retreatUnit[rangedUnit]) continue;

		//if (keepSafeDistance(meleeUnit, targets)) continue;
		BWAPI::Unit target = rangedUnit->getOrderTarget();

		if (target && target->exists()) {
			if (target->getType().isBuilding() && !target->canAttack()) {
				target = getTarget(rangedUnit, rangedUnitTargets);
			}
		} else {
			target = getTarget(rangedUnit, rangedUnitTargets);
		}

		BWAPI::Unit nearEnemy = rangedUnit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack, rangedUnit->getType().sightRange());
		if (rangedUnit->getDistance(target) > rangedUnit->getDistance(nearEnemy)) {
			target = nearEnemy;
		}

		//是否有地雷
		nearEnemy = rangedUnit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine);
		if (nearEnemy && rangedUnit->isInWeaponRange(nearEnemy)) {
			target = nearEnemy;
			Micro::AttackUnit(rangedUnit, target);
			continue;
		}

		if (order.isCombatOrder())
        {
			// If a target can be found.
			//BWAPI::Unit target = getTarget(rangedUnit, rangedUnitTargets);
			if (target && target->exists())
			{
				/*
				if (isShouldRetreat(rangedUnit, rangedUnitTargets)) {
					BWAPI::Position fleePosition = getFleePosition(rangedUnit, target);
					Micro::RightClick(rangedUnit, fleePosition);
					continue;
				}
				*/

				if (Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(rangedUnit->getPosition(), rangedUnit->getTargetPosition(), BWAPI::Colors::Purple);
				}

				// attack it
				if (target->getType().isBuilding() || 
					target->getType().groundWeapon().maxRange() >= rangedUnit->getType().groundWeapon().maxRange() ||
					target->getType().isWorker() ||
					target->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine
					) {
					Micro::AttackUnit(rangedUnit, target);
				}
				else {
					if (rangedUnit->getUnitsInRadius(4 * 32, BWAPI::Filter::IsOwned && BWAPI::Filter::CanAttack && BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Dragoon).size() > 6) {
						Micro::AttackUnit(rangedUnit, target);
					}
					else {
						Micro::KiteTarget(rangedUnit, target);
					}
				}
			}
			else
			{
				// No target found. If we're not near the order position, go there.
				if (rangedUnit->getDistance(order.getPosition()) > rangedUnit->getType().groundWeapon().maxRange())
				{
					Micro::AttackMove(rangedUnit, order.getPosition());
				}
			}
		}
	}
}

// This could return null if no target is worth attacking, but doesn't happen to.
//如果没有目标值得攻击, 则可能返回 null, 但不会发生。
BWAPI::Unit MicroDragoon::getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets)
{
	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;
	int bestPriority = -1;   // TODO debug only

	for (const auto target : targets)
	{
		if (InformationManager::Instance().isDukou) {
			BWAPI::Unit nearEnemy = rangedUnit->getClosestUnit(BWAPI::Filter::IsEnemy, rangedUnit->getType().sightRange());
			if (nearEnemy){
				return nearEnemy;
			}
		}

		if (!rangedUnit->hasPath(target)) continue;

		int priority = getAttackPriority(rangedUnit, target);     // 0..12
		int range    = rangedUnit->getDistance(target);           // 0..map size in pixels
		int toGoal   = target->getDistance(order.getPosition());  // 0..map size in pixels
		const int closerToGoal =										// positive if target is closer than us to the goal
			rangedUnit->getDistance(order.getPosition()) - target->getDistance(order.getPosition());

		// Skip targets that are too far away to worry about--outside tank range.
		if (range >= 12 * 32)
		{
			continue;
		}

		// Let's say that 1 priority step is worth 160 pixels (5 tiles).
		// We care about unit-target range and target-order position distance.
		int score = 20 * 32 * priority - range - toGoal / 2;

		// Adjust for special features.
		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		//调整特殊功能。
		//这可以调整相对的速度和方向, 所以我们不追逐什么, 我们不能赶上。

		const bool isThreat = UnitUtil::CanAttack(target, rangedUnit);   // may include workers as threats
		const bool canShootBack = isThreat && target->isInWeaponRange(rangedUnit);

		if (isThreat)
		{
			if (canShootBack)
			{
				score += 6 * 32;
			}
			else if (rangedUnit->isInWeaponRange(target))
			{
				score += 4 * 32;
			}
			else
			{
				score += 3 * 32;
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
		else if (target->getType().topSpeed() >= rangedUnit->getType().topSpeed())
		{
			score -= 5 * 32;
		}

		if (target->canBurrow()) {
			if (target->isBurrowed()) {
				score -= 48;
			}
			else {
				score += 48;
			}
		}
		
		// Prefer targets that are already hurt.
		if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() <= 5)
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

		// Prefer to hit air units that have acid spores on them from devourers.
		//更喜欢从吞食者上击中有酸性孢子的空气单位。
		if (target->getAcidSporeCount() > 0)
		{
			// Especially if we're a mutalisk with a bounce attack.
			//特别是如果我们是一个飞龙的攻击。
			if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
			{
				score += 16 * target->getAcidSporeCount();
			}
			else
			{
				score += 8 * target->getAcidSporeCount();
			}
		}

		BWAPI::DamageType damage = UnitUtil::GetWeapon(rangedUnit, target).damageType();
		if (damage == BWAPI::DamageTypes::Explosive)
		{
			if (target->getType().size() == BWAPI::UnitSizeTypes::Large)
			{
				score += 32;
			}
		}
		else if (damage == BWAPI::DamageTypes::Concussive)
		{
			if (target->getType().size() == BWAPI::UnitSizeTypes::Small)
			{
				score += 32;
			}
		}

		BWAPI::Position enemyChokePoint = InformationManager::Instance().getEnemyChokePoint();
		if (enemyChokePoint && enemyChokePoint.x != 0 && enemyChokePoint.y != 0) {
			//score += 10 * 32 - target->getDistance(enemyChokePoint);
			if (target->getType().isBuilding()) {
				score += 30 * 32 - target->getDistance(enemyChokePoint);
			}
		}

		/*
		if (!target->canAttack() && (BWAPI::UnitTypes::Terran_Bunker || BWAPI::UnitTypes::Zerg_Creep_Colony) && target->getDistance(enemyChokePoint) > 5 * 32) {
			score -= 20 * 32;
		}
		*/

		/*
		if (InformationManager::Instance().getAttackTargets(target) > (target->getHitPoints() + target->getShields()) * 0.5) {
			score += 20 * 12;
		}
		*/

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;

			bestPriority = priority;
		}
	}

	return bestScore > 0 && !shouldIgnoreTarget(rangedUnit, bestTarget) ? bestTarget : nullptr;
}

// get the attack priority of a target unit
//获取目标单元的攻击优先级
int MicroDragoon::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target) 
{
	const BWAPI::UnitType rangedType = rangedUnit->getType();
	const BWAPI::UnitType targetType = target->getType();

	// Exceptions if we're a ground unit.
	if (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine && !target->isBurrowed() ||
		targetType == BWAPI::UnitTypes::Zerg_Infested_Terran)
	{
		return 12;
	}

	/*
	if (target->getType().isBuilding() && !target->canAttack()) {
		BWAPI::Unitset canAttackUnit = target->getUnitsInRadius(5 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack);
		if (canAttackUnit.size() == 0) {
			return 12;
		}
	}
	*/

	// An addon other than a completed comsat is boring.
	// TODO should also check that it is attached
	//除已完成的通信卫星以外的附件是枯燥乏味的。
	//TODO 还应检查它是否已附加
	if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->isCompleted()))
	{
		return 1;
	}

    // if the target is building something near our base something is fishy
    BWAPI::Position ourBasePosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();
	if (target->getDistance(ourBasePosition) < 1000) {
		if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()))
		{
			return 12;
		}

		if (target->getType().isBuilding())
		{
			// This includes proxy buildings, which deserve high priority.
			// But when bases are close together, it can include innocent buildings.
			// We also don't want to disrupt priorities in case of proxy buildings
			// supported by units; we may want to target the units first.
			if (UnitUtil::CanAttackGround(target) || UnitUtil::CanAttackAir(target))
			{
				return 10;
			}

			return 8;
		}
	}
    
	if (rangedType.isFlyer()) {
		// Exceptions if we're a flyer (other than scourge, which is handled above).
		if (targetType == BWAPI::UnitTypes::Zerg_Scourge)
		{
			return 12;
		}
	}

	if (targetType == BWAPI::UnitTypes::Protoss_High_Templar)
	{
		return 12;
	}

	// Droppers are as bad as threats. They may be loaded and are often isolated and safer to attack.
	if (targetType == BWAPI::UnitTypes::Terran_Dropship ||
		targetType == BWAPI::UnitTypes::Protoss_Shuttle)
	{
		return 12;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_Reaver ||
		targetType == BWAPI::UnitTypes::Protoss_Arbiter || 
		targetType == BWAPI::UnitTypes::Protoss_Carrier)
	{
		return 11;
	}

	// Short circuit: Give bunkers a lower priority to reduce bunker obsession.
	if (targetType == BWAPI::UnitTypes::Terran_Bunker)
	{
		return 9;
	}

	// Threats can attack us. Exceptions: Workers are not threats.
	//威胁可以攻击我们。例外: 工人不是威胁。
	if (UnitUtil::CanAttack(targetType, rangedType) && !targetType.isWorker())
	{
		// Enemy unit which is far enough outside its range is lower priority than a worker.
		if (rangedUnit->getDistance(target) > 48 + UnitUtil::GetAttackRange(target, rangedUnit))
		{
			return 8;
		}

		return 10;
	}
	// Also as bad are other dangerous things.
	if (targetType == BWAPI::UnitTypes::Terran_Science_Vessel ||
		targetType == BWAPI::UnitTypes::Zerg_Scourge ||
		targetType == BWAPI::UnitTypes::Protoss_Observer)
	{
		return 10;
	}
	// Next are workers.
	if (targetType.isWorker()) 
	{
        if (rangedUnit->getType() == BWAPI::UnitTypes::Terran_Vulture)
        {
            return 11;
        }
		// Repairing or blocking a choke makes you critical.
		if (target->isRepairing() || unitNearChokepoint(target))
		{
			return 11;
		}
		// SCVs constructing are also important.
		if (target->isConstructing())
		{
			return 10;
		}

  		return 9;
	}

	// Important combat units that we may not have targeted above (esp. if we're a flyer).
	if (targetType == BWAPI::UnitTypes::Protoss_Carrier ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		return 8;
	}
	// Nydus canal is the most important building to kill.
	if (targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
	{
		return 9;
	}
	// Spellcasters are as important as key buildings.
	// Also remember to target other non-threat combat units.
	if (targetType.isSpellcaster() ||
		targetType.groundWeapon() != BWAPI::WeaponTypes::None ||
		targetType.airWeapon() != BWAPI::WeaponTypes::None)
	{
		return 7;
	}
	// Templar tech and spawning pool are more important.
	if (targetType == BWAPI::UnitTypes::Protoss_Templar_Archives)
	{
		return 7;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool)
	{
		return 7;
	}
	// Don't forget the nexus/cc/hatchery.
	if (targetType.isResourceDepot())
	{
		return 6;
	}
	if (targetType == BWAPI::UnitTypes::Protoss_Pylon)
	{
		return 5;
	}
	if (targetType == BWAPI::UnitTypes::Terran_Factory || targetType == BWAPI::UnitTypes::Terran_Armory)
	{
		return 5;
	}
	// Downgrade unfinished/unpowered buildings, with exceptions.
	if (targetType.isBuilding() &&
		(!target->isCompleted() || !target->isPowered()) &&
		!(	targetType.isResourceDepot() ||
			targetType.groundWeapon() != BWAPI::WeaponTypes::None ||
			targetType.airWeapon() != BWAPI::WeaponTypes::None ||
			targetType == BWAPI::UnitTypes::Terran_Bunker))
	{
		return 2;
	}
	if (targetType.gasPrice() > 0)
	{
		return 4;
	}
	if (targetType.mineralPrice() > 0)
	{
		return 3;
	}
	// Finally everything else.
	return 1;
}

// Should the unit stay (or return) home until ready to move out?
//该单位是否应留 (或返回) 回家, 直到准备迁出？
bool MicroDragoon::stayHomeUntilReady(const BWAPI::Unit u) const
{
	return
		u->getType() == BWAPI::UnitTypes::Protoss_Carrier && u->getInterceptorCount() < 4;
}
