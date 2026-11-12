#include "MicroManager.h"
#include "MapTools.h"
#include "UnitUtil.h"
#include "CombatCommander.h"

using namespace UAlbertaBot;
MicroManager::MicroManager() 
{
}

void MicroManager::setUnits(const BWAPI::Unitset & u) 
{ 
	_units = u; 
}

BWAPI::Position MicroManager::calcCenter() const
{
    if (_units.empty())
    {
        if (Config::Debug::DrawSquadInfo)
        {
            BWAPI::Broodwar->printf("calcCenter() called on empty squad");
        }
        return BWAPI::Position(0,0);
    }

	BWAPI::Position accum(0,0);
	for (const auto unit : _units)
	{
		accum += unit->getPosition();
	}
	return BWAPI::Position(accum.x / _units.size(), accum.y / _units.size());
}

void MicroManager::setOrder(const SquadOrder & inputOrder)
{
	order = inputOrder;
}

void MicroManager::execute(const SquadOrder & inputOrder)
{
	int frame = BWAPI::Broodwar->getFrameCount();
	// Nothing to do if we have no units.
	if (_units.empty())
	{
		return;
	}

	order = inputOrder;             // remember our order
	drawOrderText();

	// If we have no combat order (attack or defend), we're done.
	if (!order.isCombatOrder() && !order.isScoutOrder() && !order.isRepairOrder())
	{
		return;
	}

	// Discover enemies within the region of interest.
	BWAPI::Unitset nearbyEnemies;
	//BWAPI::Unitset cloakedUnits;

	// Always include enemies in the radius of the order.
	MapGrid::Instance().getUnits(nearbyEnemies, order.getPosition(), order.getRadius(), false, true);

	// For some orders, add enemies which are near our units.
	//对于一些命令, 增加敌人, 在我们的单位附近。
	if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend)
	//if (order.getType() == SquadOrderTypes::Attack)
	{
		for (const auto unit : _units) 
		{
			MapGrid::Instance().getUnits(nearbyEnemies, unit->getPosition(), 16 * 32, false, true);
			//我方单位的视野范围内是否有敌人？
			BWAPI::Unitset radiusEnemies = unit->getUnitsInRadius(unit->getType().sightRange(), BWAPI::Filter::IsEnemy);
			for (auto enemy : radiusEnemies) {
				if (enemy->isVisible() && !nearbyEnemies.contains(enemy))
				{
					nearbyEnemies.insert(enemy);
				}
			}

			BWAPI::Position fleePosition;
			BWAPI::Position myMainPosition = InformationManager::Instance().getMyUnloadPosition();

			//if (unit->isUnderAttack()) {
			if ((frame < 6 * 24 * 60 || CombatCommander::Instance().getNumCombatUnits() < 60) && unit->getDistance(myMainPosition) > 30 * 32) {
				CombatSimulation sim;

				//sim.setCombatUnits(unit->getPosition(), unit->getType().sightRange(), true);
				sim.setCombatUnits(unit->getPosition(), Config::Micro::CombatSimRadius, true);
				double score = sim.simulateCombat();
				//score = 10.0;

				if (radiusEnemies.size() > 0 && score < 0) {
					fleePosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();
					if (!fleePosition.isValid()) {
						fleePosition = MapTools::Instance().getExtendedPosition(unit->getPosition(), radiusEnemies.getPosition(), 9 * 32);
					}
					//unit->rightClick(fleePosition);
					Micro::RightClick(unit, fleePosition);
					//retreatUnit[unit] = frame + unit->getDistance(fleePosition) / unit->getType().topSpeed() - 24;
					retreatUnit[unit] = frame + (9 * 32) / unit->getType().topSpeed();
					continue;
				}
			}

			//如果有隐形单位，则退到隐形单位的攻击范围外
			/*
			if (unit->isUnderAttack() && radiusEnemies.size() == 0) {
				Micro::AttackMove(unit, myMainPosition);
				retreatUnit[unit] = frame + (3 * 32) / unit->getType().topSpeed() - 24;
			}
			*/

			if (unit->getOrder() == BWAPI::Orders::PlayerGuard) {
				for (auto enemy : nearbyEnemies) {
					if (enemy->getType().hasPermanentCloak() ||     // dark templar, observer
						enemy->getType().isCloakable() ||           // wraith, ghost
						enemy->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
						enemy->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
						enemy->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
						enemy->isBurrowed() ||
						(enemy->isVisible() && !enemy->isDetected()))
					{
						//cloakedUnits.insert(enemy);
						if (UnitUtil::CanAttack(enemy, unit)) {
							BWAPI::Broodwar->drawCircleMap(enemy->getPosition(), 4, BWAPI::Colors::Red, true);
							if (enemy->isInWeaponRange(unit) || unit->getDistance(enemy) < 3 * 32) {
								fleePosition = MapTools::Instance().getExtendedPosition(unit->getPosition(), enemy->getPosition(), 9 * 32);
								if (!fleePosition.isValid() || unit->getDistance(fleePosition) < 6 * 32) {
									fleePosition = myMainPosition;
								}

								Micro::RightClick(unit, fleePosition);
								retreatUnit[unit] = frame + (6 * 32) / unit->getType().topSpeed();
							}
						}
					}
				}
			}
		}

		/*
		for (auto enemy : nearbyEnemies) {
			//处理危险区域里的敌方单位
			std::map<BWAPI::Position, int> hazardousArea = InformationManager::Instance().getHazardousArea();
			for (auto it = hazardousArea.begin(); it != hazardousArea.end(); ++it) {
				BWAPI::Position center = (*it).first;
				int radius = (*it).second;

				if (enemy->getDistance(center) < radius) {
					Micro::RightClick(unit, order.getPosition());
					retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + Config::Micro::RetreatFrame;
				}
			}
		}
		*/
	}

	executeMicro(nearbyEnemies);

	// If the units are part of a drop squad and there are no targets remaining, release them from the drop squad
	if (nearbyEnemies.empty() && order.getType() == SquadOrderTypes::Drop)
	{
		for (auto& unit : _units)
		{
			auto squad = CombatCommander::Instance().getSquadData().getUnitSquad(*_units.begin());
			if (squad) squad->removeUnit(unit);
		}
	}
}

