#include "InformationManager.h"

#include "MapTools.h"
#include "ProductionManager.h"
#include "Random.h"
#include "UnitUtil.h"
#include "MapPosition.h"
#include "BWMap.h"

using namespace UAlbertaBot;

InformationManager::InformationManager()
    : _self(BWAPI::Broodwar->self())
    , _enemy(BWAPI::Broodwar->enemy())
	, _enemyProxy(false)
	
	, _weHaveCombatUnits(false)
	, _enemyHasCombatUnits(false)
	, _enemyHasStaticAntiAir(false)
	, _enemyHasAntiAir(false)
	, _enemyHasAirTech(false)
	, _enemyHasCloakTech(false)
	, _enemyHasMobileCloakTech(false)
	, _enemyHasOverlordHunters(false)
	, _enemyHasStaticDetection(false)
	, _enemyHasMobileDetection(_enemy->getRace() == BWAPI::Races::Zerg)
{
	initializeTheBases();
	initializeRegionInformation();
	//initializeNaturalBase();
	initializeReconLocation();//更新侦察点
}

// This fills in _theBases with neutral bases. An event will place our resourceDepot.
void InformationManager::initializeTheBases()
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		_theBases[base] = new Base(base->getPosition());
	}
}

// Set up _mainBaseLocations and _occupiedLocations.
void InformationManager::initializeRegionInformation()
{
	//配置文件中是否指定了出生位置
	if (Config::BaseLocations::LocationsModel) {
		_mainBaseLocations[_self] = BWTA::getNearestBaseLocation(Config::BaseLocations::Self);
		_mainBaseLocations[_enemy] = BWTA::getNearestBaseLocation(Config::BaseLocations::Enemy);
	}
	else {
		_mainBaseLocations[_self] = BWTA::getStartLocation(_self);
		_mainBaseLocations[_enemy] = BWTA::getStartLocation(_enemy);

		for (auto ally : BWAPI::Broodwar->allies()) {
			_mainBaseLocations[ally] = BWTA::getStartLocation(ally);
		}
	}
	
	//_mainBaseLocations[_self] = BWTA::getStartLocation(_self);
	//_mainBaseLocations[_enemy] = BWTA::getStartLocation(_enemy);
	// push that region into our occupied vector
	//把那个区域推入我们被占领的向量中

	_myRegionVertices = MapTools::Instance().calculateEnemyRegionVertices(_mainBaseLocations[_self]);
	updateOccupiedRegions(BWTA::getRegion(_mainBaseLocations[_self]->getTilePosition()), _self);
}

// Figure out what base is our "natural expansion". In rare cases, there might be none.
// Prerequisite: Call initializeRegionInformation() first.
//找出什么是我们的 "自然扩展 " 的基础。在极少数情况下, 可能没有。
//先决条件: 首先调用 initializeRegionInformation()。
void InformationManager::initializeNaturalBase()
{
	// We'll go through the bases and pick the best one as the natural.
	BWTA::BaseLocation * bestBase = nullptr;
	double bestScore = 0.0;

	BWAPI::TilePosition homeTile = _self->getStartLocation();
	BWAPI::Position myBasePosition(homeTile);

	BWTA::Chokepoint * c = BWTA::getNearestChokepoint(myBasePosition);
	_myNearestChokepoint = c->getCenter();

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		double score = 0.0;

		BWAPI::TilePosition tile = base->getTilePosition();

		/*
		if (BWAPI::Broodwar->mapHash() == "c73aa8daade77e62a3dc23bda5b4137bf7ad25bd") {
			if (homeTile.getDistance(tile) < 10) {
				_myNaturalBaseLocation = base;
				return;
			}
		}
		*/

		// The main is not the natural.
		if (tile == homeTile)
		{
			continue;
		}

		if (_mainBaseLocations[_enemy]) {
			if (tile == _mainBaseLocations[_enemy]->getTilePosition()) {
				continue;
			}
		}

		// Ww want to be close to our own base.
		double distanceFromUs = MapTools::Instance().getGroundTileDistance(BWAPI::Position(tile), myBasePosition);

		// If it is not connected, skip it. Islands do this.
		if (!BWTA::isConnected(homeTile, tile) || distanceFromUs < 0)
		{
			continue;
		}

		// Add up the score.
		score = -distanceFromUs;

		// More resources -> better.
		score += 0.01 * base->minerals() + 0.02 * base->gas();

		if (!bestBase || score > bestScore)
		{
			bestBase = base;
			bestScore = score;
		}
	}

	// bestBase may be null on unusual maps.
	_myNaturalBaseLocation = bestBase;
}

// A base is inferred to exist at the given position, without having been seen.
// Only enemy bases can be inferred; we see our own.
// Adjust its value to match. It is not reserved.
//一个基被推断为存在于给定的位置, 而不被看到。
//只有敌人的基地可以推断; 我们看到了自己的。
//调整其值以匹配。它不是保留的。
void InformationManager::baseInferred(BWTA::BaseLocation * base)
{
	if (_theBases[base]->owner != _self)
	{
		_theBases[base]->setOwner(nullptr, _enemy);
	}
}

// The given resource depot has been created or discovered.
// Adjust its value to match. It is not reserved.
// This accounts for the theoretical case that it might be neutral.
//已创建或发现给定的资源库。
//调整其值以匹配。它不是保留的。
//这就解释了它可能是中性的理论案例。
void InformationManager::baseFound(BWAPI::Unit depot)
{
	UAB_ASSERT(depot && depot->getType().isResourceDepot(), "bad args");

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (closeEnough(base->getTilePosition(), depot->getTilePosition()))
		{
			baseFound(base, depot);
			return;
		}
	}
}

// Set a base where the base and depot are both known.
// The depot must be at or near the base location; this is not checked.
//设置基地和仓库都已知的基础。
//车厂必须位于或靠近基地位置; 或未选中此复选。
void InformationManager::baseFound(BWTA::BaseLocation * base, BWAPI::Unit depot)
{
	UAB_ASSERT(base && depot && depot->getType().isResourceDepot(), "bad args");

	_theBases[base]->setOwner(depot, depot->getPlayer());
}

// Something that may be a base was just destroyed.
// If it is, update the value to match.
// If the lost base was our main, choose a new one if possible.
//可能是基地的东西刚刚被毁了
//如果是, 则更新要匹配的值。
//如果丢失的基地是我们的主要, 选择一个新的, 如果可能的话。
void InformationManager::baseLost(BWAPI::TilePosition basePosition)
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (closeEnough(base->getTilePosition(), basePosition))
		{
			baseLost(base);
			return;
		}
	}
}

// A base was lost and is now unowned.
// If the lost base was our main, choose a new one if possible.
//一个基地失去了, 现在是无主的。
//如果丢失的基地是我们的主要, 选择一个新的, 如果可能的话。
void InformationManager::baseLost(BWTA::BaseLocation * base)
{
	UAB_ASSERT(base, "bad args");

	_theBases[base]->setOwner(nullptr, BWAPI::Broodwar->neutral());
	if (base == getMyMainBaseLocation())
	{
		chooseNewMainBase();        // our main was lost, choose a new one
	}
}

// Our main base has been destroyed. Choose a new one if possible.
// Otherwise we'll keep trying to build in the old one, where the enemy may still be.
//我们的主要基地被摧毁了。如果可能, 请选择一个新的。
//否则, 我们将继续试图建立在旧的一个, 敌人可能仍然是。
void InformationManager::chooseNewMainBase()
{
	BWTA::BaseLocation * oldMain = getMyMainBaseLocation();

	// Choose a base we own which is as far away from the old main as possible.
	// Maybe that will be safer.
	double newMainDist = 0.0;
	BWTA::BaseLocation * newMain = nullptr;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == _self)
		{
			double dist = base->getAirDistance(oldMain);
			if (dist > newMainDist)
			{
				newMainDist = dist;
				newMain = base;
			}
		}
	}

	// If we didn't find a new main base, we're in deep trouble. We may as well keep the old one.
	// By decree, we always have a main base, even if it is unoccupied. It simplifies the rest.
	if (newMain)
	{
		//_mainBaseLocations[_self] = newMain;
	}
}

// With some probability, randomly choose a base as the new "main" base.
//有一定的概率, 随机选择一个基地作为新的  "主 " 基地。
void InformationManager::maybeChooseNewMainBase()
{
	// 1. If out of book, decide randomly whether to choose a new base.
	if (ProductionManager::Instance().isOutOfBook() && Random::Instance().index(2) == 0)
	{
		// 2. List my bases.
		std::vector<BWTA::BaseLocation *> myBases;
		for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
		{
			if (_theBases[base]->owner == _self &&
				_theBases[base]->resourceDepot &&
				_theBases[base]->resourceDepot->isCompleted())
			{
				 myBases.push_back(base);
			}
		}

		// 3. Choose one, if there is a choice.
		if (myBases.size() > 1)
		{
			_mainBaseLocations[_self] = myBases.at(Random::Instance().index(myBases.size()));
		}
	}
}

// The given unit was just created or morphed.
// If it is a resource depot for our new base, record it.
// NOTE: It is a base only if it's in the right position according to BWTA.
// A resource depot will not be recorded if it is offset by too much.
// NOTE: This records the initial depot at the start of the game.
// There's no need to take special action to record the starting base.
void InformationManager::maybeAddBase(BWAPI::Unit unit)
{
	if (unit->getType().isResourceDepot())
	{
		baseFound(unit);
	}
}

// The two possible base positions are close enough together
// that we can say they are "the same place" as a base.
//两个可能的基地位置是足够紧密的在一起
//我们可以说他们是 "同一个地方" 作为基地。
bool InformationManager::closeEnough(BWAPI::TilePosition a, BWAPI::TilePosition b)
{
	return abs(a.x - b.x) <= 2 && abs(a.y - b.y) <= 2;
}

