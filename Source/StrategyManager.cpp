#include "StrategyManager.h"
#include "CombatCommander.h"
#include "OpponentModel.h"
#include "ProductionManager.h"
#include "StrategyBossZerg.h"
#include "StrategyBossTerran.h"
#include "StrategyBossProtoss.h"
#include "UnitUtil.h"
#include "Random.h"

using namespace UAlbertaBot;

StrategyManager::StrategyManager() 
	: _selfRace(BWAPI::Broodwar->self()->getRace())
	, _enemyRace(BWAPI::Broodwar->enemy()->getRace())
    , _emptyBuildOrder(BWAPI::Broodwar->self()->getRace())
	, _openingGroup("")
	, _hasDropTech(false)
	, _highWaterBases(1)
{
}

StrategyManager & StrategyManager::Instance() 
{
	static StrategyManager instance;
	return instance;
}

const BuildOrder & StrategyManager::getOpeningBookBuildOrder() const
{
    auto buildOrderIt = _strategies.find(Config::Strategy::StrategyName);

    // look for the build order in the build order map
	if (buildOrderIt != std::end(_strategies))
    {
        return (*buildOrderIt).second._buildOrder;
    }
    else
    {
        UAB_ASSERT_WARNING(false, "Strategy not found: %s, returning empty initial build order", Config::Strategy::StrategyName.c_str());
        return _emptyBuildOrder;
    }
}

// This is used for terran and protoss.
//这是用于人族和神族。
//是否扩张
const bool StrategyManager::shouldExpandNow() const
{
	// if there is no place to expand to, we can't expand
	// We check mineral expansions only.
	if (MapTools::Instance().getNextExpansion(false, true, false) == BWAPI::TilePositions::None)
	{
		return false;
	}

	// if we have idle workers then we need a new expansion
	if (WorkerManager::Instance().getNumIdleWorkers() > 3)
	{
		return true;
	}

    // if we have excess minerals, expand
	if (BWAPI::Broodwar->self()->minerals() > 600)
    {
        return true;
    }

	size_t numDepots = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Command_Center)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive);
	int frame = BWAPI::Broodwar->getFrameCount();
	int minute = frame / (24 * 60);

	// we will make expansion N after array[N] minutes have passed
	std::vector<int> expansionTimes = {5, 9, 13, 17, 21, 25};

    for (size_t i(0); i < expansionTimes.size(); ++i)
    {
        if (numDepots < (i+2) && minute > expansionTimes[i])
        {
            return true;
        }
    }

	return false;
}

void StrategyManager::addStrategy(const std::string & name, Strategy & strategy)
{
    _strategies[name] = strategy;
}

void StrategyManager::setAggressiveScore()
{
	auto aggressiveScoreItr = _strategies.find(Config::Strategy::StrategyName);

	if (aggressiveScoreItr != std::end(_strategies))
	{
		_aggressiveScore = (*aggressiveScoreItr).second._aggressiveScore;
		CombatCommander::Instance().setAggressiveScore(_aggressiveScore);
	}
}

// Set _openingGroup depending on the current strategy, which in principle
// might be from the config file or from opening learning.
// This is part of initialization; it happens early on.
//根据当前策略设置 _openingGroup, 原则上
//可能来自配置文件或打开学习。
//这是初始化的一部分; 很早就发生了
void StrategyManager::setOpeningGroup()
{
	auto openingGroupItr = _strategies.find(Config::Strategy::StrategyName);

	if (openingGroupItr != std::end(_strategies))
	{
		_openingGroup = (*openingGroupItr).second._openingGroup;
	}
}

const std::string & StrategyManager::getOpeningGroup() const
{
	return _openingGroup;
}

const MetaPairVector StrategyManager::getBuildOrderGoal()
{
    if (_selfRace == BWAPI::Races::Protoss)
    {
        return getProtossBuildOrderGoal();
    }
	else if (_selfRace == BWAPI::Races::Terran)
	{
		return getTerranBuildOrderGoal();
	}
	else if (_selfRace == BWAPI::Races::Zerg)
	{
		return getZergBuildOrderGoal();
	}

    return MetaPairVector();
}

