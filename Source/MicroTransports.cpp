#include "MicroTransports.h"
#include "MapTools.h"
#include "UnitUtil.h"
#include "CombatSimulation.h"
#include "Random.h"
#include "CombatCommander.h"
#include "MapPosition.h"

using namespace UAlbertaBot;
//运输机模块
MicroTransports::MicroTransports()
	: _transportShip(nullptr)
	, _currentRegionVertexIndex(-1)
	, _minCorner(-1,-1)
	, _maxCorner(-1,-1)
	, _to(-1,-1)
	, _from(-1,-1)
{
}

// No micro to execute here. Does nothing, never called.
//这里没有微执行。什么也不做, 从不调用。
void MicroTransports::executeMicro(const BWAPI::Unitset & targets) 
{
}

void MicroTransports::calculateMapEdgeVertices()
{
	const BWAPI::Position basePosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();

	//修改，直线飞过去
	//_mapEdgeVertices.push_back(enemyBaseLocation->getPosition());
	//BWAPI::Position position(1600, 220);
	
	//BWAPI::Position position = enemyBaseLocation->getPosition();
	//position = BWAPI::Position(position.x - 4 * 32, position.y - 2 * 32);

	BWAPI::Position position = InformationManager::Instance().getMainAttackPoint();// MapPosition::Instance().getUnloadPosition();
	//BWAPI::Position position = enemyBaseLocation->getPosition() + BWAPI::Position(0, 4 * 32);

	_mapEdgeVertices.push_back(position);
	_mapEdgeVertices.push_back(basePosition);
	_to = position;// enemyBaseLocation->getPosition();
	return;

	const std::vector<BWAPI::TilePosition> & closestTobase = MapTools::Instance().getClosestTilesTo(basePosition);

	std::set<BWAPI::Position> unsortedVertices;

	int minX = std::numeric_limits<int>::max(); int minY = minX;
	int maxX = std::numeric_limits<int>::min(); int maxY = maxX;

	//compute mins and maxs
	for(auto & tile : closestTobase)
	{
		if (tile.x > maxX) maxX = tile.x;
		else if (tile.x < minX) minX = tile.x;

		if (tile.y > maxY) maxY = tile.y;
		else if (tile.y < minY) minY = tile.y;
	}

	_minCorner = BWAPI::Position(minX, minY) * 32 + BWAPI::Position(16, 16);
	_maxCorner = BWAPI::Position(maxX, maxY) * 32 + BWAPI::Position(16, 16);

	//add all(some) edge tiles! 
	for (int _x = minX; _x <= maxX; _x += 5)
	{
		unsortedVertices.insert(BWAPI::Position(_x, minY) * 32 + BWAPI::Position(16, 16));
		unsortedVertices.insert(BWAPI::Position(_x, maxY) * 32 + BWAPI::Position(16, 16));
	}

	for (int _y = minY; _y <= maxY; _y += 5)
	{
		unsortedVertices.insert(BWAPI::Position(minX, _y) * 32 + BWAPI::Position(16, 16));
		unsortedVertices.insert(BWAPI::Position(maxX, _y) * 32 + BWAPI::Position(16, 16));
	}

	std::vector<BWAPI::Position> sortedVertices;
	BWAPI::Position current = *unsortedVertices.begin();

	_mapEdgeVertices.push_back(current);
	unsortedVertices.erase(current);

	// while we still have unsorted vertices left, find the closest one remaining to current
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
    
	_mapEdgeVertices = sortedVertices;
}

void MicroTransports::drawTransportInformation(int x = 0, int y = 0)
{
	if (!Config::Debug::DrawUnitTargetInfo)
	{
		return;
	}

	BWAPI::Broodwar->drawCircleMap(_to, 4, BWAPI::Colors::Green, false);
	return;

	for (size_t i(0); i < _mapEdgeVertices.size(); ++i)
	{
		BWAPI::Broodwar->drawCircleMap(_mapEdgeVertices[i], 4, BWAPI::Colors::Green, false);
		BWAPI::Broodwar->drawTextMap(_mapEdgeVertices[i], "%d", i);
	}
}

