#include "MicroVultures.h"
#include "MapTools.h"
#include "UnitUtil.h"
#include "Random.h"
#include "CombatCommander.h"
#include "MapPosition.h"

using namespace UAlbertaBot;

//远程单位微操
MicroVultures::MicroVultures()
{ 
}

void MicroVultures::executeMicro(const BWAPI::Unitset & targets) 
{
	assignTargets(targets);
}

void MicroVultures::assignTargets(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & rangedUnits = getUnits();

	// The set of potential targets.
	BWAPI::Unitset rangedUnitTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(rangedUnitTargets, rangedUnitTargets.end()),
		[](BWAPI::Unit u) {
		return
			u->isVisible() &&
			u->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			u->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!u->isFlying() &&
			!u->isBurrowed() &&
			!u->isStasised();
	});

	BWAPI::Position initialPosition = InformationManager::Instance().getRallyPoint();
	int frame = BWAPI::Broodwar->getFrameCount();

	for (const auto rangedUnit : rangedUnits)
	{
		/*
		BWAPI::Unit nearEnemy = rangedUnit->getClosestUnit(BWAPI::Filter::CanAttack && BWAPI::Filter::IsEnemy);
		//for (auto target : rangedUnitTargets){
		if (nearEnemy) {
		int maxRange = nearEnemy->getType().groundWeapon().maxRange();
		if ((maxRange >= 5 * 32 && rangedUnit->getDistance(nearEnemy) <= maxRange) || rangedUnit->getDistance(nearEnemy) <= 2 * 32) {
		BWAPI::Position fleePosition = MapTools::Instance().getExtendedPosition(rangedUnit->getPosition(), nearEnemy->getPosition(), 10 * 32);
		rangedUnit->rightClick(fleePosition);
		return;
		}
		}
		*/

		if (unitShouldRetreat(rangedUnit, targets)) continue;

		BWAPI::Unit worker = rangedUnit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::IsWorker, rangedUnit->getType().sightRange());
		if (worker) {
			Micro::AttackUnit(rangedUnit, worker);
		}

		if (order.isScoutOrder() && rangedUnit->getDistance(InformationManager::Instance().getMyMainBaseLocation()->getPosition()) > 10 * 32) {
			if (retreatUnit[rangedUnit] && frame < retreatUnit[rangedUnit]) continue;
		}

		//是否有敌人接近
		BWAPI::Unit nearEnemy = nullptr;

		BWAPI::Unit target = getTarget(rangedUnit, rangedUnitTargets);
		if (keepSafeDistance(rangedUnit, targets)) continue;

		//if (keepDefendOrder(rangedUnit, target)) continue;

		if (unstickStuckUnit(rangedUnit)) continue;

		int spiderMineCount = rangedUnit->getSpiderMineCount();
		if (spiderMineCount == 3) {
			_spiderMineUnits[rangedUnit] = 3;
		}

		//是否需要埋雷
		BWAPI::UnitCommand currentCommand(rangedUnit->getLastCommand());

		if (currentCommand.getType() == BWAPI::UnitCommandTypes::Use_Tech_Position)
		{
			int dist = MapTools::Instance().getGroundTileDistance(rangedUnit->getPosition(), currentCommand.getTargetPosition());
			int time = dist / rangedUnit->getType().topSpeed();

			//if (BWAPI::Broodwar->getFrameCount() < BWAPI::Broodwar->getFrameCount() + time || dist > 4) continue;
			if (spiderMineCount == _spiderMineUnits[rangedUnit]) {
				continue;
			}
		}

		if (_spiderMineUnits[rangedUnit] && spiderMineCount < _spiderMineUnits[rangedUnit]) {
			_spiderMineUnits[rangedUnit] = spiderMineCount;
			retreatUnit[rangedUnit] = frame - 10;
		}

		BWAPI::Position position;

		if (order.isScoutOrder()) {
			BWAPI::Unitset nearEnemys;// = rangedUnit->getUnitsInRadius(rangedUnit->getType().sightRange(), BWAPI::Filter::CanAttack && BWAPI::Filter::IsEnemy && BWAPI::Filter::IsCompleted);
			MapGrid::Instance().getUnits(nearEnemys, rangedUnit->getPosition(), rangedUnit->getType().sightRange(), false, true);
			for (auto enemy : nearEnemys){
				if (enemy->getType().isWorker()) {
					Micro::AttackUnit(rangedUnit, enemy);
					break;
				}

				if (enemy->getType().isBuilding() && enemy->getType().groundWeapon() == BWAPI::WeaponTypes::None) {
					continue;
				}

				if (!enemy->isFlying() && enemy->getPosition().isValid()) {
					if (spiderMineCount > 0) {
						position = enemy->getPosition();
						position = MapPosition::Instance().getRandomPosition(position, 3 * 32);
						Micro::LaySpiderMine(rangedUnit, position);
						continue;
					}
				}

				int maxRange = enemy->getType().groundWeapon().maxRange();

				if (maxRange >= rangedUnit->getType().groundWeapon().maxRange() ||
					enemy->getType() == BWAPI::UnitTypes::Protoss_Dragoon ||
					enemy->getType() == BWAPI::UnitTypes::Zerg_Hydralisk){
					nearEnemy = enemy;
					break;
				}
			}

			if (nearEnemy) {
				InformationManager::Instance().setReconLocation(CombatCommander::Instance().getReconTarget(), 2 * 60 * 24);
				BWAPI::Position nextTarget = InformationManager::Instance().getReconLocation();
				CombatCommander::Instance().setReconTarget(nextTarget);
				rangedUnit->rightClick(nextTarget);
				retreatUnit[rangedUnit] = frame + Config::Micro::RetreatFrame;
				continue;
			}

			bool isLaySpiderMine = false;
			if (spiderMineCount > 0) {//&& BWAPI::Broodwar->getFrameCount() > rangedUnit->getLastCommandFrame() + 10
				if (order.isScoutOrder()) {
					if (InformationManager::Instance().getEnemyMainBaseLocation()) {

						//再在家门口埋雷
						position = BWAPI::Position(MapPosition::Instance().getMyDefensivePosition());
						if (rangedUnit->getDistance(position) < 6 * 32 && canLaySpiderMine(rangedUnit, position, 6 * 32, 6, true)) continue;
						/*
						if (BWAPI::Broodwar->getUnitsInRadius(position, 6 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine).size() < 3) {
						position = MapPosition::Instance().getRandomPosition(position, 6 * 32);
						if (rangedUnit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, position)) {
						Micro::LaySpiderMine(rangedUnit, position);
						continue;
						}
						}
						*/

						//先去敌人路口埋雷
						position = BWAPI::Position(MapPosition::Instance().getEnemyDefensivePosition());
						if (rangedUnit->getDistance(position) < 6 * 32 && canLaySpiderMine(rangedUnit, position, 6 * 32, 6, true)) continue;
						/*
						if (BWAPI::Broodwar->getUnitsInRadius(position, 6 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine).size() < 3) {
						position = MapPosition::Instance().getRandomPosition(position, 6 * 32);
						if (rangedUnit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, position)) {
						Micro::LaySpiderMine(rangedUnit, position);
						continue;
						}
						}
						*/

						//再在路中间埋雷
						position = BWAPI::Position(MapPosition::Instance().getEnemyDefensivePosition()) - BWAPI::Position(MapPosition::Instance().getMyDefensivePosition());
						if (rangedUnit->getDistance(position) < 6 * 32 && canLaySpiderMine(rangedUnit, position, 10 * 32, 6, true)) continue;
						/*
						if (BWAPI::Broodwar->getUnitsInRadius(position, 10 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine).size() < 6) {
						position = MapPosition::Instance().getRandomPosition(position, 6 * 32);
						if (rangedUnit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, position)) {
						Micro::LaySpiderMine(rangedUnit, position);
						continue;
						}
						}
						*/

						//敌方的扩张点
						std::vector<BWAPI::Position> tilePositions = MapPosition::Instance().getEnemyNextExpansionPosition();
						for (auto tilePosition : tilePositions) {
							BWAPI::Position position = tilePosition;
							if (rangedUnit->getDistance(position) < 10 * 32 && canLaySpiderMine(rangedUnit, position, 4 * 32, 2, true)) {
								isLaySpiderMine = true;
								break;
							}
							/*
							if (BWAPI::Broodwar->getUnitsInRadius(position, 4 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine).size() < 2) {
							if (rangedUnit->getDistance(position) < 6 * 32 && canLaySpiderMine(rangedUnit, position, 2 * 32, 1, false)){
							isLaySpiderMine = true;
							break;
							}
							}
							*/
						}

						if (isLaySpiderMine) continue;
					}

					//每个矿区埋雷
					/*
					for (BWTA::BaseLocation * startLocation : BWTA::getBaseLocations())
					{
						if (startLocation == InformationManager::Instance().getMyMainBaseLocation() || startLocation == InformationManager::Instance().getMyNaturalLocation()) continue;

						BWAPI::Position position = startLocation->getPosition();

						if (rangedUnit->getDistance(position) < 10 * 32 && canLaySpiderMine(rangedUnit, position, 2 * 32, 1, true)) {
							isLaySpiderMine = true;
							break;
						}
					}
					if (isLaySpiderMine) continue;
					*/

					//每个阻塞点附近
					BWAPI::Position position = rangedUnit->getPosition();
					if (initialPosition.getDistance(order.getPosition()) > 5 * 32) {
						int dist = rangedUnit->getDistance(order.getPosition());
						//if (rangedUnit->getDistance(order.getPosition()) > 5 * 32) {
						if (unitNearChokepoint(rangedUnit) ||
							(dist < 10 * 32 || dist > 2 * 32))
						{
							//if (rangedUnit->getDistance(position) < 6 * 32 && canLaySpiderMine(rangedUnit, position, 10 * 32, 6, true)) continue;

							/*
							if (rangedUnit->canUseTech(BWAPI::TechTypes::Spider_Mines, position)) {
								BWAPI::Unitset spiderMine = rangedUnit->getUnitsInRadius(5 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine);
								if (spiderMine.size() < 2 && position.getDistance(initialPosition) > initialPosition.getDistance(order.getPosition()) / 3) {
									position = MapPosition::Instance().getRandomPosition(position, 3 * 32);

									if (rangedUnit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, position)) {
										Micro::LaySpiderMine(rangedUnit, position);
										continue;
									}
								}
							}
							*/
						}
					}
				}
			}

			//如果到了矿区
			for (BWTA::BaseLocation * startLocation : BWTA::getBaseLocations())
			{
				if (startLocation == InformationManager::Instance().getMyMainBaseLocation() || startLocation == InformationManager::Instance().getMyNaturalLocation()) continue;

				BWAPI::Position position = startLocation->getPosition();
				position += BWAPI::Position(2 * 32, 1 * 32);

				if (rangedUnit->getDistance(position) < 12 * 32)
				{
					//如果有我方的建筑
					BWAPI::Unitset units = BWAPI::Broodwar->getUnitsInRadius(position, 3 * 32, BWAPI::Filter::IsOwned && BWAPI::Filter::IsBuilding);
					if (units.size() > 0) continue;

					//如果敌方建筑
					units = BWAPI::Broodwar->getUnitsInRadius(position, 4 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::IsBuilding);
					if (units.size() > 0) {
						position = MapPosition::Instance().getRandomPosition(position, 6 * 32);
					}

					if (canLaySpiderMine(rangedUnit, position, 6 * 32, 2, true)) {
						isLaySpiderMine = true;
						break;
					}

					BWAPI::Unit worker = rangedUnit->getClosestUnit(BWAPI::Filter::IsWorker && BWAPI::Filter::IsEnemy, rangedUnit->getType().sightRange());
					if (worker) {
						Micro::AttackUnit(rangedUnit, worker);
						retreatUnit[rangedUnit] = frame + Config::Micro::RetreatFrame;
					}
				}
			}

			if (isLaySpiderMine) continue;
		}

		//其它战斗命令
		if (order.isCombatOrder()) 
		{
			if (target)
			{
				/*
				if (order.isAttackOrder()) {
					if (keepTankDistance(rangedUnit, target->getPosition())) continue;
				}
				*/

				if (Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(rangedUnit->getPosition(), rangedUnit->getTargetPosition(), BWAPI::Colors::Purple);
				}
				int maxRange = target->getType().groundWeapon().maxRange();

				if (target->getType().isBuilding()) {
					Micro::AttackUnit(rangedUnit, target);
				}
				else if (target->getType().isWorker() && rangedUnit->getDistance(target) > 2 * 32) {
					Micro::AttackUnit(rangedUnit, target);
				}
				else {
					BWAPI::Unitset tanks = BWAPI::Broodwar->getUnitsInRadius(rangedUnit->getPosition(), 7 * 32,
						BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
						BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);

					if (tanks.size() > 1) {
						Micro::AttackUnit(rangedUnit, target);
					}
					else {
						Micro::MutaDanceTarget(rangedUnit, target);
					}
				}
			}
			else {
				if (rangedUnit->getDistance(order.getPosition()) >  rangedUnit->getType().sightRange())
				{
					Micro::AttackMove(rangedUnit, order.getPosition());
				}
			}
		}
		else {
			if (rangedUnit->getDistance(order.getPosition()) >  rangedUnit->getType().sightRange())
			{
				Micro::AttackMove(rangedUnit, order.getPosition());
			}
		}
	}
}