void InformationManager::update()
{
	updateUnitInfo();
	updateBaseLocationInfo();
	updateTheBases();
	updateGoneFromLastPosition();
}

void InformationManager::updateUnitInfo() 
{
	//_enemyFightScore = 0;
	for (const auto unit : _enemy->getUnits())
	{
		updateUnit(unit);
		//_enemyFightScore += getUnitFightScore(unit);//计算敌方战斗力
	}

	//_selfFightScore = 0;
	for (const auto unit : _self->getUnits())
	{
		updateUnit(unit);
		//_selfFightScore += getUnitFightScore(unit);//计算我方战斗力
	}

	_unitData[_enemy].removeBadUnits();
	_unitData[_self].removeBadUnits();
}

//计算战斗力
double InformationManager::getUnitFightScore(BWAPI::Unit unit) {
	double fightScore = 0;
	
	/*
	if (unit->getType() == BWAPI::UnitTypes::Terran_SCV) {
		return fightScore;
	}

	if (unit->getType() == BWAPI::UnitTypes::Zerg_Drone) {
		return fightScore;
	}

	if (unit->getType().isBuilding()) {
		return fightScore;
	}
	*/
	//如果没有武器攻击力，则返回0
	if (unit->getType().airWeapon() == BWAPI::WeaponTypes::None && unit->getType().groundWeapon() == BWAPI::WeaponTypes::None) {
		return 0;
	}

	fightScore = unit->getType().mineralPrice() + unit->getType().gasPrice() + unit->getType().supplyRequired() * 12.5;

	return fightScore;
}

void InformationManager::setEnemy(BWAPI::Player player) {
	_enemy = player;
	//_unloadPosition = BWAPI::Position(0, 0);
	//_enemyChokePoint = BWAPI::Position(0, 0);

	if (_mainBaseLocations[_enemy]) {
		setEnemyMainBaseLocation(_mainBaseLocations[_enemy]);
	}
}

//更新基地信息
void InformationManager::updateBaseLocationInfo() 
{
	int frame = BWAPI::Broodwar->getFrameCount();

	_occupiedRegions[_self].clear();
	_occupiedRegions[_enemy].clear();
	
	// In the early game, look for enemy overlords as evidence of the enemy base.
	//在早期的比赛中, 寻找敌人的霸主作为敌人基地的证据。
	//enemyBaseLocationFromOverlordSighting();

	//enemyBaseLocationNatural();

	// if we haven't found the enemy main base location yet
	//如果我们还没有找到敌人的主要基地位置, 但
	if (!_mainBaseLocations[_enemy]) 
	{
		// how many start locations have we explored
		//我们探索了多少开始位置
		size_t exploredStartLocations = 0;
		bool baseFound = false;

		// an unexplored base location holder
		//一个未探测到的基地位置持有者
		BWTA::BaseLocation * unexplored = nullptr;

		for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations()) 
		{
			if (isEnemyBuildingInRegion(BWTA::getRegion(startLocation->getTilePosition()))) 
			{
				updateOccupiedRegions(BWTA::getRegion(startLocation->getTilePosition()), _enemy);

				// On a competition map, our base and the enemy base will never be in the same region.
				// If we find an enemy building in our region, it's a proxy.
				if (startLocation != _mainBaseLocations[_self]) {
					for (auto ally : BWAPI::Broodwar->allies()) {
						_mainBaseLocations[ally] = BWTA::getStartLocation(ally);
					}
				}

				bool  isEnemyBase = true;
				for (auto it = _mainBaseLocations.begin(); it != _mainBaseLocations.end(); ++it) {
					//BWAPI::Position center = (*it).first;
					BWTA::BaseLocation * mainBase = (*it).second;

					if (startLocation == mainBase) {
						isEnemyBase = false;
						continue;
					}
				}

				if (isEnemyBase) {
					_mainBaseLocations[_enemy] = startLocation;
				}

				if (startLocation == getMyMainBaseLocation())
				{
					_enemyProxy = true;
				}
				else
				{
					if (Config::Debug::DrawScoutInfo)
					{
						BWAPI::Broodwar->printf("Enemy base seen");//敌人基地看见了
					}

					baseFound = true;
					/*
					_mainBaseLocations[_enemy] = startLocation;
					baseInferred(startLocation);
					*/
					setEnemyMainBaseLocation(startLocation);
				}
			}

			// TODO If the enemy is zerg, we can be a little quicker by looking for creep.
			// TODO If we see a mineral patch that has been mined, that should be a base.
			if (BWAPI::Broodwar->isExplored(startLocation->getTilePosition())) 
			{
				// Count the explored bases.
				++exploredStartLocations;
			} 
			else 
			{
				// Remember the unexplored base. It may be the only one.
				unexplored = startLocation;
			}
		}

		// if we've explored every start location except one, it's the enemy
		//如果我们探索每一个起点，除了一个，那就是敌人。
		if (!baseFound && exploredStartLocations + 1 == BWAPI::Broodwar->getStartLocations().size()) 
		{
            if (Config::Debug::DrawScoutInfo)
            {
                BWAPI::Broodwar->printf("Enemy base found by elimination");
            }
			
			/*
			_mainBaseLocations[_enemy] = unexplored;
			baseInferred(unexplored);
			*/
			setEnemyMainBaseLocation(unexplored);

			updateOccupiedRegions(BWTA::getRegion(unexplored->getTilePosition()), _enemy);
		}
	// otherwise we do know it, so push it back
	}
	else 
	{
		updateOccupiedRegions(BWTA::getRegion(_mainBaseLocations[_enemy]->getTilePosition()), _enemy);
	}

	/*
	if (_mainBaseLocations[_enemy] && !_myNaturalBaseLocation) {
		initializeNaturalBase();
	}
	*/

	checkUnloadPosition();

	if (_enemyChokePoint && _enemyChokePoint.x != 0 && _enemyChokePoint.y != 0) {
		//BWAPI::Position dropPosition = MapPosition::Instance().getUnloadPosition();
		BWAPI::Broodwar->drawCircleMap(_enemyChokePoint, 8, BWAPI::Colors::Red, true);

		if (BWAPI::Broodwar->getUnitsInRadius(_enemyChokePoint, 6 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::IsBuilding).size() > 1) {
			isDukou = true;
		}
		else {
			isDukou = false;
		}
	}

	if (_attackPosition.isValid() && _attackPosition.x != 0 && _attackPosition.y != 0) {
		BWAPI::Broodwar->drawCircleMap(_attackPosition, 4, BWAPI::Colors::Red, true);
	}

	_enemyFightScore = 0;
	// The enemy occupies a region if it has a building there.
	for (const auto & kv : _unitData[_enemy].getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isBuilding() && !ui.goneFromLastPosition)
		{
			updateOccupiedRegions(BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)), _enemy);
		}

		_enemyFightScore += ui.fightScore;//计算敌方战斗力
	}

	_selfFightScore = 0;
	// We occupy a region if we have a building there.
	for (const auto & kv : _unitData[_self].getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isBuilding() && !ui.goneFromLastPosition)
		{
			for (BWTA::BaseLocation * baseLocation : BWTA::getBaseLocations())
			{
				//如果是矿区，则标记
				if (BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)) == BWTA::getRegion(baseLocation->getTilePosition())) {
					updateOccupiedRegions(BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)), _self);
				}
			}
		}

		if (ui.type.isWorker()) continue;//我方的农民不参于计算

		if (ui.type == BWAPI::UnitTypes::Protoss_Shuttle ||
			ui.type == BWAPI::UnitTypes::Terran_Dropship) {
			_hasTransports = true;
		}

		_selfFightScore += ui.fightScore;
	}
}

//获取敌人空投点
BWAPI::Position InformationManager::getUnloadPosition() {

	int i = Random::Instance().index(3);

	if (i != 1) {
		for (auto enemy : BWAPI::Broodwar->enemies()) {
			if (enemy->leftGame()) continue;
			if (enemy == _enemy) continue;

			if (_mainBaseLocations[enemy]) {
				setEnemyMainBaseLocation(_mainBaseLocations[enemy]);
				break;
			}
		}
	}

	//如果空投点没有基地，则换一个
	if (BWAPI::Broodwar->isVisible(BWAPI::TilePosition(_unloadPosition))) {
		BWAPI::Unitset depots = BWAPI::Broodwar->getUnitsInRadius(_unloadPosition, 4 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::IsResourceDepot);
		BWAPI::Unitset workers = BWAPI::Broodwar->getUnitsInRadius(_unloadPosition, 4 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::IsWorker);

		if (depots.size() == 0 || workers.size() < 6) {
			for (auto enemy : BWAPI::Broodwar->enemies()) {
				if (enemy->leftGame()) continue;
				std::string newName = enemy->getName();
				std::string oldName = _enemy->getName();

				if (enemy->getName() == _enemy->getName()) continue;

				_enemy = enemy;
				if (_mainBaseLocations[enemy]) {
					setEnemyMainBaseLocation(_mainBaseLocations[enemy]);
					break;
				}
			}
		}
	}

	return _unloadPosition;
}

//查询空投点
BWAPI::Position InformationManager::checkUnloadPosition() {
	if (_unloadPosition && _unloadPosition.x != 0 && _unloadPosition.y != 0) {
		//BWAPI::Position dropPosition = MapPosition::Instance().getUnloadPosition();
		BWAPI::Broodwar->drawCircleMap(_unloadPosition, 8, BWAPI::Colors::Red, true);
	}

	return _unloadPosition;
}

