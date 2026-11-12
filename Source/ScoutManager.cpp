#include "ScoutManager.h"

#include "OpponentModel.h"
#include "ProductionManager.h"
#include "Random.h"
#include "BWMap.h"

// This class is responsible for early game scouting.
// It controls any scouting worker and scouting overlord that it is given.
//这个班级负责早期的游戏侦察。
//它控制任何侦察人员和侦察霸王, 它是给定的。
using namespace UAlbertaBot;

ScoutManager::ScoutManager() 
	: _overlordScout(nullptr)
    , _workerScout(nullptr)
	, _scoutStatus("None")
	, _gasStealStatus("None")
	, _scoutCommand(MacroCommandType::None)
	, _overlordAtEnemyBase(false)
	, _scoutUnderAttack(false)
	, _tryGasSteal(false)
	, _enemyGeyser(nullptr)
    , _startedGasSteal(false)
	, _queuedGasSteal(false)
	, _gasStealOver(false)
    , _currentRegionVertexIndex(-1)
    , _previousScoutHP(0)
{
	initScoutTargets();
	setScoutTargets();
}

ScoutManager & ScoutManager::Instance() 
{
	static ScoutManager instance;
	return instance;
}

void ScoutManager::initScoutTargets() {

	//std::vector<BWAPI::Position> startLocations;

	if (BWMap::Instance().UnloadPoints.size() > 0) {
		for (auto baseLocation : BWMap::Instance().UnloadPoints) {
			scoutLocations.push_back(BWAPI::TilePosition(baseLocation.second));
		}
	}
	else {
		for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
		{
			scoutLocations.push_back(BWAPI::TilePosition(startLocation->getPosition()));
		}
	}

	if (_myRegionVertices.empty()) {
		calculateMyRegionVertices();
	}

	std::sort(scoutLocations.begin(), scoutLocations.end(), SortScoutTargets());//升序排列
}

//侦察点排序
bool ScoutManager::sortScoutTargets(BWAPI::TilePosition pos1, BWAPI::TilePosition pos2) {
	BWAPI::TilePosition startLocation = BWAPI::Broodwar->self()->getStartLocation();
	return startLocation.getDistance(pos1) < startLocation.getDistance(pos2);
}

