#pragma once

#include "Common.h"
#include "MapGrid.h"
#include "SquadOrder.h"
#include "InformationManager.h"
#include "CombatSimulation.h"
#include "Micro.h"

namespace UAlbertaBot
{

class MicroManager
{
	BWAPI::Unitset		_units;

protected:
	
	SquadOrder			order;

	virtual void        executeMicro(const BWAPI::Unitset & targets) = 0;
	virtual void		getTargets(BWAPI::Unitset & targets) const;
	bool                shouldIgnoreTarget(BWAPI::Unit combatUnit, BWAPI::Unit target);
	bool				buildScarabOrInterceptor(BWAPI::Unit u) const;
	bool                checkPositionWalkable(BWAPI::Position pos);
	bool                unitNearEnemy(BWAPI::Unit unit);
	bool                unitNearEnemy(BWAPI::Unit unit, int dist);
	bool                unitNearChokepoint(BWAPI::Unit unit) const;

	bool				mobilizeUnit(BWAPI::Unit unit) const;      // unsiege or unburrow
	bool				immobilizeUnit(BWAPI::Unit unit) const;    // siege or burrow
	bool				unstickStuckUnit(BWAPI::Unit unit);

	bool				loadBunkers(BWAPI::Unit unit);//进地堡

	void				useShieldBattery(BWAPI::Unit unit, BWAPI::Unit shieldBattery);
	bool				isShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);//是否撤退

	bool				keepDefendOrder(BWAPI::Unit unit, BWAPI::Unit target);

	bool				keepTankDistance(BWAPI::Unit unit, BWAPI::Position position);//保持和坦克的距离
	bool				keepSafeDistance(BWAPI::Unit unit, BWAPI::Unitset nearEnemys);//安全距离

	bool				keepElude(BWAPI::Unit unit);//保持躲避

	BWAPI::Position		getFleePosition(BWAPI::Unit unit, BWAPI::Unit target);
	BWAPI::Position		getFleePosition(BWAPI::Position form, BWAPI::Position to, int dist);


	bool				unitShouldRetreat(BWAPI::Unit unit, BWAPI::Unitset nearEnemys);

	void                drawOrderText();

public:
	std::map<BWAPI::Unit, int>		retreatUnit;
	
	MicroManager();
    virtual				~MicroManager(){}

	const BWAPI::Unitset & getUnits() const;
	bool				containsType(BWAPI::UnitType type) const;
	BWAPI::Position     calcCenter() const;

	void				setUnits(const BWAPI::Unitset & u);
	void				setOrder(const SquadOrder & inputOrder);
	void				execute(const SquadOrder & order);
	void				regroup(const BWAPI::Position & regroupPosition) const;

	void				removeUnit(BWAPI::Unit unit);

};
}