//获取自家空投点
BWAPI::Position InformationManager::getMyUnloadPosition() {
	if (_myUnloadPosition) {
		std::string baseName = BWMap::Instance().getStartLocationName(getMyMainBaseLocation()->getTilePosition());

		if (BWMap::Instance().UnloadPoints[baseName]) {
			_myUnloadPosition = BWMap::Instance().UnloadPoints[baseName];
		}
	}

	return _myUnloadPosition;
}

bool InformationManager::myHasTransports() {
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) {
		if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Pneumatized_Carapace) > 0) {
			_hasTransports = true;
		}
	}
	
	return _hasTransports;
}

void InformationManager::enemyBaseLocationNatural() {
	if (_mainBaseLocations[_enemy])
	{
		return;
	}

	const int now = BWAPI::Broodwar->getFrameCount();

	if (now > 5 * 60 * 24)
	{
		return;
	}

	BWTA::BaseLocation * possibleEnemyBase = nullptr;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (BWAPI::Broodwar->isExplored(base->getTilePosition()))
		{
			// We've already seen this base, and the enemy is not there.
			continue;
		}

		if (isEnemyBuildingInRegion(BWTA::getRegion(base->getTilePosition()))){
			double maxDistance = 128 * 32;

			for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
			{
				double airDistance = base->getAirDistance(startLocation);
				if (airDistance < maxDistance)
				{
					// It could have started from this base.
					possibleEnemyBase = startLocation;
					maxDistance = airDistance;
				}
			}

			if (Config::Debug::DrawScoutInfo)
			{
				BWAPI::Broodwar->printf("Enemy base found by elimination");
			}

			_mainBaseLocations[_enemy] = possibleEnemyBase;
			baseInferred(possibleEnemyBase);
			updateOccupiedRegions(BWTA::getRegion(possibleEnemyBase->getTilePosition()), _enemy);
			return;
		}
	}
}

// If the opponent is zerg and it's early in the game, we may be able to infer the enemy
// base's location by seeing the first overlord.
// NOTE This doesn't quite extract all the information from an overlord sighting. In principle,
//      we might be able to exclude a base location without being sure which remaining base is
//      the enemy base. Steamhammer doesn't provide a way to exclude bases.
//如果对手是虫族, 它在游戏的早期, 我们也许能够推断出敌人
//基地的位置, 看到第一个霸王。
//注意, 这并不能完全从一个霸王目击中提取所有信息。原则上,
//我们可能可以排除一个基本位置, 而不确定哪个剩余的基数是
//敌人基地。Steamhammer 没有提供一种排除基地的方法。
void InformationManager::enemyBaseLocationFromOverlordSighting()
{
	// If we already know the base location, there's no need to try to infer it.
	//如果我们已经知道了基地的位置, 没有必要去推断它。
	if (_mainBaseLocations[_enemy])
	{
		return;
	}
	
	const int now = BWAPI::Broodwar->getFrameCount();

	if (_enemy->getRace() != BWAPI::Races::Zerg || now > 5 * 60 * 24)
	{
		return;
	}

	for (const auto unit : _enemy->getUnits())
	{
		if (unit->getType() != BWAPI::UnitTypes::Zerg_Overlord || unit->getType() != BWAPI::UnitTypes::Zerg_Drone)
		{
			continue;
		}

		// What bases could the overlord be from? Can we narrow it down to 1 possibility?
		BWTA::BaseLocation * possibleEnemyBase = nullptr;
		int countPossibleBases = 0;
		for (BWTA::BaseLocation * base : BWTA::getStartLocations())
		{
			if (BWAPI::Broodwar->isExplored(base->getTilePosition()))
			{
				// We've already seen this base, and the enemy is not there.
				continue;
			}

			// Assume the overlord came from this base.
			// Where did the overlord start from? It starts offset from the hatchery in a specific way.
			BWAPI::Position overlordStartPos;
			overlordStartPos.x = base->getPosition().x + ((base->getPosition().x < 32 * BWAPI::Broodwar->mapWidth() / 2) ? +99 : -99);
			overlordStartPos.y = base->getPosition().y + ((base->getPosition().y < 32 * BWAPI::Broodwar->mapHeight() / 2) ? +65 : -65);

			// How far could it have traveled from there?
			double maxDistance = double(now) * (BWAPI::UnitTypes::Zerg_Overlord).topSpeed();
			if (maxDistance >= double(unit->getDistance(overlordStartPos)))
			{
				// It could have started from this base.
				possibleEnemyBase = base;
				++countPossibleBases;
			}
		}

		if (countPossibleBases == 1)
		{
			// Success.
			_mainBaseLocations[_enemy] = possibleEnemyBase;
			baseInferred(possibleEnemyBase);
			updateOccupiedRegions(BWTA::getRegion(possibleEnemyBase->getTilePosition()), _enemy);
			return;
		}
	}
}

void InformationManager::initializeReconLocation()
{
	_reconLocations.clear();
	//std::vector<BWAPI::Position> clearLocations;		//不需要侦察的点

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		_reconLocations[base->getPosition()] = BWAPI::Broodwar->getFrameCount();

		/*
		for (const auto & kv : _unitData[_enemy].getUnits())
		{
			const UnitInfo & ui(kv.second);

			if (ui.type.isBuilding() && ui.unit->isCompleted() && !ui.goneFromLastPosition)
			{
				if (ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
					ui.type == BWAPI::UnitTypes::Zerg_Sunken_Colony) {

					if (BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)) == BWTA::getRegion(base->getTilePosition())) {
						clearLocations.push_back(base->getPosition());

						//如果分基地有，则主基地也不需要过去
						if (base == getEnemyNaturalLocation()) {
							clearLocations.push_back(getEnemyMainBaseLocation()->getPosition());
							//_reconLocations.push_back(MapPosition::Instance().getReconLocation(getEnemyMainBaseLocation()->getTilePosition()));
							_reconLocations.push_back(MapTools::Instance().getDistancePosition(ui.lastPosition, getMyMainBaseLocation()->getPosition(), 15 * 32));
						}
						break;
					}
				}
			}
		}

		int count = _reconLocations.size();
		for (auto clear : clearLocations)
		{
			auto iter = find(_reconLocations.begin(), _reconLocations.end(), clear);
			if (iter != _reconLocations.end()) _reconLocations.erase(iter);
		}
		*/
	}
}

// _theBases is not always correctly updated by the event-driven methods.
// Look for conflicting information and make corrections.
//事件驱动方法不总是正确更新 _theBases。
//查找冲突信息并进行更正。
void InformationManager::updateTheBases()
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		// If we can see the tile where the resource depot would be.
		if (BWAPI::Broodwar->isVisible(base->getTilePosition()))
		{
			setReconLocation(base->getPosition(), 40 * 24);

			BWAPI::Unitset units = BWAPI::Broodwar->getUnitsOnTile(base->getTilePosition());
			BWAPI::Unit depot = nullptr;
			for (const auto unit : units)
			{
				if (unit->getType().isResourceDepot())
				{
					depot = unit;
					break;
				}

				if (unit->getType() == BWAPI::UnitTypes::Terran_Bunker ||
					unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
					unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony)
				{

				}
			}
			if (depot)
			{
				// The base is occupied.
				baseFound(base, depot);
			}
			else
			{
				// The base is empty.
				baseLost(base);
			}
		}
		else
		{
			// We don't see anything. It's definitely not our base.
			//我们什么也没看到这绝对不是我们的基地
			if (_theBases[base]->owner == _self)
			{
				baseLost(base->getTilePosition());
			}
		}
	}

	for (auto local : _reconLocations) {
		BWAPI::Position localPosition = local.first;
		BWAPI::Broodwar->drawCircleMap(localPosition, 8, BWAPI::Colors::Green, true);
		BWAPI::Broodwar->drawTextMap(localPosition + BWAPI::Position(0, 10), "%d,%d", localPosition.x / 32, localPosition.y / 32);
	}
}

//更新被占领区域
void InformationManager::updateOccupiedRegions(BWTA::Region * region, BWAPI::Player player) 
{
	// if the region is valid (flying buildings may be in nullptr regions)
	if (region)
	{
		// add it to the list of occupied regions
		//将其添加到被占区域的列表中
		_occupiedRegions[player].insert(region);
		/*
		if (player == BWAPI::Broodwar->self()) {
			setReconLocation(region->getCenter(), 10 * 60 * 24);//不需要侦察
		}
		*/
	}
}

// If we can see the last known location of a remembered unit and the unit is not there,
// set the unit's goneFromLastPosition flag.
void InformationManager::updateGoneFromLastPosition()
{
	// We don't need to check often.
	// 1. The game supposedly only resets visible tiles when frame % 100 == 99.
	// 2. If the unit has only been gone from its location for a short time, it probably
	//    didn't go far (it might have been recalled or gone through a nydus).
	// So we check less than once per second.
	if (BWAPI::Broodwar->getFrameCount() % 32 == 5)
	{
		_unitData[_enemy].updateGoneFromLastPosition();
	}
}

bool InformationManager::isEnemyBuildingInRegion(BWTA::Region * region) 
{
	// invalid regions aren't considered the same, but they will both be null
	//无效区域不被视为相同, 但它们都将为 null
	if (!region)
	{
		return false;
	}

	for (const auto enemy : BWAPI::Broodwar->enemies()) {
		for (const auto & kv : _unitData[enemy].getUnits())
		{
			const UnitInfo & ui(kv.second);
			if (ui.type.isBuilding() && !ui.goneFromLastPosition)
			{
				if (BWTA::getRegion(BWAPI::TilePosition(ui.lastPosition)) == region)
				{
					if (_enemy != enemy) {
						setEnemy(enemy);
					}

					return true;
				}
			}
		}
	}

	return false;
}

const UIMap & InformationManager::getUnitInfo(BWAPI::Player player) const
{
	return getUnitData(player).getUnits();
}

