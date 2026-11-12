#pragma once

#include "Common.h"
#include "MacroCommand.h"
#include "MicroManager.h"
#include "InformationManager.h"

namespace UAlbertaBot
{
class ScoutManager 
{
	BWAPI::Unit						_overlordScout;
	BWAPI::Unit						_workerScout;
	BWAPI::Unit						_myWorkerScout;
    std::string                     _scoutStatus;
    std::string                     _gasStealStatus;
	MacroCommandType				_scoutCommand;
	MacroCommandType				_myScoutCommand;

	BWAPI::TilePosition				_overlordScoutTarget;
	BWAPI::TilePosition				_workerScoutTarget;    // only while still seeking the enemy base
	bool							_overlordAtEnemyBase;
	bool			                _scoutUnderAttack;
	bool							_tryGasSteal;
	BWAPI::Unit						_enemyGeyser;
    bool                            _startedGasSteal;
	bool							_queuedGasSteal;
	bool							_gasStealOver;
    int                             _currentRegionVertexIndex;
    int                             _previousScoutHP;
	std::vector<BWAPI::Position>    _enemyRegionVertices;
	std::vector<BWAPI::TilePosition> scoutLocations;//Õì²ì×ø±ê

	std::vector<BWAPI::Position>    _myRegionVertices;
	int                             _myRegionVertexIndex;

	std::vector< BWAPI::TilePosition>    _exploredPosition;

	ScoutManager();

	void							initScoutTargets();
	bool							sortScoutTargets(BWAPI::TilePosition pos1, BWAPI::TilePosition pos2);
	void							setScoutTargets();

	bool                            enemyWorkerInRadius();
    bool                            gasSteal();
    int                             getClosestVertexIndex(BWAPI::Unit unit);
    BWAPI::Position                 getFleePosition();

	int                             getMyClosestVertexIndex(BWAPI::Unit unit);
	BWAPI::Position                 getMyFleePosition();

	BWAPI::Unit						getAnyEnemyGeyser() const;
	BWAPI::Unit						getTheEnemyGeyser() const;
	BWAPI::Unit						enemyWorkerToHarass() const;
    void                            followPerimeter();
	void							followMyPerimeter();

	void                            moveGroundScout(BWAPI::Unit scout);
	void                            moveAirScout(BWAPI::Unit scout);
	void                            drawScoutInformation(int x, int y);
    void                            calculateEnemyRegionVertices();
	void                            calculateMyRegionVertices();
	std::vector<BWAPI::Position>    calculateEnemyRegionVertices(BWTA::BaseLocation * baseLocation);

public:

    static ScoutManager & Instance();

	void update();

	bool shouldScout();
	
	void setOverlordScout(BWAPI::Unit unit);
	void setWorkerScout(BWAPI::Unit unit);
	BWAPI::Unit getWorkerScout() const { return _workerScout; };
	void releaseWorkerScout();

	void setGasSteal() { _tryGasSteal = true; };
	bool tryGasSteal() const { return _tryGasSteal; };
	bool wantGasSteal() const { return _tryGasSteal && !_gasStealOver; };
	bool gasStealQueued() const { return _queuedGasSteal; };
	void gasStealOver() { _gasStealOver = true; };    // called by BuildingManager when releasing the worker

	void setScoutCommand(MacroCommandType cmd);

	void setMyScoutCommand(MacroCommandType cmd);
};

class SortScoutTargets
{

public:

	SortScoutTargets() {}

	// For sorting the display of items under construction.
	// Some redundant code removed here thanks to Andrey Kurdiumov.
	bool operator() (BWAPI::TilePosition pos1, BWAPI::TilePosition pos2)
	{
		BWAPI::TilePosition startLocation = BWAPI::Broodwar->self()->getStartLocation();
		return startLocation.getDistance(pos1) > startLocation.getDistance(pos2);
	}
};
}