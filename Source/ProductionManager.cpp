#include "ProductionManager.h"
#include "GameCommander.h"
#include "StrategyBossZerg.h"
#include "UnitUtil.h"
#include "Random.h"

using namespace UAlbertaBot;

ProductionManager::ProductionManager()
	: _lastProductionFrame				 (0)
	, _assignedWorkerForThisBuilding     (nullptr)
	, _haveLocationForThisBuilding       (false)
	, _delayBuildingPredictionUntilFrame (0)
	, _outOfBook                         (false)
	, _targetGasAmount                   (0)
	, _extractorTrickState			     (ExtractorTrick::None)
	, _extractorTrickUnitType			 (BWAPI::UnitTypes::None)
	, _extractorTrickBuilding			 (nullptr)
{
    setBuildOrder(StrategyManager::Instance().getOpeningBookBuildOrder());
}

void ProductionManager::setBuildOrder(const BuildOrder & buildOrder)
{
	//_queue.clearAll();

	for (size_t i(0); i<buildOrder.size(); ++i)
	{
		_queue.queueAsLowestPriority(buildOrder[i]);
	}
	_queue.resetModified();
}

void ProductionManager::update() 
{
	// TODO move this to worker manager and make it more precise; it often goes a little over
	// If we have reached a target amount of gas, take workers off gas.
	//将此移动到工作管理器, 并使其更精确;它常常有点过了
	//如果我们达到了目标量的气体, 就把工人从煤气中带走。
	if (_targetGasAmount && BWAPI::Broodwar->self()->gatheredGas() >= _targetGasAmount)  // tends to go over
	{
		WorkerManager::Instance().setCollectGas(false);
		_targetGasAmount = 0;           // clear the target
	}

	//if (BWAPI::Broodwar->getFrameCount() < _waitProductionFrame) return;//暂停时间

	// If we're in trouble, adjust the production queue to help.
	// Includes scheduling supply as needed.
	//如果我们遇到麻烦, 请调整生产队列以帮助您。
	//处理紧急生产任务
	StrategyManager::Instance().handleUrgentProductionIssues(_queue);

	// if nothing is currently building, get a new goal from the strategy manager
	if (_queue.isEmpty() || _queue.size() < 6)
	{
		if (Config::Debug::DrawBuildOrderSearchInfo)
		{
			BWAPI::Broodwar->drawTextScreen(150, 10, "Nothing left to build, replanning.");
		}

		goOutOfBook();
		StrategyManager::Instance().freshProductionPlan();//新的生产计划
	}

	// Build stuff from the production queue.
	manageBuildOrderQueue();
}

// If something important was destroyed, we may want to react.
//如果一些重要的东西被破坏了, 我们可能会想做出反应。
void ProductionManager::onUnitDestroy(BWAPI::Unit unit)
{
	// If it's not our unit, we don't care.
	// This mostly works for zerg, but the extractor trick confuses it.
	// So bail out if the extractor trick is underway.
	if (!unit || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		return;
	}

	int frame = BWAPI::Broodwar->getFrameCount();

	if (_selfRace == BWAPI::Races::Protoss) {
		if (unit->getType().isBuilding()) {
			if (unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon) {
				if (unit->getDistance(InformationManager::Instance().getMyUnloadPosition()) < 15 * 32) {
					_queue.queueAsHighestPriority(MacroAct("Photon Cannon", unit->getTilePosition()));
				}
			}

			if (unit->getType() == BWAPI::UnitTypes::Protoss_Pylon) {
				BWAPI::Unitset buildings = unit->getUnitsInRadius(4 * 32, BWAPI::Filter::IsOwned && BWAPI::Filter::IsBuilding);
				for (auto building : buildings) {
					if (!building->isPowered()) {
						_queue.queueAsHighestPriority(MacroAct("Pylon", unit->getTilePosition()));
						break;
					}
				}
			}
		}
	} else if (_selfRace == BWAPI::Races::Terran) {
		if (unit->getType().isBuilding()) {
			if (unit->getType() == BWAPI::UnitTypes::Terran_Missile_Turret && frame < 9 * 60 * 24) {
				_queue.queueAsHighestPriority(MacroAct("Missile Turret", unit->getTilePosition()));
			}

			if (unit->getType() == BWAPI::UnitTypes::Terran_Bunker && frame < 7 * 60 * 24) {
				_queue.queueAsHighestPriority(MacroAct("Bunker", unit->getTilePosition()));
			}
		}
	}
	
	// If it's a worker or a building, it affects the production plan.
	//如果是工人或建筑物, 则影响生产计划。
	if (unit->getType().isWorker() && !_outOfBook)
	{
		// We lost a worker in the opening. Replace it.
		// This helps if a small number of workers are killed. If many are killed, you're toast anyway.
		// Still, it's better than breaking out of the opening altogether.
		_queue.queueAsHighestPriority(unit->getType());

		// If we have a gateway and no zealots, or a barracks and no marines,
		// consider making a military unit first. To, you know, stay alive and stuff.
		if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
		{
			if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Gateway) > 0 &&
				UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Zealot) == 0 &&
				!_queue.anyInNextN(BWAPI::UnitTypes::Protoss_Zealot, 2) &&
				(BWAPI::Broodwar->self()->minerals() >= 150 || UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Protoss_Probe) > 3))
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Protoss_Zealot);
			}
		}
		else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
		{
			if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_Barracks) > 0 &&
				UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Marine) == 0 &&
				!_queue.anyInNextN(BWAPI::UnitTypes::Terran_Marine, 2) &&
				(BWAPI::Broodwar->self()->minerals() >= 100 || UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Terran_SCV) > 3))
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Terran_Marine);
			}
		}
		else // Zerg (unused code due to the condition at the top of the method)
		{
			if (UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 &&
				UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Zergling) == 0 &&
				!_queue.anyInNextN(BWAPI::UnitTypes::Zerg_Zergling, 2) &&
				(BWAPI::Broodwar->self()->minerals() >= 100 || UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Drone) > 3))
			{
				_queue.queueAsHighestPriority(BWAPI::UnitTypes::Zerg_Zergling);
			}
		}
	}
	else if (unit->getType().isBuilding() &&
		!(UnitUtil::CanAttackAir(unit) || UnitUtil::CanAttackGround(unit)) &&
		unit->getType().supplyProvided() == 0)
	{
		// We lost a building other than static defense or supply. It may be serious. Replan from scratch.
		//goOutOfBook();
	}
}

