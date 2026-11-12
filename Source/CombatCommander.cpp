#include <queue>

#include "CombatCommander.h"
#include "Random.h"
#include "UnitUtil.h"
#include "ProductionManager.h"
#include "ScoutManager.h"
#include "MapPosition.h"
#include "BWMap.h"

using namespace UAlbertaBot;

// Squad priorities: Which can steal units from others.
//阵容优先: 可以从别人那里窃取单位。
const size_t IdlePriority = 0;
const size_t AttackPriority = 1;
const size_t ReconPriority = 2;
const size_t BaseDefensePriority = 3;
const size_t ScoutDefensePriority = 4;
const size_t DropPriority = 5;         // don't steal from Drop squad for Defense squad//不要从空投小队中窃取防守小队
const size_t RepairPriority = 6;

// The attack squads.
//攻击小队
const int AttackRadius = 40 * 32;
const int DefendRadius = 8 * 32;

// Reconnaissance squad.
const int ReconTargetTimeout = 40 * 24;//侦察目标超时
const int ReconFlyTargetTimeout = 30 * 24;//侦察目标超时
const int ReconScoutTargetTimeout = 40 * 24;//侦察目标超时

const int ReconRadius = 12 * 32;//侦察半径

CombatCommander::CombatCommander() 
    : _initialized(false)
	, _goAggressive(true)
	, _reconTarget(BWAPI::Positions::Invalid)   // it will be changed later
	, _lastReconTargetChange(0)
{
}

// Called once at the start of the game.
// You can also create new squads at other times.
void CombatCommander::initializeSquads()
{
	// The idle squad includes workers at work (not idle at all) and unassigned overlords.
	//空闲的阵容包括工作中的工人 (不空闲) 和未分配的领主。
    SquadOrder idleOrder(SquadOrderTypes::Idle, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), 100, "Chill out");
	_squadData.addSquad(Squad("Idle", idleOrder, IdlePriority));

    // the main attack squad will pressure an enemy base location
	//主要攻击小队将向敌军基地施压
    SquadOrder mainAttackOrder(SquadOrderTypes::Defend, getMainAttackLocation(nullptr), AttackRadius, "Attack enemy base");
	_squadData.addSquad(Squad("Ground", mainAttackOrder, AttackPriority));

	//坦克单独编队
	SquadOrder tanksOrder(SquadOrderTypes::Defend, getMainAttackLocation(nullptr), AttackRadius, "Tank Squad");
	_squadData.addSquad(Squad("Tanks", tanksOrder, AttackPriority));

	// The flying squad separates air units so they can act independently.
	// It gets the same order as the attack squad.
	//飞行小队分离空气单位, 以便他们能独立行动。
	//它与攻击小队的顺序相同。
	_squadData.addSquad(Squad("Flying", mainAttackOrder, AttackPriority));

	// The recon squad carries out reconnaissance in force to deny enemy bases.
	// It is filled in when enough units are available.
	//侦察队进行侦察, 以阻止敌军基地。
	//当有足够的单位可用时, 它会被填满。
	Squad & reconSquad = Squad("Recon", idleOrder, ReconPriority);//侦察优先级
	//Squad & reconSquad = Squad("Recon", mainAttackOrder, ReconPriority);//侦察优先级
	reconSquad.setCombatSimRadius(200);  // combat sim includes units in a smaller radius than for a combat squad//战斗 sim 包括单位在一个较小的半径比作战小队
	reconSquad.setFightVisible(true);    // combat sim sees only visible enemy units (not all known enemies)//作战 sim 看见仅可见的敌人单位 (不是所有已知的敌人)
	_squadData.addSquad(reconSquad);

	Squad & reconFlySquad = Squad("ReconFly", idleOrder, ReconPriority);//侦察优先级
	//Squad & reconSquad = Squad("Recon", mainAttackOrder, ReconPriority);//侦察优先级
	reconFlySquad.setCombatSimRadius(200);  // combat sim includes units in a smaller radius than for a combat squad//战斗 sim 包括单位在一个较小的半径比作战小队
	reconFlySquad.setFightVisible(true);    // combat sim sees only visible enemy units (not all known enemies)//作战 sim 看见仅可见的敌人单位 (不是所有已知的敌人)
	_squadData.addSquad(reconFlySquad);

	Squad & reconScoutSquad = Squad("ReconScout", idleOrder, ReconPriority);//侦察优先级
	//Squad & reconSquad = Squad("Recon", mainAttackOrder, ReconPriority);//侦察优先级
	reconScoutSquad.setCombatSimRadius(200);  // combat sim includes units in a smaller radius than for a combat squad//战斗 sim 包括单位在一个较小的半径比作战小队
	reconScoutSquad.setFightVisible(true);    // combat sim sees only visible enemy units (not all known enemies)//作战 sim 看见仅可见的敌人单位 (不是所有已知的敌人)
	_squadData.addSquad(reconScoutSquad);

	Squad & repairSquad = Squad("Repair", idleOrder, RepairPriority);//维修
	//Squad & reconSquad = Squad("Recon", mainAttackOrder, ReconPriority);//侦察优先级
	repairSquad.setCombatSimRadius(200);  // combat sim includes units in a smaller radius than for a combat squad//战斗 sim 包括单位在一个较小的半径比作战小队
	repairSquad.setFightVisible(false);    // combat sim sees only visible enemy units (not all known enemies)//作战 sim 看见仅可见的敌人单位 (不是所有已知的敌人)
	_squadData.addSquad(repairSquad);

	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

    // the scout defense squad will handle chasing the enemy worker scout
	//童军防御小队将处理追捕敌方工人侦察兵
	if (Config::Micro::ScoutDefenseRadius > 0)
	{
		SquadOrder enemyScoutDefense(SquadOrderTypes::Defend, ourBasePosition, Config::Micro::ScoutDefenseRadius, "Get the scout");
		_squadData.addSquad(Squad("ScoutDefense", enemyScoutDefense, ScoutDefensePriority));//侦察兵防御
	}

	// If we're expecting to drop, create a drop squad.
	// It is initially ordered to hold ground until it can load up and go.
	//如果我们想放弃, 就创建一小队。
	//它最初被命令保持地面, 直到它可以装载和去。
	//if (StrategyManager::Instance().dropIsPlanned())
	//ourBasePosition = InformationManager::Instance().getDefensivePosition();

	//SquadOrder doDrop(SquadOrderTypes::Load, ourBasePosition, AttackRadius, "Wait for transport");
	//_squadData.addSquad(Squad("Drop", doDrop, DropPriority));

    _initialized = true;
}

void CombatCommander::update(const BWAPI::Unitset & combatUnits)
{
    if (!_initialized)
    {
        initializeSquads();

		InformationManager::Instance().setDefensivePosition();
		//InformationManager::Instance().setHazardousArea(BWAPI::Position(31, 39), 7);
    }

    _combatUnits = combatUnits;

	int gameFrameCount = BWAPI::Broodwar->getFrameCount();

	int frame8 = gameFrameCount % 8;

	if (frame8 == 1)
	{
		updateIdleSquad();
		updateDropSquads();
		updateScoutDefenseSquad();//更新侦察兵防御小队
		updateBaseDefenseSquads();
		updateReconSquad();//更新侦察小队
		updateAttackSquads();
		updateRepairSquad();//维修部队

		updateDefenseSquads();//分部队
	}
	else if (frame8 % 4 == 2)
	{
		doComsatScan();

		isAggressive();//是否可以进攻

		//_squadData.clearEmptySquad();
		if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran) {
			loadOrUnloadBunkers();
		}

		updateBaseAttackSquads();//扫矿部队

		updateMainDefenseSquads();//主矿防守部队

		//updateStuckUnit();
	}

	if (gameFrameCount % 32 == 24) {
		//检查盟友是否解盟
		for (auto player : BWAPI::Broodwar->getPlayers()) {
			if (player->isEnemy(BWAPI::Broodwar->self()) && BWAPI::Broodwar->self()->isAlly(player)) {
				BWAPI::Broodwar->setAlliance(player, false, true);
				BWAPI::Broodwar->setVision(player, false);
			}
		}
	}

	if (gameFrameCount < 4500) {
		//updateBunkerRush();
	}

	_squadData.update();          // update() all the squads

	cancelDyingItems();
}

//被卡住的单位
void CombatCommander::updateStuckUnit() {
	for (auto unit : BWAPI::Broodwar->self()->getUnits()) {
		if (unit->getType().isBuilding()) continue;

		BWAPI::Position position = MapPosition::Instance().getRandomPosition(unit->getPosition(), 2 * 32);

		//if (unit->getOrder() == BWAPI::Orders::PlayerGuard && unit->getDistance(unit->getOrderTargetPosition()) > 5 * 32) {
		if (unit->getOrder() == BWAPI::Orders::None ||
			unit->getOrder() == BWAPI::Orders::Nothing ||
			unit->getOrder() == BWAPI::Orders::Unknown){

			Micro::Stop(unit);

			unit->move(position, true);
		}

		if (unit->isIdle()){
			for (BWTA::Chokepoint * choke : BWTA::getChokepoints())
			{
				if (unit->getDistance(choke->getCenter()) < 1 * 32 && choke->getWidth() <= 3 * 32)
				{
					unit->move(position, true);
				}
			}
		}
	}
}

void CombatCommander::updateIdleSquad()
{
    Squad & idleSquad = _squadData.getSquad("Idle");
    for (const auto unit : _combatUnits)
    {
		if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine) continue;
		//if (unit->getType().isWorker()) continue;

        // if it hasn't been assigned to a squad yet, put it in the low priority idle squad
		//如果它还没有被分配到一个小队, 把它放在低优先级闲置小队
        if (_squadData.canAssignUnitToSquad(unit, idleSquad))
        {
            idleSquad.addUnit(unit);
        }

		if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture && unit->isIdle()) {
			//unit->rightClick(getDefendLocation());
		}
    }
}

//扫矿部队
void CombatCommander::updateBaseAttackSquads() {
	if (BWAPI::Broodwar->getFrameCount() < _lastDefenseTargetChange) return;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (base == InformationManager::Instance().getEnemyMainBaseLocation()) continue;
		//不是敌方主基地和分矿
		//if (base == InformationManager::Instance().getEnemyMainBaseLocation() || base == InformationManager::Instance().getEnemyNaturalLocation()) continue;
		//不是我方主基地和分矿
		//if (base == InformationManager::Instance().getMyMainBaseLocation() || base == InformationManager::Instance().getMyNaturalLocation()) continue;

		BWAPI::Unitset units;
		MapGrid::Instance().getUnits(units, base->getPosition(), 8 * 32, false, true);

		bool isStaticDetection = false;
		for (auto unit : units) {
			if (unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony) {
				isStaticDetection = true;
				break;
			}
		}

		if (units.size() > 1) {
			setDefenseSquad("BaseAttack", BWAPI::UnitTypes::Terran_Vulture, 6, MapPosition::Instance().getRandomPosition(base->getTilePosition(), 3 * 32));
			if (isStaticDetection) {
				setDefenseSquad("BaseAttack", BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, 1, MapPosition::Instance().getRandomPosition(base->getTilePosition(), 3 * 32));
			}
			/*
			if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg) {
			setDefenseSquad("BaseAttack", BWAPI::UnitTypes::Terran_Marine, 5, MapPosition::Instance().getRandomPosition(base->getTilePosition(), 4 * 32));
			setDefenseSquad("BaseAttack", BWAPI::UnitTypes::Terran_Medic, 1, MapPosition::Instance().getRandomPosition(base->getTilePosition(), 4 * 32));
			setDefenseSquad("BaseAttack", BWAPI::UnitTypes::Terran_Firebat, 1, MapPosition::Instance().getRandomPosition(base->getTilePosition(), 4 * 32));
			}
			*/
			_lastDefenseTargetChange = BWAPI::Broodwar->getFrameCount() + 30 * 24;
			break;
		}
	}
}

//主矿防守部队
void CombatCommander::updateMainDefenseSquads() {
	//BWAPI::Unitset enemyUnitsInRegion;

	for (auto enemy : BWAPI::Broodwar->enemies()) {
		for (const auto unit : enemy->getUnits()) {
			// If it's a harmless air unit, don't worry about it for base defense.
			// TODO something more sensible
			if (unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle ||
				unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Reaver ||
				unit->getType() == BWAPI::UnitTypes::Terran_Dropship ||
				unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
				unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk
				)  // floating terran building
			{
				if (unit->getDistance(InformationManager::Instance().getMyMainBaseLocation()->getPosition()) < 20 * 23) {
					setDefenseSquad("MainDefense", BWAPI::UnitTypes::Protoss_Zealot, 4, MapPosition::Instance().getRandomPosition(unit->getTilePosition(), 3 * 32));
					setDefenseSquad("MainDefense", BWAPI::UnitTypes::Protoss_Dragoon, 4, MapPosition::Instance().getRandomPosition(unit->getTilePosition(), 3 * 32));
					setDefenseSquad("MainDefense", BWAPI::UnitTypes::Protoss_Corsair, 4, MapPosition::Instance().getRandomPosition(unit->getTilePosition(), 3 * 32));
				}
			}
		}
	}
}