// Target locations are set while we are still looking for the enemy base.
// After the enemy base is found, we unset the targets and go to or ignore the enemy base as appropriate.
// If we have both an overlord and a worker, send them to search different places.
// Guarantee: We only set a target if the scout for the target is set.
//目标位置设置, 而我们仍然在寻找敌人基地。
//在敌人基地被发现后, 我们取消目标, 并酌情去或忽略敌人基地。
//如果我们有一个霸王和一个工人, 派他们去搜索不同的地方。
//保证 : 我们只设定一个目标, 如果侦察的目标是设置。
void ScoutManager::setScoutTargets()
{
	//BWTA::BaseLocation * enemyBase = InformationManager::Instance().getEnemyMainBaseLocation();
	//if (enemyBase)

	if (InformationManager::Instance().isFindEnemyMainBase())
	{
		_overlordScoutTarget = BWAPI::TilePositions::Invalid;
		_workerScoutTarget = BWAPI::TilePositions::Invalid;
		return;
	}

	if (!_overlordScout)
	{
		_overlordScoutTarget = BWAPI::TilePositions::Invalid;
	}
	if (!_workerScout)
	{
		_workerScoutTarget = BWAPI::TilePositions::Invalid;
	}

	/*
	for (auto enemy : BWAPI::Broodwar->enemies()) {
		if (InformationManager::Instance().getMainBaseLocation(enemy)) {
			if (InformationManager::Instance().getMainBaseLocation(enemy)->getTilePosition().getDistance(_workerScoutTarget) < 10 * 32) {
				//_exploredPosition.push_back(_workerScoutTarget);
				_workerScoutTarget = BWAPI::TilePositions::Invalid;
				return;
			}
		}
	}
	*/

	for (BWAPI::TilePosition startPosition : scoutLocations)
	{
		bool isFind = false;

		/*
		for (BWAPI::TilePosition exPos : _exploredPosition) {
			if (exPos.getDistance(startPosition) < 6 * 32) {
				isFind = true;
				break;
			}
		}
		*/
		/*if (_exploredPosition[scoutLocations] == true) continue;*/
		//BWAPI::TilePosition pos = BWAPI::TilePosition(startPosition);
		//BWAPI::TilePosition pos = MapGrid::Instance().getScoutPosition(startLocation->getTilePosition());
		/*
		for (auto enemy : BWAPI::Broodwar->enemies()) {
			if (InformationManager::Instance().getMainBaseLocation(enemy)) {
				if (InformationManager::Instance().getMainBaseLocation(enemy)->getTilePosition().getDistance(startPosition) < 10 * 32) {
					isFind = true;
					break;
				}
			}
		}
		*/

		if (!BWAPI::Broodwar->isExplored(startPosition) && !isFind) {
			_workerScoutTarget = startPosition;
			InformationManager::Instance().workerScoutTarget = startPosition;
		}
	}

	return;

	// Unset any targets that we have searched.
	//std::vector<BWTA::BaseLocation *> choices;
	int maxDist = 10000;

	std::vector<BWAPI::Position> startLocations;

	if (BWMap::Instance().UnloadPoints.size() > 0) {
		for (auto baseLocation : BWMap::Instance().UnloadPoints) {
			//if (!BWAPI::Broodwar->isExplored(BWAPI::TilePosition(baseLocation.second))) {
				startLocations.push_back(baseLocation.second);
			//}
		}
	}
	else {
		for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
		{
			//if (!BWAPI::Broodwar->isExplored(startLocation->getTilePosition())) {
				startLocations.push_back(startLocation->getPosition());
			//}
		}
	}

	for (BWAPI::Position startPosition : startLocations)
	{
		BWAPI::TilePosition pos = BWAPI::TilePosition(startPosition);
		bool isFind = false;
		//BWAPI::TilePosition pos = MapGrid::Instance().getScoutPosition(startLocation->getTilePosition());
		for (auto enemy : BWAPI::Broodwar->enemies()) {
			if (InformationManager::Instance().getMainBaseLocation(enemy)) {
				if (InformationManager::Instance().getMainBaseLocation(enemy)->getTilePosition().getDistance(pos) < 10 * 32) {
					isFind = true;
					break;
				}
			}
		}

		if (BWAPI::Broodwar->isExplored(pos) || isFind)
		{
			if (_overlordScoutTarget == pos)
			{
				_overlordScoutTarget = BWAPI::TilePositions::Invalid;
			}

			if (_workerScoutTarget == pos)
			{
				_workerScoutTarget = BWAPI::TilePositions::Invalid;
			}

		} else {
			//choices.push_back(startLocation);
			
			if (_workerScout) {
				//int dist = BWTA::getGroundDistance(_workerScout->getTilePosition(), pos);
				//int dist = MapTools::Instance().getGroundDistance(_workerScout->getPosition(), startPosition);
				int dist = _workerScout->getDistance(startPosition);
				/*
				if (_workerScoutTarget != BWAPI::TilePositions::Invalid) {
					maxDist = MapTools::Instance().getGroundDistance(_workerScout->getPosition(), BWAPI::Position(_workerScoutTarget));
					//maxDist = BWTA::getGroundDistance(_workerScout->getTilePosition(), _workerScoutTarget);
				}
				
				if (_workerScoutTarget == BWAPI::TilePositions::Invalid) {
					_workerScoutTarget = pos;
				}
				else if (dist < maxDist)
				{
					maxDist = dist;
					_workerScoutTarget = pos;
					InformationManager::Instance().workerScoutTarget = pos;
				}
				*/

				if (dist < maxDist) {
					maxDist = dist;
					_workerScoutTarget = pos;
					InformationManager::Instance().workerScoutTarget = pos;
				}
			}

			if (_overlordScout) {
				int dist = _overlordScout->getDistance(startPosition);

				if (_overlordScoutTarget == BWAPI::TilePositions::Invalid) {
					_overlordScoutTarget = pos;
				}
				else if (dist < _overlordScout->getDistance(BWAPI::Position(_overlordScoutTarget)))
				{
					_overlordScoutTarget = pos;
				}
			}
		}
	}

	return;
	/*
	BWAPI::TilePosition enemyStartLocation(119, 118);
	if (!BWAPI::Broodwar->isExplored(enemyStartLocation)) {
		_overlordScoutTarget = enemyStartLocation;
		_workerScoutTarget = enemyStartLocation;
	}
	else {
	}
	*/
	// Set any target that we need to search.
	//设置任何我们需要搜索的目标。
	/*
	for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
	{
		BWAPI::TilePosition pos = startLocation->getTilePosition();
		if (!BWAPI::Broodwar->isExplored(pos))
		{
			if (_overlordScout && !_overlordScoutTarget.isValid() && _workerScoutTarget != pos)
			{
				_overlordScoutTarget = pos;
			}
			else if (_workerScout && !_workerScoutTarget.isValid() && _overlordScoutTarget != pos)
			{
				_workerScoutTarget = pos;
			}
		}
	}
	
	std::vector<BWTA::BaseLocation *> choices;

	for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
	{
		BWAPI::TilePosition pos = startLocation->getTilePosition();
		if (!BWAPI::Broodwar->isExplored(pos))
		{
			choices.push_back(startLocation);
		}
	}
	*/

	// Choose randomly.
	// We may choose the same target we already have. That's OK; if there's another choice,
	// we'll probably switch to it soon.
	/*
	if (!choices.empty()) {
		BWTA::BaseLocation * base = choices.at(Random::Instance().index(choices.size()));
		BWAPI::TilePosition pos = base->getTilePosition();

		if (_overlordScout && !_overlordScoutTarget.isValid() && _workerScoutTarget != pos)
		{
			_overlordScoutTarget = pos;
		}
		else if (_workerScout && !_workerScoutTarget.isValid() && _overlordScoutTarget != pos)
		{
			_workerScoutTarget = pos;
		}
	}
	*/

	// NOTE There cannot be only one target. We found the base, or there are >= 2 possibilities.
	//      If there is one place left to search, then the enemy base is there and we know it,
	//      because InformationManager infers the enemy base position by elimination.
	//注意不能只有一个目标。我们找到了基地, 或者有2种可能性。
	//如果有一个地方可以搜索, 敌人的基地就在那里, 我们知道,
	//	因为 InformationManager 通过消灭来推断敌人的基地位置。
}