void ProductionManager::manageBuildOrderQueue() 
{
	// If the extractor trick is in progress, do that.
	//如果提取技巧正在进行中, 那么就这样做。
	/*
	if (_extractorTrickState != ExtractorTrick::None)
	{
		doExtractorTrick();
		return;
	}
	*/

	if (BWAPI::Broodwar->getFrameCount() > 3000 && BWAPI::Broodwar->getFrameCount() > _exchangePredictionUntilFrame + 12) {
		maybePermuteQueue();
	}

	// If we were planning to build and assigned a worker, but the queue was then
	// changed behind our back, release the worker and continue.
	//如果我们计划建立和分配一个工人, 但队列是然后
	//在我们后面改变了, 释放了工人, 继续。
	if (_queue.isModified() && _assignedWorkerForThisBuilding)
	{
		WorkerManager::Instance().finishedWithWorker(_assignedWorkerForThisBuilding);
		_assignedWorkerForThisBuilding = nullptr;
	}

	// We do nothing if the queue is empty (obviously).
	//如果队列是空的 (显然), 我们什么也不做。
	while (!_queue.isEmpty()) 
	{
		const BuildOrderItem & currentItem = _queue.getHighestPriorityItem();

		// If this is a command, execute it and keep going.
		//如果这是一个命令, 执行它并继续下去。
		if (currentItem.macroAct.isCommand())
		{
			executeCommand(currentItem.macroAct.getCommandType());
			//如果指定了防守点
			if (currentItem.macroAct.getCommandType().getType() == MacroCommandType::Defensive && 
				currentItem.macroAct.getMacroLocation() == MacroLocation::Absolute &&
				currentItem.macroAct.getTilePosition().getLength() > 0) {

				BWAPI::Position pos(currentItem.macroAct.getTilePosition());
				InformationManager::Instance().setDefensivePosition(pos);
				InformationManager::Instance().setRallyPoint(pos);
				CombatCommander::Instance().defenseNow();
			}

			_queue.doneWithHighestPriorityItem();
			_lastProductionFrame = BWAPI::Broodwar->getFrameCount();
			continue;
		}

		// The unit which can produce the currentItem. May be null.
		//能产生 currentItem 的单位。可能为 null。
        BWAPI::Unit producer = getProducer(currentItem.macroAct);

		// Work around a bug: If you say you want 2 comsats total, BOSS may order up 2 comsats more
		// than you already have. So we drop any extra comsat.
		/*
		if (!producer && currentItem.macroAct.isUnit() && currentItem.macroAct.getUnitType() == BWAPI::UnitTypes::Terran_Comsat_Station) {
			_queue.doneWithHighestPriorityItem();
			_lastProductionFrame = BWAPI::Broodwar->getFrameCount();
			continue;
		}
		*/
		
		// check to see if we can make it right now
		//检查现在是否可以建造
		bool canMake = producer && canMakeNow(producer, currentItem.macroAct);

		// if the next item in the list is a building and we can't yet make it
		//如果列表中的下一个项目是一个建筑物, 我们还不能使它
        if (currentItem.macroAct.isBuilding() &&
			!canMake &&
			currentItem.macroAct.whatBuilds().isWorker() &&
			BWAPI::Broodwar->getFrameCount() >= _delayBuildingPredictionUntilFrame)
		{
			// construct a temporary building object
			BWAPI::UnitType unitType = currentItem.macroAct.getUnitType();

			Building b(unitType, getDefaultTilePosition(unitType));
			b.macroLocation = currentItem.macroAct.getMacroLocation();
			
			if (b.macroLocation == MacroLocation::Absolute) {
				b.desiredPosition = currentItem.macroAct.getTilePosition();
				b.finalPosition = currentItem.macroAct.getTilePosition();
			}
			
            b.isGasSteal = currentItem.isGasSteal;

			// set the producer as the closest worker, but do not set its job yet
			producer = WorkerManager::Instance().getBuilder(b, false);

			// predict the worker movement to that building location
			// NOTE If the worker is set moving, this sets flag _movingToThisBuildingLocation = true
			//      so that we don't 
			predictWorkerMovement(b);
			break;
		}

		// if we can make the current item
		if (canMake) 
		{
			// create it
			if (create(producer, currentItem)) {
				_assignedWorkerForThisBuilding = nullptr;
				_haveLocationForThisBuilding = false;
				_delayBuildingPredictionUntilFrame = 0;

				// and remove it from the _queue
				_queue.doneWithHighestPriorityItem();
				_lastProductionFrame = BWAPI::Broodwar->getFrameCount();
			}
			// don't actually loop around in here
			// TODO because we don't keep track of resources used,
			//      we wait until the next frame to build the next thing.
			//      Can cause delays in late game!
			//其实不要在这里循环
			//	TODO, 因为我们不跟踪所使用的资源,
			//	我们等到下一帧再构建下一件事。
			//	可能导致延迟在晚游戏!
			break;
		}
		else {
		}

		// We didn't make anything. Check for a possible production jam.
		// Jams can happen due to bugs, or due to losing prerequisites for planned items.
		//我们什么都没做检查可能的生产堵塞。
		//由于 bug 或由于丢失计划项目的先决条件, 可能会出现卡纸。
		if (BWAPI::Broodwar->getFrameCount() > _lastProductionFrame + Config::Macro::ProductionJamFrameLimit)
		{
			// Looks very like a jam. Clear the queue and hope for better luck next time.
			// BWAPI::Broodwar->printf("breaking a production jam");
			goOutOfBook();
			//修改
			//_queue.removeHighestPriorityItem();
		}

		// TODO not much of a loop, eh? breaks on all branches
		//      only commands and bug workarounds continue to the next item
		break;
	}
}

//获取缺省建造位置
BWAPI::TilePosition ProductionManager::getDefaultTilePosition(BWAPI::UnitType unitType) {
	BWTA::BaseLocation * base = InformationManager::Instance().getMyMainBaseLocation();
	BWAPI::Position basePosition = base->getPosition();
	BWAPI::Position position = basePosition;

	BWAPI::Position myNearestChokepoint = InformationManager::Instance().getMyNearestChokepoint();

	if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Pylon) > 25) {
		base = InformationManager::Instance().getMyNaturalLocation();
		basePosition = base->getPosition();
		position = basePosition;

		BWTA::Chokepoint * c = BWTA::getNearestChokepoint(basePosition);
		myNearestChokepoint = c->getCenter();
	}

	//std::vector<BWAPI::Position> _myRegionVertices = InformationManager::Instance().getMyRegionVertices();

	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran) {
		/*
		if (unitType == BWAPI::UnitTypes::Terran_Missile_Turret && _myRegionVertices.size() > 0) {
			int index = Random::Instance().index(2);
			position = _myRegionVertices.at(index);
		}
		*/

		if (unitType == BWAPI::UnitTypes::Terran_Barracks || unitType == BWAPI::UnitTypes::Terran_Factory) {
			BWTA::Chokepoint * c = BWTA::getNearestChokepoint(basePosition);
			BWAPI::Position first = c->getSides().first;

			//position = c->getCenter();
			double S = sqrt(pow(first.x - basePosition.x, 2) + pow(first.y - basePosition.y, 2));
			position = MapTools::Instance().getDistancePosition(first, position, 0 - S / 2);//中心点
		}
	}

	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss) {
		double S = sqrt(pow(myNearestChokepoint.x - basePosition.x, 2) + pow(myNearestChokepoint.y - basePosition.y, 2));
		position = MapTools::Instance().getDistancePosition(myNearestChokepoint, basePosition, S / 2);//中心点

		if (unitType == BWAPI::UnitTypes::Protoss_Gateway) {
			position = MapTools::Instance().getDistancePosition(basePosition, myNearestChokepoint, unitType.width());

		}
		else if (unitType == BWAPI::UnitTypes::Protoss_Pylon) {
			position = MapTools::Instance().getDistancePosition(basePosition, myNearestChokepoint, unitType.width() + 2 * 32);
		}
		else {
			/*
			myNearestChokepoint = position;
			S = sqrt(pow(myNearestChokepoint.x - basePosition.x, 2) + pow(myNearestChokepoint.y - basePosition.y, 2));
			position = MapTools::Instance().getDistancePosition(basePosition, myNearestChokepoint, S / 2);
			*/
			position = MapTools::Instance().getDistancePosition(basePosition, myNearestChokepoint, unitType.width());
		}
	}

	return BWAPI::TilePosition(position.x / 32, position.y / 32);
}

