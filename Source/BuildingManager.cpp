#include "Common.h"
#include "BuildingManager.h"
#include "Micro.h"
#include "ProductionManager.h"
#include "ScoutManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

BuildingManager::BuildingManager()
    : _reservedMinerals(0)
    , _reservedGas(0)
{
}

// Called every frame from GameCommander.
void BuildingManager::update()
{
	//检查被分配的工人是否在途中死亡或在建造过程中死亡。
    validateWorkersAndBuildings();          // check to see if assigned workers have died en route or while constructing
	//将工人分配给未分配的建筑，并将其标注为“计划”
    assignWorkersToUnassignedBuildings();   // assign workers to the unassigned buildings and label them 'planned'
	//对于每一个计划中的建筑，如果工人没有建造，就发送命令。
    constructAssignedBuildings();           // for each planned building, if the worker isn't constructing, send the command    
	//检查是否有建筑物已开始建造和更新数据结构。
    checkForStartedConstruction();          // check to see if any buildings have started construction and update data structures
	//如果我们是人族，而建筑正在施工，没有工人，就分配一个新的。
    checkForDeadTerranBuilders();           // if we are terran and a building is under construction without a worker, assign a new one
	//检查是否有建筑物完成和更新数据结构。
    checkForCompletedBuildings();           // check to see if any buildings have completed and update data structures
	//核实保留的矿物和气体被正确计算。
	checkReservedResources();               // verify that reserved minerals and gas are counted correctly

	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran) {
		//检查需要飞的建筑
		checkFlyBuildings();
	}

	drawBuildingData();
}

// The building took too long to start, or we lost too many workers trying to build it.
// If true, the building gets canceled.
//那栋大楼花了太长时间才开工，或者我们损失了太多的工人试图建造它。
//如果真是这样，那大楼就被取消了。
bool BuildingManager::buildingTimedOut(const Building & b) const
{
	return
		BWAPI::Broodwar->getFrameCount() - b.startFrame > 60 * 24 ||
		b.buildersSent > 4;
}

//检查建筑位置
bool BuildingManager::checkBuildingTiles(Building & b)
{
	if (b.builderUnit && !b.type.isRefinery() && !b.buildingUnit && !b.buildCommandGiven) {
		//if (b.buildingUnit && b.buildingUnit->isBeingConstructed()) continue;
		BWAPI::TilePosition position = b.desiredPosition;
		if (b.macroLocation == MacroLocation::Anywhere) {
			position = b.finalPosition;
		}

		//检查是否可以在当前位置创建建筑物
		if (!BWAPI::Broodwar->canBuildHere(position, b.type, b.builderUnit)) {

			//检查是否在水晶范围内
			if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss && BWAPI::Broodwar->getFrameCount() > _buildingCheckFrame) {
				if (!BWAPI::Broodwar->hasPower(position, b.type) && (BWAPI::Broodwar->getUnitsInRadius(BWAPI::Position(position), 6 * 32, BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Pylon)).size() == 0) {
					//检查指定坐标处是否有建筑，优先在指定位置造建筑
					for (auto it = _buildingData.begin(); it != _buildingData.end(); ++it) {
						std::string name = (*it).first;
						Building bPlan = (*it).second;

						if (bPlan.type == BWAPI::UnitTypes::Protoss_Pylon) {
							if (BWAPI::Broodwar->getUnitsOnTile(bPlan.desiredPosition).size() == 0 && position.getDistance(bPlan.desiredPosition) < 6 * 32) {
								
								if (b.builderUnit->getDistance(BWAPI::Position(bPlan.desiredPosition)) < 9 * 32) {
									b.builderUnit->build(BWAPI::UnitTypes::Protoss_Pylon, bPlan.desiredPosition);
								}
								else {
									ProductionManager::Instance().getQueue().queueAsHighestPriority(MacroAct("Pylon", bPlan.desiredPosition));
								}
								_buildingCheckFrame = BWAPI::Broodwar->getFrameCount() + 9 * 24;
								break;
							}
						}
					}
				}
			}

			int top = position.x * 32; //b.type.dimensionUp();//
			int left = position.y * 32; //b.type.dimensionLeft();//
			int bottom = (position.x + b.type.tileWidth()) * 32;
			int right = (position.y + b.type.tileHeight()) * 32;

			BWAPI::Position topLeft(top, left);
			BWAPI::Position bottomRight(bottom, right);
			BWAPI::Unitset buildingUnits;// = BWAPI::Broodwar->getUnitsInRectangle(topLeft, bottomRight);
			MapGrid::Instance().getUnits(buildingUnits, topLeft, bottomRight, true, true);
			//MapGrid::Instance().getUnits(buildingUnits, BWAPI::Position(position), b.type.tileWidth() * 32, true, false);

			for (const auto unit : buildingUnits)
			{
				if (unit->isFlying()) continue;

				BWAPI::UnitType type = unit->getType();

				if (unit->getType().isBuilding()) {
					int maxHitPoints = unit->getType().maxHitPoints();
					int hitPoints = unit->getHitPoints();

					if (hitPoints > 0 && maxHitPoints > 0 && hitPoints > maxHitPoints * 0.3) {
						b.macroLocation = MacroLocation::Anywhere;
						return false;
					}
				}
				else {
					if (unit->getPlayer() == BWAPI::Broodwar->enemy()) {
						Micro::AttackUnit(b.builderUnit, unit);
						continue;
					}
					else if (unit == b.builderUnit) {
						//b.startFrame = BWAPI::Broodwar->getFrameCount();
						continue;
					}
					else if (unit->canMove() && !unit->isMoving()) {
						Micro::Move(unit, BWAPI::Position((left - 4) * 32, (top - 4) * 32));
						/*
						BWAPI::Unit refinery = unit->getClosestUnit(BWAPI::Filter::IsRefinery);
						if (refinery) {
							unit->rightClick(refinery);
						}
						else {
							Micro::Move(unit, BWAPI::Position((left - 4) * 32, (top - 4) * 32));
							//unit->rightClick(BWAPI::Position((left - 2) * 32, (top - 2) * 32));
						}
						*/
						continue;
					}
					else if (unit->isConstructing() || unit->isMoving()) {
						continue;
					}
					else if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine) {
						//b.builderUnit->attack(unit);
						Micro::AttackUnit(b.builderUnit, unit);
						continue;
					}
					else {
						b.macroLocation = MacroLocation::Anywhere;
						return false;
					}
				}
			}
		}
	}

	return true;
}