void MicroTransports::update()
{
	const BWAPI::Unitset & units = getUnits();
	int frame = BWAPI::Broodwar->getFrameCount();

	BWAPI::Unitset loadUnits;

	BWAPI::Player self = BWAPI::Broodwar->self();
	int spaceRemaining = 0;
	BWAPI::Position position;

	//运输装载点
	BWAPI::Position loadPosition = MapTools::Instance().getDistancePosition(InformationManager::Instance().getMyMainBaseLocation()->getPosition(), InformationManager::Instance().getChokepoint(), 15 * 32);
	//BWAPI::Unitset zealots = InformationManager::Instance().getUnits(self, BWAPI::UnitTypes::Protoss_Zealot);
	
	for (const auto unit : units)
	{
		/*
		if (unit->getOrderTargetPosition()) {
			BWAPI::Position orderTargetPosition = unit->getOrderTargetPosition();
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
					BWAPI::Broodwar->drawCircleMap(pos, 6, BWAPI::Colors::Green, true);

					if (pos.isValid() && pos.x > 0 && pos.y > 0) {
						if (unit->getDistance(orderTargetPosition) > orderTargetPosition.getDistance(pos)) {
							pos = MapTools::Instance().getExtendedPosition(pos, unit->getPosition(), 3 * 32);
							//unit->rightClick(pos);
							Micro::Move(unit, pos);
							retreatUnit[unit] = frame + unit->getDistance(pos) / unit->getType().topSpeed() - 2 * 24;
							//_loadTo[unit] = pos;
							continue;
						}
					}
				}
			}
		}
		*/
		if (keepElude(unit)) continue;

		if (retreatUnit[unit] && frame < retreatUnit[unit]) continue;

		_loadedUnits = unit->getLoadedUnits();
		spaceRemaining = unit->getSpaceRemaining();

		if (_loadTo[unit]) {
			position = _loadTo[unit];
		}
		else {
			position = loadPosition;
		}

		//如果是金甲，则去主要进攻点
		bool isLoadReaver = false;
		for (auto loadUnit : unit->getLoadedUnits()) {
			if (loadUnit->getType() == BWAPI::UnitTypes::Protoss_Reaver) {
				_loadTo[unit] = InformationManager::Instance().getAttackPosition();
				isLoadReaver = true;
				break;
			}
		}

		position = _loadTo[unit];
		int dist = unit->getDistance(position);

		if (_loadedUnits.size() > 0) {
			if (unit->isUnderAttack()) {
				if (unit->getHitPoints() < 30 && unit->getShields() < 20) {
					for (auto zealot : _loadedUnits) {
						unit->unload(zealot);
					}
				}

				for (auto zealot : _loadedUnits) {
					if (zealot->getType() == BWAPI::UnitTypes::Protoss_Zealot) {
						unit->unload(zealot);
						break;
					}
				}
			}

			if (!unit->isMoving()) {
				Micro::Move(unit, position);
			}
		}

		//如果已经装载完
		if (_loadDropUnit[unit] && _loadDropUnit[unit] > 0) {

			//在敌方家里空投完后回主基地和防守点中间的位置
			if (spaceRemaining == 8) {
				_loadTo[unit] = loadPosition;

				position = _loadTo[unit];
				dist = unit->getDistance(position);

				if (dist > 6 * 32) {
					Micro::Move(unit, position);
				}
				else {
					_loadDropUnit[unit] = -1;
				}
				continue;
			}

			//如果装的是闪点，检查空投点
			if (!isLoadReaver) {
				if (InformationManager::Instance().getPsionicStormFrame() > frame + dist / unit->getType().topSpeed()) continue;

				if (BWAPI::Broodwar->isVisible(BWAPI::TilePosition(position))) {
					BWAPI::Position unloadPosition = InformationManager::Instance().checkUnloadPosition();
					if (unloadPosition.isValid() && unloadPosition.x != 0 && unloadPosition.y != 0) {
						if (position.getDistance(unloadPosition) > 6 * 32) {
							position = unloadPosition;
							_loadTo[unit] = unloadPosition;
							dist = unit->getDistance(position);
						}
					}
				}
			}

			//到达空投点
			if (dist < 9 * 32) {
				if (_loadedUnits.size() > 0) {
					//unit->unloadAll();
					for (auto highTemplar : _loadedUnits) {
						unit->unload(highTemplar);
					}
				}
				else {
					_loadDropUnit[unit] = 0;
				}

				continue;
			}
			else if (unit->getDistance(position) < 12 * 32) {
				for (auto zealot : _loadedUnits) {
					if (zealot->getType() == BWAPI::UnitTypes::Protoss_Zealot) {
						unit->unload(zealot);
						break;
					}
				}
				continue;
			}
			else if (dist > 20 * 32) {
				//if (InformationManager::Instance().getPsionicStormFrame() > frame + dist / unit->getType().topSpeed()) continue;
				Micro::Move(unit, position);
				continue;
			}
			else {
				Micro::Move(unit, position);
				continue;
			}
		}
		else if(_loadDropUnit[unit] && _loadDropUnit[unit] == 0) {
			if (unit->getDistance(loadPosition) > 4 * 32) {
				Micro::Move(unit, loadPosition);
				retreatUnit[unit] = frame + unit->getDistance(loadPosition) / unit->getType().topSpeed() - 24;
			}
			else {
				_loadDropUnit[unit] = -1;
			}
			continue;
		}

		if (!_loadDropUnit[unit] || _loadDropUnit[unit] == -1) {
			if (spaceRemaining == 8) {
				_loadTo[unit] = loadPosition;
				position = _loadTo[unit];
				dist = unit->getDistance(position);
				if (dist > 18 * 32) {
					Micro::Move(unit, position);
					continue;
				}
				//装载闪电或金甲
				if (!unit->getOrderTarget()) {
					BWAPI::Unit highTemplar;
					if (InformationManager::Instance().getPsionicStormFrame() < frame + dist / unit->getType().topSpeed()) {
						//如果电还没有升级好，则不装闪电
						highTemplar = unit->getClosestUnit(BWAPI::Filter::IsOwned && BWAPI::Filter::IsCompleted && !BWAPI::Filter::IsLoaded &&
							((BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_High_Templar && BWAPI::Filter::Energy > 60) ||
							(BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Reaver && BWAPI::Filter::ScarabCount > 3)
							), 20 * 32);
					}
					else {
						highTemplar = unit->getClosestUnit(BWAPI::Filter::IsOwned && BWAPI::Filter::IsCompleted && !BWAPI::Filter::IsLoaded &&
							(
							(BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Reaver && BWAPI::Filter::ScarabCount > 3)
							), 20 * 32);
					}

					if (highTemplar && !loadUnits.contains(highTemplar)) {
						loadUnits.insert(highTemplar);
						CombatCommander::Instance().assignUnitToSquad(highTemplar, "Drop");

						highTemplar->rightClick(unit);
						if (unit->load(highTemplar)) {
							continue;
						}
					}
				}
			}
			else if (spaceRemaining < 8 && spaceRemaining >= 4) {
				//if (!unit->getOrderTarget()) {
				BWAPI::Unit zealot = unit->getClosestUnit(BWAPI::Filter::IsOwned && (BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Zealot || BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Dark_Templar) && !BWAPI::Filter::IsLoaded, 30 * 32);
				if (zealot && !loadUnits.contains(zealot)) {
					loadUnits.insert(zealot);
					CombatCommander::Instance().assignUnitToSquad(zealot, "Drop");

					zealot->rightClick(unit);
					if (unit->load(zealot)) {
						continue;
					}
				}
				//}
				/*
				BWAPI::Unitset highTemplars = unit->getUnitsInRadius(30 * 32, BWAPI::Filter::IsOwned && BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_High_Templar && !BWAPI::Filter::IsLoaded);
				int numHighTemplar = highTemplars.size();

				if (numHighTemplar > 2) {
				numHighTemplar = 2;
				}

				spaceRemaining = unit->getSpaceRemaining();
				for (auto highTemplar : highTemplars) {
				if (!highTemplar->isLoaded() && !loadUnits.contains(highTemplar)) {
				loadUnits.insert(highTemplar);
				if (unit->load(highTemplar)) {
				break;
				}
				}
				}
				*/
			}
			else {
				_loadDropUnit[unit] = unit->getLoadedUnits().size();
				//_loadTo[unit] = InformationManager::Instance().getEnemyMainBaseLocation()->getPosition();

				bool isLoadHighTemplar = false;
				for (auto loadUnit : unit->getLoadedUnits()) {
					if (loadUnit->getType() == BWAPI::UnitTypes::Protoss_High_Templar) {
						isLoadHighTemplar = true;
					}
					CombatCommander::Instance().removeUnitToSquad(loadUnit, "Drop");
				}

				if (isLoadHighTemplar && InformationManager::Instance().getEnemyMainBaseLocation()) {
					//随机获取一个点
					BWAPI::Position enemyMainPosition = InformationManager::Instance().getUnloadPosition();
					BWAPI::Position myMainPosition = InformationManager::Instance().getMyUnloadPosition();
					
					BWAPI::TilePosition newPosition = unit->getTilePosition();
					//BWAPI::TilePosition newPosition = BWAPI::TilePosition(enemyMainPosition);

					int radius = Random::Instance().index(40);

					int i = Random::Instance().index(2);
					if (myMainPosition.y != enemyMainPosition.y) {
						if (i == 1) {
							newPosition.x -= radius;
						}
						else {
							newPosition.x += radius;
						}

						if (newPosition.x <= 0) {
							newPosition.x = 1;
						}

						if (newPosition.x >= 127) {
							newPosition.x = 126;
						}
					}
					else {
						if (i == 1) {
							newPosition.y -= radius;
						}
						else {
							newPosition.y += radius;
						}
						if (newPosition.y <= 0) {
							newPosition.y = 1;
						}

						if (newPosition.y >= 127) {
							newPosition.y = 126;
						}
					}

					position = BWAPI::Position(newPosition);

					//随机点
					//position = MapPosition::Instance().getRandomPosition(unit->getPosition(), 25 * 32);
					if (position.isValid()) {
						retreatUnit[unit] = frame + unit->getDistance(position) / unit->getType().topSpeed() - 12;
						Micro::Move(unit, position);
						//continue;
					}
				}

				_loadTo[unit] = InformationManager::Instance().getUnloadPosition();// MapPosition::Instance().getUnloadPosition();
				position = _loadTo[unit];

				dist = unit->getDistance(position);
				//如果还没有升好闪电，则先不去
				if (InformationManager::Instance().getPsionicStormFrame() > frame + dist / unit->getType().topSpeed()) continue;

				CombatCommander::Instance().attackNow();
			}
		}
	}
}