// If we can't immediately produce the top item in the queue but we can produce a
// later item, we may want to move the later item to the front.
// For now, this happens under very limited conditions.
//如果我们不能立即产生的顶部项目在队列中, 但我们可以产生一个
//以后的项目, 我们可能要把后面的项目移动到前面。
//目前, 这种情况发生在非常有限的条件下。
void ProductionManager::maybePermuteQueue()
{
	if (_queue.size() < 2)
	{
		return;
	}

	// We can reorder the queue if:
	// We are waiting for gas and have excess minerals,
	// and we can move a later no-dependency no-gas item to the front,
	// and we have the minerals to cover both.
	//我们可以重新排序队列, 如果:
	//我们在等待汽油和过剩的矿物质,
	//我们可以将以后不依赖的无气体项目移到前面,
	//我们有矿物质来覆盖这两种物质。
	MacroAct top = _queue.getHighestPriorityItem().macroAct;
	int minerals = getFreeMinerals();
	int gas = getFreeGas();
	int supplys = getFreeSupply();

	int topMineralPrice = top.mineralPrice() ? top.mineralPrice() : 0;
	int topGasPrice = top.gasPrice() ? top.gasPrice() : 0;
	int topSupplyRequired = top.supplyRequired() ? top.supplyRequired() : 0;

	std::vector<int> permute;
	std::vector<int> command;
	
	//修改
	if (top.gasPrice() <= gas && top.mineralPrice() <= minerals && top.supplyRequired() <= supplys)
	{
		//for (int i = _queue.size() - 1; i >= std::max(0, int(_queue.size()) - 6); --i)
		for (int i = _queue.size() - 1; i >= 0; --i)
		{
			const MacroAct & act = _queue[i].macroAct;

			if (act.isCommand()) {
				command.push_back(i);
				continue;
			}

			//if (act.isCommand()) return;

			if (top.getName() == act.getName()) continue;

			if (!hasRequiredUnit(act)) continue;//如果没有必须的建筑，则不参于
			/*
			if (act.isUnit() &&
				act.gasPrice() == 0 &&
				act.mineralPrice() + top.mineralPrice() <= minerals &&
				independentUnitType(act.getUnitType()))
			{
				// BWAPI::Broodwar->printf("permute %d and %d", _queue.size() - 1, i);
				_queue.pullToTop(i);
				return;
			}
			*/
			int actMineralPrice = act.mineralPrice() ? act.mineralPrice() : 0;
			int actGasPrice = act.gasPrice() ? act.gasPrice() : 0;
			int actSupplyRequired = act.supplyRequired() ? act.supplyRequired() : 0;

			topMineralPrice += actMineralPrice;
			topGasPrice += actGasPrice;
			topSupplyRequired += actSupplyRequired;

			if (topMineralPrice <= minerals) {
				if ((actGasPrice == 0 || topGasPrice <= gas) && (actSupplyRequired == 0 || topSupplyRequired <= supplys)) {
					//_queue.pullToTop(i);
					if (!hasRequiredUnit(act)) break;
					permute.push_back(i);
					//break;
				}
			}
		}
	}
	
	if (permute.size() > 0) {
		for (int i = permute.size() - 1; i >= 0; --i) {
		//for (int i = 0; i < permute.size(); ++i) {
			//permute.push_back(permute[i]);
			
			for (auto Iter = command.begin(); Iter != command.end(); Iter++){
				if (*Iter > permute[i]) {
					_queue.pullToTop(*Iter);
					command.erase(Iter);
					break;
				}
			}
			
			_queue.pullToTop(permute[i]);
		}

		_exchangePredictionUntilFrame = BWAPI::Broodwar->getFrameCount();
	}
}

// The unit type has no dependencies: We can always make it if we have the minerals and a worker.
//单位类型没有依存关系: 如果有矿物质和工人, 我们总能做到。
bool ProductionManager::independentUnitType(BWAPI::UnitType type) const
{
	return
		type.supplyProvided() > 0 ||    // includes resource depot and supply unit
		type.isRefinery() ||
		type == BWAPI::UnitTypes::Zerg_Creep_Colony;
}