// The building took too long to start, or we lost too many workers trying to build it.
// If true, the building gets canceled.
//那栋大楼花了太长时间才开工，或者我们损失了太多的工人试图建造它。
//如果真是这样，那大楼就被取消了。
bool BuildingManager::buildingWaitTimedOut(const Building & b) const
{
	int waitTimer = Config::Macro::BuildingWaiting;

	return
		BWAPI::Broodwar->getFrameCount() - b.startFrame > waitTimer * 24;
}

// STEP 1: DO BOOK KEEPING ON BUILDINGS WHICH MAY HAVE DIED OR TIMED OUT
//步骤1:对可能已经死亡或超时的建筑物进行簿记。
void BuildingManager::validateWorkersAndBuildings()
{
    std::vector<Building> toRemove;
    
    // find any buildings which have become obsolete
	for (auto & b : _buildings)
	{
		if (buildingTimedOut(b) &&
			ProductionManager::Instance().isOutOfBook() &&
			(!b.buildingUnit || b.type.getRace() == BWAPI::Races::Terran && !b.builderUnit))
		{
			toRemove.push_back(b);
		}
		else if (b.status == BuildingStatus::UnderConstruction)
		{
			if (!b.buildingUnit ||
				!b.buildingUnit->exists() ||
				b.buildingUnit->getHitPoints() <= 0 ||
				!b.buildingUnit->getType().isBuilding())
			{
				toRemove.push_back(b);
			}
		}
	}

    undoBuildings(toRemove);
}

// STEP 2: ASSIGN WORKERS TO BUILDINGS WITHOUT THEM
// Also places the building.
//第二步:把工人分配到没有他们的建筑物。
//还可以把房子建好。
void BuildingManager::assignWorkersToUnassignedBuildings()
{
	// for each building that doesn't have a builder, assign one

	for (Building & b : _buildings)
	{
		if (b.waitFrame > 0 && BWAPI::Broodwar->getFrameCount() < b.waitFrame) continue;

		bool isRestartBuild = false;

		if (b.status != BuildingStatus::Unassigned)
		{
			continue;
		}

		if (!checkBuildingTiles(b)) {
			if (b.macroLocation == MacroLocation::Anywhere) {
				isRestartBuild = true;
			}
			else {
				continue;
			}
		}

		// BWAPI::Broodwar->printf("Assigning Worker To: %s", b.type.getName().c_str());
		//如果等待时间太长，重新得到位置
		if (isRestartBuild || buildingWaitTimedOut(b)) {
			//b.desiredPosition = BWAPI::TilePosition(0,0);
			b.macroLocation = MacroLocation::Anywhere;

			BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
			b.status = BuildingStatus::Unassigned;
		}

		BWAPI::TilePosition testLocation = getBuildingLocation(b);
		if (!testLocation.isValid())
		{
			b.waitFrame = BWAPI::Broodwar->getFrameCount() + Config::Macro::BuildingWaiting * 24;
			continue;
		}

		b.finalPosition = testLocation;

		setBuilderUnit(b);       // tries to set b.builderUnit
		if (!b.builderUnit || !b.builderUnit->exists())
		{
			continue;
		}

		if (b.macroLocation != MacroLocation::Absolute) {
			++b.buildersSent;    // count workers ever assigned to build it
		}

		// reserve this building's space
		//保留这栋建筑的空间。
		BuildingPlacer::Instance().reserveTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
		b.status = BuildingStatus::Assigned;
		b.startFrame = BWAPI::Broodwar->getFrameCount();
		// BWAPI::Broodwar->printf("assigned and placed building %s", b.type.getName().c_str());
	}
}