//更新分部队任务
void CombatCommander::updateDefenseSquads()
{
	for (auto it = _defenseSquadData.begin(); it != _defenseSquadData.end(); ++it) {
		std::string name = (*it).first;
		SquadInfo squadInfo = (*it).second;

		Squad & defenseSquad = _squadData.getSquad(name);//防守部队

		//MapGrid::Instance().getUnits(nearbyUnits, squadInfo.getPosition(), 6 * 32, true, false);
		std::map<BWAPI::UnitType, int> squadMap = squadInfo.getSquad();

		for (auto itr = squadMap.begin(); itr != squadMap.end(); ++itr) {
			BWAPI::UnitType type = (*itr).first;
			int num = (*itr).second;
			BWAPI::UnitType unitType;

			for (const auto unit : defenseSquad.getUnits()) {
				if (num <= 0) break;
				unitType = unit->getType();

				if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) {
					--num;
				}

				if (unit->getType() == type) {
					--num;
				}
			}

			if (num <= 0) continue;

			for (const auto unit : _combatUnits)
			{
				if (num <= 0) break;

				if (unit->getType() == type) {
					if (defenseSquad.containsUnit(unit)) {
						--num;
					}
					else if (_squadData.canAssignUnitToSquad(unit, defenseSquad)) {
						_squadData.assignUnitToSquad(unit, defenseSquad);
						--num;
					}
				}
			}
		}

		SquadOrder squadOrder(getFrameSquadOrderTypes(), BWAPI::Position(squadInfo.getPosition()), AttackRadius, name);
		defenseSquad.setSquadOrder(squadOrder);
	}
}

int CombatCommander::getFrameSquadOrderTypes() {
	int squadOrderTypes = SquadOrderTypes::Attack;
	if (BWAPI::Broodwar->getFrameCount() < Config::Micro::DefenseFrame) {
		squadOrderTypes = SquadOrderTypes::Defend;
	}

	return squadOrderTypes;
}

void CombatCommander::setDefenseSquad(std::string name, BWAPI::UnitType type, int num, BWAPI::TilePosition position) {
	std::map<std::string, SquadInfo>::iterator it;
	it = _defenseSquadData.find(name);

	if (it == _defenseSquadData.end()){
		SquadInfo squadInfo(name, type, num, position);
		_defenseSquadData[name] = squadInfo;
	}
	else {
		SquadInfo squadInfo = _defenseSquadData[name];
		squadInfo.setSquad(type, num);
		if (position) {
			squadInfo.setPosition(position);
		}
		
		_defenseSquadData[name] = squadInfo;
	}

	if (!_squadData.squadExists(name))
	{
		int squadOrderType = getFrameSquadOrderTypes();
		if (name == "BaseAttack") {
			squadOrderType = SquadOrderTypes::Attack;
		}

		SquadOrder defendRegion(squadOrderType, BWAPI::Position(position), DefendRadius, name);
		_squadData.addSquad(Squad(name, defendRegion, BaseDefensePriority));
	}
}

void CombatCommander::setDefenseSquad(std::string name, BWAPI::TilePosition position) {
	std::map<std::string, SquadInfo>::iterator it;
	it = _defenseSquadData.find(name);

	if (it == _defenseSquadData.end()){
		return;
	}
	else {
		SquadInfo squadInfo = _defenseSquadData[name];
		squadInfo.setPosition(position);
		_defenseSquadData[name] = squadInfo;
	}
}
// Update the small recon squad which tries to find and deny enemy bases.
// All units in the recon squad are the same type, depending on what is available.
// Units available to the recon squad each have a "weight".
// Weights sum to no more than maxWeight, set below.
//更新的小侦察小队试图找到和拒绝敌人基地。
//侦察小队中的所有单位都是同一类型的, 这取决于可用的内容。
//侦察小队可用的单位每人都有 "重量"。
//重量总和不超过 maxWeight, 设置如下。
void CombatCommander::updateReconSquad()
{
	const int maxWeight = 12;
	Squad & mainAttackOrder = _squadData.getSquad("Ground");

	Squad & reconSquad = _squadData.getSquad("Recon");//地面侦察小队
	Squad & reconFlySquad = _squadData.getSquad("ReconFly");//空中侦察小队
	
	Squad & reconScoutSquad = _squadData.getSquad("ReconScout");//农民侦察小队

	// What is already in the squad?
	int squadWeight = 0;
	int nMarines = 0;
	int nMedics = 0;
	int nVultures = 0;
	int nWraiths = 0;

	int nGrounds = 0;
	int nFlyers = 0;
	int nScouts = 0;

	//_reconUnitTypes.
	for (auto it = _reconUnitTypes.begin(); it != _reconUnitTypes.end(); ++it) {
		BWAPI::UnitType type = (*it).first;
		int nRecons = (*it).second;
		int allUnits = UnitUtil::GetAllUnitCount(type);

		if (type.isFlyer()) {
			for (const auto unit : reconFlySquad.getUnits())
			{
				if (unit->getType() == type) {
					++nFlyers;
				}
			}

			if (nFlyers < nRecons) {
				for (const auto unit : _combatUnits)
				{
					if (unit->getType() == type) {
						if (_squadData.canAssignUnitToSquad(unit, reconFlySquad)) {
							_squadData.assignUnitToSquad(unit, reconFlySquad);
							//++nFlyers;
						}
					}
				}
			}
		}
		else if (type.isWorker()){
			for (const auto unit : reconScoutSquad.getUnits())
			{
				if (unit->getType() == type) {
					++nScouts;
				}
			}

			if (nScouts < nRecons) {
				for (const auto unit : _combatUnits)
				{
					if (unit->getType() == type) {
						if (_squadData.canAssignUnitToSquad(unit, reconScoutSquad)) {
							_squadData.assignUnitToSquad(unit, reconScoutSquad);
							//++nScouts;
						}
					}
				}
			}
		}
		else {
			BWAPI::Unitset nullVulture;
			for (const auto unit : reconSquad.getUnits())
			{
				if (unit->getType() == type) {
					++nGrounds;
				}

				if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture && unit->getSpiderMineCount() == 0) {
					nullVulture.insert(unit);
				}
			}

			if (type == BWAPI::UnitTypes::Terran_Vulture) {
				BWAPI::Unitset spiderMineUnits;
				int numVulture = 0;// BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Vulture);

				for (const auto unit : _combatUnits)
				{
					if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture) {
						if (unit->getSpiderMineCount() > 0) {
							spiderMineUnits.insert(unit);
						}
					}
				}

				//把有雷的加到侦察队伍里
				for (const auto unit : spiderMineUnits)
				{
					if (nGrounds > nRecons + nullVulture.size()) break;

					if (_squadData.canAssignUnitToSquad(unit, reconSquad)) {
						_squadData.assignUnitToSquad(unit, reconSquad);
						++nGrounds;
					}
				}

				//如果数量大于设置数量，则移除没有雷的单位
				for (auto unit : nullVulture) {
					if (nGrounds > nRecons) {
						reconSquad.removeUnit(unit);
						mainAttackOrder.addUnit(unit);
						if (_squadData.canAssignUnitToSquad(unit, mainAttackOrder)) {
							_squadData.assignUnitToSquad(unit, mainAttackOrder);
						}
						Micro::AttackMove(unit, InformationManager::Instance().getDefensivePosition());
						--nGrounds;
					}
				}
			}
			else {
				if (nGrounds < nRecons) {
					for (const auto unit : _combatUnits)
					{
						if (unit->getType() == type) {
							if (_squadData.canAssignUnitToSquad(unit, reconSquad)) {
								_squadData.assignUnitToSquad(unit, reconSquad);
								++nGrounds;
							}
						}
					}
				}
			}
		}
	}

	if (!reconScoutSquad.isEmpty()) {
		int maxDist = 10000;

		if (BWAPI::Broodwar->getFrameCount() > _lastReconScoutTargetChange + ReconScoutTargetTimeout) {
			if (!InformationManager::Instance().getEnemyMainBaseLocation()) {
				for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
				{
					BWAPI::TilePosition pos = startLocation->getTilePosition();
					if (pos == InformationManager::Instance().workerScoutTarget) continue;
					//BWAPI::TilePosition pos = MapGrid::Instance().getScoutPosition(startLocation->getTilePosition());

					if (BWAPI::Broodwar->isExplored(pos))
					{
						if (_reconScoutTarget == pos)
						{
							_reconScoutTarget = BWAPI::TilePositions::Invalid;
						}
					}
					else {
						BWAPI::Position mainPosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();
						//int dist = BWTA::getGroundDistance(_workerScout->getTilePosition(), pos);

						int dist = MapTools::Instance().getGroundDistance(mainPosition, startLocation->getPosition());

						if (_reconScoutTarget == BWAPI::TilePositions::Invalid) {
							_reconScoutTarget = pos;
						}
						else if (dist < maxDist)
						{
							_reconScoutTarget = pos;
							maxDist = dist;
						}
					}
				}
			}

			_lastReconScoutTargetChange = BWAPI::Broodwar->getFrameCount();
		}

		// If nowhere needs seeing, disband the squad. We're done.
		if (!_reconScoutTarget.isValid())
		{
			reconScoutSquad.clear();
		}
		else {
			//最后, 下达命令。
			if (!reconScoutSquad.isEmpty() && BWAPI::Broodwar->getFrameCount() > _lastBuildingChange + 2) {
				SquadOrder reconScoutOrder(SquadOrderTypes::Scout, BWAPI::Position(_reconScoutTarget), ReconRadius, "Recon Scout in force");
				reconScoutSquad.setSquadOrder(reconScoutOrder);
			}
		}
	}

	if (!reconSquad.isEmpty()) {
		int time = 6 * 60 * 24;
		if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss) {
			time = 4 * 60 * 24;
		}

		if (BWAPI::Broodwar->getFrameCount() < time) {
			if (InformationManager::Instance().getEnemyMainBaseLocation()) {
				if (!_reconTarget) {
					_reconTarget = BWAPI::Position(MapPosition::Instance().getEnemyDefensivePosition());
				}
				chooseReconTarget();//选择侦察目标
				_lastReconTargetChange = BWAPI::Broodwar->getFrameCount();
			}
		}
		else {
			chooseReconTarget();//选择侦察目标
		}
		
		if (!_reconTarget.isValid())
		{
			reconSquad.clear();
		}
		else {
			//如果是敌方基地
			if (InformationManager::Instance().getEnemyMainBaseLocation()) {
				BWAPI::Position enemyMainLocation = InformationManager::Instance().getEnemyMainBaseLocation()->getPosition();
				BWAPI::Position enemyNaturalLocation = InformationManager::Instance().getEnemyNaturalLocation()->getPosition();

				if (BWAPI::Broodwar->getFrameCount() < time && !BWAPI::Broodwar->isExplored(BWAPI::TilePosition(enemyNaturalLocation))) {
					_reconTarget = enemyNaturalLocation;
				}
				else  if (_reconTarget.getDistance(enemyMainLocation) < 10 * 32 || _reconTarget.getDistance(enemyNaturalLocation) < 10 * 32) {
					//先到桥口
					_reconTarget = BWAPI::Position(MapPosition::Instance().getEnemyDefensivePosition());
					InformationManager::Instance().setReconLocation(_reconTarget, 2 * 60 * 24);
				}
			}

			//最后, 下达命令。
			SquadOrder reconOrder(SquadOrderTypes::Scout, _reconTarget, ReconRadius, "Recon order");
			reconSquad.setSquadOrder(reconOrder);
		}
	}

	if (!reconFlySquad.isEmpty()) {
		if (BWAPI::Broodwar->getFrameCount() < 6 * 60 * 24) {
			if (InformationManager::Instance().getEnemyMainBaseLocation()) {
				if (!_reconFlyTarget) {
					_reconFlyTarget = InformationManager::Instance().getEnemyMainBaseLocation()->getPosition();
				}
				_lastReconFlyTargetChange = BWAPI::Broodwar->getFrameCount();
			}
		}
		else {
			chooseReconFlyTarget();//选择空中侦察目标
		}

		if (!_reconFlyTarget.isValid())
		{
			reconFlySquad.clear();
		}
		else {
			//最后, 下达命令。
			SquadOrder reconFlyOrder(SquadOrderTypes::Scout, _reconFlyTarget, ReconRadius, "Fly in force");
			reconFlySquad.setSquadOrder(reconFlyOrder);
		}
	}
	return;

	for (const auto unit : reconSquad.getUnits())
	{
		squadWeight += weighReconUnit(unit);
		if (unit->getType() == BWAPI::UnitTypes::Terran_Marine)
		{
			++nMarines;
		}
		else if (unit->getType() == BWAPI::UnitTypes::Terran_Medic)
		{
			++nMedics;
		}
		else if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture)
		{
			++nVultures;
		}
	}

	// If everything except the detector is gone, let the detector go too.
	// It can't carry out reconnaissance in force.
	if (squadWeight == 0 && !reconSquad.isEmpty())
	{
		reconSquad.clear();
	}

	// What is available to put into the squad?
	//有什么可以放进球队？
	int availableWeight = 0;//可用权重
	for (const auto unit : _combatUnits)
	{
		availableWeight += weighReconUnit(unit);
	}

	// The allowed weight of the recon squad. It should steal few units.
	//侦察小队的允许重量。它应该偷很少的单位。
	int weightLimit = availableWeight >= 24 ? 2 + (availableWeight - 24) / 6 : 0;
	if (weightLimit > maxWeight)
	{
		weightLimit = maxWeight;
	}

	// If the recon squad weighs more than it should, clear it and continue.
	// Also if all marines are gone, but medics remain.
	// Units will be added back in if they should be.
	//如果侦察小队的体重超过了应有的重量, 请清除并继续。
	//而且如果所有海军陆战队员都走了, 但医务人员仍然存在。
	//单位将被添加回, 如果他们应该。
	//if (squadWeight > weightLimit || nMarines == 0 && nMedics > 0)
	if (nVultures < 2) {
		if (squadWeight > weightLimit || nVultures == 0)
		{
			reconSquad.clear();
			squadWeight = 0;
			nMarines = nMedics = 0;
		}
	}

	// Add units up to the weight limit.
	// In this loop, add no medics, and few enough marines to allow for 2 medics.
	//将单位添加到重量限制。
	//在这个循环中, 不加医务人员, 也没有足够的海军陆战队员允许2名医务人员。
	bool hasDetector = reconSquad.hasDetector();
	for (const auto unit : _combatUnits)
	{
		if (squadWeight >= weightLimit)
		{
			break;
		}

		BWAPI::UnitType type = unit->getType();
		int weight = weighReconUnit(type);
		if (weight > 0 && squadWeight + weight <= weightLimit && _squadData.canAssignUnitToSquad(unit, reconSquad))
		{
			if (type == BWAPI::UnitTypes::Terran_Vulture)
			{
				if (nVultures * weight < maxWeight - 2 * weighReconUnit(BWAPI::UnitTypes::Terran_Vulture))
				{
					_squadData.assignUnitToSquad(unit, reconSquad);
					squadWeight += weight;
					nVultures += 1;
				}
			}
			/*
			else if (type == BWAPI::UnitTypes::Terran_Marine)
			{
				if (nMarines * weight < maxWeight - 2 * weighReconUnit(BWAPI::UnitTypes::Terran_Medic))
				{
					_squadData.assignUnitToSquad(unit, reconSquad);
					squadWeight += weight;
					nMarines += 1;
				}
			}
			else if (type != BWAPI::UnitTypes::Terran_Medic)
			{
				_squadData.assignUnitToSquad(unit, reconSquad);
				squadWeight += weight;
			}
			*/
		}
		else if (!hasDetector && type.isDetector() && _squadData.canAssignUnitToSquad(unit, reconSquad))
		{
			_squadData.assignUnitToSquad(unit, reconSquad);
			hasDetector = true;
		}
	}

	// Now fill in any needed medics.
	//现在填上任何需要的医务人员。
	if (nMarines > 0 && nMedics < 2)
	{
		for (const auto unit : _combatUnits)
		{
			if (squadWeight >= weightLimit || nMedics >= 2)
			{
				break;
			}
			if (unit->getType() == BWAPI::UnitTypes::Terran_Medic &&
				_squadData.canAssignUnitToSquad(unit, reconSquad))
			{
				_squadData.assignUnitToSquad(unit, reconSquad);
				squadWeight += weighReconUnit(BWAPI::UnitTypes::Terran_Medic);
				nMedics += 1;
			}
		}
	}

	// Finally, issue the order.
	//最后, 下达命令。
	//SquadOrder reconOrder(SquadOrderTypes::Attack, _reconTarget, ReconRadius, "Reconnaissance in force");
	//reconSquad.setSquadOrder(reconOrder);
}