BWAPI::Unitset InformationManager::getUnits(BWAPI::Player player, BWAPI::UnitType type)
{
	BWAPI::Unitset unitMap;
	UnitData unitData = getUnitData(player);

	if (!unitData.getUnits().empty()) {
		for (const auto & kv : unitData.getUnits())
		{
			const UnitInfo & ui(kv.second);

			if (ui.type == type)
			{
				unitMap.insert(ui.unit);
			}
		}
	}

	return unitMap;
}

std::set<BWTA::Region *> & InformationManager::getOccupiedRegions(BWAPI::Player player)
{
	return _occupiedRegions[player];
}

BWTA::BaseLocation * InformationManager::getMainBaseLocation(BWAPI::Player player)
{
	return _mainBaseLocations[player];
}

// Guaranteed non-null. If we have no bases left, it is our start location.
BWTA::BaseLocation * InformationManager::getMyMainBaseLocation()
{
	UAB_ASSERT(_mainBaseLocations[_self], "no base location");
	return _mainBaseLocations[_self];
}

// Null until the enemy base is located.
BWTA::BaseLocation * InformationManager::getEnemyMainBaseLocation()
{
	return _mainBaseLocations[_enemy];
}

bool InformationManager::isFindEnemyMainBase() {
	bool isFind = true;
	for (auto enemy : BWAPI::Broodwar->enemies()) {
		if (!_mainBaseLocations[enemy]) {
			isFind = false;
			break;
		}
	}

	return isFind;
}

void InformationManager::setEnemyMainBaseLocation(std::string baseName) {

	if (BWMap::Instance().StartLocations[baseName]) {
		for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
		{
			BWAPI::TilePosition startPosition = startLocation->getTilePosition();
			BWAPI::TilePosition basePosition = BWMap::Instance().StartLocations[baseName];

			if (startPosition == basePosition) {
				setEnemyMainBaseLocation(startLocation);
				break;
			}
		}
	}
}

void InformationManager::setEnemyMainBaseLocation(BWTA::BaseLocation * baseLocation){
	if (_enemy->getStartLocation() != baseLocation->getTilePosition()) {
		for (auto enemy : BWAPI::Broodwar->enemies()) {
			if (enemy->getStartLocation() == baseLocation->getTilePosition()) {
				_enemy = enemy;
				//setEnemy(enemy);
				break;
			}
		}
	}

	_mainBaseLocations[_enemy] = baseLocation;
	baseInferred(baseLocation);
	//_theBases[baseLocation]->setOwner(nullptr, _enemy);

	std::string baseName = BWMap::Instance().getStartLocationName(baseLocation->getTilePosition());

	if (BWMap::Instance().UnloadPoints[baseName]) {
		_unloadPosition = BWMap::Instance().UnloadPoints[baseName];
	}

	if (BWMap::Instance().ChokePoints[baseName]) {
		_enemyChokePoint = BWMap::Instance().ChokePoints[baseName];
	}

	return;

	if (_mainBaseLocations[_enemy]) {
		//空投位置
		if (_unloadPosition && _unloadPosition.x != 0 && _unloadPosition.y != 0) {
			//BWAPI::Position dropPosition = MapPosition::Instance().getUnloadPosition();
			BWAPI::Broodwar->drawCircleMap(_unloadPosition, 8, BWAPI::Colors::Red, true);
		}
		else {
			for (BWAPI::Unitset::iterator j = _mainBaseLocations[_enemy]->getStaticMinerals().begin(); j != _mainBaseLocations[_enemy]->getStaticMinerals().end(); j++)
			{
				_unloadPosition = (*j)->getInitialPosition();
				break;
			}
		}

		//阻塞点
		if (_enemyChokePoint && _enemyChokePoint.x != 0 && _enemyChokePoint.y != 0) {
			//BWAPI::Position dropPosition = MapPosition::Instance().getUnloadPosition();
			BWAPI::Broodwar->drawCircleMap(_enemyChokePoint, 8, BWAPI::Colors::Red, true);
			if (BWAPI::Broodwar->getUnitsInRadius(_enemyChokePoint, 6 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::IsBuilding).size() > 1) {
				isDukou = true;
			}
			else {
				isDukou = false;
			}
		}
		else {
			int startLocationName = MapPosition::Instance().getStartLocationName(_mainBaseLocations[_enemy]->getTilePosition());
			//startLocation = "L" + startLocationName;
			//std::cout << startLocation << "L" << startLocationName << std::endl;
			char *buf = new char[strlen("L") + sizeof(startLocationName) + 1];
			sprintf(buf, "%s%d%", "L", startLocationName);
			_enemyChokePoint = BWMap::Instance().ChokePoints[buf];
		}
	}
}

// Self, enemy, or neutral.
BWAPI::Player InformationManager::getBaseOwner(BWTA::BaseLocation * base)
{
	return _theBases[base]->owner;
}

// If it's the enemy base, the depot will be null if it has not been seen.
// If this is our base, there is still a chance that the depot may be null.
// And if not null, the depot may be incomplete.
BWAPI::Unit InformationManager::getBaseDepot(BWTA::BaseLocation * base)
{
	return _theBases[base]->resourceDepot;
}

// The natural base, whether it is taken or not.
// May be null on some maps.
BWTA::BaseLocation * InformationManager::getMyNaturalLocation()
{
	return _myNaturalBaseLocation;
}

BWTA::BaseLocation * InformationManager::getEnemyNaturalLocation()
{
	if (_enemyNaturalBaseLocation || !getEnemyMainBaseLocation()) return _enemyNaturalBaseLocation;

	// We'll go through the bases and pick the best one as the natural.
	BWTA::BaseLocation * bestBase = nullptr;
	double bestScore = 0.0;

	BWAPI::TilePosition homeTile = _mainBaseLocations[_enemy]->getTilePosition();
	BWAPI::Position myBasePosition(homeTile);

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		double score = 0.0;

		BWAPI::TilePosition tile = base->getTilePosition();

		// The main is not the natural.
		if (tile == homeTile)
		{
			continue;
		}

		// Ww want to be close to our own base.
		double distanceFromUs = MapTools::Instance().getGroundTileDistance(BWAPI::Position(tile), myBasePosition);

		// If it is not connected, skip it. Islands do this.
		if (!BWTA::isConnected(homeTile, tile) || distanceFromUs < 0)
		{
			continue;
		}

		// Add up the score.
		score = -distanceFromUs;

		// More resources -> better.
		score += 0.01 * base->minerals() + 0.02 * base->gas();

		if (!bestBase || score > bestScore)
		{
			bestBase = base;
			bestScore = score;
		}
	}

	// bestBase may be null on unusual maps.
	_enemyNaturalBaseLocation = bestBase;
	return _enemyNaturalBaseLocation;
}

// The number of bases believed owned by the given player,
// self, enemy, or neutral.
//被认为是给定玩家拥有的基地数目,
//自我, 敌人, 或中立。
int InformationManager::getNumBases(BWAPI::Player player)
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == player)
		{
			++count;
		}
	}

	return count;
}

// The number of non-island expansions that are not yet believed taken.
int InformationManager::getNumFreeLandBases()
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == BWAPI::Broodwar->neutral() && !base->isIsland())
		{
			++count;
		}
	}

	return count;
}

// Current number of mineral patches at all of my bases.
// Decreases as patches mine out, increases as new bases are taken.
//目前我所有基地的矿物补丁的数量。
//减少作为补丁地雷, 增加作为新的基地被采取。
int InformationManager::getMyNumMineralPatches()
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (_theBases[base]->owner == _self)
		{
			count += base->getMinerals().size();
		}
	}

	return count;
}

// Current number of geysers at all my completed bases, whether taken or not.
// Skip bases where the resource depot is not finished.
int InformationManager::getMyNumGeysers()
{
	int count = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		BWAPI::Unit depot = _theBases[base]->resourceDepot;

		if (_theBases[base]->owner == _self &&
			depot &&                // should never be null, but we check anyway
			(depot->isCompleted() || UnitUtil::IsMorphedBuildingType(depot->getType())))
		{
			count += base->getGeysers().size();
		}
	}

	return count;
}

// Current number of completed refineries at my completed bases,
// and number of bare geysers available to be taken.
void InformationManager::getMyGasCounts(int & nRefineries, int & nFreeGeysers)
{
	int refineries = 0;
	int geysers = 0;

	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		BWAPI::Unit depot = _theBases[base]->resourceDepot;

		if (_theBases[base]->owner == _self &&
			depot &&                // should never be null, but we check anyway
			(depot->isCompleted() || UnitUtil::IsMorphedBuildingType(depot->getType())))
		{
			// Recalculate the base's geysers every time.
			// This is a slow but accurate way to work around the BWAPI geyser bug.
			// To save cycles, call findGeysers() only when necessary (e.g. a refinery is destroyed).
			_theBases[base]->findGeysers();

			for (const auto geyser : _theBases[base]->getGeysers())
			{
				if (geyser && geyser->exists())
				{
					if (geyser->getPlayer() == _self &&
						geyser->getType().isRefinery() &&
						geyser->isCompleted())
					{
						++refineries;
					}
					else if (geyser->getPlayer() == BWAPI::Broodwar->neutral())
					{
						++geysers;
					}
				}
			}
		}
	}

	nRefineries = refineries;
	nFreeGeysers = geysers;
}

int InformationManager::getAir2GroundSupply(BWAPI::Player player) const
{
	int supply = 0;

	for (const auto & kv : getUnitData(player).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isFlyer() && UnitUtil::TypeCanAttackGround(ui.type))
		{
			supply += ui.type.supplyRequired();
		}
	}

	return supply;
}