// May return null if no producer is found.
// NOTE closestTo defaults to BWAPI::Positions::None, meaning we don't care.
//如果未找到任何生成器, 则可能返回 null。
//注意 closestTo 默认为 BWAPI::Positions::None, 意思是我们不在乎。
BWAPI::Unit ProductionManager::getProducer(MacroAct t, BWAPI::Position closestTo)
{
	UAB_ASSERT(!t.isCommand(), "no producer of a command");

    // get the type of unit that builds this
    BWAPI::UnitType producerType = t.whatBuilds();

    // make a set of all candidate producers
	//制作一组所有候选生产者
    BWAPI::Unitset candidateProducers;
    for (const auto unit : BWAPI::Broodwar->self()->getUnits())
    {
        // Reasons that a unit cannot produce the desired type:

		if (producerType != unit->getType()) { continue; }

		// TODO Due to a BWAPI 4.1.2 bug, lair research can't be done in a hive.
		//      Also spire upgrades can't be done in a greater spire.
		//      The bug is fixed in the next version, 4.2.0.
		//      When switching to a fixed version, change the above line to the following:
		// If the producerType is a lair, a hive will do as well.
		// Note: Burrow research in a hatchery can also be done in a lair or hive, but we rarely want to.
		// Ignore the possibility so that we don't accidentally waste lair time.
		//if (!(
		//	producerType == unit->getType() ||
		//	producerType == BWAPI::UnitTypes::Zerg_Lair && unit->getType() == BWAPI::UnitTypes::Zerg_Hive ||
		//  producerType == BWAPI::UnitTypes::Zerg_Spire && unit->getType() == BWAPI::UnitTypes::Zerg_Greater_Spire
		//	))
		//{
		//	continue;
		//}

        if (!unit->isCompleted())  { continue; }
        if (unit->isTraining())    { continue; }
        if (unit->isLifted())      { continue; }
        if (!unit->isPowered())    { continue; }
		if (unit->isUpgrading())   { continue; }
		if (unit->isResearching()) { continue; }

        // if the type is an addon, some special cases
		//如果该类型是一个插件, 某些特殊情况下
        if (t.isUnit() && t.getUnitType().isAddon())
        {
            // Already has an addon, or is otherwise unable to make one.
			if (!unit->canBuildAddon())
            {
                continue;
            }

            // if we just told this unit to build an addon, then it will not be building another one
            // this deals with the frame-delay of telling a unit to build an addon and it actually starting to build
            if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Build_Addon &&
                (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 10)) 
            { 
                continue; 
            }
        }
        
        // if a unit requires an addon and the producer doesn't have one
		// TODO Addons seem a bit erratic. Bugs are likely.
		// TODO What exactly is requiredUnits()? On the face of it, the story is that
		//      this code is for e.g. making tanks, built in a factory which has a machine shop.
		//      Research that requires an addon is done in the addon, a different case.
		//      Apparently wrong for e.g. ghosts, which require an addon not on the producer.
		//如果一个单位需要一个插件, 而生产者没有一个
		//TODO 插件看起来有点古怪。可能会出现 bug。
		//	requiredUnits 到底是什么？从表面上讲, 故事是
		//	这段代码是用来制造坦克的, 它建在一个有机器车间的工厂里。
		//	需要插件的研究是在插件中完成的, 这是另一种情况。
		//	显然是错误的, 例如鬼魂, 这需要一个插件不是生产者。
		if (t.isUnit())
		{
			bool reject = false;   // innocent until proven guilty
			typedef std::pair<BWAPI::UnitType, int> ReqPair;
			for (const ReqPair & pair : t.getUnitType().requiredUnits())
			{
				BWAPI::UnitType requiredType = pair.first;
				if (requiredType.isAddon())
				{
					if (!unit->getAddon() || (unit->getAddon()->getType() != requiredType))
					{
						reject = true;
						break;     // out of inner loop
					}
				}
			}
			if (reject)
			{
				continue;
			}
		}

        // if we haven't rejected it, add it to the set of candidates
		//如果我们没有拒绝, 将其添加到候选组中
        candidateProducers.insert(unit);
    }

	// Trick: If we're producing a worker, choose the producer (command center, nexus,
	// or larva) which is farthest from the main base. That way expansions are preferentially
	// populated with less need to transfer workers.
	if (t.isUnit() && t.getUnitType().isWorker())
	{
		return getFarthestUnitFromPosition(candidateProducers,
			InformationManager::Instance().getMyMainBaseLocation()->getPosition());
	}
	else
	{
		return getClosestUnitToPosition(candidateProducers, closestTo);
	}
}
//获取最接近的单位位置
BWAPI::Unit ProductionManager::getClosestUnitToPosition(const BWAPI::Unitset & units, BWAPI::Position closestTo)
{
    if (units.size() == 0)
    {
        return nullptr;
    }

    // if we don't care where the unit is return the first one we have
    if (closestTo == BWAPI::Positions::None)
    {
        return *(units.begin());
    }

    BWAPI::Unit closestUnit = nullptr;
    int minDist(1000000);

	for (const auto unit : units) 
    {
        UAB_ASSERT(unit != nullptr, "Unit was null");

		int distance = unit->getDistance(closestTo);
		if (distance < minDist) 
        {
			closestUnit = unit;
			minDist = distance;
		}
	}

    return closestUnit;
}
//从位置获取最远的单位
BWAPI::Unit ProductionManager::getFarthestUnitFromPosition(const BWAPI::Unitset & units, BWAPI::Position farthest)
{
	if (units.size() == 0)
	{
		return nullptr;
	}

	// if we don't care where the unit is return the first one we have
	if (farthest == BWAPI::Positions::None)
	{
		return *(units.begin());
	}

	BWAPI::Unit farthestUnit = nullptr;
	int maxDist(-1);

	for (const auto unit : units)
	{
		UAB_ASSERT(unit != nullptr, "Unit was null");

		int distance = unit->getDistance(farthest);
		if (distance > maxDist)
		{
			farthestUnit = unit;
			maxDist = distance;
		}
	}

	return farthestUnit;
}
//获得最接近的幼虫位置
BWAPI::Unit ProductionManager::getClosestLarvaToPosition(BWAPI::Position closestTo)
{
	BWAPI::Unitset larvas;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Larva)
		{
			larvas.insert(unit);
		}
	}

	return getClosestUnitToPosition(larvas, closestTo);
}

// Create a unit or start research.
bool ProductionManager::create(BWAPI::Unit producer, const BuildOrderItem & item)
{
    if (!producer)
    {
		return false;
    }

    MacroAct act = item.macroAct;

	// If it's a terran add-on.
	if (act.isUnit() && act.getUnitType().isAddon())
	{
		return producer->buildAddon(act.getUnitType());
	}
	// If it's a building other than an add-on.
	else if (act.isBuilding()                                    // implies act.isUnit()
		&& !UnitUtil::IsMorphedBuildingType(act.getUnitType()))  // not morphed from another zerg building, not built//不是从另一个虫族建筑中演变而来的
	{
		// Every once in a while, pick a new base as the "main" base to build in.
		if (act.getRace() != BWAPI::Races::Protoss || act.getUnitType() == BWAPI::UnitTypes::Protoss_Pylon)
		{
			InformationManager::Instance().maybeChooseNewMainBase();
		}

		// By default, build in the main base.
		// BuildingManager will override the location if it needs to.
		// Otherwise it will find some spot near desiredLocation.
		// 默认情况下，在主基地建立。
		// BuildingManager将在需要时覆盖该位置。
		// 否则它会发现附近有一些地方。
		BWAPI::TilePosition desiredLocation = getDefaultTilePosition(act.getUnitType());

		//修改：如果有指定位置则返回指定位置
		if (act.getMacroLocation() == MacroLocation::Absolute && act.getTilePosition().x >= 0 && act.getTilePosition().y >= 0) {//act.getTilePosition().getLength() != 0
			desiredLocation = act.getTilePosition();
		}
		
		if (act.getMacroLocation() == MacroLocation::Natural)
		{
			BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
			if (natural)
			{
				desiredLocation = natural->getTilePosition();
			}
		}
		
		BuildingManager::Instance().addBuildingTask(act, desiredLocation, item.isGasSteal);
	}
	// if we're dealing with a non-building unit, or a morphed zerg building
	else if (act.isUnit())
	{
		if (act.getUnitType().getRace() == BWAPI::Races::Zerg)
		{
			// if the race is zerg, morph the unit
			return producer->morph(act.getUnitType());
		}
		else
		{
			// if not, train the unit
			return producer->train(act.getUnitType());
		}
	}
	// if we're dealing with a tech research
	else if (act.isTech())
	{
		if (!BWAPI::Broodwar->self()->isResearching(act.getTechType()) && !BWAPI::Broodwar->self()->hasResearched(act.getTechType())) {
			if (producer->research(act.getTechType())) {
				if (act.getTechType() == BWAPI::TechTypes::Psionic_Storm) {
					InformationManager::Instance().setPsionicStormFrame(BWAPI::Broodwar->getFrameCount() + 1800);
				}
				return true;
			}
		}
	}
	else if (act.isUpgrade())
	{
		if (!BWAPI::Broodwar->self()->isUpgrading(act.getUpgradeType())) {
			return producer->upgrade(act.getUpgradeType());
		}
	}
	else
	{
		UAB_ASSERT(false, "Unknown type");
	}

	return true;
}