const MetaPairVector StrategyManager::getProtossBuildOrderGoal()
{
	// the goal to return
	MetaPairVector goal;

	// These counts include uncompleted units.
	int numPylons = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Pylon);
	int numNexusCompleted = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numNexusAll = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numProbes = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Probe);
	int numCannon = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon);
	int numObservers = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Observer);
	int numZealots = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Zealot);
	int numDragoons = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);
	int numDarkTemplar = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar);
	int numReavers = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Reaver);
	int numCorsairs = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Corsair);
	int numCarriers = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Carrier);

	bool hasStargate = UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Stargate) > 0;

	int maxProbes = WorkerManager::Instance().getMaxWorkers();

	BWAPI::Player self = BWAPI::Broodwar->self();

	if (_openingGroup == "zealots")
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot, numZealots + 6));

		if (numNexusAll >= 3)
		{
			// In the end, switch to carriers; no more dragoons.
			goal.push_back(MetaPair(BWAPI::UpgradeTypes::Carrier_Capacity, 1));
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Carrier, numCarriers + 1));
		}
		else if (numNexusAll >= 2)
		{
			// Once we have a 2nd nexus, add dragoons.
			goal.push_back(MetaPair(BWAPI::UpgradeTypes::Singularity_Charge, 1));
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + 4));
		}

		// Once dragoons are out, get zealot speed.
		if (numDragoons > 0)
		{
			goal.push_back(MetaPair(BWAPI::UpgradeTypes::Leg_Enhancements, 1));
		}

		// Finally add templar archives.
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Templar_Archives, 1));
		}

		// If we have templar archives, make
		// 1. a small fixed number of dark templar to force a reaction, and
		// 2. an even number of high templar to merge into archons (so the high templar disappear quickly).
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) != 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dark_Templar, std::max(3, numDarkTemplar)));
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_High_Templar, 2));
		}
	}
	else if (_openingGroup == "dragoons")
	{
		goal.push_back(MetaPair(BWAPI::UpgradeTypes::Singularity_Charge, 1));
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + 6));

		// Once we have a 2nd nexus, add reavers.
		if (numNexusAll >= 2)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Reaver, numReavers + 1));
		}

		// If we have templar archives, make a small fixed number of DTs to force a reaction.
		if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Templar_Archives) != 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dark_Templar, std::max(3, numDarkTemplar)));
		}

	}
	else if (_openingGroup == "dark templar")
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dark_Templar, numDarkTemplar + 2));

		// Once we have a 2nd nexus, add dragoons.
		if (numNexusAll >= 2)
		{
			goal.push_back(MetaPair(BWAPI::UpgradeTypes::Singularity_Charge, 1));
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + 4));
		}
	}
	else if (_openingGroup == "drop")
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dark_Templar, numDarkTemplar + 2));

		// The drop prep is carried out entirely by the opening book.
		// Immediately transition into something else.
		_openingGroup = "dragoons";
	}
	else
	{
		UAB_ASSERT_WARNING(false, "Unknown Opening Group: %s", _openingGroup.c_str());
		_openingGroup = "dragoons";    // we're misconfigured, but try to do something
	}

	// If we're doing a corsair thing and it's still working, slowly add more.
	if (_enemyRace == BWAPI::Races::Zerg &&
		hasStargate &&
		numCorsairs < 6 &&
		self->deadUnitCount(BWAPI::UnitTypes::Protoss_Corsair) == 0)
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Corsair, numCorsairs + 1));
	}

	// Maybe get some static defense against air attack.
	const int enemyAirToGround =
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Wraith, BWAPI::Broodwar->enemy()) / 8 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Battlecruiser, BWAPI::Broodwar->enemy()) / 3 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Scout, BWAPI::Broodwar->enemy()) / 5 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Mutalisk, BWAPI::Broodwar->enemy()) / 6;
	if (enemyAirToGround > 0)
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Protoss_Photon_Cannon, enemyAirToGround));
	}

	// Get observers if we have a second base, or if the enemy has cloaked units.
	//如果我们有第二垒, 或者敌人有隐形的单位, 就能得到观察者。
	if (numNexusCompleted >= 2 || InformationManager::Instance().enemyHasCloakTech())
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1));

		if (numObservers < 3 && self->completedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
		{
			goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, numObservers + 1));
		}
	}

	// Make more probes, up to a limit.
	goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Probe, std::min(maxProbes, numProbes + 8)));

	// If the map has islands, get drop after we have 3 bases.
	if (Config::Macro::ExpandToIslands && numNexusCompleted >= 3 && MapTools::Instance().hasIslandBases())
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Shuttle, 1));
	}

	// if we want to expand, insert a nexus into the build order
	if (shouldExpandNow())
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Nexus, numNexusAll + 1));
	}

	return goal;
}