void MicroManager::getTargets(BWAPI::Unitset & targets) const
{
	// Always include enemies in the radius of the order.
	MapGrid::Instance().getUnits(targets, order.getPosition(), order.getRadius(), false, true);

	// For some orders, add enemies which are near our units.
	if (order.getType() == SquadOrderTypes::Attack ||
		order.getType() == SquadOrderTypes::KamikazeAttack ||
		order.getType() == SquadOrderTypes::Defend)
	{
		for (const auto unit : _units)
		{
			MapGrid::Instance().getUnits(targets, unit->getPosition(), 16 * 32, false, true);
		}
	}
}

// Determine if we should ignore the given target and look for something better closer to our order position
bool MicroManager::shouldIgnoreTarget(BWAPI::Unit combatUnit, BWAPI::Unit target)
{
	if (!combatUnit || !target) return true;

	// Check if this unit is currently performing a run-by of a bunker
	// If so, ignore all targets while we are doing the run-by
	/*
	auto bunkerRunBySquad = CombatCommander::Instance().getSquadData().getSquad(this).getBunkerRunBySquad(combatUnit);
	if (bunkerRunBySquad && bunkerRunBySquad->isPerformingRunBy(combatUnit))
	{
		return true;
	}
	*/

	// If the target base is no longer owned by the enemy, let our units pick their targets at will
	// This is so we don't ignore outlying buildings after we've already razed the center of the base
	
	auto base = InformationManager::Instance().baseAt(BWAPI::TilePosition(order.getPosition()));
	if (!base || base->getOwner() != BWAPI::Broodwar->enemy()) return false;

	// In some cases we want to ignore solitary bunkers and units covered by them
	// This may be because we have done a run-by or are waiting to do one
	if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
	{
		// First try to find a solitary bunker
		BWAPI::Unit solitaryBunker = nullptr;
		for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			if (!unit->exists() || !unit->isVisible() || !unit->isCompleted()) continue;
			if (unit->getType() != BWAPI::UnitTypes::Terran_Bunker) continue;

			// Break if this is the second bunker
			if (solitaryBunker)
			{
				solitaryBunker = nullptr;
				break;
			}

			solitaryBunker = unit;
		}

		// If it was found, do the checks
		/*
		if (solitaryBunker)
		{
			// The target is the bunker: ignore it if we are closer to the order position
			if (target->getType() == BWAPI::UnitTypes::Terran_Bunker)
			{
				int unitDist = PathFinding::GetGroundDistance(combatUnit->getPosition(), order.getPosition());
				int bunkerDist = PathFinding::GetGroundDistance(solitaryBunker->getPosition(), order.getPosition());
				if (unitDist != -1 && bunkerDist != -1 && unitDist < (bunkerDist - 128)) return true;
			}

			// The target isn't a bunker: ignore it if we can't attack it without coming under fire
			else
			{
				int bunkerRange = InformationManager::Instance().enemyHasInfantryRangeUpgrade() ? 6 * 32 : 5 * 32;
				int ourRange = std::max(0, UnitUtil::GetAttackRange(combatUnit, target) - 64); // be pessimistic and subtract two tiles
				if (target->getDistance(solitaryBunker) < (bunkerRange - ourRange + 32)) return true;
			}
		}
		*/
	}

	// If we are already close to our order position, this is the best target we're going to get
	if (combatUnit->getDistance(order.getPosition()) <= 200) return false;

	// Ignore workers far from the order position when rushing or doing a kamikaze attack
	/*
	if ((StrategyManager::Instance().isRushing() && order.getType() == SquadOrderTypes::Attack) ||
		order.getType() == SquadOrderTypes::KamikazeAttack)
	{
		if (combatUnit->getDistance(order.getPosition()) > 500 &&
			target->getType().isWorker() &&
			!target->isConstructing()) return true;
	}
	*/

	// Consider outlying buildings
	// Static defenses are handled separately so we can consider run-bys as a squad
	if (target->getType().isBuilding())
	{
		// Never ignore static defenses
		if (target->isCompleted() && UnitUtil::CanAttackGround(target)) return false;

		// Never ignore buildings that are part of walls
		//if (InformationManager::Instance().isEnemyWallBuilding(target)) return false;

		// Otherwise, let's ignore this and find something better to attack
		return true;
	}

	return false;
}