bool ProductionManager::canMakeNow(BWAPI::Unit producer, MacroAct t)
{
	UAB_ASSERT(producer != nullptr, "producer was null");

	bool canMake = meetsReservedResources(t);
	if (canMake)
	{
		if (t.isUnit())
		{
			canMake = BWAPI::Broodwar->canMake(t.getUnitType(), producer);
		}
		else if (t.isTech())
		{
			canMake = BWAPI::Broodwar->canResearch(t.getTechType(), producer);
		}
		else if (t.isUpgrade())
		{
			canMake = BWAPI::Broodwar->canUpgrade(t.getUpgradeType(), producer);
		}
		else if (t.isCommand())
		{
			canMake = true;     // no-op
		}
		else
		{
			UAB_ASSERT(false, "Unknown type");
		}
	}

	return canMake;
}

//是否有必须的建筑
bool ProductionManager::hasRequiredUnit(const MacroAct & act){
	BWAPI::UnitType requiredType;
	BWAPI::Player _self = BWAPI::Broodwar->self();
	if (act.isUnit()) {
		typedef std::pair<BWAPI::UnitType, int> ReqPair;
		for (const ReqPair & pair : act.getUnitType().requiredUnits())
		{
			requiredType = pair.first;
			if (act.getUnitType().isAddon()) {
				if (_self->completedUnitCount(requiredType) == _self->allUnitCount(act.getUnitType())) {
					return false;
				}
			}

			if (_self->allUnitCount(requiredType) == 0 && !BuildingManager::Instance().isBeingBuilt(requiredType)) {
				return false;
			}
		}
	}

	if (act.isUpgrade()) {
		BWAPI::UpgradeType upgType = act.getUpgradeType();

		int maxLvl = _self->getMaxUpgradeLevel(upgType);
		int currentLvl = _self->getUpgradeLevel(upgType);
		currentLvl += 1;

		if (_self->isUpgrading(upgType) || currentLvl > maxLvl) {
			return false;
		}

		//有BUG，Terran Infantry Weapons获取不到对应的单位
		requiredType = upgType.whatsRequired(currentLvl);
		if (requiredType == BWAPI::UnitTypes::None) {
			requiredType = upgType.whatUpgrades();
		}
		if (_self->allUnitCount(requiredType) == 0 && !BuildingManager::Instance().isBeingBuilt(requiredType)) {
			return false;
		}
	}

	if (act.isTech()) {
		if (_self->hasResearched(act.getTechType()) || _self->isResearching(act.getTechType())) {
			return false;
		}
		else {
			requiredType = act.getTechType().whatResearches();
			if (_self->allUnitCount(requiredType) == 0 && !BuildingManager::Instance().isBeingBuilt(requiredType)) {
				return false;
			}
		}
	}

	return true;
}

