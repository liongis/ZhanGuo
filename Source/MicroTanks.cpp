#include "MicroTanks.h"
#include "UnitUtil.h"
#include "CombatCommander.h"

using namespace UAlbertaBot;

MicroTanks::MicroTanks() 
{ 
}

void MicroTanks::executeMicro(const BWAPI::Unitset & targets) 
{
	const BWAPI::Unitset & tanks = getUnits();
	int frame = BWAPI::Broodwar->getFrameCount();

	// figure out targets
	BWAPI::Unitset tankTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(tankTargets, tankTargets.end()), 
		[](BWAPI::Unit u){ 
		return u->isVisible() && !u->isFlying(); 
	});

	int numTankTargets = tankTargets.size();
	int numTanks = tanks.size();
    
    int siegeTankRange = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 32;
    bool haveSiege = BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode);

	BWAPI::Position maxPosition;
	BWAPI::Position defensivePosition = InformationManager::Instance().getDefensivePosition();
	BWAPI::Position mainPoint = InformationManager::Instance().getMyMainBaseLocation()->getPosition();

	int maxDist = 99999;

	int numUnsiege = 0;//收起支架的坦克数
	int numSiege = 0;//架起坦克数
	int numTankes = tanks.size();

	for (const auto tank : tanks)
	{
		if (tank->canUnsiege()) {
			++numSiege;
		}
		else {
			++numUnsiege;
		}
	}

	if (numTankes < 6) {
		numUnsiege = 0;
	}

	for (const auto tank : tanks)
	{
		if (!tank->isSieged()) {
			if (unitShouldRetreat(tank, targets)) continue;
		}

		if (order.isScoutOrder() && tank->getDistance(InformationManager::Instance().getMyMainBaseLocation()->getPosition()) > 10 * 32) {
			if (retreatUnit[tank] && frame < retreatUnit[tank]) continue;
		}

		if (unitNearEnemy(tank) && InformationManager::Instance().getEnemyMainBaseLocation()) {
			int dist = MapTools::Instance().getGroundTileDistance(tank->getPosition(), InformationManager::Instance().getEnemyMainBaseLocation()->getPosition());
			if (dist < maxDist) {
				maxDist = dist;
				maxPosition = tank->getPosition();
			}
		}

		//if (keepSafeDistance(tank)) continue;

		bool tankNearChokepoint = false;
		bool wideNearChokepoint = true;//宽路口

		/*
		for (auto & choke : BWTA::getChokepoints())
		{
			if (choke->getCenter().getDistance(tank->getPosition()) < 3 * 32)
			{
				tankNearChokepoint = true;

				if (choke->getWidth() < 3 * 32) {
					wideNearChokepoint = false;
				}
				break;
			}
		}
		*/

		//bool isNearEnemy = unitNearEnemy(tank, siegeTankRange + 3 * 32);
		bool isNearEnemy = false;
		BWAPI::Unit nearEnemy = tank->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack, siegeTankRange + 2 * 32);
		if (nearEnemy) {
			isNearEnemy = true;
		}

		/*
		if (isNearEnemy && tank->canSiege()) {
			tank->siege();
			continue;
		}
		*/

		if (order.isCombatOrder())
		{
			if (!tankTargets.empty())
			{

				BWAPI::Unit target = getTarget(tank, tankTargets);

				if (keepDefendOrder(tank, target)) continue;

				if (!BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode) && !tank->isInWeaponRange(target)) continue;

				if (order.getType() == SquadOrderTypes::Drop) {
					Micro::AttackUnit(tank, target);
					continue;
				}

				if (target && Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(tank->getPosition(), tank->getTargetPosition(), BWAPI::Colors::Purple);
				}

				//是否围攻
				bool shouldSiege = !tankNearChokepoint;

				// Don't siege to fight buildings, unless they can shoot back.
				if (target &&
					target->getType().isBuilding() &&
					target->getType().groundWeapon() == BWAPI::WeaponTypes::None &&
					target->getType() != BWAPI::UnitTypes::Terran_Bunker)
				{
					shouldSiege = false;
				}

				// Also don't siege for spider mines.
				else if (target && target->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
				{
					shouldSiege = false;
				}

				// Unsiege for a target which is too close.
				//Unsiege 的目标太近了。

				bool shouldUnsiege = target && tank->getDistance(target) < 4 * 32;

				BWAPI::Unitset alliesUnit = tank->getUnitsInRadius(4 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Marine
					|| BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture
					|| BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode
					|| BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);

				if (!alliesUnit.empty()) {
					shouldUnsiege = false;
					shouldSiege = true;
				}

				//如果周边有雷，则不架
				BWAPI::Unitset spiderMineUnits = tank->getUnitsInRadius(3 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine);
				if (spiderMineUnits.size() > 0) {
					shouldSiege = false;
				}

				/*
				for (auto spiderMine : spiderMineUnits) {
					if (!tank->isSieged()) {
						tank->attack(spiderMine);
						break;
					}
				}
				*/

				if (target->isMoving()) {
					siegeTankRange += 4 * 32;
				}

				if (target &&
					tank->getDistance(target) < siegeTankRange &&
					shouldSiege &&
					!shouldUnsiege &&
					tank->canSiege() && isNearEnemy)// && wideNearChokepoint
				{
					tank->siege();
					continue;
				}
				else if (tank->canUnsiege() && (!target || tank->getDistance(target) > siegeTankRange || shouldUnsiege))
				{
					if (!tank->isInWeaponRange(target) && tank->getGroundWeaponCooldown() > 0 && alliesUnit.size() < 3) {
						if (tank->isSieged())
						{
							if (!isNearEnemy) {
								tank->unsiege();
								continue;
							}
						}
						/*
						if (numUnsiege <= numSiege && numSiege > 2 && !isNearEnemy) {
							tank->unsiege();
							++numUnsiege;
						}
						*/
					}
				}

				if (tank->isSieged())
				{
					if (tank->getDistance(target) >= siegeTankRange + 1 * 32 &&
						target->getAngle() != tank->getAngle() && !target->isMoving() && !target->getType().isBuilding())
					{
						if (!isNearEnemy) {
							tank->unsiege();
							continue;
						}
						/*
						if (numSiege > numUnsiege || numUnsiege < 4) {
						tank->unsiege();
						++numUnsiege;
						}
						*/
					}
					/*
					if (target->isVisible()) {
						Micro::AttackUnit(tank, target);
					}
					else {
						Micro::AttackMove(tank, target->getPosition());
					}
					*/
				}
				else
				{
					Micro::KiteTarget(tank, target);
				}
			}
			// if there are no targets
			else
			{
				// if we're not near the order position
				if (tank->getDistance(order.getPosition()) >= siegeTankRange + 1 * 32)
				{
					if (tank->isSieged())
					{
						if (!isNearEnemy) {
							tank->unsiege();
							continue;
						}
					}
					else
					{
						// move to it
						Micro::AttackMove(tank, order.getPosition());
						continue;
					}
				}
				else {
					if (tank->getDistance(order.getPosition()) < 3 * 32 && tank->canSiege())
					{
						tank->siege();
						continue;
					}
				}
			}
		}
		else {
			// if we're not near the order position
			if (tank->getDistance(order.getPosition()) >= siegeTankRange + 1 * 32)
			{
				if (tank->isSieged())
				{
					if (!isNearEnemy) {
						tank->unsiege();
						continue;
					}
				}
				else
				{
					// move to it
					Micro::AttackMove(tank, order.getPosition());
					continue;
				}
			}
			else {
				if (tank->getDistance(order.getPosition()) < 3 * 32 && tank->canSiege())
				{
					tank->siege();
					continue;
				}
			}
		}

		if (!isNearEnemy && tank->isSieged() && tank->getDistance(order.getPosition()) > 6 * 32) {
			tank->unsiege();
			continue;
		}

	}

	if (maxPosition) {
		InformationManager::Instance().setNearEnemyTankPosition(maxPosition);
		//InformationManager::Instance().setDefensivePosition(maxPosition);
	}
}