//unit:雷车，position:位置,dist:范围，num：雷的数量，random：是否随机埋雷
bool MicroVultures::canLaySpiderMine(BWAPI::Unit unit, BWAPI::Position position, int dist, int num, bool random) {
	if (BWAPI::Broodwar->getUnitsInRadius(position, dist, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine).size() < num) {
		if (random) {
			position = MapPosition::Instance().getRandomPosition(position, dist);
		}

		if (unit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, position)) {
			Micro::LaySpiderMine(unit, position);
			return true;
		}
	}

	return false;
}

// This could return null if no target is worth attacking, but doesn't happen to.
//如果没有目标值得攻击, 则可能返回 null, 但不会发生。
BWAPI::Unit MicroVultures::getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets)
{
	int frame = BWAPI::Broodwar->getFrameCount();

	int bestScore = -999999;
	BWAPI::Unit bestTarget = nullptr;
	int bestPriority = -1;   // TODO debug only

	BWAPI::Unitset vultureTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(vultureTargets, vultureTargets.end()),
		[](BWAPI::Unit u){ return u->isVisible() && !u->isFlying(); });

	for (const auto target : vultureTargets)
	{
		BWAPI::UnitType targetType = target->getType();

		if (frame < 9 * 60 * 24 && targetType.isBuilding() && !targetType.canAttack()) continue;

		int range = rangedUnit->getDistance(target);           // 0..map size in pixels
		if (range > rangedUnit->getType().groundWeapon().maxRange()) continue;

		int priority = getAttackPriority(rangedUnit, target);     // 0..12
		int toGoal   = target->getDistance(order.getPosition());  // 0..map size in pixels
		
		// Let's say that 1 priority step is worth 160 pixels (5 tiles).
		// We care about unit-target range and target-order position distance.
		//比方说, 1 优先级的步骤值160像素 (5 块)。
		//我们关心单位目标范围和目标 - 顺序位置距离。
		int score = 5 * 32 * priority - range - toGoal / 2;

		// Adjust for special features.
		// This could adjust for relative speed and direction, so that we don't chase what we can't catch.
		//调整特殊功能。
		//这可以调整相对的速度和方向, 所以我们不追逐什么, 我们不能赶上。
		if (rangedUnit->isInWeaponRange(target))
		{
			score += 4 * 32;
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

		if (target->isUnderAttack()) {
			score += 4 * 32;
		}

		if (target->getType().isBuilding()) {
			score -= 8 * 32;
		}

		//score += 4 * (rangedUnit->getType().groundWeapon().maxRange() - target->getType().groundWeapon().maxRange());

		if (InformationManager::Instance().getAttackTargets(target) > target->getHitPoints() + target->getShields()) {
			score -= 20 * 32;
			//score = 0;
		}

		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;

			bestPriority = priority;
		}
	}

	if (bestTarget) {
		InformationManager::Instance().setAttackTargets(bestTarget, BWAPI::Broodwar->getDamageFrom(rangedUnit->getType(), bestTarget->getType()));
	}
	return bestTarget;
}