// STEP 3: ISSUE CONSTRUCTION ORDERS TO ASSIGNED BUILDINGS AS NEEDED
//第三步:根据需要向指定的建筑物发出建筑订单。
void BuildingManager::constructAssignedBuildings()
{
    for (auto & b : _buildings)
    {
		bool isRestartBuild = false;

        if (b.status != BuildingStatus::Assigned)
        {
            continue;
        }

		if (!checkBuildingTiles(b)) {
			if (b.macroLocation == MacroLocation::Anywhere) {
				isRestartBuild = true;
			}
			else {
				continue;
			}
		}

		int dist = b.builderUnit->getDistance(BWAPI::Position(b.finalPosition));
		
		if (b.builderUnit && dist > 6 * 32) {
			double waitTime = dist / b.builderUnit->getType().topSpeed() + b.type.buildTime();
			if (BWAPI::Broodwar->getFrameCount() > waitTime * 1.5 + b.startFrame) {
				isRestartBuild = true;
			}

			if (dist > 4 * 32 && b.builderUnit->getOrder() == BWAPI::Orders::MiningMinerals || b.builderUnit->getOrder() == BWAPI::Orders::MoveToMinerals) {
				//b.buildCommandGiven = b.builderUnit->build(b.type, b.finalPosition);
				//WorkerManager::Instance().setMoveWorker(b.builderUnit, 1000, 500, BWAPI::Position(b.finalPosition));
				if (BWAPI::Broodwar->getFrameCount() > b.startFrame + 6 * 24) {
					isRestartBuild = true;
				}
				else {
					Micro::Stop(b.builderUnit);
					continue;
				}
			}
		}

		if (!b.builderUnit || isRestartBuild ||
			b.builderUnit->getPlayer() != BWAPI::Broodwar->self() ||
			!b.builderUnit->exists() && b.type != BWAPI::UnitTypes::Zerg_Extractor)
		{
			// NOTE A zerg drone builderUnit no longer exists() after starting an extractor.
			//      Other zerg buildings behave differently.
			//注意:在启动一个提取器后，虫族的无人机构建单元不再存在。
			//其他虫族建筑的行为方式不同。
			releaseBuilderUnit(b);

			b.builderUnit = nullptr;
			b.buildCommandGiven = false;
			b.status = BuildingStatus::Unassigned;
			BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
		}
		else if (!b.builderUnit->isConstructing())
        {
			if (!isBuildingPositionExplored(b))
            {
				// We haven't explored the build position. Go there.
				//我们还没有探索建造的位置。去那里。
				Micro::Move(b.builderUnit, BWAPI::Position(b.finalPosition));
				continue;
            }

			if (dist > 4 * 32 && (b.builderUnit->getOrder() != BWAPI::Orders::Move)) {
				//WorkerManager::Instance().setMoveWorker(b.builderUnit, 0, 0, BWAPI::Position(b.finalPosition));
				if (BWAPI::Broodwar->getFrameCount() > b.startFrame + 6 * 24) {
					releaseBuilderUnit(b);

					b.builderUnit = nullptr;
					b.buildCommandGiven = false;
					b.status = BuildingStatus::Unassigned;
					BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
				}
				else {
					Micro::Move(b.builderUnit, BWAPI::Position(b.finalPosition));
					continue;

				}
			}

			// if this is not the first time we've sent this guy to build this
            // it must be the case that something was in the way
			//如果这不是我们第一次派这个人来做这个。
			//一定是有什么东西挡住了我的路。
			else if (b.buildCommandGiven)
            {
                // tell worker manager the unit we had is not needed now, since we might not be able
                // to get a valid location soon enough
				//告诉工人经理我们现在不需要的单位，因为我们可能不能。
				//尽快找到一个有效的地点。
				b.buildCommandGiven = false;
				if (checkBuildingTiles(b)) {
					b.status = BuildingStatus::Assigned;
					continue;
				}

				if (b.macroLocation == MacroLocation::Anywhere) {
					releaseBuilderUnit(b);
					b.builderUnit = nullptr;

					b.status = BuildingStatus::Unassigned;

					// Unreserve the building location. The building will mark its own location.
					//取消建筑物的位置。这座大楼将标出它自己的位置。
					BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
				}
			}
            else
            {
				// Issue the build order and record whether it succeeded.
				// If the builderUnit is zerg, it changes to !exists() when it builds.
				//发布构建订单，并记录其是否成功。
				//如果builderUnit是zerg，那么它将更改为!
				b.buildCommandGiven = b.builderUnit->build(b.type, b.finalPosition);
				/*
				if (b.builderUnit->getTilePosition().getDistance(b.finalPosition) < 2 * 32) {
					b.buildCommandGiven = b.builderUnit->build(b.type, b.finalPosition);
				}
				*/
           }
		}
		else {
			if (b.builderUnit->getOrderTargetPosition().getDistance(BWAPI::Position(b.finalPosition)) > 6 * 32) {
				//b.status = BuildingStatus::Assigned;
				//releaseBuilderUnit(b);
			}
		}
    }
}

// STEP 4: UPDATE DATA STRUCTURES FOR BUILDINGS STARTING CONSTRUCTION
//第四步:更新建筑的数据结构。
void BuildingManager::checkForStartedConstruction()
{
    // for each building unit which is being constructed
    for (const auto buildingStarted : BWAPI::Broodwar->self()->getUnits())
    {
        // filter out units which aren't buildings under construction
        if (!buildingStarted->getType().isBuilding() || !buildingStarted->isBeingConstructed())
        {
            continue;
        }

        // check all our building status objects to see if we have a match and if we do, update it
        for (auto & b : _buildings)
        {
            if (b.status != BuildingStatus::Assigned)
            {
                continue;
            }
        
            // check if the positions match
            if (b.finalPosition == buildingStarted->getTilePosition())
            {
                // the resources should now be spent, so unreserve them
				//现在应该把这些资源花出去，所以不要保留它们。
                _reservedMinerals -= buildingStarted->getType().mineralPrice();
                _reservedGas      -= buildingStarted->getType().gasPrice();

                // flag it as started and set the buildingUnit
                b.underConstruction = true;
                b.buildingUnit = buildingStarted;

				// The building is started; handle zerg and protoss builders.
				// Terran builders are dealt with after the building finishes.
                if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
                {
					// If we are zerg, the builderUnit no longest exists.
					// If the building later gets canceled, a new drone will "mysteriously" appear.
					b.builderUnit = nullptr;
					// There's no drone to release, but we still want to let the ScoutManager know
					// that the gas steal is accomplished. If it's not a gas steal, this does nothing.
					//没有无人机可以发布，但我们还是想让ScoutManager知道。
					//煤气偷窃已经完成了。如果它不是气体，那就什么也不做。
					releaseBuilderUnit(b);
				}
                else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
                {
					releaseBuilderUnit(b);
					b.builderUnit = nullptr;
                }

                b.status = BuildingStatus::UnderConstruction;

                BuildingPlacer::Instance().freeTiles(b.finalPosition,b.type.tileWidth(),b.type.tileHeight());

                // only one building will match
                continue;
            }
        }
    }
}

// STEP 5: IF THE SCV DIED DURING CONSTRUCTION, ASSIGN A NEW ONE
//步骤5:如果SCV在施工过程中死亡，分配一个新的。
void BuildingManager::checkForDeadTerranBuilders()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	for (auto & b : _buildings)
	{
		if (b.status != BuildingStatus::UnderConstruction)
		{
			continue;
		}

		UAB_ASSERT(b.buildingUnit, "null buildingUnit");

		if (!UnitUtil::IsValidUnit(b.builderUnit))
		{
			b.builderUnit = WorkerManager::Instance().getBuilder(b);
			if (b.builderUnit && b.builderUnit->exists())
			{
				b.builderUnit->rightClick(b.buildingUnit);
			}
		}
	}
}