const MetaPairVector StrategyManager::getTerranBuildOrderGoal()
{
	// the goal to return
	std::vector<MetaPair> goal;

	// These counts include uncompleted units.
	int numSCVs			= UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_SCV);
    int numCC           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Command_Center);            
    int numRefineries   = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Refinery);            
    int numMarines      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Marine);
	int numMedics       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Medic);
	int numWraith       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Wraith);
    int numVultures     = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Vulture);
	int numVessels		= UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Science_Vessel);
	int numGoliaths		= UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Goliath);
    int numTanks        = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
                        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode);

	int numFactory		= UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Factory);
	int numMachineShop	= UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Machine_Shop);
	int numBarracks		= UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Barracks);

	bool hasEBay		= UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) > 0;
	bool hasAcademy		= UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Academy) > 0;
	bool hasArmory		= UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Armory) > 0;

	int maxSCVs = WorkerManager::Instance().getMaxWorkers();

	bool makeVessel = false;

	BWAPI::Player self = BWAPI::Broodwar->self();

	if (_openingGroup == "tvz200") {
		if (numBarracks < 3) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Barracks, numBarracks + 1));
		}

		if (numFactory < 2) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Factory, numFactory + 1));
		}

		if (numMachineShop < 2) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Machine_Shop, numMachineShop + 1));
		}

		if (numTanks < 10) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, numTanks + 2));
		}

		if (numVultures < 20) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Vulture, numVultures + 3));
		}

		if (numVessels < 10) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Science_Vessel, numGoliaths + 2));
		}

		if (numMedics < numMarines / 5) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Science_Vessel, numGoliaths + 3));
		}

		if (numMarines < 60) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Science_Vessel, numGoliaths + 10));
		}
	}
	else if (_openingGroup == "tvp200") {
		if (numFactory < 10) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Factory, numFactory + 2));
		}

		if (numMachineShop < (numFactory * 0.6)) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Machine_Shop, numMachineShop + 1));
		}

		if (numTanks < 60) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, numTanks + 6));
		}

		if (numVultures < 100) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Vulture, numVultures + 10));
		}

		if (numGoliaths < 10) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Goliath, numGoliaths + 4));
		}

		if (numVessels < 8) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Science_Vessel, numGoliaths + 1));
		}
	}
	else if (_openingGroup == "anti-rush")
	{
		int numRax = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Barracks);

		CombatCommander::Instance().setAggression(false);
		
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Marine, numMarines + numRax));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_SCV, std::min(maxSCVs, numSCVs + 1)));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Bunker, 1));
		
		if (self->minerals() > 250)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Barracks, numRax + 1));
		}

		// If we survived long enough, transition to something more interesting.
		//如果我们活得足够长, 过渡到更有趣的东西。
		if (numMarines >= 10)
		{
			_openingGroup = "bio";
			CombatCommander::Instance().setAggression(true);
		}
	}
	else if (_openingGroup == "bio")
    {
	    goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Marine, numMarines + 8));

		if (numMarines >= 10)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Academy, 1));
			if (numRefineries == 0)
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Refinery, 1));
			}
		}
		if (hasAcademy)
		{
			// 1 medic for each 5 marines.
			int medicGoal = std::max(numMedics, numMarines / 5);
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Medic, medicGoal));
			if (!self->hasResearched(BWAPI::TechTypes::Stim_Packs))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::TechTypes::Stim_Packs, 1));
			}
			else
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::U_238_Shells, 1));
			}
		}
        if (numMarines > 16)
        {
            goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Engineering_Bay, 1));
        }
		if (hasEBay)
		{
			int weaponsUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Infantry_Weapons);
			if (weaponsUps == 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Terran_Infantry_Weapons))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Infantry_Weapons, 1));
			}
			else if (weaponsUps > 0 &&
				self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Infantry_Armor) == 0 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Terran_Infantry_Armor))
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Infantry_Armor, 1));
			}
			else if (weaponsUps > 0 &&
				weaponsUps < 3 &&
				!self->isUpgrading(BWAPI::UpgradeTypes::Terran_Infantry_Weapons) &&
				numVessels > 0)
			{
				goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Infantry_Weapons, weaponsUps + 1));
			}
		}

		// Add in tanks if they're useful.
		//添加在坦克, 如果它们是有用的。
		const int enemiesCounteredByTanks =
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, BWAPI::Broodwar->enemy()) +
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode, BWAPI::Broodwar->enemy()) +
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Broodwar->enemy()) +
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Reaver, BWAPI::Broodwar->enemy()) +
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Lurker, BWAPI::Broodwar->enemy()) +
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Ultralisk, BWAPI::Broodwar->enemy());
		const bool enemyHasStaticDefense =
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Bunker, BWAPI::Broodwar->enemy()) > 0 ||
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Broodwar->enemy()) > 0 ||
			InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::Broodwar->enemy()) > 0;
		if (enemiesCounteredByTanks > 0 || enemyHasStaticDefense)
		{
			int nTanksWanted;
			if (enemiesCounteredByTanks > 0)
			{
				nTanksWanted = std::min(numMarines / 4, enemiesCounteredByTanks);
				nTanksWanted = std::min(nTanksWanted, numTanks + 2);
			}
			else
			{
				nTanksWanted = numTanks;
				if (numTanks < 2)
				{
					nTanksWanted = numTanks + 1;
				}
			}
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, nTanksWanted));
			goal.push_back(std::pair<MacroAct, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
		}
	}
	else if (_openingGroup == "vultures")
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Vulture, numVultures + 3));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Ion_Thrusters, 1));

		if (numVultures >= 6)
		{
			// The rush is over, transition out on the next call.
			_openingGroup = "tanks";
		}
	}
	else if (_openingGroup == "tanks")
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Vulture, numVultures + 4));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, numTanks + 2));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));

		if (numTanks >= 6)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Goliath, numGoliaths + 4));
		}
		if (numTanks >= 4)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Ion_Thrusters, 1));
		}
		if (numGoliaths >= 4)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Charon_Boosters, 1));
		}
		if (self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
		{
			// Maintain 1 vessel to spot for the tanks.
			//保持1艘船为坦克现货。
			makeVessel = true;
		}
	}
	else if (_openingGroup == "drop")
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Ion_Thrusters, 1));
		goal.push_back(MetaPair(BWAPI::UnitTypes::Terran_Vulture, numVultures + 1));

		// The drop prep is carried out entirely by the opening book.
		// Immediately transition into something else.
		//辍学准备完全由开幕书完成。
		//立即过渡到其他的东西。
		if (_enemyRace == BWAPI::Races::Zerg)
		{
			_openingGroup = "tvz200";
		}
		else
		{
			_openingGroup = "tanks";
		}
	}
	else
	{
		BWAPI::Broodwar->printf("Unknown Opening Group: %s", _openingGroup.c_str());
		_openingGroup = "bio";       // we're misconfigured, but try to do something
	}

	if (numCC > 1 || InformationManager::Instance().enemyHasCloakTech())
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Academy, 1));
		if (numRefineries == 0)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Refinery, 1));
		}
	}

	const int enemyAirToGround =
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Wraith, BWAPI::Broodwar->enemy()) / 6 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Terran_Battlecruiser, BWAPI::Broodwar->enemy()) / 2 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Protoss_Scout, BWAPI::Broodwar->enemy()) / 3 +
		InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Mutalisk, BWAPI::Broodwar->enemy()) / 4;
	if (enemyAirToGround > 0)
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Missile_Turret, enemyAirToGround));
	}

	if (numCC > 0 && hasAcademy)
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Comsat_Station, numCC));
	}

	if (makeVessel || InformationManager::Instance().enemyHasCloakTech())
	{
		if (numVessels < 2)
		{
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Science_Vessel, numVessels + 1));
		}
	}

	//VA在，则升级坦克的攻和防
	if (hasArmory){
		int weaponsUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons);
		if (weaponsUps < 3 && !self->isUpgrading(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons)) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons, weaponsUps + 1));
		}

		weaponsUps = self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Vehicle_Plating);
		if (weaponsUps < 3 && !self->isUpgrading(BWAPI::UpgradeTypes::Terran_Vehicle_Plating)) {
			goal.push_back(std::pair<MacroAct, int>(BWAPI::UpgradeTypes::Terran_Vehicle_Plating, weaponsUps + 1));
		}
	}

	// Make more SCVs, up to a limit. The anti-rush strategy makes its own SCVs.
	if (_openingGroup != "anti-rush")
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Terran_SCV, std::min(maxSCVs, numSCVs + 2 * numCC)));
	}

	// If the map has islands, get drop after we have 3 bases.
	//如果地图上有岛屿, 在我们有3个基地后, 就会下降。
	if (Config::Macro::ExpandToIslands && numCC >= 3 && MapTools::Instance().hasIslandBases())
	{
		goal.push_back(MetaPair(BWAPI::UnitTypes::Terran_Dropship, 1));
	}

	if (shouldExpandNow())
    {
        goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Terran_Command_Center, numCC + 1));
    }

	return goal;
}