void InformationManager::drawExtendedInterface()
{
    if (!Config::Debug::DrawUnitHealthBars)
    {
        return;
    }

    int verticalOffset = -10;

    // draw enemy units
    for (const auto & kv : getUnitData(_enemy).getUnits())
	{
        const UnitInfo & ui(kv.second);

		BWAPI::UnitType type(ui.type);
        int hitPoints = ui.lastHealth;
        int shields = ui.lastShields;

        const BWAPI::Position & pos = ui.lastPosition;

        int left    = pos.x - type.dimensionLeft();
        int right   = pos.x + type.dimensionRight();
        int top     = pos.y - type.dimensionUp();
        int bottom  = pos.y + type.dimensionDown();

        if (!BWAPI::Broodwar->isVisible(BWAPI::TilePosition(ui.lastPosition)))
        {
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, top), BWAPI::Position(right, bottom), BWAPI::Colors::Grey, false);
            BWAPI::Broodwar->drawTextMap(BWAPI::Position(left + 3, top + 4), "%s %c",
				ui.type.getName().c_str(),
				ui.goneFromLastPosition ? 'X' : ' ');
        }
        
        if (!type.isResourceContainer() && type.maxHitPoints() > 0)
        {
            double hpRatio = (double)hitPoints / (double)type.maxHitPoints();
        
            BWAPI::Color hpColor = BWAPI::Colors::Green;
            if (hpRatio < 0.66) hpColor = BWAPI::Colors::Orange;
            if (hpRatio < 0.33) hpColor = BWAPI::Colors::Red;

            int ratioRight = left + (int)((right-left) * hpRatio);
            int hpTop = top + verticalOffset;
            int hpBottom = top + 4 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), hpColor, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

        if (!type.isResourceContainer() && type.maxShields() > 0)
        {
            double shieldRatio = (double)shields / (double)type.maxShields();
        
            int ratioRight = left + (int)((right-left) * shieldRatio);
            int hpTop = top - 3 + verticalOffset;
            int hpBottom = top + 1 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Blue, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

    }

    // draw neutral units and our units
	//绘制中性单位和单位
    for (const auto & unit : BWAPI::Broodwar->getAllUnits())
    {
        if (unit->getPlayer() == _enemy)
        {
            continue;
        }

        const BWAPI::Position & pos = unit->getPosition();

        int left    = pos.x - unit->getType().dimensionLeft();
        int right   = pos.x + unit->getType().dimensionRight();
        int top     = pos.y - unit->getType().dimensionUp();
        int bottom  = pos.y + unit->getType().dimensionDown();

        //BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, top), BWAPI::Position(right, bottom), BWAPI::Colors::Grey, false);

        if (!unit->getType().isResourceContainer() && unit->getType().maxHitPoints() > 0)
        {
            double hpRatio = (double)unit->getHitPoints() / (double)unit->getType().maxHitPoints();
        
            BWAPI::Color hpColor = BWAPI::Colors::Green;
            if (hpRatio < 0.66) hpColor = BWAPI::Colors::Orange;
            if (hpRatio < 0.33) hpColor = BWAPI::Colors::Red;

            int ratioRight = left + (int)((right-left) * hpRatio);
            int hpTop = top + verticalOffset;
            int hpBottom = top + 4 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), hpColor, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

        if (!unit->getType().isResourceContainer() && unit->getType().maxShields() > 0)
        {
            double shieldRatio = (double)unit->getShields() / (double)unit->getType().maxShields();
        
            int ratioRight = left + (int)((right-left) * shieldRatio);
            int hpTop = top - 3 + verticalOffset;
            int hpBottom = top + 1 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Blue, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }

        if (unit->getType().isResourceContainer() && unit->getInitialResources() > 0)
        {
            
            double mineralRatio = (double)unit->getResources() / (double)unit->getInitialResources();
        
            int ratioRight = left + (int)((right-left) * mineralRatio);
            int hpTop = top + verticalOffset;
            int hpBottom = top + 4 + verticalOffset;

            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Grey, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(ratioRight, hpBottom), BWAPI::Colors::Cyan, true);
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, hpTop), BWAPI::Position(right, hpBottom), BWAPI::Colors::Black, false);

            int ticWidth = 3;

            for (int i(left); i < right-1; i+=ticWidth)
            {
                BWAPI::Broodwar->drawLineMap(BWAPI::Position(i, hpTop), BWAPI::Position(i, hpBottom), BWAPI::Colors::Black);
            }
        }
    }
}

void InformationManager::drawUnitInformation(int x, int y) 
{
	if (!Config::Debug::DrawEnemyUnitInfo)
    {
        return;
    }

	char color = white;

	BWAPI::Broodwar->drawTextScreen(x, y-10, "\x03 Self Loss:\x04 Minerals: \x1f%d \x04Gas: \x07%d", _unitData[_self].getMineralsLost(), _unitData[_self].getGasLost());
    BWAPI::Broodwar->drawTextScreen(x, y, "\x03 Enemy Loss:\x04 Minerals: \x1f%d \x04Gas: \x07%d", _unitData[_enemy].getMineralsLost(), _unitData[_enemy].getGasLost());
	BWAPI::Broodwar->drawTextScreen(x, y+10, "\x04 Enemy: %s", _enemy->getName().c_str());
	BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04 UNIT NAME");
	BWAPI::Broodwar->drawTextScreen(x+140, y+20, "\x04#");
	BWAPI::Broodwar->drawTextScreen(x+160, y+20, "\x04X");

	int yspace = 0;

	// for each unit in the queue
	for (BWAPI::UnitType t : BWAPI::UnitTypes::allUnitTypes()) 
	{
		int numUnits = _unitData[_enemy].getNumUnits(t);
		int numDeadUnits = _unitData[_enemy].getNumDeadUnits(t);

		if (numUnits > 0) 
		{
			if (t.isDetector())			{ color = purple; }		
			else if (t.canAttack())		{ color = red; }		
			else if (t.isBuilding())	{ color = yellow; }
			else						{ color = white; }

			BWAPI::Broodwar->drawTextScreen(x, y+40+((yspace)*10), " %c%s", color, t.getName().c_str());
			BWAPI::Broodwar->drawTextScreen(x+140, y+40+((yspace)*10), "%c%d", color, numUnits);
			BWAPI::Broodwar->drawTextScreen(x+160, y+40+((yspace++)*10), "%c%d", color, numDeadUnits);
		}
	}
}

void InformationManager::drawMapInformation()
{
    if (!Config::Debug::DrawBWTAInfo)
    {
        return;
    }

	// iterate through all the base locations, and draw their outlines.
	for (std::set<BWTA::BaseLocation*>::const_iterator i = BWTA::getBaseLocations().begin(); i != BWTA::getBaseLocations().end(); i++)
	{
		BWAPI::TilePosition p = (*i)->getTilePosition();
		BWAPI::Position c = (*i)->getPosition();

		//draw outline of center location
		BWAPI::Broodwar->drawBoxMap(p.x * 32, p.y * 32, p.x * 32 + 4 * 32, p.y * 32 + 3 * 32, BWAPI::Colors::Blue);

		//draw a circle at each mineral patch
		for (BWAPI::Unitset::iterator j = (*i)->getStaticMinerals().begin(); j != (*i)->getStaticMinerals().end(); j++)
		{
			BWAPI::Position q = (*j)->getInitialPosition();
			BWAPI::Broodwar->drawCircleMap(q.x, q.y, 30, BWAPI::Colors::Cyan);
		}

		//draw the outlines of vespene geysers
		for (BWAPI::Unitset::iterator j = (*i)->getGeysers().begin(); j != (*i)->getGeysers().end(); j++)
		{
			BWAPI::TilePosition q = (*j)->getTilePosition();
			BWAPI::Broodwar->drawBoxMap(q.x * 32, q.y * 32, q.x * 32 + 4 * 32, q.y * 32 + 2 * 32, BWAPI::Colors::Orange);
		}

		//if this is an island expansion, draw a yellow circle around the base location
		if ((*i)->isIsland())
			BWAPI::Broodwar->drawCircleMap(c, 80, BWAPI::Colors::Yellow);
	}

	//we will iterate through all the regions and draw the polygon outline of it in green.
	for (std::set<BWTA::Region*>::const_iterator r = BWTA::getRegions().begin(); r != BWTA::getRegions().end(); r++)
	{
		BWTA::Polygon p = (*r)->getPolygon();
		for (int j = 0; j<(int)p.size(); j++)
		{
			BWAPI::Position point1 = p[j];
			BWAPI::Position point2 = p[(j + 1) % p.size()];
			BWAPI::Broodwar->drawLineMap(point1, point2, BWAPI::Colors::Green);
		}
	}

	//we will visualize the chokepoints with red lines
	//我们会用红线来可视化瓶颈
	for (std::set<BWTA::Region*>::const_iterator r = BWTA::getRegions().begin(); r != BWTA::getRegions().end(); r++)
	{
		for (std::set<BWTA::Chokepoint*>::const_iterator c = (*r)->getChokepoints().begin(); c != (*r)->getChokepoints().end(); c++)
		{
			BWAPI::Position point1 = (*c)->getSides().first;
			BWAPI::Position point2 = (*c)->getSides().second;
			BWAPI::Broodwar->drawLineMap(point1, point2, BWAPI::Colors::Red);
		}
	}

	for (size_t i(0); i < _myRegionVertices.size(); ++i)
	{
		BWAPI::Broodwar->drawCircleMap(_myRegionVertices[i], 4, BWAPI::Colors::Green, false);
		BWAPI::Broodwar->drawTextMap(_myRegionVertices[i], "%d", i);
	}

}