BWAPI::Unit MicroTanks::getTarget(BWAPI::Unit tank, const BWAPI::Unitset & targets)
{
	int frame = BWAPI::Broodwar->getFrameCount();

	int bestScore = -999999;
    int highPriority = 0;
	int closestDist = 99999;
	BWAPI::Unit closestTarget = nullptr;

    int siegeTankRange = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 32;
    BWAPI::Unitset targetsInSiegeRange;

    for (const auto target : targets)
    {
        if (target->getDistance(tank) < siegeTankRange && !target->isFlying())
        {
            targetsInSiegeRange.insert(target);
        }
    }

    const BWAPI::Unitset & newTargets = targetsInSiegeRange.empty() ? targets : targetsInSiegeRange;

    // check first for units that are in range of our attack that can cause damage
    // choose the highest priority one from them at the lowest health
    for (const auto target : newTargets)
    {
		BWAPI::UnitType targetType = target->getType();

        if (target->isFlying())
        {
            continue;
        }

		if (frame < 9 * 60 * 24 && targetType.isBuilding() && !targetType.canAttack()) continue;

        int distance = tank->getDistance(target);
        int priority = getAttackPriority(tank, target);
		int toGoal = target->getDistance(order.getPosition());  // 0..map size in pixels

		int score = 5 * 32 * priority - (distance * 2) - toGoal / 2;

		if (tank->isInWeaponRange(target))
		{
			score += 4 * 32;
		}

		// Prefer targets that are already hurt.
		if (!target->getType().isBuilding() && target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() <= 5)
		{
			score += 32;
		}

		if (!target->getType().isBuilding() && target->getHitPoints() < target->getType().maxHitPoints() && target->getHitPoints() > 0 && target->getType().maxHitPoints() > 0)
		{
			int hit = (target->getHitPoints() / target->getType().maxHitPoints());
			if (hit > 0) {
				score += 20 / hit;
			}
		}

		if (tank->isSieged()) {
			//如果周围敌方部队多
			BWAPI::Unitset largeEnemy = target->getUnitsInRadius(2 * 32, BWAPI::Filter::IsEnemy);
			score += 20 * largeEnemy.size();

			//如果有我方部队
			largeEnemy = target->getUnitsInRadius(3 * 32, BWAPI::Filter::IsOwned);
			score -= 20 * largeEnemy.size() * 2;
		}

		if (target->isUnderAttack()) {
			score += 10 * 32;
		}

		//先攻击有进攻力的单位
		if (target->canAttack()) {
			score += 10 * 32;
		}

		if (target->getType().isBuilding()) {
			score -= 8 * 32;
		}

		if (InformationManager::Instance().getAttackTargets(target) > target->getHitPoints() + target->getShields()) {
			score -= 20 * 32;
			//score = 0;
		}

		//if (!closestTarget || (priority > highPriority) || (priority == highPriority && distance < closestDist))
		if (score > bestScore)
		{
			closestDist = distance;
			highPriority = priority;
			closestTarget = target;
		}       
    }

	if (closestTarget) {
		InformationManager::Instance().setAttackTargets(closestTarget, BWAPI::Broodwar->getDamageFrom(tank->getType(), closestTarget->getType()));
	}

	return closestTarget;
}