// BOSS method of choosing a zerg production plan. UNUSED!
// See freshProductionPlan() for the current method.
const MetaPairVector StrategyManager::getZergBuildOrderGoal() const
{
	// the goal to return
	std::vector<MetaPair> goal;
	
	// These counts include uncompleted units.
	int nLairs			= UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair);
	int nHives			= UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive);
    int nHatches        = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)
                        + nLairs + nHives;
    int nDrones			= UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Drone);
	int nHydras			= UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk);

	const int droneMax = 48;             // number of drones not to exceed

	// Simple default strategy as an example in case you want to use this method.
	//简单的默认策略作为示例, 以防您要使用此方法。
	goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Zerg_Hydralisk, nHydras + 12));
	if (shouldExpandNow())
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Zerg_Hatchery, nHatches + 1));
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Zerg_Drone, std::min(droneMax, nDrones + 10)));
	}
	else
	{
		goal.push_back(std::pair<MacroAct, int>(BWAPI::UnitTypes::Zerg_Drone, std::min(droneMax, nDrones + 2)));
	}

	return goal;
}

void StrategyManager::setEnemy(BWAPI::Player player)
{
	if (_selfRace == BWAPI::Races::Zerg)
	{
		StrategyBossZerg::Instance().setEnemy(player);
	}
	else if (_selfRace == BWAPI::Races::Terran)
	{
		StrategyBossTerran::Instance().setEnemy(player);
	}
	else if (_selfRace == BWAPI::Races::Protoss)
	{
		StrategyBossProtoss::Instance().setEnemy(player);
	}
}