const BWAPI::Unitset & MicroManager::getUnits() const
{ 
    return _units; 
}

// Unused but potentially useful.
//未使用但可能有用。
//包含类型
bool MicroManager::containsType(BWAPI::UnitType type) const
{
	for (const auto unit : _units)
	{
		if (unit->getType() == type)
		{
			return true;
		}
	}
	return false;
}

void MicroManager::removeUnit(BWAPI::Unit unit)
{
	if (_units.contains(unit)) {
		_units.erase(unit);
	}
}

//重组
void MicroManager::regroup(const BWAPI::Position & regroupPosition) const
{
	for (const auto unit : _units)
	{
		if (unit->getType().isBuilding()) continue;

		// 1. A broodling should never retreat, but attack as long as it lives.
		// 2. If none of its kind has died yet, a dark templar or lurker should not retreat.
		// 3. A ground unit next to an enemy sieged tank should not move away.
		// TODO 4. A unit in stay-home mode should stay home, not "regroup" away from home.
		// TODO 5. A unit whose retreat path is blocked by enemies should do something else, at least attack-move.
		//1。broodling 不应该退却, 只要它活着就会攻击。
		//2。如果它的同类都没有死亡, 黑暗的圣殿武士或潜伏者不应该撤退。
		//3。敌人包围坦克旁边的地面单位不应该离开。
		//TODO 4。一个在住宿模式的单位应该呆在家里, 而不是 "重新集结 " 远离家园。
		//TODO 5。一个单位的撤退路径被敌人阻止, 应该做别的事情, 至少攻击移动。
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar)
		{
			BWAPI::Unit closestUnit = unit->getClosestUnit(BWAPI::Filter::IsEnemy && (
				BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Observer ||
				BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Overlord ||
				BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Science_Vessel));

			if (closestUnit) {
				Micro::Move(unit, regroupPosition);
				continue;
			}
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Broodling ||
			(unit->getType() == BWAPI::UnitTypes::Zerg_Lurker && BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Zerg_Lurker) == 0 ||
			(BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran && !unit->isFlying() && BWAPI::Broodwar->getClosestUnit(unit->getPosition()),
				BWAPI::Filter::IsEnemy && BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode,
				4 * 32)))
		{
			Micro::AttackMove(unit, unit->getPosition());
		}
		else if (unit->getDistance(regroupPosition) > unit->getType().sightRange())   // air distance, which can be unhelpful sometimes
		{
			if (!mobilizeUnit(unit))
			{
				//修改，移动攻击
				Micro::AttackMove(unit, regroupPosition);
			}
		}
		else
		{
			// We have retreated to a good position.
			if (!immobilizeUnit(unit))
			{
				Micro::AttackMove(unit, unit->getPosition());
			}
		}
	}
}