// STEP 6: CHECK FOR COMPLETED BUILDINGS
// In case of terran gas steal, stop construction a little early,
// so we can cancel the refinery later and recover resources. 
// Zerg and protoss can't do that.
//第六步:检查已完成的建筑物。
//万一发生人族气体的盗窃，请提前停止施工，
//我们以后可以取消精炼厂，恢复资源。
//虫族和神族不能这么做。
void BuildingManager::checkForCompletedBuildings()
{
    std::vector<Building> toRemove;

    // for each of our buildings under construction
    for (auto & b : _buildings)
    {
		if (b.status != BuildingStatus::UnderConstruction)
        {
            continue;
        }

		UAB_ASSERT(b.buildingUnit, "null buildingUnit");

		bool isRemoveCC = true;

		if (b.buildingUnit->getType() == BWAPI::UnitTypes::Terran_Command_Center) {
			isRemoveCC = checkCommandCenter(b);
		}
		/*
		bool isRemoveSF = true;
		if (b.buildingUnit->getType() == BWAPI::UnitTypes::Terran_Science_Facility) {
			isRemoveSF = checkScienceFacility(b);
		}
		*/
		if (b.buildingUnit->isCompleted())
		{
			// if we are terran, give the worker back to worker manager
			// Zerg and protoss are handled when the building starts.
			//如果我们是人族, 把工人还给工人经理
			//当建筑开始时, 虫族和神族被处理。

			BWAPI::UnitType type = b.buildingUnit->getType();
			if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
			{
				releaseBuilderUnit(b);
			}
			/*
			if (b.buildingUnit->canSetRallyPosition()) {
				setRallyPoint(InformationManager::Instance().getDefensivePosition());
			}
			*/
			if (b.buildingUnit->canSetRallyPoint() && !b.type.isResourceDepot()) {
				b.buildingUnit->setRallyPoint(InformationManager::Instance().getDefensivePosition());
			}
			
			//如果是基地
			if (type == BWAPI::UnitTypes::Terran_Command_Center) {
				if (isRemoveCC) {
					toRemove.push_back(b);
				}

				//InformationManager::Instance().setDefensivePosition();
				setRallyPoint(InformationManager::Instance().getChokepoint());
			}
			/*
			else if (b.buildingUnit->getType() == BWAPI::UnitTypes::Terran_Science_Facility) {
				if (isRemoveSF) {
					toRemove.push_back(b);
				}
			}
			*/
			else {
				// And we don't want to keep the building record any more.
				//我们不想再保留建筑记录了。
				toRemove.push_back(b);
			}
		}
		else
		{
			// The building, whatever it is, is not completed.
			// If it is a terran gas steal, stop construction early.
			//这座大楼，不管它是什么，都没有完工。
			//如果它是一个人族的气体，那就早点停止施工。
			if (b.isGasSteal &&
				BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran &&
				b.builderUnit &&
				b.builderUnit->canHaltConstruction() &&
				b.buildingUnit->getRemainingBuildTime() < 24)
			{
				b.builderUnit->haltConstruction();
				releaseBuilderUnit(b);

				// Call the building done. It's as finished as we want it to be.
				toRemove.push_back(b);
			}
		}
    }

    removeBuildings(toRemove);
}

void BuildingManager::setRallyPoint(BWAPI::Position position) {
	BWAPI::Unitset myUnits = BWAPI::Broodwar->self()->getUnits();
	for (auto unit : myUnits)
	{
		if (unit->canSetRallyPoint() && !unit->getType().isResourceDepot()) {
			unit->setRallyPoint(position);
		}
	}
}

bool BuildingManager::checkCommandCenter(Building & b) {
	BWAPI::TilePosition targetPosition(0, 0);
	BWAPI::TilePosition finalPosition = b.buildingUnit->getTilePosition();
	double distance = 10000;
	//找到离当前位置最近的基地位置
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (finalPosition.getDistance(base->getTilePosition()) < distance) {
			distance = finalPosition.getDistance(base->getTilePosition());
			targetPosition = base->getTilePosition();
		}
	}

	if (b.buildingUnit->isCompleted()) {
		if (b.buildingUnit->isFlying()) {
			if (b.startFrame < BWAPI::Broodwar->getFrameCount()) {

				int top = targetPosition.x * 32; //b.type.dimensionUp();//
				int left = targetPosition.y * 32; //b.type.dimensionLeft();//
				int bottom = (targetPosition.x + b.type.tileWidth()) * 32;
				int right = (targetPosition.y + b.type.tileHeight()) * 32;

				BWAPI::Position topLeft(top, left);
				BWAPI::Position bottomRight(bottom, right);
				BWAPI::Unitset buildingUnits;// = BWAPI::Broodwar->getUnitsInRectangle(topLeft, bottomRight);
				MapGrid::Instance().getUnits(buildingUnits, topLeft, bottomRight, true, false);
				//MapGrid::Instance().getUnits(buildingUnits, BWAPI::Position(position), b.type.tileWidth() * 32, true, false);

				for (const auto unit : buildingUnits)
				{
					if (unit->isFlying()) continue;

					if (unit->canMove()) {
						unit->rightClick(BWAPI::Position((left - 2) * 32, (top - 2) * 32));
					}
				}

				Micro::Land(b.buildingUnit, targetPosition);
				b.startFrame = BWAPI::Broodwar->getFrameCount() + 20;
				//b.startFrame = BWAPI::Broodwar->getFrameCount() + finalPosition.getDistance(targetPosition) * 32 * 6;
				//toRemove.push_back(b);
			}

			if (abs(finalPosition.x - targetPosition.x) <= 2 && abs(finalPosition.y - targetPosition.y) <= 2) {
				WorkerManager::Instance().setDepotWorker(b.buildingUnit);
			}
		}
		else if (finalPosition.x != targetPosition.x && finalPosition.y != targetPosition.y) {
			if (b.buildingUnit->canLift()) {
				Micro::Lift(b.buildingUnit);
				Micro::Land(b.buildingUnit, targetPosition);
				//b.status = BuildingStatus::Size;
				b.startFrame = BWAPI::Broodwar->getFrameCount() + 20;
			}

		}

		if (finalPosition.x == targetPosition.x && finalPosition.y == targetPosition.y) {
		//if (abs(finalPosition.x - targetPosition.x) <= 2 && abs(finalPosition.y - targetPosition.y) <= 2) {
			WorkerManager::Instance().setDepotWorker(b.buildingUnit);
			return true;
		}
	}
	else {
		//在基地建好前200帧接农民过来采矿
		int remainingBuildTime = b.buildingUnit->getRemainingBuildTime();

		if (b.buildingUnit->getRemainingBuildTime() <= 250 && finalPosition.x == targetPosition.x && finalPosition.y == targetPosition.y) {
			WorkerManager::Instance().setDepotWorker(b.buildingUnit);
		}
	}

	return false;
}

