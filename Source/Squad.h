#pragma once

#include "Common.h"
#include "DistanceMap.hpp"
#include "StrategyManager.h"
#include "CombatSimulation.h"
#include "SquadOrder.h"

#include "MicroAirToAir.h"
#include "MicroMelee.h"
#include "MicroDarkTemplar.h"
#include "MicroRanged.h"
#include "MicroCarriers.h"

#include "MicroDetectors.h"
#include "MicroHighTemplar.h"
#include "MicroLurkers.h"
#include "MicroMarines.h"
#include "MicroMedics.h"
#include "MicroRepairs.h"
#include "MicroTanks.h"
#include "MicroVultures.h"
#include "MicroScienceVessel.h"
#include "MicroTransports.h"
#include "MicroDragoon.h"
#include "MicroWraith.h"

namespace UAlbertaBot
{

class Squad
{
    std::string         _name;
	BWAPI::Unitset      _units;
	bool				_combatSquad;
	int					_combatSimRadius;
	bool				_fightVisibleOnly;  // combat sim uses visible enemies only (vs. all known enemies)
	bool				_hasAir;
	bool				_hasGround;
	bool				_hasAntiAir;
	bool				_hasTank;			//部队里是否有坦克
	bool				_hasAntiGround;
	std::string         _regroupStatus;
	bool				_attackAtMax;       // turns true when we are at max supply//当我们处于最大供应量时变成真的
    int                 _lastRetreatSwitch;
    bool                _lastRetreatSwitchVal;
    size_t              _priority;
	BWAPI::Position		_targetLocation;	//目标坐标
	
	SquadOrder          _order;
	MicroAirToAir		_microAirToAir;
	MicroMelee			_microMelee;
	MicroDarkTemplar	_microDarkTemplar;
	MicroRanged			_microRanged;
	MicroCarriers		_microCarriers;
	MicroDetectors		_microDetectors;
	MicroHighTemplar	_microHighTemplar;
	MicroLurkers		_microLurkers;
	MicroMarines		_microMarines;
	MicroMedics			_microMedics;
	MicroRepairs        _microWorkers;
	MicroTanks			_microTanks;
	MicroVultures		_microVultures;
	MicroScienceVessel	_microScienceVessels;
	MicroTransports		_microTransports;
	MicroDragoon		_microDragoon;
	MicroWraith			_microWraith;

	std::map<BWAPI::Unit, bool>	_nearEnemy;

	BWAPI::Unit		getRegroupUnit();
    
	void			updateUnits();
	void			addUnitsToMicroManagers();
	void			setNearEnemyUnits();
	void			setAllUnits();
	
	bool			unitNearEnemy(BWAPI::Unit unit);
	bool			needsToRegroup();

	void			loadTransport();
	void			stimIfNeeded();

public:

	Squad(const std::string & name, SquadOrder order, size_t priority);
	Squad();
    ~Squad();

	void                update();
	void                setSquadOrder(const SquadOrder & so);
	void                addUnit(BWAPI::Unit u);
	void                removeUnit(BWAPI::Unit u);
	void				releaseWorkers();
    bool                containsUnit(BWAPI::Unit u) const;
	bool                containsUnitType(BWAPI::UnitType t) const;
	bool                isEmpty() const;
    void                clear();
    size_t              getPriority() const;
    void                setPriority(const size_t & priority);
    const std::string & getName() const;
    
	BWAPI::Position     calcCenter();
	BWAPI::Position     calcRegroupPosition();

	const BWAPI::Unitset &  getUnits() const;
	const SquadOrder &  getSquadOrder()	const;

	BWAPI::Position		getTargetLocation() { return _targetLocation; };
	void				setTargetLocation(BWAPI::Position pos) { _targetLocation = pos; };

	int					getCombatSimRadius() const { return _combatSimRadius; };
	void				setCombatSimRadius(int radius) { _combatSimRadius = radius; };

	bool				getFightVisible() const { return _fightVisibleOnly; };
	void				setFightVisible(bool visibleOnly) { _fightVisibleOnly = visibleOnly; };

	const bool			hasAir()        const { return _hasAir; };
	const bool			hasGround()     const { return _hasGround; };
	const bool			hasAntiAir()    const { return _hasAntiAir; };
	const bool			hasAntiGround() const { return _hasAntiGround; };
	const bool			hasDetector()   const { return !_microDetectors.getUnits().empty(); };
	const bool			hasCombatUnits() const;
	const bool			isOverlordHunterSquad() const;

	bool                hasMicroManager(const MicroManager* microManager) const;

	BWAPI::Unit		unitClosestToEnemy();

	BWAPI::Unit		unitClosestToEnemy(BWAPI::Position position);

};
}