// Return true if we started to build a new scarab or interceptor.
//如果我们开始建造新的圣甲虫或拦截器, 返回 true。
bool MicroManager::buildScarabOrInterceptor(BWAPI::Unit u) const
{
	if (u->getType() == BWAPI::UnitTypes::Protoss_Reaver)
	{
		if (!u->isTraining() && u->canTrain(BWAPI::UnitTypes::Protoss_Scarab))
		//if (u->canTrain(BWAPI::UnitTypes::Protoss_Scarab))
		{
			return u->train(BWAPI::UnitTypes::Protoss_Scarab);
		}
	}
	else if (u->getType() == BWAPI::UnitTypes::Protoss_Carrier)
	{
		//if (!u->isTraining() && u->canTrain(BWAPI::UnitTypes::Protoss_Interceptor))
		if (u->canTrain(BWAPI::UnitTypes::Protoss_Interceptor))
		{
			return u->train(BWAPI::UnitTypes::Protoss_Interceptor);
		}
	}

	return false;
}

//是否有敌人接近
bool MicroManager::unitNearEnemy(BWAPI::Unit unit)
{
	return unitNearEnemy(unit, unit->getType().sightRange());
}

//是否有敌人接近
bool MicroManager::unitNearEnemy(BWAPI::Unit unit, int dist)
{
	assert(unit);

	BWAPI::Unitset enemyNear;

	MapGrid::Instance().getUnits(enemyNear, unit->getPosition(), dist, false, true);

	return enemyNear.size() > 0;
}

//是否需要撤退
bool MicroManager::unitShouldRetreat(BWAPI::Unit unit, BWAPI::Unitset nearEnemys)
{
	if (BWAPI::Broodwar->getFrameCount() > 8 * 60 * 24) return false;

	bool isRetreat = false;
	int maxRange = 3 * 32;
	int dist = 0;
	//BWAPI::Unitset enemyNear;
	//MapGrid::Instance().getUnits(enemyNear, unit->getPosition(), 2 * 32, false, true);

	//如果在闪电下
	if (unit->isUnderStorm()) {
		isRetreat = true;
	}
	
	if (unit->isUnderAttack() && unit->getHitPoints() < unit->getType().maxHitPoints() * 0.7) {
		isRetreat = true;
	}

	BWAPI::Unit enemy = BWAPI::Broodwar->getClosestUnit(unit->getPosition(), BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack);
	if (unit->getType().groundWeapon().maxRange() > 32 && enemy) {
		dist = unit->getDistance(enemy);
		if (dist <= 2 * 32 && enemy->getAngle() == unit->getAngle()) {
			maxRange = enemy->getType().groundWeapon().maxRange() - dist + 1 * 32;
			isRetreat = true;
		}
	}

	if (isRetreat) {
		BWAPI::Position fleePosition = getFleePosition(unit->getPosition(), nearEnemys.getPosition(), maxRange);
		unit->rightClick(fleePosition);
		retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + Config::Micro::RetreatFrame;
	}

	return isRetreat;
}