bool BuildingManager::checkScienceFacility(Building & b) {
	BWAPI::TilePosition targetPosition(7, 46);
	BWAPI::TilePosition finalPosition = b.buildingUnit->getTilePosition();

	if (b.buildingUnit->isCompleted()) {
		if (b.buildingUnit->isFlying()) {
			if (b.startFrame < BWAPI::Broodwar->getFrameCount()) {
				//b.buildingUnit->move(BWAPI::Position(targetPosition));
				Micro::Move(b.buildingUnit, BWAPI::Position(targetPosition));
				b.startFrame = BWAPI::Broodwar->getFrameCount() + 100;
				//b.startFrame = BWAPI::Broodwar->getFrameCount() + finalPosition.getDistance(targetPosition) * 32 * 6;
				//toRemove.push_back(b);
			}
		}
		else if (finalPosition.x != targetPosition.x && finalPosition.y != targetPosition.y) {
			if (b.buildingUnit->canLift()) {
				Micro::Lift(b.buildingUnit);
				Micro::Move(b.buildingUnit, BWAPI::Position(targetPosition));
				//b.status = BuildingStatus::Size;
				b.startFrame = BWAPI::Broodwar->getFrameCount() + 100;
			}
		}
	}

	if (finalPosition.x == targetPosition.x && finalPosition.y == targetPosition.y) {
		return true;
	}

	return false;
}

// Error check: A bug in placing hatcheries can cause resources to be reserved and
// never released.
// We correct the values as a workaround, since the underlying bug has not been found.
//错误检查:放置孵卵器的错误会导致资源被保留。
//没有释放。
//我们将这些值作为一个变通方法来修正，因为根本的bug还没有找到。
void BuildingManager::checkReservedResources()
{
	// Check for errors.
	int minerals = 0;
	int gas = 0;

	for (auto & b : _buildings)
	{
		if (b.status == BuildingStatus::Assigned || b.status == BuildingStatus::Unassigned)
		{
			minerals += b.type.mineralPrice();
			gas += b.type.gasPrice();
		}
	}

	if (minerals != _reservedMinerals || gas != _reservedGas)
	{
		BWAPI::Broodwar->printf("reserves wrong: %d %d should be %d %d", _reservedMinerals, _reservedGas, minerals, gas);
		_reservedMinerals = minerals;
		_reservedGas = gas;
	}
}

// Add a new building to be constructed and return it.
//新建一栋建筑，并将其归还。
Building & BuildingManager::addTrackedBuildingTask(const MacroAct & act, BWAPI::TilePosition desiredLocation, bool isGasSteal)
{
	UAB_ASSERT(act.isBuilding(), "trying to build a non-building");

	// TODO debugging
	if (isGasSteal)
	{
		// BWAPI::Broodwar->printf("gas steal into building manager");
	}

	BWAPI::UnitType type = act.getUnitType();

	_reservedMinerals += type.mineralPrice();
	_reservedGas += type.gasPrice();

	Building b(type, desiredLocation);
	b.macroLocation = act.getMacroLocation();
	b.isGasSteal = isGasSteal;
	b.status = BuildingStatus::Unassigned;

	_buildings.push_back(b);      // make a "permanent" copy of the Building object
	return _buildings.back();     // return a reference to the permanent copy
}

// Add a new building to be constructed.
void BuildingManager::addBuildingTask(const MacroAct & act, BWAPI::TilePosition desiredLocation, bool isGasSteal)
{
	(void) addTrackedBuildingTask(act, desiredLocation, isGasSteal);
}

void BuildingManager::addFlyBuildingTask(MacroCommand command)
{
	//UAB_ASSERT(act.isBuilding(), "trying to build a non-building");

	Building b(command);
	b.desiredPosition = command.getPosition();
	b.type = command.getUnitType();
	if (command.getType() == MacroCommandType::Land) {
		b.flyStatus = FlyStatus::Land;
	}
	else {
		b.flyStatus = FlyStatus::Lift;
	}
	
	_flyBuildings.push_back(b);      // make a "permanent" copy of the Building object
}

bool BuildingManager::isBuildingPositionExplored(const Building & b) const
{
    BWAPI::TilePosition tile = b.finalPosition;

    // for each tile where the building will be built
    for (int x=0; x<b.type.tileWidth(); ++x)
    {
        for (int y=0; y<b.type.tileHeight(); ++y)
        {
            if (!BWAPI::Broodwar->isExplored(tile.x + x,tile.y + y))
            {
                return false;
            }
        }
    }

    return true;
}

char BuildingManager::getBuildingWorkerCode(const Building & b) const
{
    return b.builderUnit == nullptr ? 'X' : 'W';
}