// The recon squad is allowed up to a certain "weight" of units.
//侦察小队被允许达到某一单位的 "权重"。
int CombatCommander::weighReconUnit(const BWAPI::Unit unit) const
{
	return weighReconUnit(unit->getType());
}

// The recon squad is allowed up to a certain "weight" of units.
//侦察小队被允许达到某一单位的 "重量"。
int CombatCommander::weighReconUnit(const BWAPI::UnitType type) const
{
	if (type == BWAPI::UnitTypes::Protoss_Observer) return 10;
	if (type == BWAPI::UnitTypes::Zerg_Zergling) return 2;
	if (type == BWAPI::UnitTypes::Zerg_Hydralisk) return 3;
	if (type == BWAPI::UnitTypes::Terran_Marine) return 2;
	if (type == BWAPI::UnitTypes::Terran_Medic) return 2;
	if (type == BWAPI::UnitTypes::Terran_Vulture) return 6;
	if (type == BWAPI::UnitTypes::Terran_Wraith) return 4;
	if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) return 2;
	if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) return 2;
	if (type == BWAPI::UnitTypes::Protoss_Zealot) return 4;
	if (type == BWAPI::UnitTypes::Protoss_Dragoon) return 4;
	if (type == BWAPI::UnitTypes::Protoss_Dark_Templar) return 6;

	return 0;
}

// Keep the same reconnaissance target or switch to a new one, depending.
//保持相同的侦察目标或切换到一个新的, 取决于。
void CombatCommander::chooseReconTarget()
{
	bool change = false;       // switch targets?

	// There is nowhere that we need to see. Change to the invalid target.

	// If the current target is invalid, we're starting up.
	if (!_reconTarget.isValid())
	{
		change = true;
	}
	// If the target is in sight (of any unit, not only the recon squad) and empty of enemies, we're done.
	//如果目标是在视线 (任何单位, 不仅侦察小队) 和空的敌人, 我们已经完成。
	else if (BWAPI::Broodwar->isVisible(_reconTarget.x / 32, _reconTarget.y / 32))
	{
		//InformationManager::Instance().setReconLocation(_reconTarget, 30 * 24);

		BWAPI::Unitset enemies;
		MapGrid::Instance().getUnits(enemies, _reconTarget, ReconRadius, false, true);
		// We don't particularly care about air units, even when we could engage them.
		//我们并不特别关心空气单位, 即使我们可以接触他们。
		for (auto it = enemies.begin(); it != enemies.end();)
		{
			if ((*it)->isCompleted() && (*it)->getType().groundWeapon().maxRange() >= 5 * 32) {
				change = true;
				break;
			}

			if ((*it)->isFlying())
			{
				it = enemies.erase(it);
			}
			else
			{
				++it;
			}
		}
		if (enemies.empty())
		{
			change = true;
		}
	}
	else if (BWAPI::Broodwar->getFrameCount() - _lastReconTargetChange >= ReconTargetTimeout)
	{
		change = true;
	}

	if (change)
	{
		int time = 6 * 60 * 24;
		if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss) {
			time = 4 * 60 * 24;
		}

		BWAPI::Position nextTarget = InformationManager::Instance().getReconLocation();
		//如果是敌方基地
		if (InformationManager::Instance().getEnemyMainBaseLocation()) {
			BWAPI::Position enemyMainLocation = InformationManager::Instance().getEnemyMainBaseLocation()->getPosition();
			BWAPI::Position enemyNaturalLocation = InformationManager::Instance().getEnemyNaturalLocation()->getPosition();

			if (BWAPI::Broodwar->getFrameCount() < time && !BWAPI::Broodwar->isExplored(BWAPI::TilePosition(enemyNaturalLocation))) {
				nextTarget = enemyNaturalLocation;
			}
			else  if (nextTarget.getDistance(enemyMainLocation) < 10 * 32 || nextTarget.getDistance(enemyNaturalLocation) < 10 * 32) {
				//先到桥口
				nextTarget = BWAPI::Position(MapPosition::Instance().getEnemyDefensivePosition());
				InformationManager::Instance().setReconLocation(nextTarget, 2 * 60 * 24);
			}
		}
		
		_reconTarget = nextTarget;
		_lastReconTargetChange = BWAPI::Broodwar->getFrameCount();
	}
}
/*
void CombatCommander::chooseReconTarget()
{
	bool change = false;       // switch targets?

	BWAPI::Position nextTarget = getReconLocation();

	// There is nowhere that we need to see. Change to the invalid target.

	// If the current target is invalid, we're starting up.
	if (!_reconTarget.isValid())
	{
		change = true;
	}

	else if (!nextTarget.isValid())
	{
		change = true;
	}
	// If we have spent too long on one target, then probably the path is impassible.
	else if (BWAPI::Broodwar->getFrameCount() - _lastReconTargetChange >= ReconTargetTimeout)
	{
		change = true;
	}

	// If the target is in sight (of any unit, not only the recon squad) and empty of enemies, we're done.
	else if (BWAPI::Broodwar->isVisible(_reconTarget.x / 32, _reconTarget.y / 32))
	{
		BWAPI::Unitset enemies;
		MapGrid::Instance().getUnits(enemies, _reconTarget, ReconRadius, false, true);
		// We don't particularly care about air units, even when we could engage them.
		for (auto it = enemies.begin(); it != enemies.end(); )
		{
			if ((*it)->isFlying())
			{
				it = enemies.erase(it);
			}
			else
			{
				++it;
			}
		}
		if (enemies.empty())
		{
			change = true;
		}
	}

	if (change)
	{
		_reconTarget = nextTarget;
		_lastReconTargetChange = BWAPI::Broodwar->getFrameCount();
	}
}
*/
// Keep the same reconnaissance target or switch to a new one, depending.
//保持相同的侦察目标或切换到一个新的, 取决于。
void CombatCommander::chooseReconFlyTarget()
{
	bool change = false;       // switch targets?

	// There is nowhere that we need to see. Change to the invalid target.

	// If the current target is invalid, we're starting up.
	if (!_reconFlyTarget.isValid())
	{
		change = true;

	}
	// If the target is in sight (of any unit, not only the recon squad) and empty of enemies, we're done.
	//如果目标是在视线 (任何单位, 不仅侦察小队) 和空的敌人, 我们已经完成。
	else if (BWAPI::Broodwar->isVisible(_reconFlyTarget.x / 32, _reconFlyTarget.y / 32))
	{
		BWAPI::Unitset enemies;
		MapGrid::Instance().getUnits(enemies, _reconFlyTarget, ReconRadius, false, true);
		// We don't particularly care about air units, even when we could engage them.
		//我们并不特别关心空气单位, 即使我们可以接触他们。
		for (auto it = enemies.begin(); it != enemies.end();)
		{
			if ((*it)->getType().isBuilding())
			{
				it = enemies.erase(it);
			}
			else
			{
				++it;
			}
		}
		if (enemies.empty())
		{
			change = true;
		}
	}
	else if (BWAPI::Broodwar->getFrameCount() - _lastReconFlyTargetChange >= ReconFlyTargetTimeout)
	{
		change = true;
	}

	if (change)
	{
		BWAPI::Position nextTarget = getReconFlyLocation();

		_reconFlyTarget = nextTarget;
		_lastReconFlyTargetChange = BWAPI::Broodwar->getFrameCount();
	}
}

// Choose an empty base location for the recon squad to check out.
// Called only by setReconTarget().
//选择一个空的基地位置, 以便侦察小队检查。
//仅由 setReconTarget() 调用。
BWAPI::Position CombatCommander::getReconLocation() const
{
	std::vector<BWTA::BaseLocation *> choices;

	BWAPI::Position mainPosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();

	// The choices are neutral bases reachable by ground.
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		/*
		if (InformationManager::Instance().getBaseOwner(base) == BWAPI::Broodwar->neutral() &&
			MapTools::Instance().getGroundTileDistance(base->getPosition(), mainPosition) != -1)
			*/
		if (MapTools::Instance().getGroundTileDistance(base->getPosition(), mainPosition) != -1)
		{
			choices.push_back(base);
		}
	}

	// If there are none, return an invalid position.
	if (choices.empty())
	{
		return BWAPI::Positions::Invalid;
	}

	// Choose randomly.
	// We may choose the same target we already have. That's OK; if there's another choice,
	// we'll probably switch to it soon.
	BWTA::BaseLocation * base = choices.at(Random::Instance().index(choices.size()));
	return base->getPosition();
}

void CombatCommander::setReconTarget(BWAPI::Position target) {
	_reconTarget = target; 
	_lastReconTargetChange = BWAPI::Broodwar->getFrameCount();

	//最后, 下达命令。
	Squad & reconSquad = _squadData.getSquad("Recon");//地面侦察小队
	SquadOrder reconOrder(SquadOrderTypes::Scout, _reconTarget, ReconRadius, "Recon order");
	reconSquad.setSquadOrder(reconOrder);
}