// returns true if position:
// a) is walkable
// b) doesn't have buildings on it
// c) isn't blocked by an enemy unit that can attack ground
// NOTE Unused code, a candidate for throwing out.
//如果位置, 则返回 true:
//a) 是步行
//b) 没有建筑物在上面
//c) 不被敌方部队阻止, 可以攻击地面
//注意未使用的代码, 一个抛出的候选。
bool MicroManager::checkPositionWalkable(BWAPI::Position pos) 
{
	// get x and y from the position
	int x(pos.x), y(pos.y);

	// If it's not walkable, throw it out.
	//如果不是步行, 就把它扔掉。
	if (!BWAPI::Broodwar->isWalkable(x / 8, y / 8))
	{
		return false;
	}

	// for each of those units, if it's a building or an attacking enemy unit we don't want to go there
	//对于每一个单位, 如果它是一个建筑物或攻击敌人的单位, 我们不想去那里
	for (const auto unit : BWAPI::Broodwar->getUnitsOnTile(x / 32, y / 32))
	{
		if (unit->getType().isBuilding() ||
			unit->getType().isResourceContainer() ||
			!unit->isFlying() && unit->getPlayer() != BWAPI::Broodwar->self() && UnitUtil::CanAttackGround(unit))
		{
			return false;
		}
	}

	// otherwise it's okay
	return true;
}

//在近阻点
bool MicroManager::unitNearChokepoint(BWAPI::Unit unit) const
{
	for (BWTA::Chokepoint * choke : BWTA::getChokepoints())
	{
		if (unit->getDistance(choke->getCenter()) < 4 * 32)
		{
			return true;
		}
	}

	return false;
}

// Mobilize the unit if it is immobile: A sieged tank or a burrowed zerg unit.
// Return whether any action was taken.
//调动单位, 如果它是静止的: 包围坦克或挖虫族单位。
//返回是否采取了任何操作。
bool MicroManager::mobilizeUnit(BWAPI::Unit unit) const
{
	if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode && unit->canUnsiege())
	{
		return unit->unsiege();
	}
	if (unit->isBurrowed() && unit->canUnburrow() &&
		!unit->isIrradiated() &&
		(double(unit->getType().maxHitPoints()) / double(unit->getHitPoints()) < 6.25))  // very weak units stay burrowed
	{
		return unit->unburrow();
	}
	return false;
}

// Immobilixe the unit: Siege a tank, burrow a lurker. Otherwise do nothing.
// Return whether any action was taken.
//Immobilixe 单位: 围攻坦克, 挖洞潜伏者。否则什么也不做。
//返回是否采取了任何操作。
bool MicroManager::immobilizeUnit(BWAPI::Unit unit) const
{
	if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && unit->canSiege())
	{
		return unit->siege();
	}
	if (unit->canBurrow() &&
		(unit->getType() == BWAPI::UnitTypes::Zerg_Lurker || unit->isIrradiated()))
	{
		return unit->burrow();
	}
	return false;
}

// Sometimes a unit on ground attack-move freezes in place.
// Luckily it's easy to recognize, though units may be on PlayerGuard for other reasons.
// Return whether any action was taken.
// This solves stuck zerglings, but doesn't always prevent other units from getting stuck.
//有时地面攻击的单位-移动冻结到位。
//幸运的是, 它很容易识别, 虽然单位可能是在 PlayerGuard 的其他原因。
//返回是否采取了任何操作。
//这解决了卡住小狗, 但并不总是阻止其他单位被卡住。
bool MicroManager::unstickStuckUnit(BWAPI::Unit unit)
{
	if (!unit->isMoving() && !unit->getType().isFlyer() && !unit->isBurrowed() &&
		(unit->getOrder() == BWAPI::Orders::PlayerGuard) &&
		BWAPI::Broodwar->getFrameCount() % 12 == 0)
	{
		BWAPI::Position pos = InformationManager::Instance().getRallyPoint();//unit->getOrderTargetPosition(); //order.getPosition();
		if (!pos.isValid()) {
			pos = InformationManager::Instance().getMyMainBaseLocation()->getPosition();
		}

		Micro::RightClick(unit, pos);
		retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + 0.5 * 24;
		//Micro::Stop(unit);
		return true;
	}

	if (unit->isStuck()) {
		Micro::Stop(unit);
		//retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + 0.5 * 24;
		return true;
	}

	return false;
}