//处理紧急生产问题
void StrategyManager::handleUrgentProductionIssues(BuildOrderQueue & queue)
{
	if (_addMacroAct.lastFrame == 0 && _addMacroAct.type() != MacroActs::Default) {
		queue.queueAsHighestPriority(_addMacroAct);
		_addMacroAct.lastFrame = BWAPI::Broodwar->getFrameCount();
	}

	if (_selfRace == BWAPI::Races::Zerg)
	{
		StrategyBossZerg::Instance().handleUrgentProductionIssues(queue);
	}
	else if (_selfRace == BWAPI::Races::Terran) 
	{
		StrategyBossTerran::Instance().handleUrgentProductionIssues(queue);
	}
	else if (_selfRace == BWAPI::Races::Protoss) 
	{
		StrategyBossProtoss::Instance().handleUrgentProductionIssues(queue);
	}
	else
	{
		if (_addMacroAct.lastFrame == 0 && _addMacroAct.type() != MacroActs::Default) {
			queue.queueAsHighestPriority(_addMacroAct);
			_addMacroAct.lastFrame = BWAPI::Broodwar->getFrameCount();
		}

		// detect if there's a supply block once per second
		//检测每秒是否有一个供应块
		if ((BWAPI::Broodwar->getFrameCount() % 24 == 1) && detectSupplyBlock(queue))
		{
			if (Config::Debug::DrawBuildOrderSearchInfo)
			{
				BWAPI::Broodwar->printf("Supply block, building supply!");
			}

			queue.queueAsHighestPriority(MacroAct(BWAPI::Broodwar->self()->getRace().getSupplyProvider()));
		}

		const MacroAct * nextInQueuePtr = queue.isEmpty() ? nullptr : &(queue.getHighestPriorityItem().macroAct);

		// If we need gas, make sure it is turned on.
		int gas = BWAPI::Broodwar->self()->gas();
		BWAPI::Player self = BWAPI::Broodwar->self();

		if (nextInQueuePtr)
		{
			if (nextInQueuePtr->gasPrice() > gas)
			{
				WorkerManager::Instance().setCollectGas(true);
			}

			if (nextInQueuePtr->getName() == "None") queue.doneWithHighestPriorityItem();

			BWAPI::UnitType requiredType;

			//修改：如果必须单位还没有建造，则先删除
			if (nextInQueuePtr->isUnit()) {
				typedef std::pair<BWAPI::UnitType, int> ReqPair;
				for (const ReqPair & pair : nextInQueuePtr->getUnitType().requiredUnits())
				{
					requiredType = pair.first;
					if (self->allUnitCount(requiredType) == 0 && !BuildingManager::Instance().isBeingBuilt(requiredType)) {
						queue.doneWithHighestPriorityItem();
						//queue.queueAsHighestPriority(MacroAct(requiredType));
						break;
					}
					/*
					if (BWAPI::Broodwar->self()->allUnitCount(requiredType) == 0 && !BuildingManager::Instance().isBeingBuilt(requiredType))
					{
						queue.queueAsHighestPriority(MacroAct(requiredType));
					}
					*/
				}
			}

			if (nextInQueuePtr->isUpgrade()) {
				BWAPI::UpgradeType upgType = nextInQueuePtr->getUpgradeType();

				int maxLvl = self->getMaxUpgradeLevel(upgType);
				int currentLvl = self->getUpgradeLevel(upgType);
				currentLvl += 1;

				if (self->isUpgrading(upgType) || currentLvl > maxLvl) {
					queue.doneWithHighestPriorityItem();
				}

				//有BUG，Terran Infantry Weapons获取不到对应的单位
				requiredType = upgType.whatsRequired(currentLvl);
				if (self->allUnitCount(requiredType) == 0 && !BuildingManager::Instance().isBeingBuilt(requiredType)) {
					//queue.doneWithHighestPriorityItem();
				}
			}

			if (nextInQueuePtr->isTech()) {
				if (self->hasResearched(nextInQueuePtr->getTechType())) {
					queue.doneWithHighestPriorityItem();
				}
				else {
					requiredType = nextInQueuePtr->getTechType().whatResearches();
					if (self->allUnitCount(requiredType) == 0 && !BuildingManager::Instance().isBeingBuilt(requiredType)) {
						queue.doneWithHighestPriorityItem();
					}
				}
			}
		}

		// If we have collected too much gas, turn it off.
		//如果我们收集了太多的气体, 关掉它。
		if ((ProductionManager::Instance().isOutOfBook() && gas > 400) ||
			gas > 4 * BWAPI::Broodwar->self()->minerals())
		{
			int queueMinerals, queueGas;
			queue.totalCosts(queueMinerals, queueGas);
			if (gas >= queueGas)
			{
				WorkerManager::Instance().setCollectGas(false);
			}
		}

		if (InformationManager::Instance().enemyHasAirTech()) {
			if (_selfRace == BWAPI::Races::Terran ) {
				if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) == 0 &&
					!BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Terran_Engineering_Bay))
				{
					queue.queueAsHighestPriority(BWAPI::UnitTypes::Terran_Engineering_Bay);
				}
				else {
					if (self->completedUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) > 0 && _lastMissileTurretChange < BWAPI::Broodwar->getFrameCount()) {

						std::string itemName = "Missile_Turret";

						std::vector<BWAPI::TilePosition> missileTurretTilePosition;
						missileTurretTilePosition.push_back(BWAPI::TilePosition(14, 43));
						missileTurretTilePosition.push_back(BWAPI::TilePosition(8, 4));
						missileTurretTilePosition.push_back(BWAPI::TilePosition(21, 23));

						missileTurretTilePosition.push_back(BWAPI::TilePosition(17, 43));
						missileTurretTilePosition.push_back(BWAPI::TilePosition(12, 37));
						missileTurretTilePosition.push_back(BWAPI::TilePosition(21, 26));

						/*
						missileTurretTilePosition.push_back(BWAPI::TilePosition(12, 12));
						missileTurretTilePosition.push_back(BWAPI::TilePosition(12, 4));
						missileTurretTilePosition.push_back(BWAPI::TilePosition(12, 14));
						missileTurretTilePosition.push_back(BWAPI::TilePosition(7, 41));
						*/

						BWAPI::UnitType requiredType(BWAPI::UnitTypes::Terran_Missile_Turret);

						if (BWAPI::Broodwar->self()->allUnitCount(requiredType) == 0 && queue.numInQueue(requiredType) < 6) {
							int num = missileTurretTilePosition.size();
							for (int i = 0; i < num; i++) {
								queue.queueAsHighestPriority(MacroAct(itemName, missileTurretTilePosition[i]));
							}
							_lastMissileTurretChange = BWAPI::Broodwar->getFrameCount() + 2 * 24;
						}
						else {
							//防止过快检查
							/*
							if (BWAPI::Broodwar->getFrameCount() % 48 == 0) {
							BWAPI::Unitset staticDefense = InformationManager::Instance().getStaticDefense();

							for (auto unit : staticDefense) {
							bool isBuilding = false;
							int index = 0;
							if (unit->getType() == requiredType) {
							BWAPI::TilePosition unitPosition(unit->getTilePosition());
							isBuilding = false;
							for (int i; i < missileTurretTilePosition.size(); i++) {
							if (unit->getTilePosition().x == missileTurretTilePosition[i].x && unit->getTilePosition().y == missileTurretTilePosition[i].y) {
							isBuilding = true;
							index = i;
							break;
							}
							}

							if (!isBuilding) {
							queue.queueAsHighestPriority(MacroAct(itemName, missileTurretTilePosition[index]));
							}
							}
							}
							}
							*/
						}

						/*
						for (int i; i < missileTurretTilePosition.size(); i++) {
						queue.queueAsHighestPriority(MacroAct(itemName, missileTurretTilePosition[i]));
						}

						_hasMissileTurret = true;
						*/
					}
				}
			}
		}

		// If they have mobile cloaked units, get some static detection.
		//如果他们有移动隐形单位, 得到一些静态检测。
		if (InformationManager::Instance().enemyHasMobileCloakTech())
		{
			if (_selfRace == BWAPI::Races::Protoss)
			{
				if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon) < 2 &&
					!queue.anyInQueue(BWAPI::UnitTypes::Protoss_Photon_Cannon) &&
					!BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Photon_Cannon))
				{
					queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Protoss_Photon_Cannon));
					queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Protoss_Photon_Cannon));

					if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Forge) == 0 &&
						!BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Forge))
					{
						queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Protoss_Forge));
					}
				}
			}
			else if (_selfRace == BWAPI::Races::Terran)
			{
				if (BWAPI::Broodwar->getFrameCount() % 24 == 0) {
					if (!BWAPI::Broodwar->self()->isResearching(BWAPI::TechTypes::Spider_Mines) && !BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Terran_Missile_Turret)) {
						queue.queueAsHighestPriority(BWAPI::TechTypes::Spider_Mines);
					}
				}

				if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Terran_Missile_Turret) < 3 &&
					!queue.anyInQueue(BWAPI::UnitTypes::Terran_Missile_Turret) &&
					!BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Terran_Missile_Turret))
				{
					//queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Terran_Missile_Turret));
					//queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Terran_Missile_Turret));
					queue.queueAsHighestPriority(MacroAct("Missile_Turret", BWAPI::TilePosition(InformationManager::Instance().getDefensivePosition())));

					if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) == 0 &&
						!queue.anyInQueue(BWAPI::UnitTypes::Terran_Engineering_Bay) &&
						!BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Terran_Engineering_Bay))
					{
						queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Terran_Engineering_Bay));
					}
				}
			}
		}

		int enemyBases = InformationManager::Instance().getNumBases(BWAPI::Broodwar->enemy());
		int enemyHasCombatUnits = InformationManager::Instance().enemyHasCombatUnits();
		int nZergling = InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Broodwar->enemy());

		int gameFrameCount = BWAPI::Broodwar->getFrameCount();
		if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg) {
			if (gameFrameCount > 5000 && enemyBases < 2) {
				//如果不是2基地开局，则防守
				if (nZergling < 8) {
					CombatCommander::Instance().attackNow();
				}
			}
		}

		if (gameFrameCount < 7000) {
			int lair = InformationManager::Instance().getNumUnits(BWAPI::UnitTypes::Zerg_Lair, BWAPI::Broodwar->enemy());
			if (lair > 0) {
				CombatCommander::Instance().attackNow();

				if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Factory) > 0) {
					if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Vulture) < 3
						&& queue.getNextUnit() != BWAPI::UnitTypes::Terran_Vulture) {
						queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Terran_Vulture));
					}
				}

				if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Terran_Factory) < 2 &&
					!queue.anyInQueue(BWAPI::UnitTypes::Terran_Factory) &&
					!BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Terran_Factory))
				{
					queue.queueAsHighestPriority(MacroAct(BWAPI::UnitTypes::Terran_Factory));
				}
			}
		}

		OpeningPlan enemyPlan = OpponentModel::Instance().getEnemyPlan();
		OpeningPlan likelyEnemyPlan = OpponentModel::Instance().getBestGuessEnemyPlan();

		// If the opponent is rushing, make some defense.
		if (likelyEnemyPlan == OpeningPlan::Proxy ||
			likelyEnemyPlan == OpeningPlan::WorkerRush ||
			likelyEnemyPlan == OpeningPlan::FastRush ||
			enemyPlan == OpeningPlan::HeavyRush)           // we can react later to this
		{
			// If we are terran and have marines, make a bunker.
			if (_selfRace == BWAPI::Races::Terran)
			{
				if (!queue.anyInQueue(BWAPI::UnitTypes::Terran_Bunker) &&
					UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Marine) > 0 &&          // usefulness requirement
					UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Barracks) > 0 &&  // tech requirement for a bunker
					UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Bunker) == 0 &&
					!BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Terran_Bunker))
				{
					//queue.queueAsHighestPriority(MacroAct("Bunker", BWAPI::TilePosition(6,25)));
				}
			}

			// If we are protoss, make a shield battery.
			// NOTE This works, but is turned off because protoss can't use the battery yet.
			/*
			else if (_selfRace == BWAPI::Races::Protoss)
			{
				if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Pylon) > 0 &&    // tech requirement
					UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Gateway) > 0 &&  // tech requirement
					UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Shield_Battery) == 0 &&
					!queue.anyInQueue(BWAPI::UnitTypes::Protoss_Shield_Battery) &&
					!BuildingManager::Instance().isBeingBuilt(BWAPI::UnitTypes::Protoss_Shield_Battery))
				{
					queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Shield_Battery);
				}
			}
			*/
		}

		const BWAPI::UnitType resourceDepotType = _selfRace == BWAPI::Races::Terran
			? BWAPI::UnitTypes::Terran_Command_Center
			: BWAPI::UnitTypes::Protoss_Nexus;
		const int numDepots = UnitUtil::GetAllUnitCount(resourceDepotType);
		if (numDepots > _highWaterBases)
		{
			_highWaterBases = numDepots;
		}
		bool makeResourceDepot = false;

		// If there is no resource depot, order one if we can afford it.
		// NOTE Does not check whether we have a worker to build it.
		if (numDepots == 0 && BWAPI::Broodwar->self()->minerals() >= 400)
		{
			makeResourceDepot = true;
		}

		// If the opponent fast expanded and we haven't taken the natural yet, do that immediately.
		// Not if the enemy is zerg, though. Zerg can be ahead in expansions.
		if (enemyPlan == OpeningPlan::SafeExpand || enemyPlan == OpeningPlan::NakedExpand)
		{
			// Use _highWaterBases instead of numDepots so we don't try to remake a destroyed natural.
			if (_highWaterBases == 1 && BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg)
			{
				makeResourceDepot = true;
			}
		}

		// We only care about the next item in the queue, not possible later resource depots in the queue.
		// This should be done last, so that no other emegency reaction puts something else in front of
		// a resource depot in the queue. Otherwise the rule will fire repeatedly.
		if (makeResourceDepot &&
			(!nextInQueuePtr || !nextInQueuePtr->isUnit() || nextInQueuePtr->getUnitType() != resourceDepotType) &&
			!BuildingManager::Instance().isBeingBuilt(resourceDepotType))
		{
			queue.queueAsHighestPriority(MacroAct(resourceDepotType));
		}
	}
}