void InformationManager::drawBaseInformation(int x, int y)
{
	if (!Config::Debug::DrawBaseInfo)
	{
		return;
	}

	int yy = y;

	BWAPI::Broodwar->drawTextScreen(x, yy, "%cBases", white);

	for (auto * base : BWTA::getBaseLocations())
	{
		yy += 10;

		char color = gray;

		char reservedChar = ' ';
		if (_theBases[base]->reserved)
		{
			reservedChar = '*';
		}

		char inferredChar = ' ';
		BWAPI::Player player = _theBases[base]->owner;
		if (player == _self)
		{
			color = green;
		}
		else if (player == _enemy)
		{
			color = orange;
			if (_theBases[base]->resourceDepot == nullptr)
			{
				inferredChar = '?';
			}
		}

		char baseCode = ' ';
		if (base == getMyMainBaseLocation())
		{
			baseCode = 'M';
		}
		else if (base == _myNaturalBaseLocation)
		{
			baseCode = 'N';
		}

		BWAPI::TilePosition pos = base->getTilePosition();
		BWAPI::Broodwar->drawTextScreen(x-8, yy, "%c%c", white, reservedChar);
		BWAPI::Broodwar->drawTextScreen(x, yy, "%c%d, %d%c%c", color, pos.x, pos.y, inferredChar, baseCode);
	}
}

void InformationManager::maybeAddStaticDefense(BWAPI::Unit unit)
{
	if (unit && unit->getPlayer() == _self && UnitUtil::IsStaticDefense(unit->getType()))
	{
		_staticDefense.insert(unit);
	}
}

void InformationManager::maybeAddComsatStation(BWAPI::Unit unit)
{
	if (unit && unit->getPlayer() == _self && unit->getType() == BWAPI::UnitTypes::Terran_Comsat_Station)
	{
		_comsatStation.insert(unit);
	}
}

void InformationManager::updateUnit(BWAPI::Unit unit)
{
	BWAPI::UnitType unitType = unit->getType();
	//_unitData[unit->getPlayer()].updateUnit(unit);

	if (unit->getPlayer() == _self || unit->getPlayer()->isEnemy(_self))//unit->getPlayer() == _enemy
    {
		_unitData[unit->getPlayer()].updateUnit(unit);
	}

	/*
	if (unitType == BWAPI::UnitTypes::Terran_Command_Center) {
		_commandCenter.insert(unit);
	}

	if (unitType == BWAPI::UnitTypes::Terran_Factory) {
		_factory.insert(unit);
	}

	if (unitType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
		_tanks.insert(unit);
	}
	*/

	/*
	if (unit->getPlayer() == _enemy) {
		//飞龙塔时间
		if (unitType == BWAPI::UnitTypes::Zerg_Spire && _zergSpireOverFrame == 0) {
			int remainingBuildTime = 0;
			int hitPoints = unit->getHitPoints();
			//if (!unit->isCompleted()) {
			remainingBuildTime = 1800 - 1800 * (hitPoints / 600);
			//}

			_zergSpireOverFrame = BWAPI::Broodwar->getFrameCount() + remainingBuildTime - 15 * 24;
		}

		if (unitType == BWAPI::UnitTypes::Zerg_Mutalisk && _zergSpireOverFrame == 0) {
			_zergSpireOverFrame = BWAPI::Broodwar->getFrameCount();
		}

		//是否有黄雾
		if (unitType == BWAPI::UnitTypes::Zerg_Defiler ||
			unitType == BWAPI::UnitTypes::Zerg_Defiler_Mound)
		{
			_enemyHasDarkSwarm = true;
		}

		//不能攻击的单位不参与
		if (!unit->getType().canAttack()) return;

		//是否在黄雾下
		if (_enemyHasDarkSwarm) {
			if (unit->isUnderDarkSwarm()) {
				setHazardousArea(unit, unitType.groundWeapon().maxRange() + 6 * 32);
			}
			else {
				removeHazardousArea(unit);
			}
		}

		if (unitType == BWAPI::UnitTypes::Zerg_Sunken_Colony) {
			setHazardousArea(unit, unitType.groundWeapon().maxRange() + 1 * 32);
		}

		if (unitType == BWAPI::UnitTypes::Zerg_Lurker) {
			if (unit->isBurrowed()) {
				setHazardousArea(unit, unitType.groundWeapon().maxRange() + 1 * 32);
			}
			else {
				removeHazardousArea(unit);
			}
		}
	}
	*/
}

void InformationManager::onUnitDestroy(BWAPI::Unit unit) 
{
	BWAPI::UnitType unitType = unit->getType();

	if (unit->getPlayer() == _self || unit->getPlayer()->isEnemy(_self))//unit->getPlayer() == _enemy
	{
		_unitData[unit->getPlayer()].removeUnit(unit);
		removeAttackTargets(unit);

		// If it may be a base, remove that base.
		if (unit->getType().isResourceDepot())
		{
			baseLost(unit->getTilePosition());
		}

		//如果建筑被消灭掉了，则释放当前位置
		if (unit->getType().isBuilding()) {
			BuildingPlacer::Instance().freeTiles(unit->getTilePosition(), unit->getType().tileWidth(), unit->getType().tileHeight());
		}

		// If it is our static defense, remove it.
		if (unit->getPlayer() == _self && UnitUtil::IsStaticDefense(unit->getType()))
		{
			_staticDefense.erase(unit);
		}

		if (unit->getPlayer() == _self && unit->getType() == BWAPI::UnitTypes::Terran_Comsat_Station) {
			_comsatStation.erase(unit);
		}

		if (unit->getType() == BWAPI::UnitTypes::Terran_Factory) {
			_factory.erase(unit);
		}

		if (unit->getType() == BWAPI::UnitTypes::Terran_Command_Center) {
			_commandCenter.erase(unit);
		}

		//如果是敌人的单位
		if (unit->getPlayer() == _enemy) {
			if (unitType == BWAPI::UnitTypes::Zerg_Sunken_Colony) {
				removeHazardousArea(unit);
			}

			if (unitType == BWAPI::UnitTypes::Zerg_Lurker) {
				removeHazardousArea(unit);
			}
		}
	}
}

// Only returns units believed to be completed.
//只返回认为已完成的单位。
void InformationManager::getNearbyForce(std::vector<UnitInfo> & unitInfo, BWAPI::Position p, BWAPI::Player player, int radius) 
{
	// for each unit we know about for that player
	for (const auto & kv : getUnitData(player).getUnits())
	{
		const UnitInfo & ui(kv.second);

		// if it's a combat unit we care about
		// and it's finished! 
		if (UnitUtil::IsCombatSimUnit(ui.type) && ui.completed && !ui.goneFromLastPosition)
		{
			if (ui.type == BWAPI::UnitTypes::Terran_Medic)
			{
				// Spellcasters that the combat simulator is able to simulate.
				if (ui.lastPosition.getDistance(p) <= (radius + 64))
				{
					unitInfo.push_back(ui);
				}
			}
			else
			{
				// Non-spellcasters, aka units with weapons that have a range.

				// Determine its attack range, in the worst case.
				int range = UnitUtil::GetMaxAttackRange(ui.type);

				// Include it if it can attack into the radius we care about (with fudge factor).
				if (range && ui.lastPosition.getDistance(p) <= (radius + range + 32))
				{
					unitInfo.push_back(ui);
				}
			}
		}
		// NOTE FAP does not support detectors.
		// else if (ui.type.isDetector() && ui.lastPosition.getDistance(p) <= (radius + 250))
        // {
		//	unitInfo.push_back(ui);
        // }
	}
}

int InformationManager::getNumUnits(BWAPI::UnitType t, BWAPI::Player player) const
{
	return getUnitData(player).getNumUnits(t);
}

const UnitData & InformationManager::getUnitData(BWAPI::Player player) const
{
    return _unitData.find(player)->second;
}

const UnitData & InformationManager::getUnitData(BWAPI::Player player, BWAPI::UnitType type) const
{
	UnitData units;
	UnitData unitData = getUnitData(player);

	for (const auto & kv : unitData.getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == type)
		{
			//units
		}
	}

	return _unitData.find(player)->second;
}

Base* InformationManager::baseAt(BWAPI::TilePosition baseTilePosition)
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (closeEnough(base->getTilePosition(), baseTilePosition))
		{
			return _theBases[base];
		}
	}

	return nullptr;
}

bool InformationManager::isBaseReserved(BWTA::BaseLocation * base)
{
	return _theBases[base]->reserved;
}

void InformationManager::reserveBase(BWTA::BaseLocation * base)
{
	_theBases[base]->reserved = true;
}

void InformationManager::unreserveBase(BWTA::BaseLocation * base)
{
	_theBases[base]->reserved = false;
}

void InformationManager::unreserveBase(BWAPI::TilePosition baseTilePosition)
{
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (closeEnough(base->getTilePosition(), baseTilePosition))
		{
			_theBases[base]->reserved = false;
			return;
		}
	}

	UAB_ASSERT(false,"trying to unreserve a non-base");
}

// We have complated combat units (excluding workers).
// This is a latch, initially false and set true forever when we get our first combat units.
//我们已对作战单位(不包括工人)进行了补充。
//这是一个门闩，一开始是假的，当我们得到我们的第一个战斗单位时，它将永远为真。
bool InformationManager::weHaveCombatUnits()
{
	// Latch: Once we have combat units, pretend we always have them.
	if (_weHaveCombatUnits)
	{
		return true;
	}

	for (const auto u : _self->getUnits())
	{
		if (!u->getType().isWorker() &&
			!u->getType().isBuilding() &&
			u->isCompleted() &&
			u->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			u->getType() != BWAPI::UnitTypes::Zerg_Overlord)
		{
			_weHaveCombatUnits = true;
			return true;
		}
	}

	return false;
}