//保持和坦克的距离
bool MicroManager::keepTankDistance(BWAPI::Unit unit, BWAPI::Position position){
	//找到最近的坦克
	if (order.isCombatOrder()) {
		if (unit->getDistance(position) < 10 * 32) {
			BWAPI::Unit needTank = unit->getClosestUnit((BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode || BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) && (!BWAPI::Filter::IsEnemy),
				unit->getType().sightRange());
			if (needTank) {
				int unitRange = unit->getType().groundWeapon().maxRange();
				int tankRange = needTank->getType().groundWeapon().maxRange();
				//单位的攻击范围不超过坦克的攻击范围
				int dist = unit->getDistance(needTank);
				if (dist > tankRange - unitRange - 1 * 32) {
					BWAPI::Position fleePosition = MapTools::Instance().getDistancePosition(unit->getPosition(), needTank->getPosition(), dist / 2);// - dist
					if (!unit->isAttackFrame()) {
						//unit->rightClick(fleePosition);
						Micro::RightClick(unit, fleePosition);
						retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + Config::Micro::RetreatFrame;
					}
					return true;
				}
			}
		}
	}

	return false;
}

//保持安全距离
bool MicroManager::keepSafeDistance(BWAPI::Unit unit, BWAPI::Unitset nearEnemys) {
	//BWAPI::Unit nearEnemy = unit->getClosestUnit(BWAPI::Filter::CanAttack && BWAPI::Filter::IsEnemy);

	if (BWAPI::Broodwar->getFrameCount() < 6 * 24 * 60) return false;

	BWAPI::Unit nearEnemy = nullptr;

	//BWAPI::Unitset nearEnemys;// = unit->getUnitsInRadius(unit->getType().sightRange(), BWAPI::Filter::CanAttack && BWAPI::Filter::IsEnemy && BWAPI::Filter::IsCompleted);
	//MapGrid::Instance().getUnits(nearEnemys, unit->getPosition(), unit->getType().sightRange(), false, true);
	for(auto target : nearEnemys){
		if (!target->isCompleted() || !target->canAttack()) continue;

		if (unit->getType().isFlyer()){
			if (target->getType().airWeapon() == BWAPI::WeaponTypes::None) {
				continue;
			}
		}
		else if (target->getType().groundWeapon() == BWAPI::WeaponTypes::None) {
			continue;
		}

		int maxRange = target->getType().groundWeapon().maxRange();
		if (unit->getType().isFlyer()) {
			maxRange = target->getType().airWeapon().maxRange();
		}

		if (unit->getDistance(target) <= maxRange + 2 * 32){
			nearEnemy = target;
			break;
		}
	}

	if (nearEnemy) {
		bool flee = false;
		//BWAPI::Unitset units = unit->getUnitsInRadius(unit->getType().sightRange() * 32, BWAPI::Filter::CanAttack && (!BWAPI::Filter::IsEnemy));

		//if (!order.isScoutOrder()) {
		CombatSimulation sim;

		sim.setCombatUnits(unit->getPosition(), unit->getType().sightRange() + 1 * 32, true);
		double score = sim.simulateCombat();

		if (score < 0) {
			flee = true;
		}
		//}

		if (unit->isUnderAttack() && unit->getHitPoints() <= unit->getType().maxHitPoints() * 0.4) {
			flee = true;
		}

		if (flee) {
			BWAPI::Position fleePosition = MapTools::Instance().getExtendedPosition(unit->getPosition(), nearEnemy->getPosition(), unit->getType().sightRange() + 8 * 32);
			Micro::RightClick(unit, fleePosition);
			order.setPosition(fleePosition);
			retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + Config::Micro::RetreatFrame;
			/*
			if (fleePosition.x <= 0 || fleePosition.y <= 0) {
				BWAPI::Unit geyser = BWAPI::Broodwar->getClosestUnit(InformationManager::Instance().getRallyPoint(), BWAPI::Filter::GetType == BWAPI::UnitTypes::Resource_Vespene_Geyser, 30 * 32);
				if (geyser) {
					//unit->rightClick(geyser);
					Micro::RightClick(unit, geyser);
					order.setPosition(geyser->getPosition());
					retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + Config::Micro::RetreatFrame;
				}
				else {
					//unit->rightClick(fleePosition);
					Micro::RightClick(unit, fleePosition);
					order.setPosition(fleePosition);
					retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + Config::Micro::RetreatFrame;
				}
			}
			else {
				//unit->rightClick(fleePosition);
				Micro::RightClick(unit, fleePosition);
				order.setPosition(fleePosition);
				retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + Config::Micro::RetreatFrame;
			}
			*/
			return true;
		}
	}

	return false;
}

