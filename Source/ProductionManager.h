#pragma once

#include <Common.h>
#include "BuildOrderQueue.h"
#include "BuildingManager.h"
#include "StrategyManager.h"
//#include "BOSSManager.h"
#include "BuildOrder.h"

namespace UAlbertaBot
{

enum class ExtractorTrick { None, Start, ExtractorOrdered, UnitOrdered, MakeUnitBypass };

class ProductionManager
{
    ProductionManager();
    
	BWAPI::Player		_self = BWAPI::Broodwar->self();
	BWAPI::Race			_selfRace = BWAPI::Broodwar->self()->getRace();

    BuildOrderQueue     _queue;
	int					_lastProductionFrame;            // for detecting jams
	int					_waitProductionFrame;			//暂停生产时间

    BWAPI::TilePosition _predictedTilePosition;
    BWAPI::Unit         _assignedWorkerForThisBuilding;
    bool                _haveLocationForThisBuilding;
	int					_frameWhenDependendenciesMet;
	int					_delayBuildingPredictionUntilFrame;
	int					_exchangePredictionUntilFrame = 0; //交换生产单位时间
	bool				_outOfBook;                      // production queue is beyond the opening book
	int					_targetGasAmount;                // for "go gas until <n>"; set to 0 if no target
	ExtractorTrick		_extractorTrickState;
	BWAPI::UnitType		_extractorTrickUnitType;         // drone or zergling
	Building *			_extractorTrickBuilding;         // set depending on the extractor trick state
    
    BWAPI::Unit         getClosestUnitToPosition(const BWAPI::Unitset & units,BWAPI::Position closestTo);
	BWAPI::Unit         getFarthestUnitFromPosition(const BWAPI::Unitset & units, BWAPI::Position farthest);
	BWAPI::Unit         getClosestLarvaToPosition(BWAPI::Position closestTo);
	
	void				executeCommand(MacroCommand command);
    bool                meetsReservedResources(MacroAct type);
    bool                create(BWAPI::Unit producer,const BuildOrderItem & item);
    void                manageBuildOrderQueue();
	void				maybePermuteQueue();
	bool				independentUnitType(BWAPI::UnitType type) const;
    bool                canMakeNow(BWAPI::Unit producer,MacroAct t);
    void                predictWorkerMovement(const Building & b);

	bool				hasRequiredUnit(const MacroAct & act);//是否有必须建筑？

    int                 getFreeMinerals() const;
    int                 getFreeGas() const;
	int                 getFreeSupply() const;

	void				doExtractorTrick();

	BWAPI::Unit getProducer(MacroAct t, BWAPI::Position closestTo = BWAPI::Positions::None);

	BWAPI::TilePosition getDefaultTilePosition(BWAPI::UnitType unitType);

public:

    static ProductionManager &	Instance();

	BuildOrderQueue &  getQueue() { return _queue; };

    void	drawQueueInformation(std::map<BWAPI::UnitType,int> & numUnits,int x,int y,int index);
	void	setBuildOrder(const BuildOrder & buildOrder);
	void	update();
	void	onUnitMorph(BWAPI::Unit unit);
	void	onUnitDestroy(BWAPI::Unit unit);
	void	drawProductionInformation(int x, int y);
	void	startExtractorTrick(BWAPI::UnitType type);

	void	queueGasSteal();
	bool	isGasStealInQueue() const;

	bool	nextIsBuilding() const;

	void	goOutOfBook();
	bool	isOutOfBook() const { return _outOfBook; };

	void	setWaitProductionFrame(int wait) { _waitProductionFrame = wait; }

	bool	canMakeUnit(BWAPI::UnitType type, int minerals, int gas, int supply);

	void	queueAsHighestPriority(std::string act);
};


class CompareWhenStarted
{

public:

    CompareWhenStarted() {}

    // For sorting the display of items under construction.
	// Some redundant code removed here thanks to Andrey Kurdiumov.
    bool operator() (BWAPI::Unit u1,BWAPI::Unit u2)
    {
        int startedU1 = u1->getType().buildTime() - u1->getRemainingBuildTime();
        int startedU2 = u2->getType().buildTime() - u2->getRemainingBuildTime();
        return startedU1 > startedU2;
    }
};

}