// Choose an empty base location for the recon squad to check out.
// Called only by setReconTarget().
//选择一个空的基地位置, 以便侦察小队检查。
//仅由 setReconTarget() 调用。
BWAPI::Position CombatCommander::getReconFlyLocation() const
{
	std::vector<BWTA::BaseLocation *> choices;

	BWAPI::Position mainPosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();

	// The choices are neutral bases reachable by ground.
	for (BWTA::BaseLocation * base : BWTA::getStartLocations())
	{
		choices.push_back(base);
	}

	// If there are none, return an invalid position.
	if (choices.empty())
	{
		return BWAPI::Positions::Invalid;
	}

	// Choose randomly.
	// We may choose the same target we already have. That's OK; if there's another choice,
	// we'll probably switch to it soon.
	BWTA::BaseLocation * base = choices.at(Random::Instance().index(choices.size()));
	return base->getPosition();
}

// Form the ground squad and the flying squad, the main attack squads.
// NOTE Arbiters and guardians go into the ground squad.
//      Devourers, scourge, and carriers are flying squad if it exists, otherwise ground.
//      Other air units always go into the flying squad.
//组成地面小队和飞行小队, 主要的进攻小队。
//注意仲裁员和监护人进入地面小队。
//吞食者, 灾祸, 和承运人是飞行小队, 如果它存在, 否则地面。
//其他的空军部队总是进入飞行小队。
void CombatCommander::updateAttackSquads()
{
	//Squad & idleSquad = _squadData.getSquad("Idle");

    Squad & mainAttackSquad = _squadData.getSquad("Ground");
	Squad & flyingSquad = _squadData.getSquad("Flying");

	Squad & tanksSquad = _squadData.getSquad("Tanks");

	_mainAttackUnits = 0;
	//_mainAttackUnits = mainAttackSquad.getUnits().size() + flyingSquad.getUnits().size();

	// Include exactly 1 detector in each squad, for detection.
	bool mainDetector = mainAttackSquad.hasDetector();
	bool mainSquadExists = mainAttackSquad.hasCombatUnits();

	bool flyingDetector = flyingSquad.hasDetector();
	bool flyingSquadExists = false;
	for (const auto unit : flyingSquad.getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk ||
			unit->getType() == BWAPI::UnitTypes::Terran_Wraith ||
			unit->getType() == BWAPI::UnitTypes::Terran_Valkyrie ||
			unit->getType() == BWAPI::UnitTypes::Terran_Battlecruiser ||
			unit->getType() == BWAPI::UnitTypes::Terran_Science_Vessel ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Corsair ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Scout)
		{
			flyingSquadExists = true;
		}
	}

	for (const auto unit : _combatUnits)
    {
		// Scourge, devourers, carriers go into the flying squad only if it already exists.
		// Otherwise they go into the ground squad.
		//灾祸, 吞食者, 航母进入飞行小队, 只有当它已经存在。
		//否则他们就会进入地面小队
		if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine) continue;
		if (unit->getType().isWorker()) continue;
		if (!unit->isCompleted()) continue;
		//if (unit->canAttack()) {
			_mainAttackUnits += unit->getType().supplyRequired();
		//}

		bool isDetector = unit->getType().isDetector();
		if (_squadData.canAssignUnitToSquad(unit, flyingSquad)
			  &&
			(unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Scourge && flyingSquadExists ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Devourer && flyingSquadExists ||
			unit->getType() == BWAPI::UnitTypes::Terran_Wraith ||
			unit->getType() == BWAPI::UnitTypes::Terran_Valkyrie ||
			unit->getType() == BWAPI::UnitTypes::Terran_Battlecruiser ||
			//unit->getType() == BWAPI::UnitTypes::Terran_Science_Vessel ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Corsair ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Scout ||
			unit->getType() == BWAPI::UnitTypes::Protoss_Carrier && flyingSquadExists ||
			isDetector && !flyingDetector && flyingSquadExists))
		{
			_squadData.assignUnitToSquad(unit, flyingSquad);
			if (isDetector)
			{
				flyingDetector = true;
			}
		}
		else if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode || unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) {
			if (_squadData.canAssignUnitToSquad(unit, tanksSquad)) {
				_squadData.assignUnitToSquad(unit, tanksSquad);
			}
		}
		/*
        else if (!unit->getType().isWorker() &&
			(!isDetector || isDetector && !mainDetector && mainSquadExists) &&
			_squadData.canAssignUnitToSquad(unit, mainAttackSquad))*/
		else if (_squadData.canAssignUnitToSquad(unit, mainAttackSquad))
        {
			_squadData.assignUnitToSquad(unit, mainAttackSquad);
			if (isDetector)
			{
				mainDetector = true;
			}
        }
    }
	
	int squadType = SquadOrderTypes::Attack;
	if (!_goAggressive) {
		squadType = SquadOrderTypes::Defend;
	}

	/*
	SquadOrder mainAttackOrder(squadType, getMainAttackLocation(&mainAttackSquad), AttackRadius, "Attack enemy base");
	mainAttackSquad.setSquadOrder(mainAttackOrder);

	SquadOrder flyingAttackOrder(squadType, getMainAttackLocation(&flyingSquad), AttackRadius, "Attack enemy base");
	flyingSquad.setSquadOrder(flyingAttackOrder);

	return;
	*/
	//BWAPI::Position mainAttackLocation;
	//BWAPI::Position centerLocation;
	BWAPI::Position targetLocation;

	if (!tanksSquad.isEmpty()) {
		targetLocation = getTargetLocation(&tanksSquad);
		SquadOrder tanksOrder(squadType, targetLocation, AttackRadius, "Tanks Squad");
		tanksSquad.setSquadOrder(tanksOrder);
		//_mainAttackTarget = targetLocation;
		//InformationManager::Instance().setMainAttackPoint(targetLocation);
	}

	if (!mainAttackSquad.isEmpty()) {
		if (_mainAttackTarget.isValid() && (_mainAttackTarget.x > 0 && _mainAttackTarget.y > 0)){
			targetLocation = _mainAttackTarget;
		}
		else {
			targetLocation = getTargetLocation(&mainAttackSquad);
		}
		
		SquadOrder mainAttackOrder(squadType, targetLocation, AttackRadius, "Attack enemy base");
		mainAttackSquad.setSquadOrder(mainAttackOrder);
		//_mainAttackTarget = targetLocation;
		//InformationManager::Instance().setMainAttackPoint(targetLocation);
		/*
		centerLocation = mainAttackSquad.calcCenter();
		targetLocation = mainAttackSquad.getTargetLocation();
		mainAttackLocation = getMainAttackLocation(&mainAttackSquad);//getMainAttackLocation(&flyingSquad)

		if (!targetLocation) {

			if (mainAttackLocation.getDistance(centerLocation) > 15 * 32) {
				targetLocation = MapTools::Instance().getDistancePosition(centerLocation, mainAttackLocation, 6 * 32);
				mainAttackSquad.setTargetLocation(targetLocation);
			}
		}
		
		if (!_goAggressive) {
			SquadOrder mainAttackOrder(SquadOrderTypes::Attack, mainAttackLocation, AttackRadius, "Attack enemy base");
			mainAttackSquad.setSquadOrder(mainAttackOrder);
		}
		else {
			if (centerLocation.getDistance(targetLocation) > 4 * 32) {
				SquadOrder mainAttackOrder(SquadOrderTypes::Attack, targetLocation, AttackRadius, "Attack enemy base");
				mainAttackSquad.setSquadOrder(mainAttackOrder);
			}
		}
		*/
	}

	if (!flyingSquad.isEmpty()) {
		targetLocation = getTargetLocation(&flyingSquad);
		SquadOrder flyingAttackOrder(squadType, targetLocation, AttackRadius, "Attack enemy base");
		flyingSquad.setSquadOrder(flyingAttackOrder);
		/*
		centerLocation = flyingSquad.calcCenter();
		targetLocation = flyingSquad.getTargetLocation();

		if (!targetLocation) {
			mainAttackLocation = getMainAttackLocation(&flyingSquad);//getMainAttackLocation(&flyingSquad)

			if (mainAttackLocation.getDistance(centerLocation) > 15 * 32) {
				targetLocation = MapTools::Instance().getDistancePosition(centerLocation, mainAttackLocation, 6 * 32);
				flyingSquad.setTargetLocation(targetLocation);
			}
		}

		if (centerLocation.getDistance(targetLocation) > 4 * 32) {
			SquadOrder flyingAttackOrder(SquadOrderTypes::Attack, mainAttackLocation, AttackRadius, "Attack enemy base");
			flyingSquad.setSquadOrder(flyingAttackOrder);
		}
		*/
	}

	//idleSquad.setSquadOrder(mainAttackOrder);
}

void CombatCommander::updateRepairSquad()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran) return;

	return;

	//if (BWAPI::Broodwar->getFrameCount() < 7 * 60 * 24) return;

	Squad & repairSquad = _squadData.getSquad("Repair");

	BWAPI::Position defensivePosition = InformationManager::Instance().getDefensivePosition();
	bool enemyIsNear = false;

	if (BWAPI::Broodwar->getFrameCount() < 4000){

		for (const auto unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (!unit->getType().isBuilding() || !unit->isCompleted()) continue;

			bool isUrgent = false;

			if (unit->isUnderAttack()) {
				isUrgent = true;
			}
			else {
				BWAPI::Unit enemy = BWAPI::Broodwar->getClosestUnit(unit->getPosition(), BWAPI::Filter::IsEnemy &&
					BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Zergling, 
					4 * 32);
				if (enemy) {
					isUrgent = true;
				}
			}

			if (isUrgent) {
				BWAPI::Unit repairWorker = WorkerManager::Instance().getClosestMineralWorkerTo(unit, isUrgent);
				//setRepairWorkerToTarget(unit, 2);
			}
		}
	}
	else {
		// 1. Is any enemy unit within a small radius?
		//1。在一个小半径内是否有敌方单位？
		BWAPI::Unitset enemiesNear;// = BWAPI::Broodwar->getUnitsInRadius(defensivePosition, 12 * 32, BWAPI::Filter::IsEnemy);
		MapGrid::Instance().getUnits(enemiesNear, defensivePosition, 12 * 32, false, true);
		if (enemiesNear.size() > 4)
		{
			enemyIsNear = true;

			if (repairSquad.isEmpty() || repairSquad.getUnits().size() < enemiesNear.size()) {
				BWAPI::Unitset workers = BWAPI::Broodwar->getUnitsInRadius(defensivePosition, 20 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_SCV);
				int nRepairWorker = 0;
				for (auto repairWorker : workers)
				{
					if (repairSquad.getUnits().size() < enemiesNear.size() && repairSquad.getUnits().size() < 6) {
						if (repairWorker == ScoutManager::Instance().getWorkerScout()) {
							ScoutManager::Instance().releaseWorkerScout();
						}
						//WorkerManager::Instance().setRepairWorker(repairWorker, unit);
						++nRepairWorker;

						if (_squadData.canAssignUnitToSquad(repairWorker, repairSquad)) {
							_squadData.assignUnitToSquad(repairWorker, repairSquad);
						}
					}
					else {
						break;
					}
				}
			}

			SquadOrder repairOrder(SquadOrderTypes::Repair, defensivePosition, ReconRadius, "Repair in force");
			repairSquad.setFightVisible(false);
			repairSquad.setSquadOrder(repairOrder);

			BWAPI::Unitset tanks = BWAPI::Broodwar->getUnitsInRadius(defensivePosition, 10 * 32,
				BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode || BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);

			for (auto worker : repairSquad.getUnits()) {
				for (auto tank : tanks) {
					if (tank->getHitPoints() < tank->getType().maxHitPoints() * 0.8) {
						Micro::Repair(worker, tank);
						tanks.erase(tank);
						break;
					}
				}
			}
		}
	}

	/*
	for (auto worker : BWAPI::Broodwar->self()->getUnits()) {
		if (worker->getType().isWorker() && worker->isIdle() && repairSquad.getUnits().size() < 4) {
			if (_squadData.canAssignUnitToSquad(worker, repairSquad)) {
				WorkerManager::Instance().setRepairWorker(worker);
				_squadData.assignUnitToSquad(worker, repairSquad);
			}
		}
	}

	SquadOrder repairOrder(SquadOrderTypes::Repair, defensivePosition, AttackRadius, "Repair Squad");
	repairSquad.setSquadOrder(repairOrder);
	*/
}

bool CombatCommander::assignUnitToSquad(BWAPI::Unit unit, const std::string & squadName) {

	if (!_squadData.squadExists(squadName)) {
		SquadOrder doDrop(SquadOrderTypes::Load, InformationManager::Instance().getDefensivePosition(), AttackRadius, "Wait for transport");
		_squadData.addSquad(Squad(squadName, doDrop, DropPriority));
	}

	Squad & dropSquad = _squadData.getSquad(squadName);

	if (_squadData.canAssignUnitToSquad(unit, dropSquad)) {
		_squadData.assignUnitToSquad(unit, dropSquad);
		return true;
	}

	return false;
}

bool CombatCommander::removeUnitToSquad(BWAPI::Unit unit, const std::string & squadName) {

	if (!_squadData.squadExists(squadName)) {
		return false;
	}

	Squad & dropSquad = _squadData.getSquad(squadName);

	if (dropSquad.containsUnit(unit)) {
		dropSquad.removeUnit(unit);
		return true;
	}

	return false;
}