void MicroTransports::update_old()
{
	int frame = BWAPI::Broodwar->getFrameCount();
	// If we haven't found our transport, or it went away, look again.
	// Only supports having 1 transport unit.
	//如果我们没有找到我们的交通工具, 或者它走了, 再看一遍。
	//仅支持拥有1个传输单元。
	if (!UnitUtil::IsValidUnit(_transportShip))
	{
		if (getUnits().empty())
		{
			_transportShip = nullptr;
		}
		else
		{
			_transportShip = *(getUnits().begin());
		}
	}

	// If we still have no transport, or it's still gone, there is nothing to do.
	if (!UnitUtil::IsValidUnit(_transportShip))
	{
		_transportShip = nullptr;
		return;
	}

	// Calculate a sneaky path to reach the enemy base.
	if (_mapEdgeVertices.empty())
	{
		calculateMapEdgeVertices();
	}

	// If we're not full yet, wait.
	if (_transportShip->getSpaceRemaining() > 0)
	{
		//return;
	}

	BWAPI::Unit closeEnemy = _transportShip->getClosestUnit(BWAPI::Filter::IsEnemy && (!BWAPI::Filter::IsFlyer), 7 * 32);
	BWAPI::Unit downEnemy = _transportShip->getClosestUnit(BWAPI::Filter::IsEnemy && (!BWAPI::Filter::IsFlyer), 1 * 32);

	BWAPI::Position position = _transportShip->getPosition();

	_loadedUnits = _transportShip->getLoadedUnits();

	/*
	if (BWAPI::Broodwar->getFrameCount() >= _loadFrameChange) {
	const int transportHP = _transportShip->getHitPoints() + _transportShip->getShields();

	if (_loadedUnits.size() == 0 && _transportShip->getDistance(InformationManager::Instance().getRallyPoint()) > 6 * 32) {
	_to = InformationManager::Instance().getRallyPoint();
	Micro::Move(_transportShip, _to);
	return;
	}

	if (transportHP < 20 && _transportShip->canUnloadAtPosition(position) && _loadedUnits.size() > 0) {
	//unloadTroops();
	_transportShip->unloadAll();
	_loadFrameChange = BWAPI::Broodwar->getFrameCount() + 12;
	}

	//BWAPI::Unitset enemies = _transportShip->getUnitsInRadius(8 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Hydralisk);
	//如果飞龙或口水接进
	BWAPI::Unit closeEnemy = _transportShip->getClosestUnit(BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Hydralisk
	|| BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Mutalisk
	|| BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Spore_Colony
	|| BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Dragoon
	|| BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Photon_Cannon
	, 9 * 32);

	if (!closeEnemy && _mapEdgeVertices[0] && _mapEdgeVertices[0].getDistance(_to) > 15 * 32) {
	_to = _mapEdgeVertices[0];
	Micro::Move(_transportShip, _to);
	_loadFrameChange = BWAPI::Broodwar->getFrameCount() + 12;
	}

	if (closeEnemy && _loadedUnits.size() > 0) {
	//BWAPI::Unit closeEnemy = _transportShip->getClosestUnit(BWAPI::Filter::IsEnemy);
	_to = MapTools::Instance().getExtendedPosition(position, closeEnemy->getPosition(), 10 * 32);
	if (_to.isValid()) {
	Micro::Move(_transportShip, _to);
	_loadFrameChange = BWAPI::Broodwar->getFrameCount() + 12;
	}
	//return;
	}

	if (_to && position.getDistance(_to) > 3 * 32) {
	moveTransport();
	}
	else {
	_transportShip->unloadAll();
	}
	}

	drawTransportInformation();

	return;
	*/
	if (retreatUnit[_transportShip] && frame < retreatUnit[_transportShip]) return;

	if (frame % 8 == 0) {
		if (_loadedUnits.size() == 0) {
			_to = InformationManager::Instance().getDropPoint();
		}
		else {
			_to = InformationManager::Instance().getMainAttackPoint() + BWAPI::Position(0, 4 * 32);
		}

		if (_to && position.getDistance(_to) > 3 * 32) {
			Micro::Move(_transportShip, _to);
		}
		//return;
	}

	// All clear. Go do stuff.
	if (BWAPI::Broodwar->getFrameCount() % 2 == 0) {

		BWAPI::Unitset nearUnits = _transportShip->getUnitsInRadius(3 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);
		//BWAPI::Unitset nearUnits;
		//MapGrid::Instance().getUnits(nearUnits, _to, 3 * 32, true, false);

		for (const BWAPI::Unit unit : nearUnits)
		{
			if (unit != _transportShip && !unit->isLoaded() && _transportShip->getSpaceRemaining() > 0)
			{
				//_transportShip->load(unit);
				unit->rightClick(_transportShip);
				loadUnits[unit] = BWAPI::Broodwar->getFrameCount() + 14;
				//_loadFrameChange = BWAPI::Broodwar->getFrameCount() + 16;
				//return;
			}
		}

		if (_loadedUnits.size() > 0 && _transportShip->canUnloadAtPosition(position) && closeEnemy) {
			for (const BWAPI::Unit unit : _loadedUnits)
			{
				if (BWAPI::Broodwar->getFrameCount() > loadUnits[unit]) {
					_transportShip->unload(unit);
					_loadFrameChange = frame + 6;
					_nLoad = 0;
					//return;
				}
			}
		}

		if (BWAPI::Broodwar->getFrameCount() % 2 == 0 && _transportShip->getSpaceRemaining() > 0) {
			//如果离坦克太远，移动到坦克的位置
			for (auto unit : loadUnits) {
				_to = unit.first->getPosition();
				if (!unit.first->isLoaded() && _to.getDistance(position) > 2 * 32) {
					Micro::Move(_transportShip, _to);
					return;
				}
			}

			if (downEnemy) {
				_to = MapPosition::Instance().getRandomPosition(_to, 4 * 32);
				//return;
			}

			if (_to && position.getDistance(_to) <= 7 * 32 && frame > _lastFrameChange) {
				BWAPI::Position newPosition = MapPosition::Instance().getRandomPosition(position, 4 * 32);
				Micro::Move(_transportShip, newPosition);
			}

			/*
			if (BWAPI::Broodwar->getFrameCount() % 12 == 0) {
			if (_to && position.getDistance(_to) < 3 * 32) {
			BWAPI::Unit closeEnemy = _transportShip->getClosestUnit(BWAPI::Filter::IsEnemy && (!BWAPI::Filter::IsFlyer), 8 * 32);
			if (!closeEnemy) {

			BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

			if (!enemyBaseLocation)
			{
			return;
			}

			//_to = enemyBaseLocation->getPosition();
			//_to = BWAPI::Position(MapPosition::Instance().getUnloadPosition());
			_to = InformationManager::Instance().getDropPoint();

			Micro::Move(_transportShip, _to);
			_loadFrameChange = BWAPI::Broodwar->getFrameCount() + 24;
			return;
			}
			}


			if (_to && position.getDistance(_to) < 3 * 32 && frame > _lastFrameChange) {
			std::vector<BWAPI::Position> choices;
			double radius = 1.2 * 32;
			choices.push_back(BWAPI::Position(_to.x + radius, _to.y + radius));
			choices.push_back(BWAPI::Position(_to.x - radius, _to.y + radius));
			choices.push_back(BWAPI::Position(_to.x - radius, _to.y - radius));
			choices.push_back(BWAPI::Position(_to.x + radius, _to.y - radius));

			//if (_transportShip->isBraking()) {
			BWAPI::Position newPosition = choices.at(Random::Instance().index(choices.size()));
			if (position.getDistance(newPosition) < 40) {
			newPosition = choices.at(Random::Instance().index(choices.size()));
			}

			BWAPI::Position newPosition = MapPosition::Instance().getRandomPosition(position, 3 * 32);
			Micro::Move(_transportShip, newPosition);
			//_transportShip->rightClick(newPosition);
			//_loadFrameChange = BWAPI::Broodwar->getFrameCount() + 8;
			}

			}
			*/
		}

		if (BWAPI::Broodwar->getFrameCount() >= _loadFrameChange) {
			/*
			if (_to && position.getDistance(_to) > 6 * 32) {
				moveTransport();
			}

			_loadedUnits = _transportShip->getLoadedUnits();
			*/

			const int transportHP = _transportShip->getHitPoints() + _transportShip->getShields();

			if (transportHP < 20 && _transportShip->canUnloadAtPosition(position) && _loadedUnits.size() > 0) {
				//unloadTroops();
				_transportShip->unloadAll();
				_loadFrameChange = BWAPI::Broodwar->getFrameCount() + 12;
			}

			//BWAPI::Unitset enemies = _transportShip->getUnitsInRadius(8 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Hydralisk);
			//如果飞龙或口水接进
			closeEnemy = _transportShip->getClosestUnit(BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Hydralisk
				|| BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Mutalisk
				|| BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Spore_Colony
				|| BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Dragoon
				|| BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Photon_Cannon
				, 6 * 32);

			/*
			if (!closeEnemy && _mapEdgeVertices[0] && _mapEdgeVertices[0].getDistance(_to) > 15 * 32) {
				_to = _mapEdgeVertices[0];
				Micro::Move(_transportShip, _to);
				_loadFrameChange = BWAPI::Broodwar->getFrameCount() + 12;
			}
			*/

			if (closeEnemy) {
				//BWAPI::Unit closeEnemy = _transportShip->getClosestUnit(BWAPI::Filter::IsEnemy);
				int dist = _transportShip->getDistance(closeEnemy);
				_to = MapTools::Instance().getExtendedPosition(position, closeEnemy->getPosition(), 7 * 32 - dist);
				if (_to.isValid()) {
					//Micro::Move(_transportShip, _to);
					_transportShip->rightClick(_to);
					retreatUnit[_transportShip] = frame + Config::Micro::RetreatFrame;
					_loadFrameChange = frame + 12;
				}
				//return;
			}
		}
	}
	drawTransportInformation();

	return;

	if (_to && position.getDistance(_to) < 5 * 32) {
		if (_transportShip->canUnloadAtPosition(position) && _loadedUnits.size() > 0) {
			unloadTroops();
			_loadFrameChange = BWAPI::Broodwar->getFrameCount() + 24;
		}
		
		if (BWAPI::Broodwar->getFrameCount() >= _loadFrameChange) {
			loadTroops();
		}
	}
	
}