// When the next item in the _queue is a building, this checks to see if we should move to
// its location in preparation for construction. If so, it orders the move.
// This function is here as it needs to access prodction manager's reserved resources info.
//当 _queue 中的下一个项目是建筑物时, 这将检查我们是否应该移动到
//其在筹建中的位置。如果是的话, 它会下令行动。
//此函数在这里, 因为它需要访问生产管理器的保留资源信息。
void ProductionManager::predictWorkerMovement(const Building & b)
{
    if (b.isGasSteal)
    {
        return;
    }

	// get a possible building location for the building
	//为这栋建筑找一个可能的建筑位置。
	if (!_haveLocationForThisBuilding)
	{
		_predictedTilePosition = BuildingManager::Instance().getBuildingLocation(b);
	}

	if (_predictedTilePosition != BWAPI::TilePositions::None)
	{
		_haveLocationForThisBuilding = true;
	}
	else
	{
		// BWAPI::Broodwar->printf("can't place building %s", b.type.getName().c_str());
		// If we can't place the building now, we probably can't place it next frame either.
		// Delay for a while before trying again. We could overstep the time limit.
		//如果我们现在不能安置大楼, 我们可能也不能把它下一帧。
		//延迟一段时间再试一次。我们可以超越时间限制。
		_delayBuildingPredictionUntilFrame = 12 + BWAPI::Broodwar->getFrameCount();
		return;
	}

	int framesUntilDependenciesMet = 0;
	if (_frameWhenDependendenciesMet > 0)
	{
		framesUntilDependenciesMet = _frameWhenDependendenciesMet - BWAPI::Broodwar->getFrameCount();
	}
	else
	{
		// Get the set of unbuilt requirements for the building
		std::set<BWAPI::UnitType> requirements;
		for (auto req : b.type.requiredUnits())
			if (BWAPI::Broodwar->self()->completedUnitCount(req.first) == 0)
				requirements.insert(req.first);

		// If the tile position is unpowered, add a pylon
		if (!BWAPI::Broodwar->hasPower(_predictedTilePosition, b.type))
			requirements.insert(BWAPI::UnitTypes::Protoss_Pylon);

		// Check if we have any
		for (auto req : requirements)
		{
			if (BWAPI::Broodwar->self()->completedUnitCount(req) > 0) continue;
			BWAPI::Unit nextCompletedUnit = UnitUtil::GetNextCompletedBuildingOfType(req);
			if (!nextCompletedUnit)
			{
				// Prerequisite is queued for build, check again in a bit
				_delayBuildingPredictionUntilFrame = 20 + BWAPI::Broodwar->getFrameCount();
				return;
			}

			framesUntilDependenciesMet = std::max(framesUntilDependenciesMet, nextCompletedUnit->getRemainingBuildTime() + 80);
		}

		_frameWhenDependendenciesMet = BWAPI::Broodwar->getFrameCount() + framesUntilDependenciesMet;
	}

	int x1 = _predictedTilePosition.x * 32;
	int y1 = _predictedTilePosition.y * 32;

	// draw a box where the building will be placed
	if (Config::Debug::DrawWorkerInfo)
    {
		int x2 = x1 + (b.type.tileWidth()) * 32;
		int y2 = y1 + (b.type.tileHeight()) * 32;
		BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Blue, false);
    }

	// where we want the worker to walk to
	//我们希望工人步行到的地方
	BWAPI::Position walkToPosition = BWAPI::Position(x1 + (b.type.tileWidth() / 2) * 32, y1 + (b.type.tileHeight() / 2) * 32);
	//BWAPI::Position walkToPosition = BWAPI::Position(x1 + (b.type.tileWidth()) * 32, y1 + (b.type.tileHeight()) * 32);

	// compute how many resources we need to construct this building
	//计算建造这座大楼需要多少资源
	int mineralsRequired				= std::max(0, b.type.mineralPrice() - getFreeMinerals());
	int gasRequired						= std::max(0, b.type.gasPrice() - getFreeGas());

	// get a candidate worker to move to this location
	//让应聘者移动到此位置
	BWAPI::Unit moveWorker				= WorkerManager::Instance().getMoveWorker(walkToPosition);

	//过去需要花的时间
	/*
	int dist = moveWorker->getDistance(walkToPosition) / moveWorker->getType().topSpeed();
	//if (dist > 4 * 32) {
		mineralsRequired = mineralsRequired - dist / 3;
		gasRequired = gasRequired - dist / 6;
	//}
	*/

	BWAPI::UnitType requiredType;

	//修改：如果必须单位还没有建造，则先删除
	if (b.type.isBuilding()) {
		typedef std::pair<BWAPI::UnitType, int> ReqPair;
		for (const ReqPair & pair : b.type.requiredUnits())
		{
			requiredType = pair.first;

			if (BWAPI::Broodwar->self()->completedUnitCount(requiredType) == 0) {
				return;
			}
		}
	}

	// Conditions under which to move the worker: 
	//		- there's a valid worker to move
	//		- we haven't yet assigned a worker to move to this location
	//		- the build position is valid
	//		- we will have the required resources by the time the worker gets there
	//移动工人的条件://-有一个有效的工人移动
	//-我们还没有指定工作人员移动到此位置
	//	- 生成位置有效
	//	- 在工人到达时, 我们将拥有所需的资源。
	if (moveWorker &&
		!_assignedWorkerForThisBuilding &&
		_haveLocationForThisBuilding &&
		(_predictedTilePosition != BWAPI::TilePositions::None) &&
		WorkerManager::Instance().willHaveResources(mineralsRequired, gasRequired, moveWorker->getDistance(walkToPosition)) )
	{
		// we have assigned a worker
		_assignedWorkerForThisBuilding = moveWorker;

		// tell the worker manager to move this worker
		//通知工作人员经理移动此工作人员
		//moveWorker->stop();

		WorkerManager::Instance().setMoveWorker(moveWorker, mineralsRequired, gasRequired, walkToPosition);

		if (b.type.isRefinery()) {
			moveWorker->rightClick(walkToPosition);
			/*
			BWAPI::Unit geyser = BWAPI::Broodwar->getClosestUnit(walkToPosition, BWAPI::Filter::GetType == BWAPI::UnitTypes::Resource_Vespene_Geyser, 10 * 32);
			if (geyser) {
				moveWorker->rightClick(geyser);
			}
			else {
				moveWorker->rightClick(walkToPosition);
			}
			*/
		}
	}
}

int ProductionManager::getFreeMinerals() const
{
	return BWAPI::Broodwar->self()->minerals() - BuildingManager::Instance().getReservedMinerals();
}

int ProductionManager::getFreeGas() const
{
	return BWAPI::Broodwar->self()->gas() - BuildingManager::Instance().getReservedGas();
}

int ProductionManager::getFreeSupply() const
{
	return BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed();
}

void ProductionManager::executeCommand(MacroCommand command)
{
	MacroCommandType cmd = command.getType();

	if (cmd == MacroCommandType::Scout ||
		cmd == MacroCommandType::ScoutIfNeeded||
		cmd == MacroCommandType::ScoutLocation ||
		cmd == MacroCommandType::ScoutOnceOnly ||
		cmd == MacroCommandType::ScoutWhileSafe)
	{
		ScoutManager::Instance().setScoutCommand(cmd);
	}
	else if (cmd == MacroCommandType::ScoutMyLocation)
	{
		ScoutManager::Instance().setMyScoutCommand(cmd);
	}
	else if (cmd == MacroCommandType::StealGas)
	{
		ScoutManager::Instance().setGasSteal();
	}
	else if (cmd == MacroCommandType::StopGas)
	{
		WorkerManager::Instance().setCollectGas(false);
	}
	else if (cmd == MacroCommandType::StartGas)
	{
		WorkerManager::Instance().setCollectGas(true);
	}
	else if (cmd == MacroCommandType::GasUntil)
	{
		WorkerManager::Instance().setCollectGas(true);
		_targetGasAmount = BWAPI::Broodwar->self()->gatheredGas()
			- BWAPI::Broodwar->self()->gas()
			+ command.getAmount();
	}
	else if (cmd == MacroCommandType::ExtractorTrickDrone)
	{
		startExtractorTrick(BWAPI::UnitTypes::Zerg_Drone);
	}
	else if (cmd == MacroCommandType::ExtractorTrickZergling)
	{
		startExtractorTrick(BWAPI::UnitTypes::Zerg_Zergling);
	}
	else if (cmd == MacroCommandType::Aggressive)
	{
		CombatCommander::Instance().setAggression(true);
		CombatCommander::Instance().attackNow();
	}
	else if (cmd == MacroCommandType::Defensive)
	{
		CombatCommander::Instance().setAggression(false);
	}
	else if (cmd == MacroCommandType::Recon)
	{
		CombatCommander::Instance().setReconUnitTypes(command.getUnitType(), command.getAmount());
	}
	else if (cmd == MacroCommandType::Squad)
	{
		CombatCommander::Instance().setDefenseSquad(command.getSquadName(), command.getUnitType(), command.getAmount(), command.getPosition());
	}
	else if (cmd == MacroCommandType::PullWorkers)
	{
		int nWorkers = WorkerManager::Instance().getNumMineralWorkers() + WorkerManager::Instance().getNumGasWorkers();
		//修改 如果为小于0，则为保留多少个农民
		if (command.getAmount() < 0) {
			nWorkers -= command.getAmount();
		}
		else {
			nWorkers = command.getAmount();
		}

		CombatCommander::Instance().pullWorkers(nWorkers);
		//CombatCommander::Instance().pullWorkers(command.getAmount());
	}
	else if (cmd == MacroCommandType::PullWorkersLeaving)
	{
		int nWorkers = WorkerManager::Instance().getNumMineralWorkers() + WorkerManager::Instance().getNumGasWorkers();
		//修改 如果为小于0，则为保留多少个农民
		if (command.getAmount() < 0) {
			nWorkers -= command.getAmount();
		}
		else {
			nWorkers = command.getAmount();
		}

		CombatCommander::Instance().pullWorkers(nWorkers);
	}
	else if (cmd == MacroCommandType::ReleaseWorkers)
	{
		CombatCommander::Instance().releaseWorkers();
	}
	else if (cmd == MacroCommandType::Nonadaptive)
	{
		StrategyBossZerg::Instance().setNonadaptive(true);
	}
	else if (cmd == MacroCommandType::Lift) {
		if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran) {
			UAB_ASSERT(false, "Terran MacroCommand");
			return;
		}

		BuildingManager::Instance().addFlyBuildingTask(command);
		/*
		for (auto unit : BWAPI::Broodwar->self()->getUnits()) {
			if (unit->getType().isBuilding() && unit->getType() == command.getUnitType()) {
				if (unit->isFlying()) {
					Micro::Move(unit, BWAPI::Position(command.getPosition()));
				}
				else {
					unit->lift();
					unit->rightClick(BWAPI::Position(command.getPosition()), true);
				}

				return;
			}
		}
		*/
	}
	else if (cmd == MacroCommandType::Land) {
		if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran) {
			UAB_ASSERT(false, "Terran MacroCommand");
			return;
		}

		BuildingManager::Instance().addFlyBuildingTask(command);
		/*
		for (auto unit : BWAPI::Broodwar->self()->getUnits()) {
			if (unit->getType().isBuilding() && unit->getType() == command.getUnitType()) {
				if (unit->isFlying()) {
					Micro::Land(unit, command.getPosition());
					//unit->rightClick(command.getPosition());
				}
				else {
					unit->lift();
					unit->land(command.getPosition());
				}

				return;
			}
		}
		*/
	}
	else
	{
		UAB_ASSERT(false, "unknown MacroCommand");
	}
}