// Despite the name, this supports only 1 drop squad which has 1 transport.
// Furthermore, it can only drop once and doesn't know how to reset itself to try again.
// Still, it's a start and it can be effective.
//尽管名字, 这支持仅1个下落小队有1运输。
//此外, 它只能下降一次, 不知道如何重新设置自己再试一次。
//不过, 这是一个开始, 它可以是有效的。
void CombatCommander::updateDropSquads()
{
	if (!_squadData.squadExists("Drop")) {
		SquadOrder doDrop(SquadOrderTypes::Load, InformationManager::Instance().getDefensivePosition(), AttackRadius, "Wait for transport");
		_squadData.addSquad(Squad("Drop", doDrop, DropPriority));
	}

	Squad & dropSquad = _squadData.getSquad("Drop");

	SquadOrder dropOrder = SquadOrder(SquadOrderTypes::Load, InformationManager::Instance().getDefensivePosition(), 7 * 32, "Go drop!");
	dropSquad.setSquadOrder(dropOrder);

	return;

	// If we don't have a drop squad, then we don't want to drop.
	// It is created in initializeSquads().
	BWAPI::Position rallyPoint = _mainAttackTarget;// InformationManager::Instance().getRallyPoint();
	BWAPI::Unitset tanks = InformationManager::Instance().getTankUnits();

	int maxDist = 100000;
	BWAPI::Unit closestUnit = nullptr;

	for (auto unit : tanks) {
		int dist = unit->getDistance(rallyPoint);
		if (dist < maxDist) {
			closestUnit = unit;
			maxDist = dist;
		}
	}

	if (closestUnit) {
		rallyPoint = closestUnit->getPosition();
	}

	if (InformationManager::Instance().myHasTransports())
	{
		if (!_squadData.squadExists("Drop")) {
			SquadOrder doDrop(SquadOrderTypes::Load, rallyPoint, AttackRadius, "Wait for transport");
			_squadData.addSquad(Squad("Drop", doDrop, DropPriority));
		}

		Squad & dropSquad = _squadData.getSquad("Drop");

		BWAPI::Unitset transports = InformationManager::Instance().getUnits(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Terran_Dropship);
		for (auto unit : transports) {
			if (unit->getLoadedUnits().size() == 0) {
				if (_squadData.canAssignUnitToSquad(unit, dropSquad)) {
					_squadData.assignUnitToSquad(unit, dropSquad);
					break;
				}
			}
		}

		if (dropSquad.getUnits().size() > 0) {
			SquadOrder dropOrder(SquadOrderTypes::Drop, rallyPoint, AttackRadius, "Go drop!");
			dropSquad.setSquadOrder(dropOrder);
			InformationManager::Instance().setDropPoint(rallyPoint);
		}
	}
	else {
		return;
	}

    //Squad & dropSquad = _squadData.getSquad("Drop");

	// The squad is initialized with a Hold order.
	// There are 3 phases, and in each phase the squad is given a different order:
	// Collect units (Hold); load the transport (Load); go drop (Drop).
	// If it has already been told to go, we are done.
	if (dropSquad.getSquadOrder().getType() != SquadOrderTypes::Hold &&
		dropSquad.getSquadOrder().getType() != SquadOrderTypes::Load)
	{
		return;
	}

    // What units do we have, what units do we need?
	BWAPI::Unit transportUnit = nullptr;
    int transportSpotsRemaining = 8;      // all transports are the same size
	bool anyUnloadedUnits = false;
	const auto & dropUnits = dropSquad.getUnits();

    for (const auto unit : dropUnits)
    {
		if (unit->exists())
		{
			if (unit->isFlying() && unit->getType().spaceProvided() > 0)
			{
				transportUnit = unit;
			}
			else
			{
				transportSpotsRemaining -= unit->getType().spaceRequired();
				if (!unit->isLoaded())
				{
					anyUnloadedUnits = true;
				}
			}
		}
    }

	if (transportUnit && transportSpotsRemaining == 0)
	{
		if (anyUnloadedUnits)
		{
			// The drop squad is complete. Load up.
			// See Squad::loadTransport().
			SquadOrder loadOrder(SquadOrderTypes::Load, rallyPoint, AttackRadius, "Load up");
			dropSquad.setSquadOrder(loadOrder);
		}
		else
		{
			// We're full. Change the order to Drop.
			//我们都饱了更改要删除的订单。
			/*
			BWAPI::Position position = InformationManager::Instance().getEnemyMainBaseLocation()
				? InformationManager::Instance().getEnemyMainBaseLocation()->getPosition()
				: getMainAttackLocation(&dropSquad);
			
			//BWAPI::Position target(3580, 320);
			position = BWAPI::Position(target.x - 4 * 32, target.y - 2 * 32);
			*/
			//BWAPI::TilePosition position = MapPosition::Instance().getUnloadPosition();

			SquadOrder dropOrder = SquadOrder(SquadOrderTypes::Drop, BWAPI::Position(rallyPoint), 7 * 32, "Go drop!");
			dropSquad.setSquadOrder(dropOrder);

			/*
			for (auto unit : transportUnit->getLoadedUnits()) {
				dropSquad.removeUnit(unit);
			}
			*/
		}
	}
	else
    {
		// The drop squad is not complete. Look for more units.
        for (const auto unit : _combatUnits)
        {
            // If the squad doesn't have a transport, try to add one.
			if (!transportUnit &&
				unit->getType().spaceProvided() > 0 && unit->isFlying() &&
				_squadData.canAssignUnitToSquad(unit, dropSquad))
            {
                _squadData.assignUnitToSquad(unit, dropSquad);
				transportUnit = unit;
            }

            // If the unit fits and is good to drop, add it to the squad.
			// Rewrite unitIsGoodToDrop() to select the units of your choice to drop.
			// Simplest to stick to units that occupy the same space in a transport, to avoid difficulties
			// like "add zealot, add dragoon, can't add another dragoon--but transport is not full, can't go".
			//如果该单位适合和是好的下降, 添加到小队。
			//重写 unitIsGoodToDrop() 以选择要删除的选项的单位。
			//最简单的坚持在运输中占用相同空间的单位, 以避免困难
			//像  "添加狂热分子, 添加骑兵, 不能添加另一个骑兵-但运输是不完整的, 不能去 "。
			else if (unit->getType().spaceRequired() <= transportSpotsRemaining &&
				unitIsGoodToDrop(unit) && unit->getDistance(rallyPoint) < 10 * 32 &&
				_squadData.canAssignUnitToSquad(unit, dropSquad))
            {
				//_squadData.assignUnitToSquad(unit, dropSquad);
                transportSpotsRemaining -= unit->getType().spaceRequired();
            }
        }
    }
}

//更新侦察兵防御小队
void CombatCommander::updateScoutDefenseSquad() 
{
	if (Config::Micro::ScoutDefenseRadius == 0 || _combatUnits.empty())
    { 
        return; 
    }

    // if the current squad has units in it then we can ignore this
	//如果目前的阵容中有单位, 那么我们可以忽略这一点
    Squad & scoutDefenseSquad = _squadData.getSquad("ScoutDefense");
  
    // get the region that our base is located in
	/*
    BWTA::Region * myRegion = BWTA::getRegion(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition());

    if (!myRegion || !myRegion->getCenter().isValid())
    {
        return;
    }
	*/

	BWAPI::Unitset enemyUnitsInRegion;
	for (BWTA::Region * myRegion : InformationManager::Instance().getOccupiedRegions(BWAPI::Broodwar->self()))
	{
		// get all of the enemy units in this region
		//获取这个地区所有的敌军单位
		for (auto enemys : BWAPI::Broodwar->enemies()) {
			for (const auto unit : enemys->getUnits()) {
				if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
				{
					enemyUnitsInRegion.insert(unit);
				}
			}
		}
	}

    // if there's an enemy worker in our region then assign someone to chase him
	//如果我们地区有个敌人，就派人去追他。
    bool assignScoutDefender = enemyUnitsInRegion.size() >= 1 && (*enemyUnitsInRegion.begin())->getType().isWorker();

    // if our current squad is empty and we should assign a worker, do it
	//如果我们现在的队伍是空的，我们应该分配一个工人，那就去做。
    if (scoutDefenseSquad.isEmpty() && assignScoutDefender)
    {
        // the enemy worker that is attacking us
        BWAPI::Unit enemyWorker = *enemyUnitsInRegion.begin();

        // get our worker unit that is mining that is closest to it
        BWAPI::Unit workerDefender = findClosestWorkerToTarget(_combatUnits, enemyWorker);

		if (enemyWorker && workerDefender)
		{
			// grab it from the worker manager and put it in the squad
            if (_squadData.canAssignUnitToSquad(workerDefender, scoutDefenseSquad))
            {
				WorkerManager::Instance().setCombatWorker(workerDefender);
                _squadData.assignUnitToSquad(workerDefender, scoutDefenseSquad);
				workerDefender->attack(enemyWorker);
            }
		}
    }
    // if our squad is not empty and we shouldn't have a worker chasing then take it out of the squad
    else if (!scoutDefenseSquad.isEmpty() && !assignScoutDefender)
    {
        scoutDefenseSquad.clear();     // also releases the worker
    }

	if (!scoutDefenseSquad.isEmpty()) {
		BWAPI::Unit enemyWorker = *enemyUnitsInRegion.begin();
		if (enemyWorker) {
			scoutDefenseSquad.getUnits().attack(enemyWorker);
		}
	}
}