// Only targets that the tank can potentially attack go into the target set.
//只有坦克可能攻击的目标进入目标集合。
int MicroTanks::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target) 
{
	BWAPI::UnitType rangedType = rangedUnit->getType();
	BWAPI::UnitType targetType = target->getType();

    if (target->getType() == BWAPI::UnitTypes::Zerg_Larva || target->getType() == BWAPI::UnitTypes::Zerg_Egg)
    {
        return 0;
    }

	// The most dangerous enemy units.
	//最危险的敌人单位。
	if (targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
		targetType == BWAPI::UnitTypes::Zerg_Lurker ||
		targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Reaver ||
		targetType == BWAPI::UnitTypes::Zerg_Infested_Terran)
	{
		return 12;
	}

	if (target->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony)
	{
		return 11;
	}

	if (target->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon) {
		return 11;
	}

	if (target->getType() == BWAPI::UnitTypes::Protoss_Dragoon) {
		return 10;
	}

	if (target->getType() == BWAPI::UnitTypes::Protoss_Zealot) {
		return 4;
	}

    // if the target is building something near our base something is fishy
	//如果目标是建立在我们基地附近的东西是可疑的东西
    BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
    if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()) && target->getDistance(ourBasePosition) < 1200)
    {
        return 12;
    }

    if (target->getType().isBuilding() && (target->isCompleted() || target->isBeingConstructed()) && target->getDistance(ourBasePosition) < 1200)
    {
        return 12;
    }

	if (target->getType().isWorker()) {
		return 10;
	}

	bool isThreat = UnitUtil::TypeCanAttackGround(targetType);    // includes bunkers
	if (target->getType().isWorker())
	{
		isThreat = false;
	}
	// something that can attack us or aid in combat
    if (isThreat)
    {
        return 11;
    }
	// next priority is any unit on the ground
	if (!targetType.isBuilding()) 
	{
  		return 9;
	}

    // next is special buildings
	if (targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
	{
		return 6;
	}
	if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool)
	{
		return 5;
	}
	if (targetType == BWAPI::UnitTypes::Protoss_Pylon || targetType == BWAPI::UnitTypes::Protoss_Templar_Archives)
	{
		return 5;
	}

	// any buildings that cost gas
	if (targetType.gasPrice() > 0)
	{
		return 4;
	}
	if (targetType.mineralPrice() > 0)
	{
		return 3;
	}

	// then everything else
	return 1;
}