// Should we send out a worker scout now?
//我们现在应该派出一名工人侦察兵吗？
bool ScoutManager::shouldScout()
{
	// If we're stealing gas, it doesn't matter what the scout command is: We need to send a worker.
	//如果我们在偷汽油, 侦察兵的命令不重要: 我们需要派一名工人。
	if (_tryGasSteal)
	{
		return true;
	}

	if (_scoutCommand == MacroCommandType::None)
	{
		return false;
	}

	// If we only want to find the enemy base location and we already know it, don't send a worker.
	//如果我们只想找到敌人基地的位置, 我们已经知道了, 不要派遣工人
	if ((_scoutCommand == MacroCommandType::ScoutIfNeeded || _scoutCommand == MacroCommandType::ScoutLocation))
	{
		return !InformationManager::Instance().getEnemyMainBaseLocation();
	}

	return true;
}

void ScoutManager::update()
{
	if (_myScoutCommand == MacroCommandType::ScoutMyLocation) {
		if (!_myWorkerScout) {
			_myWorkerScout = WorkerManager::Instance().getAnyFreeWorker();
			WorkerManager::Instance().setScoutWorker(_myWorkerScout);
		}

		if (_myWorkerScout) {
			followMyPerimeter();
		}
	}

	// If we're not scouting now, minimum effort.
	if (!_workerScout && !_overlordScout)
	{
		return;
	}

	// If a scout is gone, admit it.
	if (_workerScout &&
		(!_workerScout->exists() || _workerScout->getHitPoints() <= 0 ||   // it died (or became a zerg extractor)
		_workerScout->getPlayer() != BWAPI::Broodwar->self()))             // it got mind controlled!
	{
		_workerScout = nullptr;
	}
	if (_overlordScout &&
		(!_overlordScout->exists() || _overlordScout->getHitPoints() <= 0 ||   // it died
		_overlordScout->getPlayer() != BWAPI::Broodwar->self()))               // it got mind controlled!
	{
		_overlordScout = nullptr;
	}

	// Find out if the opponent model wants us to steal gas.
	//找出对手模型是否要我们偷汽油。
	if (_workerScout && OpponentModel::Instance().getRecommendGasSteal())
	{
		_tryGasSteal = true;
	}

	/*
	if (InformationManager::Instance().getEnemyMainBaseLocation()) {
		BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

		if (!enemyBaseLocation)
		{
			return;
		}

		//BWAPI::TilePosition enemyPosition = BWTA::getNearestBaseLocation(enemyBaseLocation->getTilePosition())->getTilePosition();
		BWAPI::TilePosition enemyPosition(48,43);//89, 15

		BWTA::BaseLocation * myBaseLocation = InformationManager::Instance().getMyMainBaseLocation();
		//BWAPI::TilePosition basePosition = BWTA::getNearestBaseLocation(myBaseLocation->getTilePosition())->getTilePosition();
		BWAPI::TilePosition basePosition(15, 39);

		std::list<BWTA::Chokepoint*> chokepointList = BWTA::getShortestPath2(basePosition, enemyPosition);
		std::vector<BWTA::Chokepoint*> chokepoints;
		for (BWTA::Chokepoint * it : chokepointList) {
			chokepoints.push_back(it);
		}
	}
	*/

	// If we only want to locate the enemy base and we have, release the scout worker.
	//如果我们只想找到敌人基地, 我们就释放侦察兵。
	if (_scoutCommand == MacroCommandType::ScoutLocation &&
		InformationManager::Instance().isFindEnemyMainBase() &&
		!wantGasSteal())
	{
		releaseWorkerScout();
	}

	// If we're done with a gas steal and we don't want to keep scouting, release the worker.
	// If we're zerg, the worker may have turned into an extractor. That is handled elsewhere.
	//如果我们已经完成了气体窃取, 我们不想继续侦察, 释放工人。
	//如果我们是虫族, 工人可能变成了一个提取者。在别处处理。
	if (_scoutCommand == MacroCommandType::None && _gasStealOver)
	{
		releaseWorkerScout();
	}

    // Calculate waypoints around the enemy base if we expect to need them.
	// We use these to go directly to the enemy base if its location is known,
	// as well as to run circuits around it.
	//如果我们想需要的话, 计算一下敌人基地周围的航点。
	//我们使用这些直接到敌人基地, 如果它的位置是已知的,
	//以及围绕它运行电路。
    if (_workerScout && _enemyRegionVertices.empty())
	{
        calculateEnemyRegionVertices();
    }

	// Do the actual scouting. Also steal gas if called for.
	//做真正的侦察。也要偷汽油, 如果叫。
	setScoutTargets();

	if (_workerScout)
	{
		bool moveScout = true;
		if (wantGasSteal())              // implies !_gasStealOver
		{
			if (gasSteal())
			{
				moveScout = false;
			}
			else if (_queuedGasSteal)    // gas steal queued but not finished
			{
				// We're in the midst of stealing gas. Let BuildingManager control the worker.
				moveScout = false;
				_gasStealStatus = "Stealing gas";
			}
		}
		else
		{
			if (_gasStealOver)
			{
				_gasStealStatus = "Finished or failed";
			}
			else
			{
				_gasStealStatus = "Not planned";
			}
		}

		if (moveScout)
		{
			moveGroundScout(_workerScout);
		}
	}
	else if (_gasStealOver)
	{
		// We get here if we're stealing gas as zerg when the worker is turns into an extractor.
		_gasStealStatus = "Finished or failed";
	}

	if (_overlordScout)
	{
		moveAirScout(_overlordScout);
	}

    drawScoutInformation(200, 320);
}