// Called when the transport exists and is not full.
//在传输存在且未满时调用。
void MicroTransports::loadTroops()
{
	// If we're still busy loading the previous unit, wait.
	if (_transportShip->getLastCommand().getType() == BWAPI::UnitCommandTypes::Load)
	{
		return;
	}

	BWAPI::Unitset loadUnits = _transportShip->getUnitsInRadius(2 * 32);

	for (const BWAPI::Unit unit : loadUnits)
	{
		if (unit != _transportShip && !unit->isLoaded())
		{
			_transportShip->load(unit);
			_loadFrameChange = BWAPI::Broodwar->getFrameCount() + 24;
			return;
		}
	}

	//_loadFrameChange = BWAPI::Broodwar->getFrameCount() + (loadUnits.size() + 1) * 24;

}

bool MicroTransports::checkUnloadPosition() {
	bool change = false;

	BWAPI::Position position = _transportShip->getPosition();

	BWAPI::Position enemiePosition;
	double dist = 6 * 32;

	BWAPI::Unit closestEnemy = nullptr;
	double closestDistance = 6 * 32;

	BWAPI::Unitset enemies = _transportShip->getUnitsInRadius(4 * 32, BWAPI::Filter::IsEnemy);
	//BWAPI::Unit enemy = _transportShip->getClosestUnit(BWAPI::Filter::IsEnemy, 8 * 32);
	CombatSimulation sim;
	int scores = sim.simulateCombat(_transportShip->getLoadedUnits(), enemies);

	if (scores < 0) {
		for (auto enemy : enemies) {
			if (enemy->getType().groundWeapon() == BWAPI::WeaponTypes::None) continue;
			enemiePosition = enemy->getPosition();

			int maxRange = enemy->getType().groundWeapon().maxRange();
			double dist = maxRange - position.getDistance(enemiePosition);
			if (dist > 0 && dist < closestDistance) {
				change = true;
				closestDistance = dist;
				closestEnemy = enemy;
			}
		}
	}

	if (!change) {
		//_transportShip->unloadAll(position);
		_to = position;
	}
	else {
		if (closestEnemy) {
			enemiePosition = closestEnemy->getPosition();
			position = MapTools::Instance().getExtendedPosition(position, enemiePosition, closestDistance + 1 * 32);
			//Micro::Move(_transportShip, position);
			//_mapEdgeVertices.push_back(position);
			_to = position;
			_lastFrameChange = BWAPI::Broodwar->getFrameCount();
		}
	}

	return change;
}