// Enemy has complated combat units (excluding workers).
bool InformationManager::enemyHasCombatUnits()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasCombatUnits)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (!ui.type.isWorker() &&
			!ui.type.isBuilding() &&
			ui.completed &&
			ui.type != BWAPI::UnitTypes::Zerg_Larva &&
			ui.type != BWAPI::UnitTypes::Zerg_Overlord)
		{
			_enemyHasCombatUnits = true;
			return true;
		}
	}

	return false;
}

// Enemy has spore colonies, photon cannons, or turrets.
//敌人有孢子菌落、光子炮或炮塔。
bool InformationManager::enemyHasStaticAntiAir()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasStaticAntiAir)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Missile_Turret ||
			ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
			ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
		{
			_enemyHasStaticAntiAir = true;
			return true;
		}
	}

	return false;
}

//静态地面防御
bool InformationManager::enemyHasStaticAntiGround()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasStaticAntiGround)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Bunker ||
			ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
			ui.type == BWAPI::UnitTypes::Zerg_Sunken_Colony)
		{
			_enemyHasStaticAntiGround = true;
			return true;
		}
	}

	return false;
}

// Enemy has mobile units that can shoot up, or the tech to produce them.
//敌人有移动设备, 可以射击, 或技术生产。
bool InformationManager::enemyHasAntiAir()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasAntiAir)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (
			// For terran, anything other than SCV, command center, depot is a hit.
			// Surely nobody makes ebay before barracks!
			(_enemy->getRace() == BWAPI::Races::Terran &&
			ui.type != BWAPI::UnitTypes::Terran_SCV &&
			ui.type != BWAPI::UnitTypes::Terran_Command_Center &&
			ui.type != BWAPI::UnitTypes::Terran_Supply_Depot)

			||

			// Otherwise, any mobile unit that has an air weapon.
			(!ui.type.isBuilding() && UnitUtil::TypeCanAttackAir(ui.type))

			||

			// Or a building for making such a unit.
			ui.type == BWAPI::UnitTypes::Protoss_Cybernetics_Core ||
			ui.type == BWAPI::UnitTypes::Protoss_Stargate ||
			ui.type == BWAPI::UnitTypes::Protoss_Fleet_Beacon ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Zerg_Hydralisk_Den ||
			ui.type == BWAPI::UnitTypes::Zerg_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Greater_Spire

			)
		{
			_enemyHasAntiAir = true;
			return true;
		}
	}

	return false;
}

// Enemy has air units or air-producing tech.
// Overlords and lifted buildings are excluded.
// A queen's nest is not air tech--it's usually a prerequisite for hive
// rather than to make queens. So we have to see a queen for it to count.
// Protoss robo fac and terran starport are taken to imply air units.
//敌人有航空单位或空气生产技术。
//统治者和被取消的大厦被排除。
//皇后巢不是空气技术 - 它通常是蜂巢的先决条件
//而不是做皇后。所以我们必须要看到一个皇后来计算。
//神族机器人和人族 starport 被带到空中单位。
bool InformationManager::enemyHasAirTech()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasAirTech)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if ((ui.type.isFlyer() && ui.type != BWAPI::UnitTypes::Zerg_Overlord) ||
			ui.type == BWAPI::UnitTypes::Terran_Starport ||
			ui.type == BWAPI::UnitTypes::Terran_Control_Tower ||
			ui.type == BWAPI::UnitTypes::Terran_Science_Facility ||
			ui.type == BWAPI::UnitTypes::Terran_Covert_Ops ||
			ui.type == BWAPI::UnitTypes::Terran_Physics_Lab ||
			ui.type == BWAPI::UnitTypes::Protoss_Stargate ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Protoss_Fleet_Beacon ||
			ui.type == BWAPI::UnitTypes::Protoss_Robotics_Facility ||
			ui.type == BWAPI::UnitTypes::Protoss_Robotics_Support_Bay ||
			ui.type == BWAPI::UnitTypes::Protoss_Observatory ||
			ui.type == BWAPI::UnitTypes::Zerg_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Greater_Spire)
		{
			_enemyHasAirTech = true;
			return true;
		}
	}

	return false;
}

// This test is good for "can I benefit from detection?"
//此测试对 "我能从检测中获益" 有好处吗？
bool InformationManager::enemyHasCloakTech()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasCloakTech)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.hasPermanentCloak() ||                             // DT, observer
			ui.type.isCloakable() ||                                   // wraith, ghost
			ui.type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			ui.type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun ||    // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Templar_Archives ||   // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Observatory ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker_Egg ||
			ui.unit->isBurrowed())
		{
			_enemyHasCloakTech = true;
			return true;
		}
	}

	return false;
}

// This test is better for "do I need detection to live?"
// It doesn't worry about spider mines, observers, or burrowed units except lurkers.
//这个测试对  "我需要检测才能生存" 更好吗？
//它不担心蜘蛛矿, 观察员, 或掘进单位, 除了 lurkers。
bool InformationManager::enemyHasMobileCloakTech()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasMobileCloakTech)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type.isCloakable() ||                                   // wraith, ghost
			ui.type == BWAPI::UnitTypes::Protoss_Dark_Templar ||
			ui.type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun ||    // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Templar_Archives ||   // assume DT
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal ||
			ui.type == BWAPI::UnitTypes::Protoss_Arbiter ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker ||
			ui.type == BWAPI::UnitTypes::Zerg_Lurker_Egg)
		{
			_enemyHasMobileCloakTech = true;
			return true;
		}
	}

	return false;
}

// Enemy has air units good for hunting down overlords.
// A stargate counts, but not a fleet beacon or arbiter tribunal.
// A starport does not count; it may well be for something else.
//敌人的空军部队有很好的狩猎霸主。
//星际之门是重要的, 但不是舰队灯塔或仲裁庭。
//starport 不算数; 这很可能是为了别的东西。
bool InformationManager::enemyHasOverlordHunters()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasOverlordHunters)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Wraith ||
			ui.type == BWAPI::UnitTypes::Terran_Valkyrie ||
			ui.type == BWAPI::UnitTypes::Terran_Battlecruiser ||
			ui.type == BWAPI::UnitTypes::Protoss_Corsair ||
			ui.type == BWAPI::UnitTypes::Protoss_Scout ||
			ui.type == BWAPI::UnitTypes::Protoss_Carrier ||
			ui.type == BWAPI::UnitTypes::Protoss_Stargate ||
			ui.type == BWAPI::UnitTypes::Zerg_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Greater_Spire ||
			ui.type == BWAPI::UnitTypes::Zerg_Mutalisk ||
			ui.type == BWAPI::UnitTypes::Zerg_Scourge)
		{
			_enemyHasOverlordHunters = true;
			return true;
		}
	}

	return false;
}

// Enemy has spore colonies, photon cannons, turrets, or spider mines.
// It's the same as enemyHasStaticAntiAir() except for spider mines.
// Spider mines only catch cloaked ground units, so this routine is not for countering wraiths.
//敌人有孢子殖民地, 光子大炮, 炮塔, 或蜘蛛矿。
//除了蜘蛛矿之外, 它和 enemyHasStaticAntiAir 一样。
//蜘蛛矿只捕捉隐形地面单位, 所以这一例行不是为了对抗幽灵。
bool InformationManager::enemyHasStaticDetection()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasStaticDetection)
	{
		return true;
	}

	if (enemyHasStaticAntiAir())
	{
		_enemyHasStaticDetection = true;
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
		{
			_enemyHasStaticDetection = true;
			return true;
		}
	}

	return false;
}

// Enemy has overlords, observers, comsat, or science vessels.
//敌人有领主、观察员、通信卫星或科学船只。
bool InformationManager::enemyHasMobileDetection()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasMobileDetection)
	{
		return true;
	}

	// If the enemy is zerg, they have overlords.
	// If they went random, we may not have known until now.
	//如果敌人是虫族, 他们就有领主。
	//如果他们去随机, 我们可能不知道, 直到现在。
	if (_enemy->getRace() == BWAPI::Races::Zerg)
	{
		_enemyHasMobileDetection = true;
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Terran_Comsat_Station ||
			ui.type == BWAPI::UnitTypes::Terran_Science_Facility ||
			ui.type == BWAPI::UnitTypes::Terran_Science_Vessel ||
			ui.type == BWAPI::UnitTypes::Protoss_Observatory ||
			ui.type == BWAPI::UnitTypes::Protoss_Observer)
		{
			_enemyHasMobileDetection = true;
			return true;
		}
	}

	return false;
}

// Enemy has overlords, observers, comsat, or science vessels.
//敌人是否有黄雾。
bool InformationManager::enemyHasDarkSwarm()
{
	// Latch: Once they're known to have the tech, they always have it.
	if (_enemyHasDarkSwarm)
	{
		return true;
	}

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == BWAPI::UnitTypes::Zerg_Defiler ||
			ui.type == BWAPI::UnitTypes::Zerg_Defiler_Mound)
		{
			_enemyHasDarkSwarm = true;
			return true;
		}
	}

	return false;
}
// Our nearest shield battery, by air distance.
// Null if none.
//我们最近的盾牌电池, 由空中距离。
//如果没有, 则为 Null。
BWAPI::Unit InformationManager::nearestShieldBattery(BWAPI::Position pos) const
{
	if (_self->getRace() == BWAPI::Races::Protoss)
	{
		int closestDist = 999999;
		BWAPI::Unit closest = nullptr;
		for (BWAPI::Unit building : _staticDefense)
		{
			if (building->getType() == BWAPI::UnitTypes::Protoss_Shield_Battery)
			{
				int dist = building->getDistance(pos);
				if (dist < closestDist)
				{
					closestDist = dist;
					closest = building;
				}
			}
		}
		return closest;
	}
	return nullptr;
}