// If zerg, our first overlord is used to scout immediately at the start of the game.
void ScoutManager::setOverlordScout(BWAPI::Unit unit)
{
	_overlordScout = unit;
}

// The worker scout is assigned only when it is time to go scout.
void ScoutManager::setWorkerScout(BWAPI::Unit unit)
{
    // if we have a previous worker scout, release it back to the worker manager
	releaseWorkerScout();

    _workerScout = unit;
    WorkerManager::Instance().setScoutWorker(_workerScout);
}

// Send the worker scout home.
void ScoutManager::releaseWorkerScout()
{
	if (_workerScout)
	{
		WorkerManager::Instance().finishedWithWorker(_workerScout);
		_workerScout = nullptr;
	}
}

void ScoutManager::setScoutCommand(MacroCommandType cmd)
{
	UAB_ASSERT(
		cmd == MacroCommandType::Scout ||
		cmd == MacroCommandType::ScoutIfNeeded ||
		cmd == MacroCommandType::ScoutLocation ||
		cmd == MacroCommandType::ScoutOnceOnly,
		"bad scout command");

	_scoutCommand = cmd;
}

void ScoutManager::setMyScoutCommand(MacroCommandType cmd)
{
	UAB_ASSERT(
		cmd == MacroCommandType::ScoutMyLocation,
		"bad scout command");

	_myScoutCommand = cmd;
}

void ScoutManager::drawScoutInformation(int x, int y)
{
    if (!Config::Debug::DrawScoutInfo)
    {
        return;
    }

    BWAPI::Broodwar->drawTextScreen(x, y, "Scout info: %s", _scoutStatus.c_str());
    BWAPI::Broodwar->drawTextScreen(x, y+10, "Gas steal: %s", _gasStealStatus.c_str());
	std::string more = "not yet";
	if (_scoutCommand == MacroCommandType::Scout)
	{
		more = "and stay";
	}
	else if (_scoutCommand == MacroCommandType::ScoutLocation)
	{
		more = "location";
	}
	else if (_scoutCommand == MacroCommandType::ScoutOnceOnly)
	{
		more = "once around";
	}
	else if (_scoutCommand == MacroCommandType::ScoutWhileSafe)
	{
		more = "while safe";
	}
	else if (wantGasSteal())
	{
		more = "to steal gas";
	}
	// NOTE "go scout if needed" doesn't need to be represented here.
	BWAPI::Broodwar->drawTextScreen(x, y + 20, "Go scout: %s", more.c_str());

    for (size_t i(0); i < _enemyRegionVertices.size(); ++i)
    {
        BWAPI::Broodwar->drawCircleMap(_enemyRegionVertices[i], 4, BWAPI::Colors::Green, false);
        BWAPI::Broodwar->drawTextMap(_enemyRegionVertices[i], "%d", i);
    }
}