// Only called when the transport exists and is loaded.
void MicroTransports::unloadTroops()
{
	// Unload if we're close to the destination, or if we're scary low on hit points.
	// It's possible that we'll land on a cliff and the units will be stuck there.
	//如果我们接近目的地, 或者如果我们的命中点很低, 我们就卸载。
	//有可能我们会降落在悬崖上, 单位将被困在那里。
	const int transportHP = _transportShip->getHitPoints() + _transportShip->getShields();
	
	const BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();
	BWAPI::Position position = _transportShip->getPosition();

	/*
	if ((transportHP < 50 || (enemyBaseLocation && _transportShip->getDistance(enemyBaseLocation->getPosition()) < 1 * 32)) &&
		_transportShip->canUnloadAtPosition(_transportShip->getPosition()))
		*/
	if ((transportHP < 50 || (_to && position.getDistance(_to) < 5 * 32)) &&
		_transportShip->canUnloadAtPosition(position))
	{
		// get the unit's current command
		BWAPI::UnitCommand currentCommand(_transportShip->getLastCommand());

		// if we've already told this unit to unload, wait
		if (currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All || currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All_Position)
		{
			return;
		}

		bool change = false;
		BWAPI::Position enemiePosition;
		double dist = 10 * 32;

		BWAPI::Unit closestEnemy = nullptr;
		double closestDistance = 10 * 32;

		BWAPI::Unitset enemies = _transportShip->getUnitsInRadius(8 * 32, BWAPI::Filter::IsEnemy);
		//BWAPI::Unit enemy = _transportShip->getClosestUnit(BWAPI::Filter::IsEnemy, 8 * 32);
		CombatSimulation sim;
		int scores = sim.simulateCombat(_transportShip->getLoadedUnits(), enemies);

		if (scores < 0) {
			for (auto enemy : enemies) {
				if (enemy->getType().groundWeapon() == BWAPI::WeaponTypes::None) continue;
				enemiePosition = enemy->getPosition();

				int maxRange = enemy->getType().groundWeapon().maxRange();
				double dist = maxRange - position.getDistance(enemiePosition);
				if (dist > 0 && dist < closestDistance) {
					change = true;
					closestDistance = dist;
					closestEnemy = enemy;
				}
			}
		}

		if (!change) {
			_transportShip->unloadAll(position);
			_to = position;
		}
		else {
			if (closestEnemy) {
				enemiePosition = closestEnemy->getPosition();
				position = MapTools::Instance().getExtendedPosition(position, enemiePosition, closestDistance + 1 * 32);
				//Micro::Move(_transportShip, position);
				_mapEdgeVertices.push_back(position);
				_to = position;
				_lastFrameChange = BWAPI::Broodwar->getFrameCount();
			}
		}
	}	
}