void StrategyManager::bunkerRush() {

}

//获取下一次升级
bool StrategyManager::obtainNextUpgrade(BWAPI::UpgradeType upgType)
{
	BWAPI::Player self = BWAPI::Broodwar->self();
	int maxLvl = self->getMaxUpgradeLevel(upgType);
	int currentLvl = self->getUpgradeLevel(upgType);


	if (!self->isUpgrading(upgType) && currentLvl < maxLvl &&
		self->completedUnitCount(upgType.whatsRequired(currentLvl + 1)) > 0)
		return true;
	return false;
}

// Called to refill the production queue when it is empty.
//在空时调用以重新填充生产队列。
void StrategyManager::freshProductionPlan()
{
	if (_selfRace == BWAPI::Races::Zerg)
	{
		ProductionManager::Instance().setBuildOrder(StrategyBossZerg::Instance().freshProductionPlan());
	}
	else if (_selfRace == BWAPI::Races::Terran)
	{
		ProductionManager::Instance().setBuildOrder(StrategyBossTerran::Instance().freshProductionPlan());
		//performBuildOrderSearch();
	}
	else if (_selfRace == BWAPI::Races::Protoss)
	{
		ProductionManager::Instance().setBuildOrder(StrategyBossProtoss::Instance().freshProductionPlan());
		//performBuildOrderSearch();
	}
	else
	{
		performBuildOrderSearch();
	}
}