// Send the protoss unit to the shield battery and recharge its shields.
// The caller should have already checked all conditions.
//把神族单位送到盾牌电池并给它的盾牌充电。
//调用方应该已经检查了所有条件。
void MicroManager::useShieldBattery(BWAPI::Unit unit, BWAPI::Unit shieldBattery)
{
	if (unit->getDistance(shieldBattery) >= 32)
	{
		// BWAPI::Broodwar->printf("move to battery %d at %d", unit->getID(), shieldBattery->getID());
		Micro::Move(unit, shieldBattery->getPosition());
	}
	else
	{
		// BWAPI::Broodwar->printf("recharge shields %d at %d", unit->getID(), shieldBattery->getID());
		Micro::RightClick(unit, shieldBattery);
	}
}

BWAPI::Position MicroManager::getFleePosition(BWAPI::Unit unit, BWAPI::Unit target) {
	return getFleePosition(unit->getPosition(), target->getPosition(), unit->getType().sightRange());
}

BWAPI::Position MicroManager::getFleePosition(BWAPI::Position form, BWAPI::Position to, int dist){
	BWAPI::Position fleePosition = MapTools::Instance().getExtendedPosition(form, to, dist);// - dist

	/*
	if (!unit->hasPath(fleePosition)) {
		fleePosition = InformationManager::Instance().getRallyPoint();
	}
	*/

	return fleePosition;
}