// Called when the transport exists and is loaded.
//在传输存在并加载时调用。
void MicroTransports::moveTransport()
{
	// If I didn't finish unloading the troops, wait
	//如果我没把部队卸完, 等一下
	BWAPI::UnitCommand currentCommand(_transportShip->getLastCommand());
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All || currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All_Position) &&
		_transportShip->getLoadedUnits().size() > 0)
	{
		return;
	}

	if (_to.isValid() && _from.isValid())
	{
		followPerimeter(_to, _from);
	}
	else
	{
		//followPerimeter();
		Micro::Move(_transportShip, _to);
	}
}

void MicroTransports::followPerimeter(int clockwise)
{
	BWAPI::Position goTo = getFleePosition(clockwise);

	if (Config::Debug::DrawUnitTargetInfo)
	{
		BWAPI::Broodwar->drawCircleMap(goTo, 5, BWAPI::Colors::Red, true);
	}

	Micro::Move(_transportShip, goTo);
}

//跟踪外围区域
void MicroTransports::followPerimeter(BWAPI::Position to, BWAPI::Position from)
{
	static int following = 0;
	if (following)
	{
		followPerimeter(following);
		return;
	}

	//assume we're near FROM! 
	//假设我们在附近!
	if (_transportShip->getDistance(from) < 50 && _waypoints.empty())
	{
		//compute waypoints
		std::pair<int, int> wpIDX = findSafePath(to, from);
		bool valid = (wpIDX.first > -1 && wpIDX.second > -1);
		UAB_ASSERT(valid, "waypoints not valid");
		_waypoints.push_back(_mapEdgeVertices[wpIDX.first]);
		_waypoints.push_back(_mapEdgeVertices[wpIDX.second]);

		// BWAPI::Broodwar->printf("WAYPOINTS: [%d] - [%d]", wpIDX.first, wpIDX.second);

		Micro::Move(_transportShip, _waypoints[0]);
	}
	else if (_waypoints.size() > 1 && _transportShip->getDistance(_waypoints[0]) < 100)
	{
		// BWAPI::Broodwar->printf("FOLLOW PERIMETER TO SECOND WAYPOINT!");
		//follow perimeter to second waypoint! 
		//clockwise or counterclockwise? 
		//int closestPolygonIndex = getClosestVertexIndex(_transportShip);
		int closestPolygonIndex = getClosestVertexIndex(to);
		UAB_ASSERT(closestPolygonIndex != -1, "Couldn't find a closest vertex");  // ensures map edge exists

		// This controls which way around the map we go.
		if (_mapEdgeVertices[(closestPolygonIndex + 1) % _mapEdgeVertices.size()].getApproxDistance(_waypoints[1]) <
			_mapEdgeVertices[(closestPolygonIndex - 1) % _mapEdgeVertices.size()].getApproxDistance(_waypoints[1]))
		{
			following = 1;
			followPerimeter(following);
		}
		else
		{
			following = -1;
			followPerimeter(following);
		}

	}
	else if (_waypoints.size() > 1 && _transportShip->getDistance(_waypoints[1]) < 50)
	{	
		//if close to second waypoint, go to destination!
		following = 0;
		Micro::Move(_transportShip, to);
	}
}