// Move the worker scout.
//移动工人侦察兵。
void ScoutManager::moveGroundScout(BWAPI::Unit scout)
{
	const int scoutDistanceThreshold = 30;    // in tiles

	BWAPI::Unit target = nullptr;

	// 1. Is any enemy unit within a small radius?
	//1。在视野范围内是否有敌人
	int maxDist = scout->getType().sightRange();

	BWAPI::Unitset enemiesNear = scout->getUnitsInRadius(maxDist, BWAPI::Filter::IsEnemy);
	if (!enemiesNear.empty())
	{
		for (auto unit : enemiesNear) {
			BWAPI::UnitType type = unit->getType();

			/*
			if (type.isWorker() || type.isBuilding() ||
				type == BWAPI::UnitTypes::Zerg_Overlord || type == BWAPI::UnitTypes::Zerg_Egg || type == BWAPI::UnitTypes::Zerg_Larva) continue;
			*/

			if (type.isWorker() || type.groundWeapon() == BWAPI::WeaponTypes::None) continue;

			int dist = scout->getDistance(unit);

			if (dist < maxDist)
			{
				maxDist = dist;
				target = unit;
			}
		}
	}

	if (_workerScoutTarget.isValid())
	{
		if (target){
			BWAPI::Position rp1, rp2, pos;
			BWAPI::Position position(_workerScoutTarget);

			MapTools::Instance().getCutPoint(target->getPosition(), 6 * 32, scout->getPosition(), rp1, rp2);
			if (rp1.isValid() && rp2.isValid()) {
				pos = rp1;
				if (position.getDistance(rp1) > position.getDistance(rp2)) {
					pos = rp2;
				}

				BWAPI::Broodwar->drawCircleMap(target->getPosition(), 6 * 32, BWAPI::Colors::Red);
				BWAPI::Broodwar->drawCircleMap(pos, 6, BWAPI::Colors::Green, true);

				if (pos.isValid() && pos.x > 0 && pos.y > 0) {
					if (scout->getDistance(position) > position.getDistance(pos)) {
						pos = MapTools::Instance().getExtendedPosition(pos, scout->getPosition(), 4 * 32);
						scout->rightClick(pos);
						//Micro::Move(scout, pos);
						return;
					}
				}
			}
		}

		// The target is valid exactly when we are still looking for the enemy base.
		_scoutStatus = "Seeking enemy base";
		Micro::Move(_workerScout, BWAPI::Position(_workerScoutTarget));
	}
	else {
		const BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

		if (!enemyBaseLocation) return;

		UAB_ASSERT(enemyBaseLocation, "no enemy base");

		int scoutDistanceToEnemy = MapTools::Instance().getGroundTileDistance(scout->getPosition(), enemyBaseLocation->getPosition());
		bool scoutInRangeOfenemy = scoutDistanceToEnemy <= scoutDistanceThreshold;

		// we only care if the scout is under attack within the enemy region
		// this ignores if their scout worker attacks it on the way to their base
		//我们只关心侦察兵是否在敌军区域内受到攻击
		//这不理会, 如果他们的侦察工人攻击它的方式到他们的基地
		int scoutHP = scout->getHitPoints() + scout->getShields();
		if (scoutHP < _previousScoutHP)
		{
			_scoutUnderAttack = true; //侦察下攻击
		}
		_previousScoutHP = scoutHP;

		if (!scout->isUnderAttack() && !enemyWorkerInRadius())
		{
			_scoutUnderAttack = false;
		}

		if (scoutInRangeOfenemy)
		{
			if (_scoutUnderAttack)
			{
				_scoutStatus = "Under attack, fleeing";
				followPerimeter();
			}
			else
			{
				BWAPI::Unit closestWorker = enemyWorkerToHarass();

				// If configured and reasonable, harass an enemy worker.
				//如果配置合理, 骚扰敌方工人。
				if (Config::Strategy::ScoutHarassEnemy && closestWorker && !wantGasSteal())
				{
					_scoutStatus = "Harass enemy worker";
					_currentRegionVertexIndex = -1;
					Micro::AttackUnit(scout, closestWorker);
				}
				// otherwise keep circling the enemy region
				else
				{
					_scoutStatus = "Following perimeter";
					followPerimeter();
				}
			}
		}
		// if the scout is not in the enemy region
		else if (_scoutUnderAttack)
		{
			_scoutStatus = "Under attack, fleeing";
			followPerimeter();
		}
		else
		{
			_scoutStatus = "Enemy located, going there";
			followPerimeter();    // goes toward the first waypoint inside the enemy base
		}
	}
}

// Move the overlord scout.
//移动霸王侦察兵
void ScoutManager::moveAirScout(BWAPI::Unit scout)
{
	// get the enemy base location, if we have one
	// Note: In case of an enemy proxy or weird map, this might be our own base. Roll with it.
	const BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	if (enemyBaseLocation)
	{
		// We know where the enemy base is.
		_overlordScoutTarget = BWAPI::TilePositions::Invalid;    // it's only set while we are seeking the enemy base
		if (!_overlordAtEnemyBase)
		{
			if (!_workerScout)
			{
				_scoutStatus = "Overlord to enemy base";
			}
			Micro::Move(_overlordScout, enemyBaseLocation->getPosition());
			if (_overlordScout->getDistance(enemyBaseLocation->getPosition()) < 8)
			{
				_overlordAtEnemyBase = true;
			}
		}
		if (_overlordAtEnemyBase)
		{
			if (!_workerScout)
			{
				_scoutStatus = "Overlord at enemy base";
			}
			// TODO Probably should patrol around the enemy base to see more.
		}
	}
	else
	{
		// We haven't found the enemy base yet.
		if (!_workerScout)   // give the worker priority in reporting status
		{
			_scoutStatus = "Overlord scouting";
		}

		if (_overlordScoutTarget.isValid())
		{
			Micro::Move(_overlordScout, BWAPI::Position(_overlordScoutTarget));
		}
	}
}

//跟踪外围区域
void ScoutManager::followPerimeter()
{
	int previousIndex = _currentRegionVertexIndex;

    BWAPI::Position fleeTo = getFleePosition();

	if (Config::Debug::DrawScoutInfo)
	{
		BWAPI::Broodwar->drawCircleMap(fleeTo, 5, BWAPI::Colors::Red, true);
	}

	// We've been told to circle the enemy base only once.
	if (_scoutCommand == MacroCommandType::ScoutOnceOnly && !wantGasSteal())
	{
		// NOTE previousIndex may be -1 if we're just starting the loop.
		if (_currentRegionVertexIndex < previousIndex)
		{
			releaseWorkerScout();
			return;
		}
	}

	Micro::Move(_workerScout, fleeTo);
}

