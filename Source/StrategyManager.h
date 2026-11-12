#pragma once

#include "Common.h"
#include "InformationManager.h"
#include "WorkerManager.h"
#include "BuildOrder.h"
#include "BuildOrderQueue.h"

namespace UAlbertaBot
{
typedef std::pair<MacroAct, size_t> MetaPair;
typedef std::vector<MetaPair> MetaPairVector;

struct Strategy
{
    std::string _name;
    BWAPI::Race _race;
	std::string _openingGroup;
    BuildOrder  _buildOrder;
	int			_aggressiveScore = 99999;

    Strategy()
        : _name("None")
        , _race(BWAPI::Races::None)
 		, _openingGroup("")
    {
    }

	Strategy(const std::string & name, const BWAPI::Race & race, const std::string & openingGroup, const BuildOrder & buildOrder)
        : _name(name)
        , _race(race)
		, _openingGroup(openingGroup)
		, _buildOrder(buildOrder)
	{
    }

	Strategy(const std::string & name, const BWAPI::Race & race, const std::string & openingGroup, const BuildOrder & buildOrder, int aggressiveScore)
		: _name(name)
		, _race(race)
		, _openingGroup(openingGroup)
		, _buildOrder(buildOrder)
		, _aggressiveScore(aggressiveScore)
	{
	}
};

class StrategyManager 
{
	StrategyManager();

	BWAPI::Player					_enemy;
	BWAPI::Race					    _selfRace;
	BWAPI::Race					    _enemyRace;
    std::map<std::string, Strategy> _strategies;
	int								_aggressiveScore;

    int                             _totalGamesPlayed;
    const BuildOrder                _emptyBuildOrder;
	std::string						_openingGroup;

	bool							_hasDropTech;
	int								_highWaterBases;   // most bases we've ever had, terran and protoss only
	int								_lastMissileTurretChange;//最后检查防守时间
	MacroAct						_addMacroAct;		//新增加的指令

	const	bool				    shouldExpandNow() const;

    const	MetaPairVector		    getProtossBuildOrderGoal();
	const	MetaPairVector		    getTerranBuildOrderGoal();
	const	MetaPairVector		    getZergBuildOrderGoal() const;

	bool							detectSupplyBlock(BuildOrderQueue & queue);
	bool							canPlanBuildOrderNow() const;
	void							performBuildOrderSearch();

	void							bunkerRush();

public:
    
	static	StrategyManager &	    Instance();

	void					setEnemy(BWAPI::Player player);

            void                    addStrategy(const std::string & name, Strategy & strategy);
			void					setAggressiveScore();
			void					setOpeningGroup();
			void					setOpeningGroup(std::string _openingGroupName) { _openingGroup = _openingGroupName; };
	const	std::string &			getOpeningGroup() const;

	const	MetaPairVector		    getBuildOrderGoal();
	const	BuildOrder &            getOpeningBookBuildOrder() const;

			void					handleUrgentProductionIssues(BuildOrderQueue & queue);
			void					freshProductionPlan();

			bool					dropIsPlanned() const;
			bool					hasDropTech();
			bool					obtainNextUpgrade(BWAPI::UpgradeType upgType);

			void					addMacroAct(MacroAct act) { _addMacroAct = act; };
};

}