//占领区防御部队
void CombatCommander::updateBaseDefenseSquads() 
{
	if (_combatUnits.empty()) 
    { 
        return; 
    }

	//_squadData.clearEmptySquad();
    
    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();
    BWTA::Region * enemyRegion = nullptr;
    if (enemyBaseLocation)
    {
        enemyRegion = BWTA::getRegion(enemyBaseLocation->getPosition());
    }

	bool isUnderAttack = false;
	BWAPI::Position underAttackPosition;
	//Squad & defenseSquad = _squadData.getSquad("Ground");

	// for each of our occupied regions
	//为我们每个被占领的地区
	for (BWTA::Region * myRegion : InformationManager::Instance().getOccupiedRegions(BWAPI::Broodwar->self()))
	{
        // don't defend inside the enemy region, this will end badly when we are stealing gas
        if (myRegion == enemyRegion)
        {
            continue;
        }

		BWAPI::Position regionCenter = myRegion->getCenter();
		//修改 防御中心
		//BWAPI::Position regionCenter = InformationManager::Instance().getDefensivePosition();
		if (!regionCenter.isValid())
		{
			continue;
		}

		// start off assuming all enemy units in region are just workers
		const int numDefendersPerEnemyUnit = 2;

		// all of the enemy units in this region
		BWAPI::Unitset enemyUnitsInRegion;
		for (auto enemy : BWAPI::Broodwar->enemies()) {
			for (const auto unit : enemy->getUnits()) {
				// If it's a harmless air unit, don't worry about it for base defense.
				// TODO something more sensible
				if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord ||
					unit->getType() == BWAPI::UnitTypes::Protoss_Observer ||
					unit->isLifted())  // floating terran building
				{
					continue;
				}

				if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
				{
					enemyUnitsInRegion.insert(unit);
				}
			}
		}

		if (enemyUnitsInRegion.size() == 0) continue;

        // we ignore the first enemy worker in our region since we assume it is a scout
		// This is because we can't catch it early. Should skip this check when we can. 
		// TODO replace with something sensible
		/*
        for (const auto unit : enemyUnitsInRegion)
        {
            if (unit->getType().isWorker())
            {
                enemyUnitsInRegion.erase(unit);
                continue;
            }
        }
		*/
        std::stringstream squadName;
        squadName << "Base Defense " << regionCenter.x << " " << regionCenter.y; 
        
		// if there's nothing in this region to worry about
        if (enemyUnitsInRegion.empty())
        {
            // if a defense squad for this region exists, empty it
            if (_squadData.squadExists(squadName.str()))
            {
				_squadData.getSquad(squadName.str()).clear();
				//_squadData.removeSquad(squadName.str());
			}
            
            // and return, nothing to defend here
            continue;
        }
        else 
        {
            // if we don't have a squad assigned to this region already, create one
			//如果我们没有一个小队已经分配到这个地区, 创建一个
            if (!_squadData.squadExists(squadName.str()))
            {
				//regionCenter = InformationManager::Instance().getDefensivePosition();
				SquadOrder defendRegion(getFrameSquadOrderTypes(), regionCenter, AttackRadius, "Defend region");
                _squadData.addSquad(Squad(squadName.str(), defendRegion, BaseDefensePriority));
			}
        }

		int numEnemyFlyingInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return u->isFlying(); });
		int numEnemyGroundInRegion = enemyUnitsInRegion.size() - numEnemyFlyingInRegion;

		// assign units to the squad
		UAB_ASSERT(_squadData.squadExists(squadName.str()), "Squad should exist: %s", squadName.str().c_str());
		Squad & defenseSquad = _squadData.getSquad(squadName.str());

        // figure out how many units we need on defense
		//算出我们在防守上需要多少单位
	    int flyingDefendersNeeded = numDefendersPerEnemyUnit * numEnemyFlyingInRegion;
	    int groundDefendersNeeded = numDefendersPerEnemyUnit * numEnemyGroundInRegion;

		// Count static defense as air defenders.
		// Ignore bunkers; they're more complicated.
		//计数静态防御作为空气防御者。
		//忽略地堡; 它们更复杂。
		for (const auto unit : BWAPI::Broodwar->self()->getUnits()) {
			if ((unit->getType() == BWAPI::UnitTypes::Terran_Missile_Turret ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Spore_Colony) &&
				unit->isCompleted() && unit->isPowered() &&
				BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
			{
				flyingDefendersNeeded -= 3;
			}
		}

		flyingDefendersNeeded = std::max(flyingDefendersNeeded, 0);

		// Count static defense as ground defenders.
		// Ignore bunkers; they're more complicated.
		// Cannons are double-counted as air and ground, which can be a mistake.
		//计数静态防御作为地面防御者。
		//忽略地堡; 它们更复杂。
		//大炮被双重计算为空气和地面, 这可能是一个错误。
		bool sunkenDefender = false;
		for (const auto unit : BWAPI::Broodwar->self()->getUnits()) {
			if ((unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony) &&
				unit->isCompleted() && unit->isPowered() &&
				BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
			{
				sunkenDefender = true;
				groundDefendersNeeded -= 4;
			}

			if (unit->isUnderAttack()) {
				isUnderAttack = true;
				underAttackPosition = unit->getPosition();
			}
		}
		groundDefendersNeeded = std::max(groundDefendersNeeded, 0);

		// Pull workers only in narrow conditions.
		// Pulling workers (as implemented) can lead to big losses.
		//只在狭窄的条件下拉工人。
		//拉动工人(如实施) 可能导致巨大损失。
		bool pullWorkers =
			Config::Micro::WorkersDefendRush &&
			(BWAPI::Broodwar->getFrameCount() < 4 * 60 * 24 || numZerglingsInOurBase() > 0 || buildingRush());

		updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefendersNeeded, pullWorkers);
    }

    // for each of our defense squads, if there aren't any enemy units near the position, clear the squad
	// TODO partially overlaps with "is enemy in region check" above
	//对于我们的每一个防守小队, 如果在阵地附近没有敌方部队, 请清除小队
	//	TODO 部分与  "在区域检查 " 中的敌人重叠
	//_squadData.clearEmptySquad();
	
	for (const auto & kv : _squadData.getSquads())
	{
		const Squad & squad = kv.second;
		const SquadOrder & order = squad.getSquadOrder();

		/*
		if (order.getType() != SquadOrderTypes::Attack || squad.isEmpty())
		{
			continue;
		}
		*/

		bool enemyUnitInRange = false;
		for (auto enemy : BWAPI::Broodwar->enemies()) {
			for (const auto unit : enemy->getUnits()) {
				if (unit->getDistance(order.getPosition()) < order.getRadius())
				{
					enemyUnitInRange = true;
					underAttackPosition = unit->getPosition();
					break;
				}
			}
		}

		if (!enemyUnitInRange)
		{
			//_squadData.getSquad(squad.getName()).clear();
			std::string squadName = squad.getName();
			std::size_t found = squadName.find("Base Defense");
			if (found == 0) {
				_squadData.removeSquad(squad.getName());
			}
			return;
		}
		else {
			//squad.getUnits().attack(underAttackPosition);
		}
	}

	/*
	if (isUnderAttack && underAttackPosition.isValid() && !defenseSquad.isEmpty()) {
		for (auto unit : defenseSquad.getUnits()) {
			if (unit->getDistance(underAttackPosition) > unit->getType().groundWeapon().maxRange()) {
				Micro::AttackMove(unit, underAttackPosition);
			}
		}
	}
	*/
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded, bool pullWorkers)
{
	// if there's nothing left to defend, clear the squad
	if (flyingDefendersNeeded == 0 && groundDefendersNeeded == 0)
	{
		defenseSquad.clear();
		return;
	}

	const BWAPI::Unitset & squadUnits = defenseSquad.getUnits();

	// NOTE Defenders can be double-counted as both air and ground defenders. It can be a mistake.
	size_t flyingDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackAir);
	size_t groundDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackGround);

	// add flying defenders if we still need them
	size_t flyingDefendersAdded = 0;
	BWAPI::Unit defenderToAdd;
	while (flyingDefendersNeeded > flyingDefendersInSquad + flyingDefendersAdded &&
		(defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), true, false)))
	{
		UAB_ASSERT(!defenderToAdd->getType().isWorker(), "flying worker defender");
		_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
		++flyingDefendersAdded;
	}

	// add ground defenders if we still need them
	size_t groundDefendersAdded = 0;
	while (groundDefendersNeeded > groundDefendersInSquad + groundDefendersAdded &&
		(defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), false, pullWorkers)))
	{
		if (defenderToAdd->getType().isWorker())
		{
			UAB_ASSERT(pullWorkers, "pulled worker defender mistakenly");
			WorkerManager::Instance().setCombatWorker(defenderToAdd);
		}
		_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
		++groundDefendersAdded;
	}
}

// Choose a defender to join the base defense squad.
//选择一个后卫加入基地防御小队。
BWAPI::Unit CombatCommander::findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender, bool pullWorkers)
{
	BWAPI::Unit closestDefender = nullptr;
	int minDistance = 99999;

	for (const auto unit : _combatUnits) 
	{
		if ((flyingDefender && !UnitUtil::CanAttackAir(unit)) ||
			(!flyingDefender && !UnitUtil::CanAttackGround(unit)))
        {
            continue;
        }

        if (!_squadData.canAssignUnitToSquad(unit, defenseSquad))
        {
            continue;
        }

		int dist = unit->getDistance(pos);

		// Pull workers only if requested, and not from distant bases.
		if (unit->getType().isWorker() && (!pullWorkers || dist > 30 * 32))
        {
            continue;
        }

		if (dist < minDistance)
        {
            closestDefender = unit;
            minDistance = dist;
        }
	}

	return closestDefender;
}

void CombatCommander::setDefenseSquadUnits(BWAPI::Position pos, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded, bool pullWorkers)
{
	BWAPI::Unit closestDefender = nullptr;
	int minDistance = 99999;

	std::stringstream squadName;
	squadName << "Base Defense " << pos.x << " " << pos.y;

	if (!_squadData.squadExists(squadName.str()))
	{
		SquadOrder defendRegion(getFrameSquadOrderTypes(), pos, 25 * 32, "Defend region");
		_squadData.addSquad(Squad(squadName.str(), defendRegion, BaseDefensePriority));
	}

	Squad & defenseSquad = _squadData.getSquad(squadName.str());

	updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefendersNeeded, pullWorkers);
}

void CombatCommander::updateBunkerRush() {
	Squad & reconScoutSquad = _squadData.getSquad("ReconScout");//农民侦察小队

	int gameFrameCount = BWAPI::Broodwar->getFrameCount();
	if (gameFrameCount < 4000 && gameFrameCount > _lastBuildingChange + 12) {
		if (_rushBunker) return;

		//if (gameFrameCount % 8 == 0) {
		BWAPI::TilePosition bunkerPosition;
		BWAPI::Unitset zergHatchery;

		std::vector<BWAPI::TilePosition> rushBunkerPosition = MapPosition::Instance().getRushBunkerPosition();
		for (auto pos: rushBunkerPosition) {
			zergHatchery = BWAPI::Broodwar->getUnitsInRadius(BWAPI::Position(pos), 6 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Hatchery);
			if (zergHatchery.size() > 0) {
				bunkerPosition = pos;
				break;
			}
		}
		//BWAPI::Unitset zergHatchery = BWAPI::Broodwar->getUnitsInRadius(BWAPI::Position(bunkerPosition), 6 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Hatchery);
		
		if (zergHatchery.size() > 0 && !_builderUnit){
		//if (!_builderUnit){
			int maxDist = 20 * 32;
			for (auto worker : BWAPI::Broodwar->self()->getUnits()) {
				int dist = worker->getDistance(BWAPI::Position(bunkerPosition));
				if (dist < maxDist) {
					_builderUnit = worker;
					maxDist = dist;
				}
			}

			if (!_builderUnit) return;

			if (_builderUnit == ScoutManager::Instance().getWorkerScout()) {
				ScoutManager::Instance().releaseWorkerScout();
			}
			else {
				reconScoutSquad.removeUnit(_builderUnit);
			}

			_reconScoutTarget = bunkerPosition;

			InformationManager::Instance().setDefensivePosition(BWAPI::Position(bunkerPosition));
			_goAggressive = false;

			WorkerManager::Instance().setMoveWorker(_builderUnit, 10, 0, BWAPI::Position(bunkerPosition));

			MacroAct act("Bunker", bunkerPosition);
			StrategyManager::Instance().addMacroAct(act);
			//ProductionManager::Instance().setWaitProductionFrame(gameFrameCount + 2 * 24);
		}

		if (_builderUnit && _builderUnit->getDistance(BWAPI::Position(bunkerPosition)) < 4 * 32) {
			//reconScoutSquad.removeUnit(_builderUnit);
		}

		if (_builderUnit) {
			if (_builderUnit->isConstructing()) {
				_rushBunker = _builderUnit->getOrderTarget();
				_lastBuildingChange = BWAPI::Broodwar->getFrameCount();
			}
			else {

				/*
				Building b(BWAPI::UnitTypes::Terran_Bunker, bunkerPosition);
				b.macroLocation = MacroLocation::Absolute;
				b.builderUnit = _builderUnit;
				b.buildCommandGiven = false;

				if (!BWAPI::Broodwar->isExplored(bunkerPosition)) {
					Micro::Move(_builderUnit, BWAPI::Position(bunkerPosition));
				}
				else {
					//reconScoutSquad.removeUnit(_builderUnit);
					b.buildCommandGiven = b.builderUnit->build(b.type, bunkerPosition);
					WorkerManager::Instance().setBuildingWorker(_builderUnit, b);
				}

				if (b.buildCommandGiven) {
					//groundSquad.assignUnitToSquad(_builderUnit);
					//reconScoutSquad.removeUnit(_builderUnit);
				}
				*/
				//_builderUnit->build(BWAPI::UnitTypes::Terran_Bunker, defensePosition);
				_lastBuildingChange = BWAPI::Broodwar->getFrameCount();
			}
		}
	}

	if (_rushBunker && _rushBunker->isCompleted()) {
		//reconScoutSquad.removeUnit(_builderUnit);
		ScoutManager::Instance().setWorkerScout(_builderUnit);
		WorkerManager::Instance().setRepairWorker(_builderUnit, _rushBunker);

		Squad & repairSquad = _squadData.getSquad("Repair");//维修部队
		if (_squadData.canAssignUnitToSquad(_builderUnit, repairSquad)) {
			_squadData.assignUnitToSquad(_builderUnit, repairSquad);
		}

		SquadOrder repairOrder(SquadOrderTypes::Repair, BWAPI::Position(_reconScoutTarget), ReconRadius, "Repair in force");
		repairSquad.setSquadOrder(repairOrder);
		repairSquad.setFightVisible(false);

		_multiplierAggressive = 0.5;
		InformationManager::Instance().setDefensivePosition(BWAPI::Position(_reconScoutTarget));
	}
}