void BuildingManager::setBuilderUnit(Building & b)
{
	if (b.isGasSteal)
	{
		// If it's a gas steal, use the scout worker.
		// Even if other workers are close by, they may have different jobs.
		//如果是煤气被盗，请使用侦察员。
		//即使其他工人在附近，他们也可能有不同的工作。
		b.builderUnit = ScoutManager::Instance().getWorkerScout();
	}
	else
	{
		// Otherwise, grab the closest worker from WorkerManager.
		//否则，从WorkerManager那里抓最接近的工人。
		b.builderUnit = WorkerManager::Instance().getBuilder(b);
	}
}

// Notify the worker manager that the worker is free again,
// but not if the scout manager owns the worker.
//通知工人经理，工人又自由了，
//但如果童军经理拥有工人，那就不是了。
void BuildingManager::releaseBuilderUnit(const Building & b)
{
	if (b.isGasSteal)
	{
		ScoutManager::Instance().gasStealOver();
	}
	else
	{
		if (b.builderUnit)
		{
			WorkerManager::Instance().finishedWithWorker(b.builderUnit);
		}
	}
}

int BuildingManager::getReservedMinerals() const
{
    return _reservedMinerals;
}

int BuildingManager::getReservedGas() const
{
    return _reservedGas;
}

// In the building queue with any status.
bool BuildingManager::isBeingBuilt(BWAPI::UnitType type) const
{
	for (const auto & b : _buildings)
	{
		if (b.type == type)
		{
			return true;
		}
	}

	return false;
}

int BuildingManager::getNumBeingBuilt(BWAPI::UnitType type) {
	int count = 0;
	for (const auto & b : _buildings)
	{
		if (b.type == type)
		{
			++count;
		}
	}

	return count;
}

// Number in the building queue with status other than "under constrution".
size_t BuildingManager::getNumUnstarted(BWAPI::UnitType type) const
{
	size_t count = 0;

	for (const auto & b : _buildings)
	{
		if (b.type == type && b.status != BuildingStatus::UnderConstruction)
		{
			++count;
		}
	}

	return count;
}

bool BuildingManager::isGasStealInQueue() const
{
	for (const auto & b : _buildings)
	{
		if (b.isGasSteal)
		{
			return true;
		}
	}

	return false;
}

void BuildingManager::addBuildingData(std::string name, std::string unitTypeName, BWAPI::TilePosition postion) {
	Building b;
	for (const BWAPI::UnitType & unitType : BWAPI::UnitTypes::allUnitTypes())
	{
		// check to see if the names match exactly
		std::string typeName = unitType.getName();
		std::replace(typeName.begin(), typeName.end(), '_', ' ');
		std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
		if (typeName == unitTypeName)
		{
			b = Building(unitType, postion);
			b.desiredPosition = postion;
			_buildingData[name] = b;
			return;
		}

		// check to see if the names match without the race prefix
		std::string raceName = unitType.getRace().getName();
		std::transform(raceName.begin(), raceName.end(), raceName.begin(), ::tolower);
		if ((typeName.length() > raceName.length()) && (typeName.compare(raceName.length() + 1, typeName.length(), unitTypeName) == 0))
		{
			b = Building(unitType, postion);
			b.desiredPosition = postion;
			_buildingData[name] = b;
			return;
		}
	}
}

void BuildingManager::drawBuildingData() {
	if (!Config::Debug::DrawBuildingInfo)
	{
		return;
	}

	for (auto it = _buildingData.begin(); it != _buildingData.end(); ++it) {
		std::string name = (*it).first;
		Building b = (*it).second;

		int x1 = b.desiredPosition.x * 32;
		int y1 = b.desiredPosition.y * 32;
		int x2 = (b.desiredPosition.x + b.type.tileWidth()) * 32;
		int y2 = (b.desiredPosition.y + b.type.tileHeight()) * 32;

		int x = x1 + (x2 - x1) / 2;
		int y = y1 + (y2 - y1) / 2;

		BWAPI::Broodwar->drawTextMap(x, y, "\x04 %s", name.c_str());
		BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Green, false);
	}
}

void BuildingManager::drawBuildingInformation(int x, int y)
{
    if (!Config::Debug::DrawBuildingInfo)
    {
        return;
    }

    for (const auto &unit : BWAPI::Broodwar->self()->getUnits())
    {
        BWAPI::Broodwar->drawTextMap(unit->getPosition().x,unit->getPosition().y+5,"\x07%d",unit->getID());
    }

	BWAPI::Broodwar->drawTextScreen(x, y, "\x04 Building Information:");
    BWAPI::Broodwar->drawTextScreen(x,y+20,"\x04 Name");
    BWAPI::Broodwar->drawTextScreen(x+150,y+20,"\x04 State");

    int yspace = 0;

	for (const auto & b : _buildings)
    {
        if (b.status == BuildingStatus::Unassigned)
        {
			int x1 = b.desiredPosition.x * 32;
			int y1 = b.desiredPosition.y * 32;
			int x2 = (b.desiredPosition.x + b.type.tileWidth()) * 32;
			int y2 = (b.desiredPosition.y + b.type.tileHeight()) * 32;

			BWAPI::Broodwar->drawTextScreen(x, y + 40 + ((yspace)* 10), "\x03 %s", NiceMacroActName(b.type.getName()).c_str());
			BWAPI::Broodwar->drawTextScreen(x + 150, y + 40 + ((yspace++) * 10), "\x03 Need %c", getBuildingWorkerCode(b));
			BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Green, false);
        }
        else if (b.status == BuildingStatus::Assigned)
        {
			BWAPI::Broodwar->drawTextScreen(x, y + 40 + ((yspace)* 10), "\x03 %s %d", NiceMacroActName(b.type.getName()).c_str(), b.builderUnit->getID());
            BWAPI::Broodwar->drawTextScreen(x+150,y+40+((yspace++)*10),"\x03 A %c (%d,%d)",getBuildingWorkerCode(b),b.finalPosition.x,b.finalPosition.y);

            int x1 = b.finalPosition.x*32;
            int y1 = b.finalPosition.y*32;
            int x2 = (b.finalPosition.x + b.type.tileWidth())*32;
            int y2 = (b.finalPosition.y + b.type.tileHeight())*32;

            BWAPI::Broodwar->drawLineMap(b.builderUnit->getPosition().x,b.builderUnit->getPosition().y,(x1+x2)/2,(y1+y2)/2,BWAPI::Colors::Orange);
            BWAPI::Broodwar->drawBoxMap(x1,y1,x2,y2,BWAPI::Colors::Red,false);
        }
        else if (b.status == BuildingStatus::UnderConstruction)
        {
            BWAPI::Broodwar->drawTextScreen(x,y+40+((yspace)*10),"\x03 %s %d",NiceMacroActName(b.type.getName()).c_str(),b.buildingUnit->getID());
            BWAPI::Broodwar->drawTextScreen(x+150,y+40+((yspace++)*10),"\x03 Const %c",getBuildingWorkerCode(b));
        }
    }
}

