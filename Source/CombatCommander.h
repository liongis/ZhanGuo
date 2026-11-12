#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"
#include "InformationManager.h"
#include "StrategyManager.h"

namespace UAlbertaBot
{
class SquadInfo
{
	std::string     _name;//名称
	std::map<BWAPI::UnitType, int> _squad;
	BWAPI::TilePosition _position;//位置
	int				_updateFrame;//更新时间

public:
	SquadInfo()
		: _name("")
		, _updateFrame(0)
		, _position()
	{
		_squad[BWAPI::UnitTypes::None] = 0;
	}

	SquadInfo(std::string name, BWAPI::UnitType type, int num, BWAPI::TilePosition position)
		: _name(name)
		, _position(position)
		, _updateFrame(BWAPI::Broodwar->getFrameCount())
	{
		_squad[type] = num;
	}

	const bool operator == (std::string squadName) const
	{
		return _name == squadName;
	}


	std::string						getName() { return _name; };
	std::map<BWAPI::UnitType, int>	getSquad() { return _squad; };
	BWAPI::TilePosition				getPosition(){ return _position; };
	void							setSquad(BWAPI::UnitType type, int num) { _squad[type] = num; };
	void							setPosition(BWAPI::TilePosition position) { _position = position; };
};

class CombatCommander
{
	BWAPI::Player		_self = BWAPI::Broodwar->self();

	SquadData			_squadData;
    BWAPI::Unitset		_combatUnits;
	int					_mainAttackUnits;
    bool				_initialized;

	bool				_goAggressive;
	int					_aggressiveScore;
	bool				_needsToRegroup = false;//重组

	double				_multiplierAggressive = 1.2;	//进攻倍数
		
	BWAPI::Position		_reconTarget;					//地面侦察目标
	BWAPI::Position		_reconFlyTarget;				//空中侦察目标
	BWAPI::TilePosition	_reconScoutTarget;				//农民侦察目标

	BWAPI::Position		_mainAttackTarget;

	BWAPI::UnitInterface *	_rushBunker;
	BWAPI::UnitInterface *	_builderUnit;
	int				_lastBuildingChange;

	int				_lastReconTargetChange;         // frame number
	int				_lastReconFlyTargetChange;
	int				_lastReconScoutTargetChange;
	int				_lastScanTargetChange;			//最后撒雷达时间
	int				_lastDefenseTargetChange;		//扫矿时间

	std::map<BWAPI::UnitType, int>	_reconUnitTypes;//侦察类型为数量
	std::map<std::string, SquadInfo>	_defenseSquadData;//侦察类型为数量

    void            updateScoutDefenseSquad();
	void            updateBaseDefenseSquads();//所有占领区防守
	void			updateMainDefenseSquads();//主基地防守
	void            updateDefenseSquads();
	void			updateReconSquad();
	void            updateAttackSquads();
    void            updateDropSquads();
	void            updateIdleSquad();
	void			updateRepairSquad();

	void			updateStuckUnit();
	void			updaateDarkSwarm();//黄雾

	void			updateBaseAttackSquads();//扫矿部队

	void			updateBunkerRush();

	void			loadOrUnloadBunkers();
	void			doComsatScan();
	void			isAggressive();

	int				weighReconUnit(const BWAPI::Unit unit) const;
	int				weighReconUnit(const BWAPI::UnitType type) const;

	bool			unitIsGoodToDrop(const BWAPI::Unit unit) const;

	void			cancelDyingItems();

	int             getNumType(BWAPI::Unitset & units, BWAPI::UnitType type);

	int				getFrameSquadOrderTypes();

	BWAPI::Position getDefendLocation();
	void			chooseReconTarget();
	void			chooseReconFlyTarget();
	BWAPI::Position getReconLocation() const;
	BWAPI::Position getReconFlyLocation() const;
	BWAPI::Position getMainAttackLocation(const Squad * squad);
	BWAPI::Position getTargetLocation(Squad * squad);

    void            initializeSquads();
    void            assignFlyingDefender(Squad & squad);
    void            emptySquad(Squad & squad, BWAPI::Unitset & unitsToAssign);
    int             getNumGroundDefendersInSquad(Squad & squad);
    int             getNumAirDefendersInSquad(Squad & squad);

    int             numZerglingsInOurBase() const;
    bool            buildingRush() const;

	static int		workerPullScore(BWAPI::Unit worker);

public:

	CombatCommander();

	void			update(const BWAPI::Unitset & combatUnits);
	int				getNumCombatUnits(){ return _combatUnits.size(); };
	int				getNumMainAttackUnits(){ return _mainAttackUnits; };

	void			setAggression(bool aggressive) { _goAggressive = aggressive;  }
	bool			getAggression() const { return _goAggressive; };

	void			setAggressiveScore(int score);
	const	int &	getAggressiveScore() const;

	bool			getNeedsToRegroup() { return _needsToRegroup; };
	void			setNeedsToRegroup(bool isNeeds) { _needsToRegroup = isNeeds; };

	void			attackNow();
	void			defenseNow();

	void			setMultiplierAggressive(double multiplier) { _multiplierAggressive = multiplier; }
	double			getMultiplierAggressive() { return _multiplierAggressive; }

	void			setReconUnitTypes(BWAPI::UnitType type, int num) { _reconUnitTypes[type] = num; };

	void			pullWorkers(int n);
	void			releaseWorkers();

	void			updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded, bool pullWorkers);
	void			setDefenseSquadUnits(BWAPI::Position pos, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded, bool pullWorkers);
	//设置分部队
	void			setDefenseSquad(std::string name, BWAPI::UnitType type, int num, BWAPI::TilePosition position);
	void			setDefenseSquad(std::string name, BWAPI::TilePosition position);
	void			clearDefenseSquad(std::string name);

	void			setReconTarget(BWAPI::Position target);
	void			setReconFlyTarget(BWAPI::Position target) { _reconFlyTarget = target; _lastReconFlyTargetChange = BWAPI::Broodwar->getFrameCount(); };
	BWAPI::Position getReconTarget() { return _reconTarget; };
	BWAPI::Position getReconFlyTarget() { return _reconFlyTarget; };

	BWAPI::Unit     findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender, bool pullWoekers);
	BWAPI::Unit     findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target);

	void			setRepairWorkerToTarget(BWAPI::Unit target, int num);

	bool			assignUnitToSquad(BWAPI::Unit unit, const std::string & squadName);
	bool			removeUnitToSquad(BWAPI::Unit unit, const std::string & squadName);

	void			drawSquadInformation(int x, int y);

	SquadData& getSquadData() { return _squadData; };

	static CombatCommander & Instance();
};
}