// NOTE This implementation is kind of cheesy. Orders ought to be delegated to a squad.
//注意这个实现是有点俗气。命令应该被委派给一个小队。
void CombatCommander::loadOrUnloadBunkers()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	bool isUnderAttack = false;
	BWAPI::Unit lurker;
	
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->isUnderAttack()) {
			isUnderAttack = true;
		}
	}
	
	Squad & repairSquad = _squadData.getSquad("Repair");//维修部队

	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Terran_Bunker)
		{
			//BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 10 * 32, BWAPI::Colors::Cyan);
			//BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 16 * 32, BWAPI::Colors::Orange);

			// Are there enemies close to the bunker?
			int radius = 5 * 32;
			if (_self->getUpgradeLevel(BWAPI::UpgradeTypes::U_238_Shells)) {
				radius = 6 * 32;
			}

			//处理lurker近身的问题
			lurker = BWAPI::Broodwar->getClosestUnit(unit->getPosition(), BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Lurker, radius);
			if (lurker && lurker->isVisible() &&
				(!lurker->isDetected() || lurker->getOrder() == BWAPI::Orders::Burrowing) &&
				lurker->getPosition().isValid()) {

				BWAPI::Unitset comsatStation = InformationManager::Instance().getComsatStation();
				for (auto comsat : comsatStation) {
					if (Micro::Scan(comsat, lurker->getPosition())) break;
				}
			}

			bool enemyIsNear = false;

			// 1. Is any enemy unit within a small radius?
			//1。在一个小半径内是否有敌方单位？
			BWAPI::Unitset enemiesNear = BWAPI::Broodwar->getUnitsInRadius(unit->getPosition(), 8 * 32, BWAPI::Filter::IsEnemy);
			if (enemiesNear.empty())
			{
				// 2. Is a fast enemy unit within a wider radius?
				//2。在更宽的半径范围内是一个快速的敌人单位吗？
				enemiesNear = BWAPI::Broodwar->getUnitsInRadius(unit->getPosition(), 12 * 32,
					BWAPI::Filter::IsEnemy &&
					(BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Vulture ||
					BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Mutalisk)
					);
				enemyIsNear = !enemiesNear.empty();
			}
			else
			{
				enemyIsNear = true;
			}

			/*
			if (enemyIsNear && enemiesNear.size() > 6) {

				BWAPI::Unitset workers = BWAPI::Broodwar->getUnitsInRadius(
					unit->getPosition(), 12 * 32,
					BWAPI::Filter::IsOwned && BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_SCV
					);

				int size = 0;

				for (auto worker : workers) {
					WorkerManager::Instance().setRepairWorker(worker, unit);
					if (unit->getSpaceRemaining() == 0) {
						//worker->rightClick(unit);
						Micro::RightClick(worker, unit);
					}
					
					++size;
					if (size >= enemiesNear.size() / 3 || size >= 4) break;
				}
			}
			*/

			BWAPI::Unit marine = BWAPI::Broodwar->getClosestUnit(
				unit->getPosition(),
				BWAPI::Filter::IsOwned && (BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Marine || BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Firebat),
				12 * 32);

			if (enemyIsNear)
			{
				// Load one marine at a time if there is free space.
				if (unit->getSpaceRemaining() > 0)
				{
					if (marine)
					{
						//敌人接近或处于防守
						unit->load(marine);
					}
				}
			}
			else if (isUnderAttack) {
				for (auto loader : unit->getLoadedUnits()) {
					if (loader->getHitPoints() > loader->getType().maxHitPoints() * 0.8 && unit->getSpaceRemaining() < 2) {
						unit->unload(loader);
						break;
					}
				}
				//unit->unloadAll();
			}
			else if (!_goAggressive) {
				if (unit->getSpaceRemaining() > 2) {
					unit->load(marine);
				}
			}
			else {
				//unit->unloadAll();
			}

			//维修
			if (unit->isCompleted() && (enemyIsNear || unit->isUnderAttack() || unit->getHitPoints() < unit->getType().maxHitPoints())) {
				setRepairWorkerToTarget(unit, enemiesNear.size() / 2);
				/*
				if (repairSquad.isEmpty() || repairSquad.getUnits().size() < enemiesNear.size()) {
					BWAPI::Unitset workers = unit->getUnitsInRadius(15 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_SCV);
					int nRepairWorker = repairSquad.getUnits().size();
					for (auto repairWorker : workers)
					{
						if (nRepairWorker < enemiesNear.size() && nRepairWorker < 6) {
							WorkerManager::Instance().setCombatWorker(repairWorker);
							
							if (_squadData.canAssignUnitToSquad(repairWorker, repairSquad)) {
								_squadData.assignUnitToSquad(repairWorker, repairSquad);
								++nRepairWorker;
							}
						}
						else {
							break;
						}
					}
				}

				SquadOrder repairOrder(SquadOrderTypes::Repair, unit->getPosition(), ReconRadius, "Repair in force");
				repairSquad.setFightVisible(false);
				repairSquad.setSquadOrder(repairOrder);

				//repairSquad.getUnits().repair(unit);
				for (auto worker : repairSquad.getUnits()) {
					if (worker->isRepairing()) continue;

					if (unit->getSpaceRemaining() == 0) {
						Micro::RightClick(worker, unit);
					}
					else {
						Micro::Repair(worker, unit);
					}
				}
				*/
			}
			else {
				if (!repairSquad.isEmpty()) {
					repairSquad.clear();
				}
			}
		}
	}
}

void CombatCommander::setRepairWorkerToTarget(BWAPI::Unit target, int num) {
	Squad & repairSquad = _squadData.getSquad("Repair");//维修部队

	if (repairSquad.isEmpty() || repairSquad.getUnits().size() < num) {
		//BWAPI::Unitset workers = target->getUnitsInRadius(40 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_SCV);
		int nRepairWorker = repairSquad.getUnits().size();

		for (int nRepairWorker = repairSquad.getUnits().size(); nRepairWorker < num; ++nRepairWorker) {
			BWAPI::Unit repairWorker = WorkerManager::Instance().getClosestTargetWorkerTo(target->getPosition(), true);
			if (repairWorker->getOrder() == BWAPI::Orders::Repair) continue;

			if (_squadData.canAssignUnitToSquad(repairWorker, repairSquad)) {
				if (repairWorker == ScoutManager::Instance().getWorkerScout()) {
					//ScoutManager::Instance().releaseWorkerScout();
					continue;
				}
				//WorkerManager::Instance().setRepairWorker(repairWorker, target);
				WorkerManager::Instance().setCombatWorker(repairWorker);
				_squadData.assignUnitToSquad(repairWorker, repairSquad);
			}
		}
	}

	SquadOrder repairOrder(SquadOrderTypes::Repair, target->getPosition(), ReconRadius, "Repair in force");
	repairSquad.setFightVisible(false);
	repairSquad.setSquadOrder(repairOrder);

	//repairSquad.getUnits().repair(unit);
	for (auto worker : repairSquad.getUnits()) {
		if (worker->getDistance(target) > 3 * 32) {
			Micro::RightClick(worker, target);
		}
		else {
			if (target->getType() == BWAPI::UnitTypes::Terran_Bunker && target->getSpaceRemaining() == 0) {
				Micro::RightClick(worker, target);
			}
			else {
				Micro::Repair(worker, target);
			}
		}
	}
}

//立即进攻
void CombatCommander::attackNow() {
	setMultiplierAggressive(0.1);
	setAggression(true);
	setAggressiveScore(100);
	BWAPI::Broodwar->sendText("go");
}
//立即防守
void CombatCommander::defenseNow() {
	setMultiplierAggressive(10);
	setAggression(false);
	setAggressiveScore(10000);
}

//是否可以进攻
void CombatCommander::isAggressive() {
	double selfFightScore = InformationManager::Instance().getSelfFightScore();
	double enemyFightScore = InformationManager::Instance().getEnemyFightScore();

	if (selfFightScore > getAggressiveScore() && selfFightScore >= enemyFightScore * _multiplierAggressive) {
		setAggression(true);
	}
	else {
		setAggression(false);
	}
}

// Set _openingGroup depending on the current strategy, which in principle
// might be from the config file or from opening learning.
// This is part of initialization; it happens early on.
void CombatCommander::setAggressiveScore(int score)
{
	if (score >= 0) {
		_aggressiveScore = score;
	}
}

const int & CombatCommander::getAggressiveScore() const
{
	return _aggressiveScore;
}

// Scan enemy cloaked units.
//扫描敌方隐形单位。
void CombatCommander::doComsatScan()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Comsat_Station) == 0)
	{
		return;
	}

	BWAPI::Unitset comsatStation = InformationManager::Instance().getComsatStation();

	bool isScan = false;
	// Does the enemy have undetected cloaked units that we may be able to engage?
	//敌人是否有未探测到的隐形单位，我们可以参与?
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->isVisible() &&
			(!unit->isDetected() || unit->getOrder() == BWAPI::Orders::Burrowing) &&
			unit->getPosition().isValid())
		{
			BWAPI::Unitset tanks = BWAPI::Broodwar->getUnitsInRadius(unit->getPosition(), 12 * 32, BWAPI::Filter::IsOwned && BWAPI::Filter::CanAttack);
			if (tanks.size() > 0) {
				for (auto tank : tanks) {
					if (tank->getType().groundWeapon().maxRange() >= unit->getType().groundWeapon().maxRange()) {
						isScan = true;
						break;
					}
				}
			}
			else {
				BWAPI::Unit bunker = BWAPI::Broodwar->getClosestUnit(unit->getPosition(), BWAPI::Filter::IsOwned && BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Bunker, 5 * 32);
				if (bunker && bunker->getSpaceRemaining() < 2) {
					isScan = true;
				}
			}

			if (isScan || unit->getType() != BWAPI::UnitTypes::Zerg_Lurker) {
				// At most one scan per call. We don't check whether it succeeds.
				for (auto comsat : comsatStation) {
					if (Micro::Scan(comsat, unit->getPosition())) {
						_lastScanTargetChange = BWAPI::Broodwar->getFrameCount() + 15 * 24;
						break;
					}
				}
				break;
			}
		}
	}

	if (BWAPI::Broodwar->getFrameCount() > _lastScanTargetChange) {

		BWAPI::Position pos;
		int numEnergy = 0; //能量值
		int frameChange = 0;//时间值

		for (auto comsat : comsatStation) {
			//如果还没有找到敌人出生点，则先扫描
			if (InformationManager::Instance().getEnemyMainBaseLocation()) {
				if (BWAPI::Broodwar->isExplored(InformationManager::Instance().getEnemyMainBaseLocation()->getTilePosition())) {
					numEnergy = 50 / comsatStation.size();
					frameChange = 20 * 24;
				}
			}

			//如果有隐形单位，则保留一点雷达能量
			if (InformationManager::Instance().enemyHasCloakTech() && BWAPI::Broodwar->getFrameCount() < 8 * 60 * 24) {
				numEnergy = 80 / comsatStation.size();
				frameChange = 30 * 24;
			}

			if (comsat->getEnergy() < 50 + numEnergy) continue;

			if (BWAPI::Broodwar->getFrameCount() < 6 * 60 * 24) {
				pos = InformationManager::Instance().getReconLocation(true);
			}
			else {
				pos = InformationManager::Instance().getReconLocation();
				/*
				if (BWAPI::Broodwar->getFrameCount() % 12 == 0) {
				pos = InformationManager::Instance().getReconLocation();
				}
				else {
				pos = InformationManager::Instance().getReconLocation(true);
				}
				*/
			}

			/*
			std::vector<BWAPI::Position> enemyOccupiedPosition;
			std::set<BWTA::Region *> enemyOccupiedRegions = InformationManager::Instance().getOccupiedRegions(BWAPI::Broodwar->enemy());
			for (BWTA::Region * myRegion : enemyOccupiedRegions)
			{
			enemyOccupiedPosition.push_back(myRegion->getCenter());
			}

			if (enemyOccupiedPosition.size() == 0) {
			pos = InformationManager::Instance().getReconLocation(true);
			if (pos.isValid()) {
			enemyOccupiedPosition.push_back(pos);
			}
			}
			else {
			int index = Random::Instance().index(enemyOccupiedRegions.size());
			pos = enemyOccupiedPosition.at(index);
			InformationManager::Instance().setReconLocation(pos, 30 * 24);
			//BWAPI::Position pos = InformationManager::Instance().getReconLocation();
			pos = MapGrid::Instance().getRandomPosition(pos, 8 * 32);
			}
			*/
			pos = MapPosition::Instance().getRandomPosition(pos, 6 * 32);
			if (pos.isValid() && (!BWAPI::Broodwar->isVisible(BWAPI::TilePosition(pos)))) {
				if (Micro::Scan(comsat, pos)) {
					_lastScanTargetChange = BWAPI::Broodwar->getFrameCount() + frameChange;
					return;
				}
			}
		}
	}
}

// What units do you want to drop into the enemy base from a transport?
//你想从运输中落入敌方基地的什么单位？
bool CombatCommander::unitIsGoodToDrop(const BWAPI::Unit unit) const
{
	return
		//unit->getType() == BWAPI::UnitTypes::Terran_Vulture ||
		unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
		unit->getType() == BWAPI::UnitTypes::Protoss_Reaver;
}

// Get our money back at the last moment for stuff that is about to be destroyed.
// It is not ideal: A building which is destined to die only after it is completed
// will be completed and die.
// Special case for a zerg sunken colony while it is morphing: It will lose up to
// 100 hp when the morph finishes, so cancel if it would be weak when it finishes.
// NOTE See BuildingManager::cancelBuilding() for another way to cancel buildings.
void CombatCommander::cancelDyingItems()
{
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		BWAPI::UnitType type = unit->getType();
		if (unit->isUnderAttack() && type.isBuilding() && unit->getHitPoints() < 30) {
			if (unit->isCompleted()) {
				if (unit->canCancelTrain()) {
					unit->cancelTrain();
				}

				if (unit->canCancelUpgrade()) {
					unit->cancelUpgrade();
				}

				if (unit->canResearch()) {
					unit->cancelResearch();
				}
			}
			else {
				if (unit->isBeingConstructed() && unit->canCancelConstruction())
				{
					unit->cancelConstruction();
				}
			}
		}

		if (unit->isUnderAttack() &&
			(	type.isBuilding() && !unit->isCompleted() ||
				type == BWAPI::UnitTypes::Zerg_Egg ||
				type == BWAPI::UnitTypes::Zerg_Lurker_Egg ||
				type == BWAPI::UnitTypes::Zerg_Cocoon
			) &&
			(	unit->getHitPoints() < 30 ||
				(type == BWAPI::UnitTypes::Zerg_Sunken_Colony && unit->getHitPoints() < 130)
			))
		{
			if (unit->isMorphing() && unit->canCancelMorph())
			{
				unit->cancelMorph();
			}
			else if (unit->isBeingConstructed() && unit->canCancelConstruction())
			{
				unit->cancelConstruction();
			}
		}
	}
}