// get the attack priority of a target unit
//获取目标单元的攻击优先级
int MicroVultures::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target) 
{
	const BWAPI::UnitType rangedType = rangedUnit->getType();
	const BWAPI::UnitType targetType = target->getType();

	if (target->isFlying())
	{
		// Can't target it.
		return 0;
	} 

	if (targetType.isWorker() || targetType == BWAPI::UnitTypes::Protoss_High_Templar)
	{
		return 12;
	}

	if (targetType == BWAPI::UnitTypes::Zerg_Hydralisk)
	{
		return 6;
	}

	if (targetType == BWAPI::UnitTypes::Zerg_Zergling)
	{
		return 11;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_Zealot)
	{
		return 11;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_Dragoon)
	{
		return 6;
	}

	// An addon other than a completed comsat is boring.
	// TODO should also check that it is attached
	//除已完成的通信卫星以外的附件是枯燥乏味的。
	//TODO 还应检查它是否已附加
	if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->isCompleted()))
	{
		return 1;
	}

    // if the target is building something near our base something is fishy
    BWAPI::Position ourBasePosition = BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
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
			//这包括代理建筑, 值得高度重视。
			//但是, 当基地紧密结合, 它可以包括无辜的建筑物。
			//我们也不想扰乱的优先级的情况下, 代理建筑
			//单位支持; 我们可能要先瞄准这些单位。
			if (UnitUtil::CanAttackGround(target) || UnitUtil::CanAttackAir(target))
			{
				return 10;
			}
			return 8;
		}
	}
    
	// Exceptions if we're a ground unit.
	if (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine && !target->isBurrowed() ||
		targetType == BWAPI::UnitTypes::Zerg_Infested_Terran)
	{
		return 12;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_High_Templar)
	{
		return 12;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_Reaver ||
		targetType == BWAPI::UnitTypes::Protoss_Arbiter)
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
	//if (UnitUtil::CanAttack(targetType, rangedType) && !targetType.isWorker())
	if (UnitUtil::CanAttack(targetType, rangedType))
	{
		// Enemy unit which is far enough outside its range is lower priority than a worker.
		if (rangedUnit->getDistance(target) > 48 + UnitUtil::GetAttackRange(target, rangedUnit))
		{
			return 8;
		}
		return 10;
	}

	// Important combat units that we may not have targeted above (esp. if we're a flyer).
	if (targetType == BWAPI::UnitTypes::Protoss_Carrier ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		return 6;
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
		return 6;
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
		return 6;
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