BuildingManager & BuildingManager::Instance()
{
    static BuildingManager instance;
    return instance;
}

// The buildings queued and not yet started.
std::vector<BWAPI::UnitType> BuildingManager::buildingsQueued()
{
    std::vector<BWAPI::UnitType> buildingsQueued;

    for (const auto & b : _buildings)
    {
        if (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned)
        {
            buildingsQueued.push_back(b.type);
        }
    }

    return buildingsQueued;
}

// Cancel a given building when possible.
// Used as part of the extractor trick or in an emergency.
// NOTE CombatCommander::cancelDyingBuildings() can also cancel buildings, including
//      morphing zerg structures which the BuildingManager does not handle.
void BuildingManager::cancelBuilding(Building & b)
{
	if (b.status == BuildingStatus::Unassigned)
	{
		undoBuildings({ b });
	}
	else if (b.status == BuildingStatus::Assigned)
	{
		releaseBuilderUnit(b);
		b.builderUnit = nullptr;
		BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
		undoBuildings({ b });
	}
	else if (b.status == BuildingStatus::UnderConstruction)
	{
		if (b.buildingUnit && b.buildingUnit->exists() && !b.buildingUnit->isCompleted())
		{
			b.buildingUnit->cancelConstruction();
			BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
		}
		undoBuildings({ b });
	}
	else
	{
		UAB_ASSERT(false, "unexpected building status");
	}
}

// It's an emergency. Cancel all buildings which are not yet started.
void BuildingManager::cancelQueuedBuildings()
{
	for (Building & b : _buildings)
	{
		if (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned)
		{
			cancelBuilding(b);
		}
	}
}

// It's an emergency. Cancel all buildings of a given type.
void BuildingManager::cancelBuildingType(BWAPI::UnitType t)
{
	for (Building & b : _buildings)
	{
		if (b.type == t)
		{
			cancelBuilding(b);
		}
	}
}

// TODO fails in placing a hatchery after all others are destroyed - why?
BWAPI::TilePosition BuildingManager::getBuildingLocation(const Building & b)
{
	//修改：按指定位置建造
	if (b.macroLocation == MacroLocation::Absolute && b.desiredPosition.getLength() > 0) {
		return b.desiredPosition;
	}

	if (b.isGasSteal)
    {
        BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();
        UAB_ASSERT(enemyBaseLocation,"Should find enemy base before gas steal");
        UAB_ASSERT(enemyBaseLocation->getGeysers().size() > 0,"Should have spotted an enemy geyser");

        for (const auto geyser : enemyBaseLocation->getGeysers())
        {
			return geyser->getTilePosition();
        }
    }

	if (b.type.isRefinery())
	{
		return BuildingPlacer::Instance().getRefineryPosition();
	}

	if (b.type.isResourceDepot())
	{
		BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
		if (b.macroLocation == MacroLocation::Natural && natural)
		{
			return natural->getTilePosition();
		}

		if (b.macroLocation != MacroLocation::Macro)
		{
			return MapTools::Instance().getNextExpansion(false, true, false);
			//return MapTools::Instance().getNextExpansion(b.macroLocation == MacroLocation::Hidden, true, b.macroLocation != MacroLocation::MinOnly);
		}
		// Else if it's a macro hatchery, treat it like any other building.
	}

	//检查指定坐标处是否有建筑，优先在指定位置造建筑
	for (auto it = _buildingData.begin(); it != _buildingData.end(); ++it) {
		std::string name = (*it).first;
		Building bPlan = (*it).second;

		if (b.type == bPlan.type) {
			if (BWAPI::Broodwar->getUnitsOnTile(bPlan.desiredPosition).size() == 0) {
				return bPlan.desiredPosition;
			}
		}
	}

	int numPylons = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Pylon);
	if (b.type.requiresPsi() && numPylons == 0)
	{
		return BWAPI::TilePositions::None;
	}

    int distance = Config::Macro::BuildingSpacing;
	if (b.type == BWAPI::UnitTypes::Terran_Bunker ||
		b.type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
		b.type == BWAPI::UnitTypes::Zerg_Creep_Colony)
	{
		// Pack defenses tightly together.
		distance = 0;
	}
	else if (b.type == BWAPI::UnitTypes::Protoss_Pylon)
    {
		if (numPylons <= 4)
		{
			// Early pylons may be spaced differently than other buildings.
			distance = Config::Macro::PylonSpacing;
		}
		else if (numPylons <= 10)
		{
			// Building spacing == 1 is usual. Be more generous with pylons.
			distance = 4;
		} if (numPylons > 24) {
			distance = 8;
		}
		else {
			distance = 2;
		}
	}

	// Try to pack protoss buildings more closely together. Space can run out.
	//试着把神族的建筑更紧密地结合起来。空间可以耗尽。
	bool noVerticalSpacing = false;

	if (
		b.type == BWAPI::UnitTypes::Protoss_Forge || 
		b.type == BWAPI::UnitTypes::Protoss_Stargate || 
		b.type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun || 
		b.type == BWAPI::UnitTypes::Protoss_Cybernetics_Core ||
		b.type == BWAPI::UnitTypes::Protoss_Templar_Archives)
	//if (b.type != BWAPI::UnitTypes::Protoss_Pylon)
	{
		distance = 0;
		noVerticalSpacing = true;
	}
	else if (b.type == BWAPI::UnitTypes::Protoss_Gateway) {
		distance = 2;
		noVerticalSpacing = true;
	}

	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran) {
		if (b.type == BWAPI::UnitTypes::Terran_Barracks || b.type == BWAPI::UnitTypes::Factories) {
			distance = Config::Macro::BuildingSpacing;
		}
		else {
			distance = 1;
			noVerticalSpacing = true;
		}
	}
	//修改 如果是绝对定位，则不考虑建筑的间隙
	if (b.macroLocation == MacroLocation::Absolute) {
		distance = 0;
		noVerticalSpacing = true;
	}

	BWAPI::TilePosition buildingLocation = BuildingPlacer::Instance().getBuildLocationNear(b, distance, noVerticalSpacing);
	//标记当前范围为我方范围
	InformationManager::Instance().updateOccupiedRegions(BWTA::getRegion(buildingLocation), BWAPI::Broodwar->self());

	// Get a position within our region.
	return buildingLocation;
}