int MicroTransports::getClosestVertexIndex(BWAPI::Unit unit)
{
	return getClosestVertexIndex(unit->getPosition());
}

int MicroTransports::getClosestVertexIndex(BWAPI::Position p)
{
	int closestIndex = -1;
	int closestDistance = 10000000;

	for (size_t i(0); i < _mapEdgeVertices.size(); ++i)
	{
		int dist = p.getApproxDistance(_mapEdgeVertices[i]);
		if (dist < closestDistance)
		{
			closestDistance = dist;
			closestIndex = i;
		}
	}

	return closestIndex;
}

//获取安全路径
std::pair<int,int> MicroTransports::findSafePath(BWAPI::Position to, BWAPI::Position from)
{
	// BWAPI::Broodwar->printf("FROM: [%d,%d]",from.x, from.y);
	// BWAPI::Broodwar->printf("TO: [%d,%d]", to.x, to.y);

	//closest map edge point to destination
	int endPolygonIndex = getClosestVertexIndex(to);
	//BWAPI::Broodwar->printf("end indx: [%d]", endPolygonIndex);

	UAB_ASSERT_WARNING(endPolygonIndex != -1, "Couldn't find a closest vertex");
	BWAPI::Position enemyEdge = _mapEdgeVertices[endPolygonIndex];

	BWAPI::Position enemyPosition = order.getPosition();

	//find the projections on the 4 edges
	UAB_ASSERT_WARNING((_minCorner.isValid() && _maxCorner.isValid()), "Map corners should have been set! (transport mgr)");
	std::vector<BWAPI::Position> p;
	p.push_back(BWAPI::Position(from.x, _minCorner.y));
	p.push_back(BWAPI::Position(from.x, _maxCorner.y));
	p.push_back(BWAPI::Position(_minCorner.x, from.y));
	p.push_back(BWAPI::Position(_maxCorner.x, from.y));

	//for (auto _p : p)
		//BWAPI::Broodwar->printf("p: [%d,%d]", _p.x, _p.y);

	int d1 = p[0].getApproxDistance(enemyPosition);
	int d2 = p[1].getApproxDistance(enemyPosition);
	int d3 = p[2].getApproxDistance(enemyPosition);
	int d4 = p[3].getApproxDistance(enemyPosition);

	//have to choose the two points that are not max or min (the sides!)
	int maxIndex = (d1 > d2 ? d1 : d2) > (d3 > d4 ? d3 : d4) ?
						  (d1 > d2 ? 0 : 1) : (d3 > d4 ? 2 : 3);
	
	int minIndex = (d1 < d2 ? d1 : d2) < (d3 < d4 ? d3 : d4) ?
						   (d1 < d2 ? 0 : 1) : (d3 < d4 ? 2 : 3);

	if (maxIndex > minIndex)
	{
		p.erase(p.begin() + maxIndex);
		p.erase(p.begin() + minIndex);
	}
	else
	{
		p.erase(p.begin() + minIndex);
		p.erase(p.begin() + maxIndex);
	}

	//BWAPI::Broodwar->printf("new p: [%d,%d] [%d,%d]", p[0].x, p[0].y, p[1].x, p[1].y);

	//get the one that works best from the two.
	BWAPI::Position waypoint = (enemyEdge.getApproxDistance(p[0]) < enemyEdge.getApproxDistance(p[1])) ? p[0] : p[1];

	int startPolygonIndex = getClosestVertexIndex(waypoint);

	return std::pair<int, int>(startPolygonIndex, endPolygonIndex);

}