//跟踪外围区域
void ScoutManager::followMyPerimeter()
{
	int previousIndex = _myRegionVertexIndex;

	BWAPI::Position fleeTo = getMyFleePosition();

	if (Config::Debug::DrawScoutInfo)
	{
		BWAPI::Broodwar->drawCircleMap(fleeTo, 5, BWAPI::Colors::Red, true);
	}

	// NOTE previousIndex may be -1 if we're just starting the loop.
	if (_myRegionVertexIndex < previousIndex)
	{
		_myScoutCommand = MacroCommandType::None;
		WorkerManager::Instance().finishedWithWorker(_myWorkerScout);
		return;
	}

	Micro::Move(_myWorkerScout, fleeTo);
}

// Called only when a gas steal is requested.
// Return true to say that gas stealing controls the worker, and
// false if the caller gets control.
//只有在要求气体窃取时才调用。
//返回 true 说, 气体窃取控制工人, 并
//如果调用方得到控制, 则为 false。
bool ScoutManager::gasSteal()
{
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();
	if (!enemyBaseLocation)
	{
		_gasStealStatus = "Enemy base not found";
		return false;
	}

	_enemyGeyser = getTheEnemyGeyser();
	if (!_enemyGeyser)
	{
		// No need to set status. It will change on the next frame.
		_gasStealOver = true;
		return false;
	}

	// The conditions are met. Do it!
	_startedGasSteal = true;

	if (_enemyGeyser->isVisible() && _enemyGeyser->getType() != BWAPI::UnitTypes::Resource_Vespene_Geyser)
	{
		// We can see the geyser and it has become something else.
		// That should mean that the geyser has been taken since we first saw it.
		_gasStealOver = true;    // give up
		// No need to set status. It will change on the next frame.
		return false;
	}
	else if (_enemyGeyser->isVisible() && _workerScout->getDistance(_enemyGeyser) < 300)
	{
		// We see the geyser. Queue the refinery, if it's not already done.
		// We can't rely on _queuedGasSteal to know, because the queue may be cleared
		// if a surprise occurs.
		// Therefore _queuedGasSteal affects mainly the debug display for the UI.
		if (!ProductionManager::Instance().isGasStealInQueue())
		{
			//BWAPI::Broodwar->printf("queueing gas steal");
			// NOTE Queueing the gas steal orders the building constructed.
			// Control of the worker passes to the BuildingManager until it releases the
			// worker with a call to gasStealOver().
			ProductionManager::Instance().queueGasSteal();
			_queuedGasSteal = true;
			// Regardless, make sure we are moving toward the geyser.
			// It makes life easier on the building manager.
			Micro::Move(_workerScout, _enemyGeyser->getInitialPosition());
		}
		_gasStealStatus = "Stealing gas";
	}
	else
	{
		// We don't see the geyser yet. Move toward it.
		Micro::Move(_workerScout, _enemyGeyser->getInitialPosition());
		_gasStealStatus = "Moving to steal gas";
	}
	return true;
}

// Choose an enemy worker to harass, or none.
//选择一个敌人的工作人员骚扰, 或没有。
BWAPI::Unit ScoutManager::enemyWorkerToHarass() const
{
	// First look for any enemy worker that is building.
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType().isWorker() && unit->isConstructing() && unit->getHitPoints() + unit->getShields() <= _workerScout->getHitPoints() + _workerScout->getShields())
		{
			return unit;
		}
	}

	BWAPI::Unit enemyWorker = nullptr;
	int maxDist = 500;    // ignore any beyond this range

	// Failing that, find the enemy worker closest to the gas.
	//如果失败了, 找到最接近毒气的敌人。
	BWAPI::Unit geyser = getAnyEnemyGeyser();
	if (geyser)
	{
		for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			if (unit->getType().isWorker() && unit->getHitPoints() + unit->getShields() <= _workerScout->getHitPoints() + _workerScout->getShields())
			{
				int dist = unit->getDistance(geyser->getInitialPosition());

				if (dist < maxDist)
				{
					maxDist = dist;
					enemyWorker = unit;
				}
			}
		}
	}

	return enemyWorker;
}

// Used in choosing an enemy worker to harass.
// Find an enemy geyser and return it, if there is one.
//用于选择敌方工作人员骚扰。
//找到一个敌人的喷泉, 并返回它, 如果有一个。
BWAPI::Unit ScoutManager::getAnyEnemyGeyser() const
{
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	BWAPI::Unitset geysers = enemyBaseLocation->getGeysers();
	if (geysers.size() > 0)
	{
		return *(geysers.begin());
	}

	return nullptr;
}