BWAPI::Position CombatCommander::getDefendLocation()
{
	return BWTA::getRegion(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition())->getCenter();
}

// How good is it to pull this worker for combat?
//把这个工人拉上战场有多好？
int CombatCommander::workerPullScore(BWAPI::Unit worker)
{
	return
		(worker->getHitPoints() == worker->getType().maxHitPoints() ? 10 : 0) +
		(worker->getShields() == worker->getType().maxShields() ? 4 : 0) +
		(worker->isCarryingGas() ? -3 : 0) +
		(worker->isCarryingMinerals() ? -2 : 0);
}

// Pull workers off of mining and into the attack squad.
// The argument n can be zero or negative or huge. Nothing awful will happen.
// Tries to pull the "best" workers for combat, as decided by workerPullScore() above.
//把工人从采矿和攻击小队中拉出来。
//参数 n 可以是零或负或大。不会有什么可怕的事情发生。
//按照上面的 workerPullScore() 的决定, 试图拉下 "最佳 " 工人进行战斗。
void CombatCommander::pullWorkers(int n)
{
	auto compare = [](BWAPI::Unit left, BWAPI::Unit right)
	{
		return workerPullScore(left) < workerPullScore(right);
	};

	std::priority_queue<BWAPI::Unit, std::vector<BWAPI::Unit>, decltype(compare)> workers;

	Squad & groundSquad = _squadData.getSquad("Ground");//Repair

	for (const auto unit : _combatUnits)
	{
		if (unit->getType().isWorker() &&
			WorkerManager::Instance().isFree(unit) &&
			_squadData.canAssignUnitToSquad(unit, groundSquad))
		{
			workers.push(unit);
		}
	}

	int nLeft = n;

	while (nLeft > 0 && !workers.empty())
	{
		BWAPI::Unit worker = workers.top();
		//WorkerManager::Instance().setRepairWorker(worker);
		WorkerManager::Instance().setCombatWorker(worker);
		workers.pop();
		_squadData.assignUnitToSquad(worker, groundSquad);
		--nLeft;
		//worker->rightClick(BWAPI::Position(600, 400));
	}
}

// Release workers from the attack squad.
void CombatCommander::releaseWorkers()
{
	Squad & groundSquad = _squadData.getSquad("Ground");
	groundSquad.releaseWorkers();

	Squad & reconScoutSquad = _squadData.getSquad("ReconScout");
	reconScoutSquad.releaseWorkers();
}

void CombatCommander::drawSquadInformation(int x, int y)
{
	BWAPI::Broodwar->drawTextScreen(x, y - 10, "\x04Squads: %d[%d,%d]", _mainAttackUnits, Config::Strategy::minAttackUnits, Config::Strategy::maxAttackUnits);
	
	_squadData.drawSquadInformation(x, y);
}

BWAPI::Position CombatCommander::getTargetLocation(Squad * squad) {
	BWAPI::Position mainAttackLocation;
	BWAPI::Position centerLocation;
	BWAPI::Position targetLocation;
	int allUnits = squad->getUnits().size();

	mainAttackLocation = getMainAttackLocation(squad);//getMainAttackLocation(&flyingSquad)

	if (_self->getRace() != BWAPI::Races::Terran) {
		return mainAttackLocation;
	}

	//return mainAttackLocation;
	targetLocation = mainAttackLocation;

	if (!squad->isEmpty()) {
		BWAPI::Unit unitClosest = squad->unitClosestToEnemy(mainAttackLocation);

		if (unitClosest) {
			centerLocation = unitClosest->getPosition();

			if (_self->getRace() == BWAPI::Races::Terran) {
				if (squad->getName() == "Tanks") {
					_mainAttackTarget = centerLocation;
					InformationManager::Instance().setMainAttackPoint(_mainAttackTarget);
				}
			}
			else if (squad->getName() == "Ground") {
				_mainAttackTarget = centerLocation;
				InformationManager::Instance().setMainAttackPoint(_mainAttackTarget);
			}
			
			BWAPI::Unitset units;
			MapGrid::Instance().getUnits(units, unitClosest->getPosition(), 6 * 32, true, false);

			if (centerLocation.getDistance(targetLocation) > 15 * 32) {
				if (units.size() > allUnits / 2) {
					for (int i = 1; i < 10; i++) {
						targetLocation = MapTools::Instance().getDistancePosition(centerLocation, mainAttackLocation, (12 + i) * 32);
						if (unitClosest->hasPath(targetLocation)) break;
					}
					/*
					targetLocation = MapTools::Instance().getDistancePosition(centerLocation, mainAttackLocation, 5 * 32);
					if (!unitClosest->hasPath(targetLocation)) {
						targetLocation = MapTools::Instance().getDistancePosition(centerLocation, mainAttackLocation, 16 * 32);
					}
					*/
				}
				else {
					/*
					for (int i = 1; i < 10; i++) {
						targetLocation = MapTools::Instance().getDistancePosition(centerLocation, mainAttackLocation, (6 + i) * 32);
						if (unitClosest->hasPath(targetLocation)) break;
					}
					*/
					targetLocation = unitClosest->getPosition();
				}
			}
		}
	}

	if (!targetLocation.isValid() || (targetLocation.x == 0 || targetLocation.y == 0)) {
		targetLocation = mainAttackLocation;
	}

	squad->setTargetLocation(targetLocation);
	return targetLocation;
}

// Choose a point of attack for the given squad (which may be null).
//为给定的小队 (可能为 null) 选择攻击点。
BWAPI::Position CombatCommander::getMainAttackLocation(const Squad * squad)
{
	// 0. If we're defensive, look for a front line to hold. No attacks.
	//0。如果我们是防守的, 找一条前线等待。没有攻击。
	if (!_goAggressive)
	{
		//如果指定了防守点
		if (InformationManager::Instance().getDefensivePosition().getLength() > 0) return InformationManager::Instance().getDefensivePosition();

		//修改：在最近的塞口防守
		BWAPI::Position chokepoint = InformationManager::Instance().getRallyPoint();
		return chokepoint;
		/*
		double S = sqrt(pow(first.x - second.x, 2) + pow(first.y - second.y, 2));
		
		BWAPI::Position chokepoint = MapTools::Instance().getDistancePosition(first, second, 0 - S / 2);//中心点
		*/
		/*
		if (BWAPI::Broodwar->getFrameCount() > 10000 && InformationManager::Instance().getSelfFightScore() > 2000) {
			chokepoint = BWAPI::Position(790, 1160);
		}

		return chokepoint;
		*/

		//chokepoint = first;
		//BWAPI::Position chokepoint((first.x + second.x) / 2, (first.y + second.y) / 2);
		//if (MapTools::Instance().getGroundDistance(position, chokepoint) < 1000) {
		if (InformationManager::Instance().getDefensivePosition().getDistance(chokepoint) < 1000) {
			//InformationManager::Instance().setDefensivePosition(chokepoint);
			return chokepoint;
		}

		//return base->getPosition();
	}

	// Otherwise we are aggressive. Look for a spot to attack.

	// Ground and air considerations.
	bool isGround = true;
	bool isAir = false;
	bool canAttackAir = false;
	bool canAttackGround = true;
	if (squad)
	{
		isGround = squad->hasGround();
		isAir = squad->hasAir();
		canAttackAir = squad->hasAntiAir();
		canAttackGround = squad->hasAntiGround();
	}

	// 1. Attack the enemy base with the weakest static defense.
	// Only if the squad can attack ground. Lift the command center and it is no longer counted as a base.
	//1。用最弱的静态防御攻击敌人基地。
	//只有当小队能攻击地面时。提起命令中心, 它不再算作基地。
	if (canAttackGround)
	{
		BWTA::BaseLocation * bestBase = nullptr;
		int bestScore = -99999;
		for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
		{
			int score = 0;
			if (InformationManager::Instance().getBaseOwner(base) == InformationManager::Instance().getEnemy())//BWAPI::Broodwar->enemy()
			{
				std::vector<UnitInfo> enemies;
				InformationManager::Instance().getNearbyForce(enemies, base->getPosition(), InformationManager::Instance().getEnemy(), AttackRadius);
				for (const auto & enemy : enemies)
				{
					if (enemy.type.isBuilding())
					{
						if (isGround && UnitUtil::TypeCanAttackGround(enemy.type) ||
							isAir && UnitUtil::TypeCanAttackAir(enemy.type))
						{
							--score;
							return enemy.lastPosition;
						}
					}
				}
				if (score > bestScore)
				{
					bestBase = base;
					bestScore = score;
				}
			}
		}
		if (bestBase)
		{
			return bestBase->getPosition();
		}
	}

	// 2. Attack known enemy buildings.
	// We assume that a terran can lift the buildings; otherwise, the squad must be able to attack ground.
	//2。攻击已知的敌人建筑物。
	//我们假设一个人类可以举起这些建筑物; 否则, 小队必须能够攻击地面。
	if (canAttackGround)
	{
		for (const auto & kv : InformationManager::Instance().getUnitInfo(InformationManager::Instance().getEnemy()))
		{
			const UnitInfo & ui = kv.second;

			if (ui.type.isBuilding() && ui.lastPosition.isValid() && !ui.goneFromLastPosition)
			{
				return ui.lastPosition;
			}
		}
	}

	// 3. Attack visible enemy units.
	// TODO score the units and attack the most important
	//3。攻击可见的敌人单位。
	//TODO 得分单位和攻击最重要的
	for (const auto unit : InformationManager::Instance().getEnemy()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Larva ||
			!unit->exists() ||
			!unit->isDetected() ||
			!unit->getPosition().isValid())
		{
			continue;
		}

		if ((unit->isFlying() && canAttackAir) || (!unit->isFlying() && canAttackGround))
		{
			return unit->getPosition();
		}
	}

	// 4. We can't see anything, so explore the map until we find something.
	//4。我们看不到任何东西, 所以探索地图, 直到我们找到一些东西。
	return MapGrid::Instance().getLeastExplored();
}

// Choose one worker to pull for scout defense.
//选择一名工人为侦察兵辩护。
BWAPI::Unit CombatCommander::findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target)
{
    UAB_ASSERT(target != nullptr, "target was null");

    if (!target)
    {
        return nullptr;
    }

    BWAPI::Unit closestMineralWorker = nullptr;
	int closestDist = Config::Micro::ScoutDefenseRadius + 128;    // more distant workers do not get pulled
    
	for (const auto unit : unitsToAssign)
	{
		if (unit->getType().isWorker() && WorkerManager::Instance().isFree(unit))
		{
			int dist = unit->getDistance(target);
			if (unit->isCarryingMinerals())
			{
				dist += 96;
			}

            if (dist < closestDist)
            {
                closestMineralWorker = unit;
                dist = closestDist;
            }
		}
	}

    return closestMineralWorker;
}


int CombatCommander::numZerglingsInOurBase() const
{
    const int concernRadius = 15 * 32;
    int zerglings = 0;
	
	BWTA::BaseLocation * main = InformationManager::Instance().getMyMainBaseLocation();
	BWAPI::Position myBasePosition(main->getPosition());

	BWAPI::Position myUnloadPosition(main->getPosition());

	std::string baseName = BWMap::Instance().getStartLocationName(main->getTilePosition());

	if (BWMap::Instance().UnloadPoints[baseName]) {
		myUnloadPosition = BWMap::Instance().UnloadPoints[baseName];
	}

	for (auto enemies : BWAPI::Broodwar->enemies()) {
		for (auto unit : enemies->getUnits())
		{
			if (unit->canAttack() &&
				(unit->getDistance(myBasePosition) < concernRadius || unit->getDistance(myUnloadPosition) < concernRadius))
			{
				++zerglings;
			}
		}
	}

	return zerglings;
}

// Is an enemy building near our base? If so, we may pull workers.
//我们基地附近有敌人大楼吗？如果是的话, 我们可以拉工人。
bool CombatCommander::buildingRush() const
{
	// If we have units, there will be no need to pull workers.
	if (InformationManager::Instance().weHaveCombatUnits())
	{
		return false;
	}

	BWTA::BaseLocation * main = InformationManager::Instance().getMyMainBaseLocation();
	BWAPI::Position myBasePosition(main->getPosition());

	for (const auto enemys : BWAPI::Broodwar->enemies()) {
		for (const auto unit : enemys->getUnits())
		{
			if (unit->getType().isBuilding() && unit->getDistance(myBasePosition) < 30 * 32)
			{
				return true;
			}
		}
	}

    return false;
}

CombatCommander & CombatCommander::Instance()
{
	static CombatCommander instance;
	return instance;
}