// Can we afford it, taking into account reserved resources?
//考虑到后备资源, 我们能负担得起吗？
bool ProductionManager::meetsReservedResources(MacroAct act)
{
	return (act.mineralPrice() <= getFreeMinerals()) && (act.gasPrice() <= getFreeGas());
	/*
	if (act.isBuilding()) {
		return (act.mineralPrice() <= getFreeMinerals()) && (act.gasPrice() <= getFreeGas());
	}

	return (act.mineralPrice() <= BWAPI::Broodwar->self()->minerals()) && (act.gasPrice() <= BWAPI::Broodwar->self()->gas());
	*/
}

void ProductionManager::drawProductionInformation(int x, int y)
{
    if (!Config::Debug::DrawProductionInfo)
    {
        return;
    }

	BWAPI::Broodwar->drawTextScreen(x, y - 10, "\x04 minerals: %d, gas: %d", BWAPI::Broodwar->self()->gatheredMinerals(), BWAPI::Broodwar->self()->gatheredGas());

	y += 10;
	if (_extractorTrickState == ExtractorTrick::None)
	{
		if (WorkerManager::Instance().isCollectingGas())
		{
			if (_targetGasAmount)
			{
				BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 gas %d, target %d", BWAPI::Broodwar->self()->gatheredGas(), _targetGasAmount);
			}
			else
			{
				BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 gas %d", BWAPI::Broodwar->self()->gatheredGas());
			}
		}
		else
		{
			BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 gas %d, stopped", BWAPI::Broodwar->self()->gatheredGas());
		}
	}
	else if (_extractorTrickState == ExtractorTrick::Start)
	{
		BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 extractor trick: start");
	}
	else if (_extractorTrickState == ExtractorTrick::ExtractorOrdered)
	{
		BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 extractor trick: extractor ordered");
	}
	else if (_extractorTrickState == ExtractorTrick::UnitOrdered)
	{
		BWAPI::Broodwar->drawTextScreen(x - 10, y, "\x04 extractor trick: unit ordered");
	}
	y += 2;

	// fill prod with each unit which is under construction
	std::vector<BWAPI::Unit> prod;
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");
		//正在构建
		if (unit->isBeingConstructed())
		{
			prod.push_back(unit);
		}
	}
	
	// sort it based on the time it was started
	std::sort(prod.begin(), prod.end(), CompareWhenStarted());

	for (auto & unit : prod) 
    {
		y += 10;

		BWAPI::UnitType t = unit->getType();
        if (t == BWAPI::UnitTypes::Zerg_Egg)
        {
            t = unit->getBuildType();
        }

		BWAPI::Broodwar->drawTextScreen(x, y, " %c%s", green, NiceMacroActName(t.getName()).c_str());
		BWAPI::Broodwar->drawTextScreen(x - 35, y, "%c%6d", green, unit->getRemainingBuildTime());
	}

	_queue.drawQueueInformation(x, y+10, _outOfBook);
}

ProductionManager & ProductionManager::Instance()
{
	static ProductionManager instance;
	return instance;
}

// We're zerg and doing the extractor trick to get an extra drone or pair of zerglings,
// as specified in the argument.
// Set a flag to start the procedure, and handle various error cases.
//我们是虫族, 做提取技巧, 以获得额外的无人机或一对小狗,
//在参数中指定。
//设置标志以启动过程, 并处理各种错误情况。
void ProductionManager::startExtractorTrick(BWAPI::UnitType type)
{
	// Only zerg can do the extractor trick.
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Zerg)
	{
		return;
	}

	// If we're not supply blocked, then we may have lost units earlier.
	// We may or may not have a larva available now, so instead of finding a larva and
	// morphing the unit here, we set a special case extractor trick state to do it
	// when a larva becomes available.
	// We can't queue a unit, because when we return the caller will delete the front queue
	// item--Steamhammer used to do that, but Arrak found the bug.
	if (BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed() >= 2)
	{
		if (_extractorTrickUnitType != BWAPI::UnitTypes::None)
		{
			_extractorTrickState = ExtractorTrick::MakeUnitBypass;
			_extractorTrickUnitType = type;
		}
		return;
	}
	
	// We need a free drone to execute the trick.
	if (WorkerManager::Instance().getNumMineralWorkers() <= 0)
	{
		return;
	}

	// And we need a free geyser to do it on.
	if (BuildingPlacer::Instance().getRefineryPosition() == BWAPI::TilePositions::None)
	{
		return;
	}
	
	_extractorTrickState = ExtractorTrick::Start;
	_extractorTrickUnitType = type;
}