// Return true if we're supply blocked and should build supply.
// Note: This understands zerg supply but is not used when we are zerg.
//返回 true, 如果我们的供应受阻, 并应建立供应。
//注意: 这了解虫族供应, 但在我们是虫族时不使用。
bool StrategyManager::detectSupplyBlock(BuildOrderQueue & queue)
{
	// If the _queue is empty or supply is maxed, there is no block.
	if (queue.isEmpty() || BWAPI::Broodwar->self()->supplyTotal() >= 400)
	{
		return false;
	}

	// If supply is being built now, there's no block. Return right away.
	// Terran and protoss calculation:
	if (BuildingManager::Instance().isBeingBuilt(BWAPI::Broodwar->self()->getRace().getSupplyProvider()))
	{
		return false;
	}

	// Terran and protoss calculation:
	int supplyAvailable = BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed();

	// Zerg calculation:
	// Zerg can create an overlord that doesn't count toward supply until the next check.
	// To work around it, add up the supply by hand, including hatcheries.
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) {
		supplyAvailable = -BWAPI::Broodwar->self()->supplyUsed();
		for (auto & unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
			{
				supplyAvailable += 16;
			}
			else if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg &&
				unit->getBuildType() == BWAPI::UnitTypes::Zerg_Overlord)
			{
				return false;    // supply is building, return immediately
				// supplyAvailable += 16;
			}
			else if ((unit->getType() == BWAPI::UnitTypes::Zerg_Hatchery && unit->isCompleted()) ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Lair ||
				unit->getType() == BWAPI::UnitTypes::Zerg_Hive)
			{
				supplyAvailable += 2;
			}
		}
	}

	int supplyCost = queue.getHighestPriorityItem().macroAct.supplyRequired();
	// Available supply can be negative, which breaks the test below. Fix it.
	supplyAvailable = std::max(0, supplyAvailable);

	// if we don't have enough supply, we're supply blocked
	if (supplyAvailable < supplyCost)
	{
		// If we're zerg, check to see if a building is planned to be built.
		// Only count it as releasing supply very early in the game.
		if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg
			&& BuildingManager::Instance().buildingsQueued().size() > 0
			&& BWAPI::Broodwar->self()->supplyTotal() <= 18)
		{
			return false;
		}
		return true;
	}

	return false;
}