BWAPI::Position MicroTransports::getFleePosition(int clockwise)
{
	UAB_ASSERT(!_mapEdgeVertices.empty(), "We should have a transport route!");

	//BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	// if this is the first flee, we will not have a previous perimeter index
	if (_currentRegionVertexIndex == -1)
	{
		// so return the closest position in the polygon
		int closestPolygonIndex = getClosestVertexIndex(_transportShip);

		UAB_ASSERT_WARNING(closestPolygonIndex != -1, "Couldn't find a closest vertex");

		if (closestPolygonIndex == -1)
		{
			return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
		}
		else
		{
			// set the current index so we know how to iterate if we are still fleeing later
			_currentRegionVertexIndex = closestPolygonIndex;
			return _mapEdgeVertices[closestPolygonIndex];
		}
	}
	// if we are still fleeing from the previous frame, get the next location if we are close enough
	else
	{
		double distanceFromCurrentVertex = _mapEdgeVertices[_currentRegionVertexIndex].getDistance(_transportShip->getPosition());

		// keep going to the next vertex in the perimeter until we get to one we're far enough from to issue another move command
		while (distanceFromCurrentVertex < 128*2)
		{
			_currentRegionVertexIndex = (_currentRegionVertexIndex + clockwise*1) % _mapEdgeVertices.size();

			distanceFromCurrentVertex = _mapEdgeVertices[_currentRegionVertexIndex].getDistance(_transportShip->getPosition());
		}

		return _mapEdgeVertices[_currentRegionVertexIndex];
	}
}

void MicroTransports::setTransportShip(BWAPI::Unit unit)
{
	_transportShip = unit;
}

bool MicroTransports::hasTransportShip() const
{
	return UnitUtil::IsValidUnit(_transportShip);
}

void MicroTransports::setFrom(BWAPI::Position from)
{
	if (from.isValid())
	{
		_from = from;
	}
}

void MicroTransports::setTo(BWAPI::Position to)
{
	if (to.isValid()){
		_to = to;
	}
}