// The building failed or is canceled.
// Undo any connections with other data structures, then delete.
//这座大楼失败了，或者被取消了。
//撤消与其他数据结构的任何连接，然后删除。
void BuildingManager::undoBuildings(const std::vector<Building> & toRemove)
{
	for (const Building & b : toRemove)
	{
		// If the building was to establish a base, unreserve the base location.
		if (b.type.isResourceDepot() && b.macroLocation != MacroLocation::Macro && b.finalPosition.isValid())
		{
			InformationManager::Instance().unreserveBase(b.finalPosition);
		}

		// If the building is not yet under construction, release its resources.
		if (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned)
		{
			_reservedMinerals -= b.type.mineralPrice();
			_reservedGas -= b.type.gasPrice();
		}

		// Cancel a terran building under construction. Zerg and protoss finish on their own,
		// but terran needs another SCV to be sent, and it won't happen.
		if (b.buildingUnit &&
			b.buildingUnit->getType().getRace() == BWAPI::Races::Terran &&
			b.buildingUnit->exists() &&
			b.buildingUnit->canCancelConstruction())
		{
			b.buildingUnit->cancelConstruction();
		}

		// Release the worker, if necessary.
		releaseBuilderUnit(b);
	}

	removeBuildings(toRemove);
}

void BuildingManager::checkFlyBuildings() {
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran) {
		return;
	}

	std::vector<Building> toRemove;
	int frame = BWAPI::Broodwar->getFrameCount();

	for (auto & b : _flyBuildings)
	{
		if (b.flyStatus != FlyStatus::Lift && b.flyStatus != FlyStatus::Land)
		{
			continue;
		}

		if (frame < b.startFrame) continue;

		if (!b.buildingUnit) {
			b.buildingUnit = getFlyBuildingUnit(b.type, b.desiredPosition);
		}

		if (!b.buildingUnit) continue;

		BWAPI::Position desiredPosition(b.desiredPosition);

		BWAPI::Unit unit = b.buildingUnit;
		int x = unit->getLeft();
		int y = unit->getTop();

		//BWAPI::Broodwar->drawTextScreen(x, y, "\x03 %c", std::to_string(x) + "," + std::to_string(y) + " " + std::to_string(b.desiredPosition.x) + "," + std::to_string(b.desiredPosition.y));

		if (unit->getType().isBuilding() && unit->getType() == b.type) {
			if (!unit->isFlying()) {
				if (b.flyStatus == FlyStatus::Land) {
					if (unit->getPosition().getDistance(desiredPosition) < 3 * 32) {
						toRemove.push_back(b);
						continue;
					}
				}

				Micro::Lift(unit);
				b.startFrame = frame + 3 * 24;
				continue;
			}
			else {
				if (unit->getPosition().getDistance(desiredPosition) > 3 * 32) {
					Micro::Move(unit, BWAPI::Position(b.desiredPosition));
					b.startFrame = frame + 3 * 24;
					continue;
				}
				else {
					if (b.flyStatus == FlyStatus::Land) {
						if (unit->isFlying()) {
							Micro::Land(unit, b.desiredPosition);
							b.startFrame = frame + 3 * 24;
							continue;
						}
						else {
							toRemove.push_back(b);
							continue;
						}
					}
					else {
						toRemove.push_back(b);
						continue;
					}
				}
			}
		}
	}
	
	removeFlyBuildings(toRemove);
}


BWAPI::Unit BuildingManager::getFlyBuildingUnit(BWAPI::UnitType type, BWAPI::TilePosition pos) {
	BWAPI::Unit chooseUnit = nullptr;
	double maxDist = 10000;

	for (auto unit : BWAPI::Broodwar->self()->getUnits()) {
		if (!unit->isCompleted()) continue;

		if (unit->getType().isBuilding() && unit->getType() == type) {
			double dist = pos.getDistance(unit->getTilePosition());
			if (dist < maxDist) {
				chooseUnit = unit;
				maxDist = dist;
			}
		}
	}

	return chooseUnit;
}

// Remove buildings from the list of buildings--nothing more, nothing less.
void BuildingManager::removeBuildings(const std::vector<Building> & toRemove)
{
    for (auto & b : toRemove)
    {
		auto & it = std::find(_buildings.begin(), _buildings.end(), b);

        if (it != _buildings.end())
        {
            _buildings.erase(it);
        }
    }
}


// Remove buildings from the list of buildings--nothing more, nothing less.
void BuildingManager::removeFlyBuildings(const std::vector<Building> & toRemove)
{
	for (auto & b : toRemove)
	{
		auto & it = std::find(_flyBuildings.begin(), _flyBuildings.end(), b);

		if (it != _flyBuildings.end())
		{
			_flyBuildings.erase(it);
		}
	}
}