// Used in stealing gas.
// If there is exactly 1 geyser in the enemy base and it may be untaken, return it.
// If 0 we can't steal it, and if >1 then it's no use to steal one.
//用来偷汽油。
//如果敌人基地有1个间歇泉, 可能是 untaken, 那就归还吧。
//如果 0, 我们不能偷它, 如果 >> 1 那么它是没有用的偷一个。
BWAPI::Unit ScoutManager::getTheEnemyGeyser() const
{
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	BWAPI::Unitset geysers = enemyBaseLocation->getGeysers();
	if (geysers.size() == 1)
	{
		BWAPI::Unit geyser = *(geysers.begin());
		// If the geyser is visible, we may be able to reject it as already taken.
		// TODO get the type from InformationManager, which may remember
		if (!geyser->isVisible() || geyser->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
		{
			// We see it is untaken, or we don't see the geyser. Assume the best.
			return geyser;
		}
	}

	return nullptr;
}

//在半径的敌人工作者
bool ScoutManager::enemyWorkerInRadius()
{
	for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType().isWorker() && (unit->getDistance(_workerScout) < 6 * 32) && unit->getHitPoints() + unit->getShields() <= _workerScout->getHitPoints() + _workerScout->getShields())
		{
			return true;
		}
	}

	return false;
}

int ScoutManager::getClosestVertexIndex(BWAPI::Unit unit)
{
    int closestIndex = -1;
    int closestDistance = 10000000;

    for (size_t i(0); i < _enemyRegionVertices.size(); ++i)
    {
        int dist = unit->getDistance(_enemyRegionVertices[i]);
        if (dist < closestDistance)
        {
            closestDistance = dist;
            closestIndex = i;
        }
    }

    return closestIndex;
}

BWAPI::Position ScoutManager::getFleePosition()
{
    UAB_ASSERT_WARNING(!_enemyRegionVertices.empty(), "should have enemy region vertices");
    
    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

    // if this is the first flee, we will not have a previous perimeter index
    if (_currentRegionVertexIndex == -1)
    {
        // so return the closest position in the polygon
        int closestPolygonIndex = getClosestVertexIndex(_workerScout);

        UAB_ASSERT_WARNING(closestPolygonIndex != -1, "Couldn't find a closest vertex");

        if (closestPolygonIndex == -1)
        {
            return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
        }

        // set the current index so we know how to iterate if we are still fleeing later
        _currentRegionVertexIndex = closestPolygonIndex;
        return _enemyRegionVertices[closestPolygonIndex];
    }

    // if we are still fleeing from the previous frame, get the next location if we are close enough
    double distanceFromCurrentVertex = _enemyRegionVertices[_currentRegionVertexIndex].getDistance(_workerScout->getPosition());

    // keep going to the next vertex in the perimeter until we get to one we're far enough from to issue another move command
    while (distanceFromCurrentVertex < 128)
    {
        _currentRegionVertexIndex = (_currentRegionVertexIndex + 1) % _enemyRegionVertices.size();

        distanceFromCurrentVertex = _enemyRegionVertices[_currentRegionVertexIndex].getDistance(_workerScout->getPosition());
    }

    return _enemyRegionVertices[_currentRegionVertexIndex];
}


int ScoutManager::getMyClosestVertexIndex(BWAPI::Unit unit)
{
	int closestIndex = -1;
	int closestDistance = 10000000;

	for (size_t i(0); i < _myRegionVertices.size(); ++i)
	{
		int dist = unit->getDistance(_myRegionVertices[i]);
		if (dist < closestDistance)
		{
			closestDistance = dist;
			closestIndex = i;
		}
	}

	return closestIndex;
}

BWAPI::Position ScoutManager::getMyFleePosition()
{
	UAB_ASSERT_WARNING(!_myRegionVertices.empty(), "should have enemy region vertices");

	BWTA::BaseLocation * baseLocation = InformationManager::Instance().getMyMainBaseLocation();

	// if this is the first flee, we will not have a previous perimeter index
	if (_myRegionVertexIndex == -1)
	{
		// so return the closest position in the polygon
		int closestPolygonIndex = getMyClosestVertexIndex(_myWorkerScout);

		UAB_ASSERT_WARNING(closestPolygonIndex != -1, "Couldn't find a closest vertex");

		if (closestPolygonIndex == -1)
		{
			return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
		}

		// set the current index so we know how to iterate if we are still fleeing later
		_myRegionVertexIndex = closestPolygonIndex;
		return _myRegionVertices[closestPolygonIndex];
	}

	// if we are still fleeing from the previous frame, get the next location if we are close enough
	double distanceFromCurrentVertex = _myRegionVertices[_myRegionVertexIndex].getDistance(_myWorkerScout->getPosition());

	// keep going to the next vertex in the perimeter until we get to one we're far enough from to issue another move command
	while (distanceFromCurrentVertex < 128)
	{
		_myRegionVertexIndex = (_myRegionVertexIndex + 1) % _myRegionVertices.size();

		distanceFromCurrentVertex = _myRegionVertices[_myRegionVertexIndex].getDistance(_myWorkerScout->getPosition());
	}

	return _myRegionVertices[_myRegionVertexIndex];
}