// The extractor trick is in progress. Take the next step, when possible.
// At most one step occurs per frame.
//提取技巧正在进行中。在可能的情况下, 采取下一步。
//每帧最多有一步发生。
void ProductionManager::doExtractorTrick()
{
	if (_extractorTrickState == ExtractorTrick::Start)
	{
		UAB_ASSERT(!_extractorTrickBuilding, "already have an extractor trick building");
		int nDrones = WorkerManager::Instance().getNumMineralWorkers();
		if (nDrones <= 0)
		{
			// Oops, we can't do it without a free drone. Give up.
			_extractorTrickState = ExtractorTrick::None;
		}
		// If there are "many" drones mining, assume we'll get resources to finish the trick.
		// Otherwise wait for the full 100 before we start.
		// NOTE 100 assumes we are making a drone or a pair of zerglings.
		else if (getFreeMinerals() >= 100 || (nDrones >= 6 && getFreeMinerals() >= 76))
		{
			// We also need a larva to make the drone.
			if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Zerg_Larva) > 0)
			{
				BWAPI::TilePosition loc = BWAPI::TilePosition(0, 0);     // this gets ignored
				Building & b = BuildingManager::Instance().addTrackedBuildingTask(MacroAct(BWAPI::UnitTypes::Zerg_Extractor), loc, false);
				_extractorTrickState = ExtractorTrick::ExtractorOrdered;
				_extractorTrickBuilding = &b;    // points into building manager's queue of buildings
			}
		}
	}
	else if (_extractorTrickState == ExtractorTrick::ExtractorOrdered)
	{
		if (_extractorTrickUnitType == BWAPI::UnitTypes::None)
		{
			_extractorTrickState = ExtractorTrick::UnitOrdered;
		}
		else
		{
			int supplyAvail = BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed();
			if (supplyAvail >= 2 &&
				getFreeMinerals() >= _extractorTrickUnitType.mineralPrice() &&
				getFreeGas() >= _extractorTrickUnitType.gasPrice())
			{
				// We can build a unit now: The extractor started, or another unit died somewhere.
				// Well, there is one more condition: We need a larva.
				BWAPI::Unit larva = getClosestLarvaToPosition(BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition()));
				if (larva && _extractorTrickUnitType != BWAPI::UnitTypes::None)
				{
					if (_extractorTrickUnitType == BWAPI::UnitTypes::Zerg_Zergling &&
						UnitUtil::GetCompletedUnitCount(BWAPI::UnitTypes::Zerg_Spawning_Pool) == 0)
					{
						// We want a zergling but don't have the tech.
						// Give up by doing nothing and moving on.
					}
					else
					{
						larva->morph(_extractorTrickUnitType);
					}
					_extractorTrickState = ExtractorTrick::UnitOrdered;
				}
			}
			else if (supplyAvail < -2)
			{
				// Uh oh, we must have lost an overlord or a hatchery. Give up by moving on.
				_extractorTrickState = ExtractorTrick::UnitOrdered;
			}
			else if (WorkerManager::Instance().getNumMineralWorkers() <= 0)
			{
				// Drone massacre, or all drones pulled to fight. Give up by moving on.
				_extractorTrickState = ExtractorTrick::UnitOrdered;
			}
		}
	}
	else if (_extractorTrickState == ExtractorTrick::UnitOrdered)
	{
		UAB_ASSERT(_extractorTrickBuilding, "no extractor to cancel");
		BuildingManager::Instance().cancelBuilding(*_extractorTrickBuilding);
		_extractorTrickState = ExtractorTrick::None;
		_extractorTrickUnitType = BWAPI::UnitTypes::None;
		_extractorTrickBuilding = nullptr;
	}
	else if (_extractorTrickState == ExtractorTrick::MakeUnitBypass)
	{
		// We did the extractor trick when we didn't need to, whether because the opening was
		// miswritten or because units were lost before we got here.
		// This special state lets us construct the unit we want anyway, bypassing the extractor.
		//我们做了提取技巧时, 我们不需要, 是否因为开幕
		//错字还是因为单位在我们来之前就丢了
		//这个特殊的状态让我们构造我们想要的单位, 绕过提取器。
		BWAPI::Unit larva = getClosestLarvaToPosition(BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition()));
		if (larva &&
			getFreeMinerals() >= _extractorTrickUnitType.mineralPrice() &&
			getFreeGas() >= _extractorTrickUnitType.gasPrice())
		{
			larva->morph(_extractorTrickUnitType);
			_extractorTrickState = ExtractorTrick::None;
		}
	}
	else
	{
		UAB_ASSERT(false, "unexpected extractor trick state (possibly None)");
	}
}

void ProductionManager::queueGasSteal()
{
	_queue.queueAsHighestPriority(MacroAct(BWAPI::Broodwar->self()->getRace().getRefinery()), true);
}

// Has a gas steal has been queued?
bool ProductionManager::isGasStealInQueue() const
{
	return _queue.isGasStealInQueue() || BuildingManager::Instance().isGasStealInQueue();
}

// The next item in the queue is a building that requires a worker to construct.
// Addons and morphed buildings (e.g. lair) do not need a worker.
//队列中的下一项是要求工作人员构造的建筑物。
//插件和仿人建筑(如巢穴) 不需要工人。
bool ProductionManager::nextIsBuilding() const
{
	if (_queue.isEmpty())
	{
		return false;
	}

	const MacroAct & next = _queue.getHighestPriorityItem().macroAct;

	return next.isBuilding() &&
		!next.getUnitType().isAddon() &&
		!UnitUtil::IsMorphedBuildingType(next.getUnitType());
}

// We have finished our book line, or are breaking out of it early.
// Clear the queue, set _outOfBook, go aggressive.
void ProductionManager::goOutOfBook()
{
	_queue.clearAll();
	_outOfBook = true;
}

bool ProductionManager::canMakeUnit(BWAPI::UnitType type, int minerals, int gas, int supply) {
	BWAPI::UnitType requiredType;
	BWAPI::Player _self = BWAPI::Broodwar->self();

	/*
	int minerals = _self->minerals();
	int gas = _self->gas();
	int supply = _self->supplyTotal() - _self->supplyUsed();
	*/

	if (minerals < type.mineralPrice()) {
		return false;
	}

	if (type.gasPrice() > 0 && gas < type.gasPrice()) {
		return false;
	}

	if (supply < type.supplyRequired()) {
		return false;
	}

	typedef std::pair<BWAPI::UnitType, int> ReqPair;
	for (const ReqPair & pair : type.requiredUnits())
	{
		requiredType = pair.first;
		if (type.isAddon()) {
			if (_self->completedUnitCount(requiredType) == _self->allUnitCount(type)) {
				return false;
			}
		}

		if (_self->allUnitCount(requiredType) == 0) {
			return false;
		}
	}

	return true;
}

void ProductionManager::queueAsHighestPriority(std::string act) {
	_queue.queueAsHighestPriority(MacroAct(act));
}
