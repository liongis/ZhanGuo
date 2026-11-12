#pragma once

#include <Common.h>
#include "BuildingManager.h"
#include "WorkerData.h"

namespace UAlbertaBot
{
class Building;

class WorkerManager
{
    WorkerData  workerData;
    BWAPI::Unit previousClosestWorker;
	BWAPI::Unit buildingWorker;//建筑农民

	bool		_collectGas;

	BWAPI::Unitset nearbyEnemies;//接近主基地的危险敌人
	bool		isEscape = false;//是否逃跑
	BWAPI::Unitset escapeWorker;//
	BWAPI::Unit escapeGas;

	int			escapeFrame = 10 * 24;

	void        setReturnCargoWorker(BWAPI::Unit unit);
	bool		refineryHasDepot(BWAPI::Unit refinery);
	bool        isGasStealRefinery(BWAPI::Unit unit);

	void        handleGasWorkers();
	void        handleIdleWorkers();
	void		handleReturnCargoWorkers();
	void        handleRepairWorkers();
    void        handleMoveWorkers();

	void		handleBaseDefense();//主基地防御

	void        handleMoveToMineralWorkers();//移动去采矿的农民
	bool		defendSelf(BWAPI::Unit worker, BWAPI::Unit resource);

	BWAPI::Unit getAnyClosestDepot(BWAPI::Unit worker);      // don't care whether it's full
	BWAPI::Unit getClosestNonFullDepot(BWAPI::Unit worker);  // only if it can accept more mineral workers

	WorkerManager();

public:

    void        update();
    void        onUnitDestroy(BWAPI::Unit unit);
    void        onUnitMorph(BWAPI::Unit unit);
    void        onUnitShow(BWAPI::Unit unit);
    void        onUnitRenegade(BWAPI::Unit unit);

	void        setMineralWorker(BWAPI::Unit unit);
	void        setMineralWorker(BWAPI::Unit unit, BWAPI::Unit depot);
	void        setDepotWorker(BWAPI::Unit depot);
	void        finishedWithWorker(BWAPI::Unit unit);

    void        drawResourceDebugInfo();
    void        updateWorkerStatus();
    void        drawWorkerInformation(int x,int y);

    int         getNumMineralWorkers() const;
    int         getNumGasWorkers() const;
	int         getNumReturnCargoWorkers() const;
	int			getNumCombatWorkers() const;
	int         getNumIdleWorkers() const;
	BWAPI::Unitset getIdleWorkers() const;

	int			getMaxWorkers() const;

    void        setScoutWorker(BWAPI::Unit worker);

	// NOTE _collectGas == false allows that a little more gas may still be collected.
	bool		isCollectingGas()              { return _collectGas; };
	void		setCollectGas(bool collectGas) { _collectGas = collectGas; };

    bool        isWorkerScout(BWAPI::Unit worker);
	bool		isCombatWorker(BWAPI::Unit worker);
    bool        isFree(BWAPI::Unit worker);
    bool        isBuilder(BWAPI::Unit worker);

	BWAPI::Unit getAnyFreeWorker();

    BWAPI::Unit getBuilder(const Building & b,bool setJobAsBuilder = true);
    BWAPI::Unit getMoveWorker(BWAPI::Position p);
    BWAPI::Unit getGasWorker(BWAPI::Unit refinery);
	BWAPI::Unit getClosestMineralWorkerTo(BWAPI::Unit enemyUnit,bool urgent = false);//选择离指定目标最近的农民
	BWAPI::Unit getClosestTargetWorkerTo(BWAPI::Position position, bool urgent = false);//选择离指定坐标最近的农民
    BWAPI::Unit getWorkerScout();

    void        setBuildingWorker(BWAPI::Unit worker,Building & b);
	void        setRepairWorker(BWAPI::Unit worker);
    void        setRepairWorker(BWAPI::Unit worker,BWAPI::Unit unitToRepair);
    void        stopRepairing(BWAPI::Unit worker);
	void        setMoveWorker(BWAPI::Unit worker, int mineralsNeeded, int gasNeeded, BWAPI::Position & p);
    void        setCombatWorker(BWAPI::Unit worker);
	void        setEscapeWorker(BWAPI::Unit worker);//逃跑农民

    bool        willHaveResources(int mineralsRequired,int gasRequired,double distance);
    void        rebalanceWorkers();

	bool		maybeMineMineralBlocks(BWAPI::Unit worker);

	bool        isUrgentRepairTarget(BWAPI::Unit unit);  //是否为紧急维修目标

	BWAPI::Unit findEnemyTargetForWorker(BWAPI::Unit worker) const;
	BWAPI::Unit findEscapeMinerals(BWAPI::Unit worker) const;
	BWAPI::Unit findEscapeMinerals(BWAPI::Position position) const;

	BWAPI::Unitset getWorkers(){ return workerData.getWorkers(); };

    static WorkerManager &  Instance();
};
}