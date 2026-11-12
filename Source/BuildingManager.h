#pragma once

#include "WorkerManager.h"
#include "BuildingPlacer.h"
#include "InformationManager.h"
#include "MacroAct.h"
#include "MapTools.h"

namespace UAlbertaBot
{
class BuildingManager
{
    BuildingManager();

    std::vector<Building> _buildings;
	std::vector<Building> _flyBuildings;

	std::map<std::string, Building> _buildingData;

    int             _reservedMinerals;				// minerals reserved for planned buildings//为规划建筑物预留的矿物。
    int             _reservedGas;					// gas reserved for planned buildings

	int				_buildingCheckFrame = 0;

    bool            isBuildingPositionExplored(const Building & b) const;
	void			undoBuildings(const std::vector<Building> & toRemove);
    void            removeBuildings(const std::vector<Building> & toRemove);
	void            removeFlyBuildings(const std::vector<Building> & toRemove);

    void            validateWorkersAndBuildings();		    // STEP 1
    void            assignWorkersToUnassignedBuildings();	// STEP 2
    void            constructAssignedBuildings();			// STEP 3
    void            checkForStartedConstruction();			// STEP 4
    void            checkForDeadTerranBuilders();			// STEP 5
    void            checkForCompletedBuildings();			// STEP 6
	void			checkReservedResources();				// error check

	void			checkFlyBuildings();					//检测需要飞的建筑

	bool			checkCommandCenter(Building & b);
	bool			checkScienceFacility(Building & b);

	bool			checkBuildingTiles(Building & b);
	bool			buildingTimedOut(const Building & b) const;
	bool			buildingWaitTimedOut(const Building & b) const;
    char            getBuildingWorkerCode(const Building & b) const;

	void			setBuilderUnit(Building & b);
	BWAPI::Unit		getFlyBuildingUnit(BWAPI::UnitType type, BWAPI::TilePosition pos);
	void			releaseBuilderUnit(const Building & b);

public:
    
    static BuildingManager &	Instance();

    void                update();
    void                onUnitMorph(BWAPI::Unit unit);
    void                onUnitDestroy(BWAPI::Unit unit);
	Building &		    addTrackedBuildingTask(const MacroAct & act, BWAPI::TilePosition desiredLocation, bool isGasSteal);
	void                addBuildingTask(const MacroAct & act, BWAPI::TilePosition desiredLocation, bool isGasSteal);
    BWAPI::TilePosition getBuildingLocation(const Building & b);

	void                drawBuildingInformation(int x, int y);
	void                drawBuildingData();

    int                 getReservedMinerals() const;
    int                 getReservedGas() const;

	bool				anythingBeingBuilt() const { return !_buildings.empty();  };
    bool                isBeingBuilt(BWAPI::UnitType type) const;
	size_t              getNumUnstarted(BWAPI::UnitType type) const;
	int					getNumBeingBuilt(BWAPI::UnitType type);
	bool				isGasStealInQueue() const;

    std::vector<BWAPI::UnitType> buildingsQueued();

	void                cancelBuilding(Building & b);
	void				cancelQueuedBuildings();
	void				cancelBuildingType(BWAPI::UnitType t);

	void				setRallyPoint(BWAPI::Position position);
	void				addFlyBuildingTask(MacroCommand command);

	void				addBuildingData(std::string name, std::string unitTypeName, BWAPI::TilePosition postion);

	//std::map<std::string, Building> getBuildingData() { return _buildingData; };
};

}