std::vector<BWAPI::Position> ScoutManager::calculateEnemyRegionVertices(BWTA::BaseLocation * baseLocation)
{
	std::vector<BWAPI::Position> sortedVertices;

	BWTA::Region * enemyRegion = baseLocation->getRegion();

	if (!enemyRegion)
	{
		return sortedVertices;
	}

	const BWAPI::Position basePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	const std::vector<BWAPI::TilePosition> & closestTobase = MapTools::Instance().getClosestTilesTo(basePosition);

	std::set<BWAPI::Position> unsortedVertices;

	// check each tile position
	for (size_t i(0); i < closestTobase.size(); ++i)
	{
		const BWAPI::TilePosition & tp = closestTobase[i];

		if (BWTA::getRegion(tp) != enemyRegion)
		{
			continue;
		}

		// a tile is 'on an edge' unless
		// 1) in all 4 directions there's a tile position in the current region
		// 2) in all 4 directions there's a buildable tile
		//瓷砖是 ' 在边缘 ', 除非
		//1) 在所有4个方向在当前区域有一个瓷砖位置
		//2) 在所有4个方向有一个可生成瓷砖
		bool edge =
			BWTA::getRegion(BWAPI::TilePosition(tp.x + 1, tp.y)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x + 1, tp.y))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x, tp.y + 1)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x, tp.y + 1))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x - 1, tp.y)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x - 1, tp.y))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x, tp.y - 1)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x, tp.y - 1));

		// push the tiles that aren't surrounded
		//推开没有被包围的瓷砖
		if (edge && BWAPI::Broodwar->isBuildable(tp))
		{
			if (Config::Debug::DrawScoutInfo)
			{
				int x1 = tp.x * 32 + 2;
				int y1 = tp.y * 32 + 2;
				int x2 = (tp.x + 1) * 32 - 2;
				int y2 = (tp.y + 1) * 32 - 2;

				BWAPI::Broodwar->drawTextMap(x1 + 3, y1 + 2, "%d", MapTools::Instance().getGroundTileDistance(BWAPI::Position(tp), basePosition));
				BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Green, false);
			}

			unsortedVertices.insert(BWAPI::Position(tp) + BWAPI::Position(16, 16));
		}
	}

	BWAPI::Position current = *unsortedVertices.begin();

	_enemyRegionVertices.push_back(current);
	unsortedVertices.erase(current);

	// while we still have unsorted vertices left, find the closest one remaining to current
	//虽然我们仍然有未排序的顶点左, 找到最接近的一个剩余的当前
	while (!unsortedVertices.empty())
	{
		double bestDist = 1000000;
		BWAPI::Position bestPos;

		for (const BWAPI::Position & pos : unsortedVertices)
		{
			double dist = pos.getDistance(current);

			if (dist < bestDist)
			{
				bestDist = dist;
				bestPos = pos;
			}
		}

		current = bestPos;
		sortedVertices.push_back(bestPos);
		unsortedVertices.erase(bestPos);
	}

	// let's close loops on a threshold, eliminating death grooves
	//让我们关闭循环的门槛, 消除死亡凹槽
	int distanceThreshold = 100;

	while (true)
	{
		// find the largest index difference whose distance is less than the threshold
		//查找距离小于阈值的最大索引差异
		int maxFarthest = 0;
		int maxFarthestStart = 0;
		int maxFarthestEnd = 0;

		// for each starting vertex
		for (int i(0); i < (int)sortedVertices.size(); ++i)
		{
			int farthest = 0;
			int farthestIndex = 0;

			// only test half way around because we'll find the other one on the way back
			for (size_t j(1); j < sortedVertices.size() / 2; ++j)
			{
				int jindex = (i + j) % sortedVertices.size();

				if (sortedVertices[i].getDistance(sortedVertices[jindex]) < distanceThreshold)
				{
					farthest = j;
					farthestIndex = jindex;
				}
			}

			if (farthest > maxFarthest)
			{
				maxFarthest = farthest;
				maxFarthestStart = i;
				maxFarthestEnd = farthestIndex;
			}
		}

		// stop when we have no long chains within the threshold
		if (maxFarthest < 4)
		{
			break;
		}

		std::vector<BWAPI::Position> temp;

		for (size_t s(maxFarthestEnd); s != maxFarthestStart; s = (s + 1) % sortedVertices.size())
		{
			temp.push_back(sortedVertices[s]);
		}

		sortedVertices = temp;
	}

	return sortedVertices;
}

// NOTE This algorithm sometimes produces bizarre paths when unbuildable tiles
//      are found deep inside the enemy base.
// TODO Eventually replace with real-time pathing that tries to see as much as possible
//      while remaining safe.
//注意此算法有时会产生奇怪的路径时, unbuildable 瓷砖
//被发现在敌人基地深处。
//TODO 最终替换为尝试尽可能多地查看的实时路径。
//同时保持安全。
void ScoutManager::calculateEnemyRegionVertices()
{
	BWTA::BaseLocation * baseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	if (!baseLocation)
    {
        return;
    }

	_enemyRegionVertices = calculateEnemyRegionVertices(baseLocation);
}

void ScoutManager::calculateMyRegionVertices()
{
	BWTA::BaseLocation * baseLocation = InformationManager::Instance().getMyMainBaseLocation();

	if (!baseLocation)
	{
		return;
	}

	_myRegionVertices = calculateEnemyRegionVertices(baseLocation);
}