void StrategyManager::performBuildOrderSearch()
{
	if (!canPlanBuildOrderNow())
	{
		return;
	}

	/*
	BuildOrder & buildOrder = BOSSManager::Instance().getBuildOrder();

	if (buildOrder.size() > 0)
	{
		ProductionManager::Instance().setBuildOrder(buildOrder);
		BOSSManager::Instance().reset();
	}
	else
	{
		if (!BOSSManager::Instance().isSearchInProgress())
		{
			BOSSManager::Instance().startNewSearch(getBuildOrderGoal());
		}
	}
	*/
}

// this will return true if any unit is on the first frame of its training time remaining
// this can cause issues for the build order search system so don't plan a search on these frames
bool StrategyManager::canPlanBuildOrderNow() const
{
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getRemainingTrainTime() == 0)
		{
			continue;
		}

		BWAPI::UnitType trainType = unit->getLastCommand().getUnitType();

		if (unit->getRemainingTrainTime() == trainType.buildTime())
		{
			return false;
		}
	}

	return true;
}

// Do we expect or plan to drop at some point during the game?
//我们是否期望或计划在比赛中的某个时刻下降？
bool StrategyManager::dropIsPlanned() const
{
	// Don't drop in ZvZ.
	if (_selfRace == BWAPI::Races::Zerg && BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
	{
		return false;
	}

	// Otherwise plan drop if the opening says so, or if the map has islands to take.
	return getOpeningGroup() == "drop" ||
		Config::Macro::ExpandToIslands && MapTools::Instance().hasIslandBases();
}

// Whether we have the tech and transport to drop.
bool StrategyManager::hasDropTech()
{
	if (_selfRace == BWAPI::Races::Zerg)
	{
		// NOTE May be slow drop.
		return BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Ventral_Sacs) > 0 &&
			UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Overlord) > 0;
	}
	if (_selfRace == BWAPI::Races::Protoss)
	{
		return UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Shuttle) > 0;
	}
	if (_selfRace == BWAPI::Races::Terran)
	{
		return UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Dropship) > 0;
	}

	return false;
}