//保持躲避
bool MicroManager::keepElude(BWAPI::Unit unit) {
	if (unit->getType() == BWAPI::UnitTypes::Protoss_Interceptor) return false;

	int frame = BWAPI::Broodwar->getFrameCount();

	if (unit->getOrderTargetPosition()) {
		BWAPI::Position orderTargetPosition = unit->getOrderTargetPosition();

		if (unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle || unit->getType() == BWAPI::UnitTypes::Terran_Dropship) {
			if (unit->getDistance(orderTargetPosition) < 8 * 32) return false;
		}

		//如果遇到敌人
		BWAPI::Unit nearEnemy = nullptr;// = unit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack, unit->getType().sightRange());
		BWAPI::Unitset nearEnemies = unit->getUnitsInRadius(unit->getType().sightRange(), BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack);
		//MapGrid::Instance().getUnits(nearEnemies, unit->getPosition(), unit->getType().sightRange(), false, true);
		for (auto enemy : nearEnemies) {
			if (UnitUtil::CanAttack(enemy, unit)) {
				nearEnemy = enemy;
				break;
			}
		}

		if (nearEnemy) {
			BWAPI::Position rp1, rp2, pos;
			MapTools::Instance().getCutPoint(nearEnemy->getPosition(), unit->getType().sightRange(), unit->getPosition(), rp1, rp2);
			if (rp1.isValid() && rp2.isValid()) {
				pos = rp1;
				if (orderTargetPosition.getDistance(rp1) > orderTargetPosition.getDistance(rp2)) {
					pos = rp2;
				}

				BWAPI::Broodwar->drawCircleMap(nearEnemy->getPosition(), unit->getType().sightRange(), BWAPI::Colors::Red);
				BWAPI::Broodwar->drawCircleMap(pos, 4, BWAPI::Colors::Green, true);

				if (pos.isValid() && pos.x > 0 && pos.y > 0) {
					if (nearEnemy->getDistance(pos) < unit->getDistance(pos)) return false;

					if (unit->getDistance(orderTargetPosition) > unit->getType().sightRange()) {
						if (nearEnemy->getDistance(pos) < nearEnemy->getType().airWeapon().maxRange()) {
							BWAPI::Position fleeVector = MapTools::Instance().getKiteVector(unit, nearEnemy);
							pos = unit->getPosition() + fleeVector;
						}

						if (unit->getDistance(orderTargetPosition) > orderTargetPosition.getDistance(pos)) {
							pos = MapTools::Instance().getExtendedPosition(pos, unit->getPosition(), 3 * 32);
						}

						//unit->rightClick(pos);
						//Micro::Move(unit, pos);
						Micro::RightClick(unit, pos);
						retreatUnit[unit] = frame + unit->getDistance(pos) / unit->getType().topSpeed() - 2 * 24;
						//_loadTo[unit] = pos;
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool MicroManager::loadBunkers(BWAPI::Unit unit) {
	if (!unit->getType().getRace() == BWAPI::Races::Terran) return false;

	if (unit->isUnderAttack() && unit->getHitPoints() < unit->getType().maxHitPoints() * 0.8) {
		BWAPI::Unit bunker = unit->getClosestUnit(BWAPI::Filter::IsCompleted && BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Bunker && (!BWAPI::Filter::IsEnemy), 8 * 32);
		if (bunker && bunker->getSpaceRemaining() > 0) {
			//unit->rightClick(bunker);
			Micro::RightClick(unit, bunker);
			retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + Config::Micro::RetreatFrame;
			return true;
		}
	}

	return false;
}

// Retreat hurt units to allow them to regenerate health (zerg) or shields (protoss).
//撤退伤害单位允许他们再生健康 (虫族) 或盾牌 (神族)。
bool MicroManager::isShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	if (BWAPI::Broodwar->getFrameCount() > 5 * 60 * 24) return false;

	BWAPI::Unit target = meleeUnit->getOrderTarget();

	if (meleeUnit->getOrderTarget() && target->getHitPoints() < target->getType().maxHitPoints() * 0.4) {
		return false;
	}

	if (meleeUnit->isUnderAttack() && meleeUnit->getHitPoints() < meleeUnit->getType().maxHitPoints() * 0.4) {
		return true;
	}

	// terran don't regen so it doesn't make sense to retreat
	if (!meleeUnit->getType().getRace() == BWAPI::Races::Protoss)
	{
		if (meleeUnit->isUnderAttack() && meleeUnit->getHitPoints() < meleeUnit->getType().maxHitPoints() * 0.3) {
			return true;
		}
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

	return false;
}

bool MicroManager::keepDefendOrder(BWAPI::Unit unit, BWAPI::Unit target) {
	if (order.isCombatOrder()) {
		//危险区域
		BWAPI::Position center;
		BWAPI::Position targetPosition = unit->getOrderTargetPosition();
		BWAPI::Unit enemy;
		CombatSimulation sim;

		std::map<BWAPI::Unit, int> hazardousArea = InformationManager::Instance().getHazardousArea();
		for (auto it = hazardousArea.begin(); it != hazardousArea.end(); ++it) {
			enemy = (*it).first;
			center = enemy->getPosition();
			int radius = (*it).second;

			if (unit->isFlying()) {
				if (enemy->getType().airWeapon() == BWAPI::WeaponTypes::None) continue;
			}
			
			if (targetPosition.getDistance(center) < radius && unit->getType().groundWeapon().maxRange() < radius && unit->getDistance(center) < radius + 2 * 32) {
				//Micro::RightClick(unit, order.getPosition());
				//unit->rightClick(order.getPosition());
				sim.setCombatUnits(unit->getPosition(), unit->getType().groundWeapon().maxRange() - 2 * 32, true);
				double score = sim.simulateCombat();

				if (score < 0) {
					Micro::Hold(unit);
					retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + Config::Micro::RetreatFrame;
					return true;
				}
			}
		}

		if (unit->getDistance(order.getPosition()) + unit->getType().groundWeapon().maxRange() > order.getRadius()) {
			Micro::RightClick(unit, order.getPosition());
			retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + Config::Micro::RetreatFrame;
			return true;
		}

		if (unit->isIdle() && unit->getDistance(order.getPosition()) > unit->getType().groundWeapon().maxRange()) {
			Micro::AttackMove(unit, order.getPosition());
			retreatUnit[unit] = BWAPI::Broodwar->getFrameCount() + Config::Micro::RetreatFrame;
			return true;
		}
	}

	return false;
}

void MicroManager::drawOrderText() 
{
	if (Config::Debug::DrawUnitTargetInfo)
    {
		for (const auto unit : _units)
		{
			BWAPI::Broodwar->drawTextMap(unit->getPosition().x, unit->getPosition().y, "%s", order.getStatus().c_str());
		}
	}
}