// Zerg specific calculation: How many scourge hits are needed
// to kill the enemy's known air fleet?
// This counts individual units--you get 2 scourge per egg.
// One scourge does 110 normal damage.
// NOTE This ignores air armor, which might make a difference in rare cases.
//虫族具体计算: 需要多少灾祸命中
//杀死敌人已知的空中舰队？
//这算个别单位--每只蛋有2鞭。
//一个祸害造成110正常伤害。
//注意, 这忽略了空气装甲, 这可能会改变罕见的情况下。
int InformationManager::nScourgeNeeded()
{
	int count = 0;

	for (const auto & kv : getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		// A few unit types should not usually be scourged. Skip them.
		if (ui.type.isFlyer() &&
			ui.type != BWAPI::UnitTypes::Zerg_Overlord &&
			ui.type != BWAPI::UnitTypes::Zerg_Scourge &&
			ui.type != BWAPI::UnitTypes::Protoss_Interceptor)
		{
			int hp = ui.type.maxHitPoints() + ui.type.maxShields();      // assume the worst
			count += (hp + 109) / 110;
		}
	}

	return count;
}

//获取集合点,isNatural 是否为分矿
BWAPI::Position InformationManager::getChokepoint(bool isNatural) {
	// We are guaranteed to always have a main base location, even if it has been destroyed.
	BWTA::BaseLocation * base = InformationManager::Instance().getMyMainBaseLocation();

	isNatural = false;
	// We may have taken our natural. If so, call that the front line.
	////我们可能是天生的。如果是这样，那就叫它第一线。
	BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
	if (isNatural) {
		//if (natural && BWAPI::Broodwar->self() == InformationManager::Instance().getBaseOwner(natural)){
		if (natural){
			base = natural;
		}
	}

	//修改：在最近的塞口防守
	BWTA::Chokepoint * c = BWTA::getNearestChokepoint(base->getPosition());

	BWAPI::Position chokepoint = c->getCenter();

	//chokepoint = MapTools::Instance().getDistancePosition(chokepoint, base->getPosition(), 2 * 32);
	if (chokepoint.isValid()) {
		return chokepoint;
	}

	return base->getPosition();
}

//获取集合点
BWAPI::Position InformationManager::getRallyPoint() {
	if (!_rallyPoint.isValid() || (_rallyPoint.x == 0 || _rallyPoint.y == 0)) {
		_rallyPoint = getChokepoint(false);
		_rallyPoint = MapTools::Instance().getDistancePosition(_rallyPoint, InformationManager::Instance().getMyMainBaseLocation()->getPosition(), 2 * 32);
	}

	return _rallyPoint;
}

//获取侦察点,isStartLocation是否只获取开始点位
BWAPI::Position InformationManager::getReconLocation(bool isStartLocation) {
	BWAPI::Position pos;
	int frame = BWAPI::Broodwar->getFrameCount();
	std::vector<BWAPI::Position> locations;

	int minTime = frame;
	for (auto local : _reconLocations) {
		if (local.second < minTime) {
			if (isStartLocation) {
				for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations()) {
					if (local.first == startLocation->getPosition()) {
						pos = local.first;
						minTime = local.second;
						break;
					}
				}
			}
			else {
				pos = local.first;
				minTime = local.second;
			}
		}
	}
	/*
	for (auto local : _reconLocations) {
		if (frame < 6 * 60 * 24) {
			if (InformationManager::Instance().getEnemyMainBaseLocation()) {
				pos == InformationManager::Instance().getEnemyMainBaseLocation()->getPosition();
				if (_reconLocations[pos] < minTime) break;

				minTime = _reconLocations[pos];

				std::vector<BWAPI::TilePosition> enemyNextExpansionPosition = MapPosition::Instance().getEnemyNextExpansionPosition();
				for (BWAPI::TilePosition position : enemyNextExpansionPosition) {
					pos = BWAPI::Position(position);
					if (local.first == pos && local.second < minTime) {
						//locations.push_back(local.first);
						pos = local.first;
						minTime = local.second;
					}
				}
			}
		}
		else if (isStartLocation) {
			for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations()) {
				if (local.first == startLocation->getPosition() && local.second < minTime) {
					//locations.push_back(local.first);
					pos = local.first;
					minTime = local.second;
				}
			}
		}
		else {
			if (local.second < minTime) {
				//locations.push_back(local.first);
				pos = local.first;
				minTime = local.second;
			}
		}
	}

	if (InformationManager::Instance().getEnemyMainBaseLocation()) {
		if (pos == InformationManager::Instance().getEnemyMainBaseLocation()->getPosition()) {
			setReconLocation(pos, 60 * 24);
			pos = InformationManager::Instance().getEnemyNaturalLocation()->getPosition();
			setReconLocation(pos, 10 * 24);
		}
		else if (pos == InformationManager::Instance().getEnemyNaturalLocation()->getPosition()) {
			setReconLocation(pos, 60 * 24);
			pos = InformationManager::Instance().getEnemyMainBaseLocation()->getPosition();
			setReconLocation(pos, 10 * 24);
		}
	}
	*/
	/*
	for (auto local : _reconLocations) {
		if (local.second < frame) {
			if (isStartLocation) {
				for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations()) {
					if (local.first == startLocation->getPosition()) {
						locations.push_back(local.first);
					}
				}
			}
			else {
				locations.push_back(local.first);
			}
		}
	}
	*/
	int time = frame / (60 * 24);
	if (pos.isValid() && (pos.x > 0 && pos.y > 0)) {
		//if (locations.size() > 0) {
		//pos = locations.at(Random::Instance().index(locations.size()));

		/*
		if (InformationManager::Instance().getEnemyMainBaseLocation()) {
		if (pos == InformationManager::Instance().getEnemyMainBaseLocation()->getPosition()) {
		setReconLocation(pos, 1 * 60 * 24);
		}
		else if (pos == InformationManager::Instance().getEnemyNaturalLocation()->getPosition()) {
		setReconLocation(pos, 1 * 60 * 24);
		}
		}
		*/
	}
	else {
		if (getEnemyMainBaseLocation()) {
			pos = getEnemyMainBaseLocation()->getPosition();
		}
	}

	//setReconLocation(pos, 2 * 60 * 24);
	
	return pos;
}

//设置侦察点
void InformationManager::setReconLocation(BWAPI::Position position) {
	_reconLocations[position] = BWAPI::Broodwar->getFrameCount();
}

//设置侦察点
void InformationManager::setReconLocation(int weight) {
	for (auto & local : _reconLocations) {
		local.second = BWAPI::Broodwar->getFrameCount() + weight;
	}
}

//设置侦察点
void InformationManager::setReconLocation(BWAPI::Position position, int weight) {
	_reconLocations[position] = BWAPI::Broodwar->getFrameCount() + weight;
}

//设置防御点
void InformationManager::setDefensivePosition(BWAPI::Position position) {
	int frame = BWAPI::Broodwar->getFrameCount();
	if (frame > _lastDefensiveChange) {
		_defensivePosition = position;
		_lastDefensiveChange = frame + 2 * 24;

		if (!_mainAttackTarget.isValid() || (_mainAttackTarget.x == 0 || _mainAttackTarget.y == 0)) {
			_mainAttackTarget = _defensivePosition;
		}
	}
}

void InformationManager::setHazardousArea(BWAPI::Unit unit, int radius){
	if (!unit->exists() && _hazardousArea[unit]) {
		_hazardousArea.erase(unit);
		return;
	}

	if (radius == 0) {
		if (_hazardousArea[unit]) {
			_hazardousArea.erase(unit);
		}
	}
	else {
		_hazardousArea[unit] = radius;
	}
}

void InformationManager::removeHazardousArea(BWAPI::Unit unit) {
	if (_hazardousArea[unit]) {
		_hazardousArea.erase(unit);
	}
}


int InformationManager::getWeaponDamage(BWAPI::Player player, BWAPI::WeaponType wpn)
{
	if (player == BWAPI::Broodwar->self()) return player->damage(wpn);

	int last = enemyWeaponDamage[wpn];
	int current = player->damage(wpn);
	if (current > last)
	{
		enemyWeaponDamage[wpn] = current;
		return current;
	}

	return last;
}

int InformationManager::getWeaponRange(BWAPI::Player player, BWAPI::WeaponType wpn)
{
	if (player == BWAPI::Broodwar->self()) return player->weaponMaxRange(wpn);

	int last = enemyWeaponRange[wpn];
	int current = player->weaponMaxRange(wpn);
	if (current > last)
	{
		enemyWeaponRange[wpn] = current;
		return current;
	}

	return last;
}

int InformationManager::getUnitCooldown(BWAPI::Player player, BWAPI::UnitType type)
{
	if (player == BWAPI::Broodwar->self()) return player->weaponDamageCooldown(type);

	int last = enemyUnitCooldown[type];
	int current = player->weaponDamageCooldown(type);
	if (current > last)
	{
		enemyUnitCooldown[type] = current;
		return current;
	}

	return last;
}

double InformationManager::getUnitTopSpeed(BWAPI::Player player, BWAPI::UnitType type)
{
	if (player == BWAPI::Broodwar->self()) return player->topSpeed(type);

	double last = enemyUnitTopSpeed[type];
	double current = player->topSpeed(type);
	if (current > last)
	{
		enemyUnitTopSpeed[type] = current;
		return current;
	}

	return last;
}

int InformationManager::getUnitArmor(BWAPI::Player player, BWAPI::UnitType type)
{
	if (player == BWAPI::Broodwar->self()) return player->armor(type);

	int last = enemyUnitArmor[type];
	int current = player->armor(type);
	if (current > last)
	{
		enemyUnitArmor[type] = current;
		return current;
	}

	return last;
}

InformationManager & InformationManager::Instance()
{
	static InformationManager instance;
	return instance;
}
