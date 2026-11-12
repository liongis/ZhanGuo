#include "Squad.h"

#include "ScoutManager.h"
#include "UnitUtil.h"
#include "CombatCommander.h"

using namespace UAlbertaBot;

Squad::Squad()
	: _name("Default")
	, _combatSquad(false)
	, _combatSimRadius(Config::Micro::CombatSimRadius)
	, _fightVisibleOnly(false)
	, _hasAir(false)
	, _hasGround(false)
	, _hasAntiAir(false)
	, _hasAntiGround(false)
	, _attackAtMax(false)
    , _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(0)
{
    int a = 10;   // only you can prevent linker errors
}

// A "combat" squad is any squad except the Idle squad, which is full of workers
// (and possibly unused units like unassigned overlords).
// The usual work of workers is managed by WorkerManager. If we put workers into
// another squad, we have to notify WorkerManager.
//"战斗 " 小队是任何球队, 除了闲置的阵容, 这是充满工人
//(可能未使用的单位, 如未分配的领主)。
//工人通常的工作由 WorkerManager 管理。如果我们把工人们
//另一小队我们得通知 WorkerManager
Squad::Squad(const std::string & name, SquadOrder order, size_t priority)
	: _name(name)
	, _combatSquad(name != "Idle")
	, _combatSimRadius(Config::Micro::CombatSimRadius)
	, _fightVisibleOnly(false)
	, _hasAir(false)
	, _hasGround(false)
	, _hasAntiAir(false)
	, _hasAntiGround(false)
	, _attackAtMax(false)
	, _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(priority)
	, _order(order)
{
}

Squad::~Squad()
{
    clear();
}

// TODO make a proper dispatch system for different orders
void Squad::update()
{
	// update all necessary unit information within this squad
	updateUnits();

	if (_units.empty())
	{
		return;
	}
	
	/*
	if (_order.getType() == SquadOrderTypes::Load)
	{
		loadTransport();
		return;
	}

	if (_order.getType() == SquadOrderTypes::Drop)
	{
		_microTransports.update();
		// And fall through to let the rest of the drop squad attack.
	}
	*/

	bool needToRegroup = needsToRegroup();//需要重新部署
	//bool needToRegroup = false;

	if (Config::Debug::DrawSquadInfo && _order.isRegroupableOrder()) 
	{
		BWAPI::Broodwar->drawTextScreen(200, 350, "%c%s", white, _regroupStatus.c_str());
	}

	if (needToRegroup)
	{
		// Regroup, aka retreat. Only fighting units care about regrouping.
		BWAPI::Position regroupPosition = calcRegroupPosition();

		InformationManager::Instance().setDefensivePosition(regroupPosition);
		//CombatCommander::Instance().defenseNow();

        if (Config::Debug::DrawCombatSimulationInfo)
        {
		    BWAPI::Broodwar->drawTextScreen(200, 150, "REGROUP");
        }

		if (Config::Debug::DrawSquadInfo)
		{
			BWAPI::Broodwar->drawCircleMap(regroupPosition.x, regroupPosition.y, 10, BWAPI::Colors::Purple, true);
		}

		BWAPI::Unitset enemyNear;

		MapGrid::Instance().getUnits(enemyNear, regroupPosition, 12 * 32, false, true);

		if (enemyNear.size() == 0) {
			_microAirToAir.regroup(regroupPosition);
			_microMelee.regroup(regroupPosition);
			_microDarkTemplar.regroup(regroupPosition);
			_microRanged.regroup(regroupPosition);
			_microMarines.regroup(regroupPosition);
			//_microLurkers.regroup(regroupPosition);    // does more harm than good
			_microVultures.regroup(regroupPosition);
			//_microScienceVessels.regroup(regroupPosition);
			_microDragoon.regroup(regroupPosition);
			_microWraith.regroup(regroupPosition);
			_microTanks.regroup(regroupPosition);
			_microCarriers.regroup(regroupPosition);
		}
		else {
			//如果集结点有敌人，则攻击
			// No need to regroup. Execute micro.
			_microAirToAir.execute(_order);
			_microMelee.execute(_order);
			_microDarkTemplar.execute(_order);
			_microRanged.execute(_order);
			_microMarines.execute(_order);
			_microVultures.execute(_order);
			//_microScienceVessels.execute(_order);
			//_microWorkers.execute(_order);
			_microDragoon.execute(_order);
			_microWraith.execute(_order);
			_microTanks.execute(_order);
			_microCarriers.execute(_order);
		}
	}
	else
	{
		// No need to regroup. Execute micro.
		_microAirToAir.execute(_order);
		_microMelee.execute(_order);
		_microDarkTemplar.execute(_order);
		_microRanged.execute(_order);
		_microMarines.execute(_order);
		_microVultures.execute(_order);
		//_microScienceVessels.execute(_order);
		//_microWorkers.execute(_order);
		_microDragoon.execute(_order);
		_microWraith.execute(_order);
		_microTanks.execute(_order);
		_microCarriers.execute(_order);
	}

	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss) {
		_microTransports.update();
	}

	// Lurkers never regroup, always execute their order.
	// TODO It is because regrouping works poorly. It retreats and unburrows them too often.
	
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) {
		_microLurkers.execute(_order);
	}

	// Maybe stim marines and firebats.
	//机枪兵或火焰兵打兴奋剂
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran) {
		stimIfNeeded();
	}

	// The remaining non-combat micro managers try to keep units near the front line.
	if (BWAPI::Broodwar->getFrameCount() % 12 == 0)    // deliberately lag a little behind reality
	{
		BWAPI::Unit vanguard = unitClosestToEnemy();

		// Medics.
		BWAPI::Position medicGoal = vanguard && vanguard->getPosition().isValid() ? vanguard->getPosition() : calcCenter();
		if (_name == "Ground") {
			InformationManager::Instance().setAttackPosition(medicGoal);
		}

		_microMedics.update(medicGoal);
		_microWorkers.update(medicGoal);
		//_microScienceVessels.execute(_order);
		_microScienceVessels.setUnitClosestToEnemy(vanguard);
		_microScienceVessels.execute(_order);

		if (_order.isDropOrder()) {
			_microHighTemplar.update(_order.getPosition());
		}
		else {
			_microHighTemplar.update(medicGoal);
		}

		//_microTransports.setTo(medicGoal);
		//_microTransports.update();

		// Detectors.
		_microDetectors.setUnitClosestToEnemy(vanguard);
		_microDetectors.execute(_order);
	}
}

bool Squad::isEmpty() const
{
    return _units.empty();
}

size_t Squad::getPriority() const
{
    return _priority;
}

void Squad::setPriority(const size_t & priority)
{
    _priority = priority;
}

void Squad::updateUnits()
{
	setAllUnits();
	setNearEnemyUnits();
	addUnitsToMicroManagers();
}

// Clean up the _units vector.
// Also notice and remember a few facts about the members of the squad.
// Note: Some units may be loaded in a bunker or transport and cannot accept orders.
//       Check unit->isLoaded() before issuing orders.
void Squad::setAllUnits()
{
	_hasAir = false;
	_hasGround = false;
	_hasAntiAir = false;
	_hasAntiGround = false;
	_hasTank = false;

	BWAPI::Unitset goodUnits;
	for (const auto unit : _units)
	{
		if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode || unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
			_hasTank = true;
		}

		if (UnitUtil::IsValidUnit(unit))
		{
			goodUnits.insert(unit);

			if (unit->isFlying())
			{
				if (!unit->getType().isDetector())    // mobile detectors don't count
				{
					_hasAir = true;
				}
			}
			else
			{
				_hasGround = true;
			}
			if (UnitUtil::CanAttackAir(unit))
			{
				_hasAntiAir = true;
			}
			if (UnitUtil::CanAttackGround(unit))
			{
				_hasAntiGround = true;
			}
		}
	}
	_units = goodUnits;
}

void Squad::setNearEnemyUnits()
{
	_nearEnemy.clear();

	for (const auto unit : _units)
	{
		if (!unit->getPosition().isValid())   // excludes loaded units
		{
			continue;
		}

		_nearEnemy[unit] = unitNearEnemy(unit);

		if (Config::Debug::DrawSquadInfo) {
			int left = unit->getType().dimensionLeft();
			int right = unit->getType().dimensionRight();
			int top = unit->getType().dimensionUp();
			int bottom = unit->getType().dimensionDown();

			int x = unit->getPosition().x;
			int y = unit->getPosition().y;

			BWAPI::Broodwar->drawBoxMap(x - left, y - top, x + right, y + bottom,
				(_nearEnemy[unit]) ? Config::Debug::ColorUnitNearEnemy : Config::Debug::ColorUnitNotNearEnemy);
		}
	}
}

void Squad::addUnitsToMicroManagers()
{
	BWAPI::Unitset airToAirUnits;
	BWAPI::Unitset meleeUnits;
	BWAPI::Unitset darkTemplarUnits;
	BWAPI::Unitset workerUnits;
	BWAPI::Unitset rangedUnits;
	BWAPI::Unitset carrierUnits;
	BWAPI::Unitset detectorUnits;
	BWAPI::Unitset highTemplarUnits;
	BWAPI::Unitset transportUnits;
	BWAPI::Unitset lurkerUnits;
    BWAPI::Unitset tankUnits;
	BWAPI::Unitset vultureUnits;
	BWAPI::Unitset marineUnits;
    BWAPI::Unitset medicUnits;
	BWAPI::Unitset scienceVesselUnits;
	BWAPI::Unitset dragoonUnits;
	BWAPI::Unitset wraithUnits;

	for (const auto unit : _units)
	{
		BWAPI::UnitType unitType = unit->getType();
		if (unit->isCompleted() && unit->exists() && unit->getHitPoints() > 0 && !unit->isLoaded() && 
			unit->getType() != BWAPI::UnitTypes::Terran_Vulture_Spider_Mine &&
			unit->getType() != BWAPI::UnitTypes::Protoss_Interceptor)
		{
			if (unitType == BWAPI::UnitTypes::Terran_Valkyrie ||
				unitType == BWAPI::UnitTypes::Protoss_Corsair ||
				unitType == BWAPI::UnitTypes::Zerg_Devourer)
			{
				airToAirUnits.insert(unit);
			}
			else if (unitType == BWAPI::UnitTypes::Terran_Marine) {
				marineUnits.insert(unit);
			}
			else if (unitType == BWAPI::UnitTypes::Terran_Medic)
            {
                medicUnits.insert(unit);
            }
			else if (unitType == BWAPI::UnitTypes::Terran_Vulture)
			{
				vultureUnits.insert(unit);
			}
			else if (unitType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
				unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
            {
                tankUnits.insert(unit);
			}
			else if (unitType == BWAPI::UnitTypes::Terran_Wraith) {
				wraithUnits.insert(unit);
			}
			else if (unitType == BWAPI::UnitTypes::Terran_Science_Vessel){
				scienceVesselUnits.insert(unit);
				//detectorUnits.insert(unit);
				/*
				if (detectorUnits.size() > 0) {
					scienceVesselUnits.insert(unit);
				}
				else {
					detectorUnits.insert(unit);
				}
				*/
			}
			else  if (unitType == BWAPI::UnitTypes::Protoss_Arbiter)
			{
				detectorUnits.insert(unit);
			}
			else if (unit->getType() == BWAPI::UnitTypes::Protoss_Carrier)
			{
				carrierUnits.insert(unit);
			}
			else if (unitType == BWAPI::UnitTypes::Protoss_High_Templar)
			{
				highTemplarUnits.insert(unit);
			}
			else if (unitType == BWAPI::UnitTypes::Protoss_Dragoon)
			{
				dragoonUnits.insert(unit);
			}
			else if (unitType == BWAPI::UnitTypes::Zerg_Lurker)
			{
				lurkerUnits.insert(unit);
			}
			else if (unitType.isDetector() && !unit->getType().isBuilding())
			{
				detectorUnits.insert(unit);
			}
			// NOTE Excludes overlords as transports (they are also detectors, a confusing case).
			//注意不包括领主作为运输 (他们也是探测器, 一个混淆的案件)。
			else if (unitType == BWAPI::UnitTypes::Protoss_Shuttle ||
				unitType == BWAPI::UnitTypes::Terran_Dropship)
			{
				transportUnits.insert(unit);
			}
			else if (unitType == BWAPI::UnitTypes::Protoss_Dark_Templar) {
				darkTemplarUnits.insert(unit);
			}
			// NOTE Excludes some units: spellcasters, valkyries, corsairs, devourers.
			//注释不包括某些单位: 施法者、武神、海盗、吞食者。
			else if ((unitType.groundWeapon().maxRange() > 32) ||
				unitType == BWAPI::UnitTypes::Zerg_Scourge ||
				unitType == BWAPI::UnitTypes::Protoss_Reaver)
			{
				rangedUnits.insert(unit);
			}
			else if (unitType.isWorker() && _combatSquad)
			{
				// If this is a combat squad, then workers are melee units like any other,
				// but we have to tell WorkerManager about them.
				// If it's not a combat squad, WorkerManager owns them; don't add them to a micromanager.
				//WorkerManager::Instance().setCombatWorker(unit);
				workerUnits.insert(unit);
				//meleeUnits.insert(unit);
			}
			else if (unitType.groundWeapon().maxRange() <= 32)
			{
				meleeUnits.insert(unit);
			}
			// NOTE Some units may fall through and not be assigned.
		}
	}

	_microAirToAir.setUnits(airToAirUnits);
	_microRanged.setUnits(rangedUnits);
	_microCarriers.setUnits(carrierUnits);
	_microDetectors.setUnits(detectorUnits);
	_microHighTemplar.setUnits(highTemplarUnits);
	_microLurkers.setUnits(lurkerUnits);
	_microMarines.setUnits(marineUnits);
	_microMedics.setUnits(medicUnits);
	_microWorkers.setUnits(workerUnits);
	_microScienceVessels.setUnits(scienceVesselUnits);
	_microTransports.setUnits(transportUnits);
	_microDragoon.setUnits(dragoonUnits);
	_microMelee.setUnits(meleeUnits);
	_microDarkTemplar.setUnits(darkTemplarUnits);
	_microWraith.setUnits(wraithUnits);
	_microVultures.setUnits(vultureUnits);
	_microTanks.setUnits(tankUnits);
}

// Calculates whether to regroup, aka retreat. Does combat sim if necessary.
//计算是否重组, 又称撤退。如果需要, 也要打击 sim。
bool Squad::needsToRegroup()
{
	if (_units.empty())
	{
		_regroupStatus = std::string("No attackers available");
		return false;
	}

	// If we are not attacking, never regroup.
	// This includes the Defend and Drop orders (among others).
	/*
	if (!_order.isRegroupableOrder())
	{
		_regroupStatus = std::string("No attack order");
		return false;
	}
	*/

	if (CombatCommander::Instance().getNumCombatUnits() > 60) {
		return false;
	}

	// If we're nearly maxed and have good income or cash, don't retreat.
	//如果我们几乎被透支, 有良好的收入或现金, 不要退缩。
	if (BWAPI::Broodwar->self()->supplyUsed() >= 160 &&
		(BWAPI::Broodwar->self()->minerals() > 600 || WorkerManager::Instance().getNumMineralWorkers() > 40))
	{
		_attackAtMax = true;
	}

	if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) + BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) > 2){
		_attackAtMax = true;
	}

	if (_attackAtMax)
	{
		if (BWAPI::Broodwar->self()->supplyUsed() < 100)
		{
			return true;
		}
		else
		{
			_regroupStatus = std::string("Maxed. Banzai!");//透支.万岁!
			return false;
		}
	}

	BWAPI::Unit unitClosest = unitClosestToEnemy();

	if (!unitClosest)
	{
		_regroupStatus = std::string("No closest unit");//没有最接近的单位
		return false;
	}

	/* - simplification and supposed improvement suggested by bftjoe - remove this section
	std::vector<UnitInfo> enemyCombatUnits;
    const auto & enemyUnitInfo = InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy());

	// if none of our units are in range of any enemy units, don't retreat
	bool anyInRange = false;
    for (const auto & eui : enemyUnitInfo)
    {
		for (const auto u : _units)
        {
			if (!u->exists() || u->isLoaded())
			{
				continue;
			}

			// Max of weapon range and vision range. Vision range is as long or longer, except for tanks.
			// We assume that the tanks may siege, and check the siege range of unsieged tanks.
			if (UnitUtil::CanAttack(eui.second.type, u->getType()))
			{
				int range = 0;     // range of enemy unit
				if (eui.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
					eui.second.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
				{
					range = (BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode).groundWeapon().maxRange() + 128;  // plus safety fudge
				}
				else
				{
					// Sight range is >= weapon range, so we can stop here.
					range = eui.second.type.sightRange();
				}
				range += 128;    // plus some to account for our squad spreading out

				if (range >= u->getDistance(eui.second.lastPosition))
				{
					anyInRange = true;
					break;   // break out of inner loop
				}
			}
        }

		if (anyInRange)
        {
            break;       // break out of outer loop
        }
    }

    if (!anyInRange)
    {
        _regroupStatus = std::string("No enemy units in range");
        return false;
    }
	/* END proposed removal */

	// If we most recently retreated, don't attack again until retreatDuration frames have passed.
	//如果我们最近撤退, 不要再次攻击, 直到 retreatDuration 帧已经通过。
	const int retreatDuration = 2 * 24;
	bool retreat = _lastRetreatSwitchVal && (BWAPI::Broodwar->getFrameCount() - _lastRetreatSwitch < retreatDuration);

	if (!retreat)
	{
		// All other checks are done. Finally do the expensive combat simulation.
		//所有其他检查都已完成。最后做了昂贵的作战模拟。
		CombatSimulation sim;

		sim.setCombatUnits(unitClosest->getPosition(), _combatSimRadius, _fightVisibleOnly);
		double score = sim.simulateCombat();
		//score = 10.0;

		retreat = score < 0;
		_lastRetreatSwitch = BWAPI::Broodwar->getFrameCount();
		_lastRetreatSwitchVal = retreat;
	}
	
	if (retreat)
	{
		_regroupStatus = std::string("Retreat");
	}
	else
	{
		_regroupStatus = std::string("Attack");
	}

	return retreat;
}

void Squad::setSquadOrder(const SquadOrder & so)
{
	if (so.getType() != _order.getType() ||
		so.getPosition() != _order.getPosition())
	{
		//Log().Debug() << "Order for " << _name << " changed to " << so.getCharCode() << " " << BWAPI::TilePosition(so.getPosition());
	}

	_order = so;

	// Pass the order on to all micromanagers.

	_microAirToAir.setOrder(so);
	_microMelee.setOrder(so);
	_microRanged.setOrder(so);
	_microCarriers.setOrder(so);
	_microDetectors.setOrder(so);
	_microDarkTemplar.setOrder(so);
	_microHighTemplar.setOrder(so);
	_microLurkers.setOrder(so);
	_microMedics.setOrder(so);
	_microTanks.setOrder(so);
	_microTransports.setOrder(so);
}

bool Squad::containsUnit(BWAPI::Unit u) const
{
    return _units.contains(u);
}

bool Squad::containsUnitType(BWAPI::UnitType t) const
{
	for (const auto u : _units)
	{
		if (u->getType() == t)
		{
			return true;
		}
	}
	return false;
}

void Squad::clear()
{
	for (const auto unit : _units)
	{
		if (unit->getType().isWorker())
		{
			WorkerManager::Instance().finishedWithWorker(unit);
		}
	}

	_units.clear();
}

bool Squad::unitNearEnemy(BWAPI::Unit unit)
{
	UAB_ASSERT(unit, "missing unit");

	BWAPI::Unitset enemyNear;

	MapGrid::Instance().getUnits(enemyNear, unit->getPosition(), 7 * 32, false, true);

	return enemyNear.size() > 0;
}

BWAPI::Position Squad::calcCenter()
{
    if (_units.empty())
    {
        if (Config::Debug::DrawSquadInfo)
        {
            BWAPI::Broodwar->printf("Squad::calcCenter() of empty squad");
        }
        return BWAPI::Position(0,0);
    }

	BWAPI::Position accum(0,0);
	for (const auto unit : _units)
	{
		if (unit->getPosition().isValid())
		{
			accum += unit->getPosition();
		}
	}
	return BWAPI::Position(accum.x / _units.size(), accum.y / _units.size());
}

//计算重组位置
BWAPI::Position Squad::calcRegroupPosition()
{
	BWAPI::Position regroup(0,0);
	//BWAPI::Position regroup(490, 1160);

	int minDist = 100000;

	// Retreat to the location of the squad unit not near the enemy which is
	// closest to the order position.
	// NOTE May retreat somewhere silly if the chosen unit was newly produced.
	//      Zerg sometimes retreats back and forth through the enemy when new
	//      zerg units are produced in bases on opposite sides.
	//撤退到小队单位的位置不附近的敌人, 这是
	//最接近订单位置。
	//如果选择的单位是新生产的, 请注意可以退到某处愚蠢的地方。
	//	虫族有时通过敌人撤退, 当新的
	//	虫族单位是以反面为基地生产的。
	for (const auto unit : _units)
	{
		// Count combat units only. Bug fix originally thanks to AIL, it's been rewritten a bit since then.
		//仅计数作战单位。Bug 修复最初多亏了烦恼, 从那以后, 它被重写了一点。
		if (!_nearEnemy[unit] &&
			!unit->getType().isDetector() &&
			unit->getType() != BWAPI::UnitTypes::Terran_Medic && 
			unit->getType() != BWAPI::UnitTypes::Terran_Vulture_Spider_Mine &&
			unit->getPosition().isValid())    // excludes loaded units
		{
			int dist = unit->getDistance(_order.getPosition());

			if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
				dist -= 8 * 32;
			}

			if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) {
				dist -= 10 * 32;
			}

			if (unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar) {
				dist -= 10 * 32;
			}

			if (unit->getType() == BWAPI::UnitTypes::Protoss_Reaver) {
				dist -= 20 * 32;
			}

			if (dist < minDist)
			{
				// If the squad has any ground units, don't try to retreat to the position of an air unit
				// which is flying in a place that a ground unit cannot reach.
				//如果小队有任何地面单位, 不要试图撤退到空气单位的位置
				//它在一个地面单位无法到达的地方飞行
				if (!_hasGround || -1 != MapTools::Instance().getGroundTileDistance(unit->getPosition(), _order.getPosition()))
				{
					minDist = dist;
					regroup = unit->getPosition();
				}
			}
		}
	}

	// Failing that, retreat to a base we own.
	//失败, 撤退到我们自己的基地。
	if (regroup == BWAPI::Position(0,0))
	{
		// Retreat to the main base (guaranteed not null, even if the buildings were destroyed).
		//撤退到主要基地 (保证不空, 即使大厦被毁坏了)。
		BWTA::BaseLocation * base = InformationManager::Instance().getMyMainBaseLocation();

		// If the natural has been taken, retreat there instead.
		// 如果自然被采取了, 撤退那里而不是。
		BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
		if (natural && InformationManager::Instance().getBaseOwner(natural) == BWAPI::Broodwar->self())
		{
			base = natural;
		}
		return BWTA::getRegion(base->getTilePosition())->getCenter();
	}

	return regroup;
}

BWAPI::Unit Squad::unitClosestToEnemy()
{
	return unitClosestToEnemy(_order.getPosition());
}

// Return the unit closest to the order position (not actually closest to the enemy).
//返回最接近订单位置的单位 (实际上不离敌人最近)。
BWAPI::Unit Squad::unitClosestToEnemy(BWAPI::Position position)
{
	BWAPI::Unit closest = nullptr;
	int closestDist = 100000;

	UAB_ASSERT(_order.getPosition().isValid(), "bad order position");

	for (const auto unit : _units)
	{
		// Non-combat units should be ignored for this calculation.
		//对于此计算, 应忽略非战斗单元。
		if (unit->getType().isDetector() ||
			!unit->getPosition().isValid() ||       // includes units loaded into bunkers or transports//包括装载到地堡或运输中的单位
			unit->getType() == BWAPI::UnitTypes::Terran_Medic ||
			unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine
			)
		{
			continue;
		}

		if (_order.isCombatOrder() && _hasTank) {
			if (unit->getType() != BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && unit->getType() != BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) continue;
		}

		int dist;
		if (_hasGround)
		{
			// A ground or air-ground squad. Use ground distance.
			// It is -1 if no ground path exists.
			//地面或空地小队。使用地面距离。
			//如果不存在接地路径, 则为 - 1。
			dist = MapTools::Instance().getGroundDistance(unit->getPosition(), position);
		}
		else
		{
			// An all-air squad. Use air distance (which is what unit->getDistance() gives).
			//全空中小队使用空气距离
			dist = unit->getDistance(position);
		}

		if (dist < closestDist && dist != -1)
		{
			closest = unit;
			closestDist = dist;
		}
	}

	return closest;
}

const BWAPI::Unitset & Squad::getUnits() const	
{ 
	return _units; 
} 

const SquadOrder & Squad::getSquadOrder() const			
{ 
	return _order; 
}

void Squad::addUnit(BWAPI::Unit u)
{
	_units.insert(u);
}

void Squad::removeUnit(BWAPI::Unit u)
{
	if (_combatSquad && u->getType().isWorker())
	{
		WorkerManager::Instance().finishedWithWorker(u);
	}
	_units.erase(u);
}

// Remove all workers from the squad, releasing them back to WorkerManager.
void Squad::releaseWorkers()
{
	UAB_ASSERT(_combatSquad, "Idle squad should not release workers");

	for (const auto unit : _units)
	{
		if (unit->getType().isWorker())
		{
			removeUnit(unit);
		}
	}
}

const std::string & Squad::getName() const
{
    return _name;
}

// The drop squad has been given a Load order. Load up the transports for a drop.
// Unlike other code in the drop system, this supports any number of transports, including zero.
// Called once per frame while a Load order is in effect.
//空投小队已经得到了一份装货令。加载传输以放置一滴。
//与放置系统中的其他代码不同, 这支持任意数量的传输, 包括零。
//在加载顺序有效时, 每帧调用一次。
void Squad::loadTransport()
{
	for (const auto trooper : _units)
	{
		// If it's not the transport itself, send it toward the order location,
		// which is set to the transport's initial location.
		if (trooper->exists() && !trooper->isLoaded() && trooper->getType().spaceProvided() == 0)
		{
			Micro::Move(trooper, _order.getPosition());
		}
	}

	for (const auto transport : _microTransports.getUnits())
	{
		if (!transport->exists())
		{
			continue;
		}
		
		for (const auto unit : _units)
		{
			if (transport->getSpaceRemaining() == 0)
			{
				break;
			}

			/*
			if (_microTransports.loadUnits.size() == 4) {
				break;
			}
			*/

			if (transport->load(unit))
			{
				_microTransports.loadUnits[unit] = BWAPI::Broodwar->getFrameCount();
				break;
			}
		}
	}
}

// Stim marines and firebats if possible and appropriate.
// This stims for combat. It doesn't consider stim to travel faster.
// We bypass the micro managers because it simplifies the bookkeeping, but a disadvantage
// is that we don't have access to the target list. Should refactor a little to get that,
// because it can help us figure out how important it is to stim.
//传感海军陆战队和 firebats 如果可能和适当的。
//这兴奋剂的战斗。它不认为传感走得更快。
//我们绕过微经理, 因为它简化了簿记, 但缺点
//是我们无法访问目标列表。应该重构一点来得到它,
//因为它能帮助我们找出传感的重要性。
void Squad::stimIfNeeded()
{
	// Do we have stim?
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran ||
		!BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
	{
		return;
	}

	// Are there enemies nearby that we may want to fight?
	if (_nearEnemy.empty())
	{
		return;
	}

	// So far so good. Time to get into details.

	// Stim can be used more freely if we have medics with lots of energy.
	int totalMedicEnergy = _microMedics.getTotalEnergy();

	// Stim costs 10 HP, which requires 5 energy for a medic to heal.
	const int stimEnergyCost = 5;

	// Firebats first, because they are likely to be right up against the enemy.
	for (const auto firebat : _microMelee.getUnits())
	{
		// Invalid position means the firebat is probably in a bunker or transport.
		if (firebat->getType() != BWAPI::UnitTypes::Terran_Firebat || !firebat->getPosition().isValid())
		{
			continue;
		}
		// Don't overstim and lose too many HP.
		if (firebat->getHitPoints() < 35 || totalMedicEnergy <= 0 && firebat->getHitPoints() < 45)
		{
			continue;
		}

		BWAPI::Unitset nearbyEnemies;
		MapGrid::Instance().getUnits(nearbyEnemies, firebat->getPosition(), 64, false, true);

		// NOTE We don't check whether the enemy is attackable or worth attacking.
		if (!nearbyEnemies.empty())
		{
			Micro::Stim(firebat);
			totalMedicEnergy -= stimEnergyCost;
		}
	}

	// Next marines, treated the same except for range and hit points.
	//下一届海军陆战队, 除射程和命中点外, 一视同仁。
	for (const auto marine : _microMarines.getUnits())
	{
		// Invalid position means the marine is probably in a bunker or transport.
		//无效位置意味着海军陆战队员可能在地堡或运输。
		if (marine->getType() != BWAPI::UnitTypes::Terran_Marine || !marine->getPosition().isValid())
		{
			continue;
		}
		// Don't overstim and lose too many HP.
		//不要 overstim, 失去太多 HP。
		if (marine->getHitPoints() <= 30 || totalMedicEnergy <= 0 && marine->getHitPoints() < 40)
		{
			continue;
		}

		BWAPI::Unitset nearbyEnemies;
		MapGrid::Instance().getUnits(nearbyEnemies, marine->getPosition(), 5 * 32, false, true);

		if (!nearbyEnemies.empty())
		{
			Micro::Stim(marine);
			totalMedicEnergy -= stimEnergyCost;
		}
	}
}

const bool Squad::hasCombatUnits() const
{
	// If the only units we have are detectors, then we have no combat units.
	//如果我们唯一的单位是探测器, 那么我们就没有作战单位了。
	return !(_units.empty() || _units.size() == _microDetectors.getUnits().size());
}

// Is every unit in the squad an overlord hunter (or a detector)?
// An overlord hunter is a fast air unit that is strong against overlords.
//小队中的每个单位都是霸王猎人 (还是探测器)？
//霸王猎人是一个快速的空气单位, 是强大的对抗领主。
const bool Squad::isOverlordHunterSquad() const
{
	if (!hasCombatUnits())
	{
		return false;
	}

	for (const auto unit : _units)
	{
		const BWAPI::UnitType type = unit->getType();
		if (!type.isFlyer())
		{
			return false;
		}
		if (!type.isDetector() &&
			type != BWAPI::UnitTypes::Terran_Wraith &&
			type != BWAPI::UnitTypes::Terran_Valkyrie &&
			type != BWAPI::UnitTypes::Zerg_Mutalisk &&
			type != BWAPI::UnitTypes::Zerg_Scourge &&      // questionable, but the squad may have both
			type != BWAPI::UnitTypes::Protoss_Corsair &&
			type != BWAPI::UnitTypes::Protoss_Scout)
		{
			return false;
		}
	}
	return true;
}

bool Squad::hasMicroManager(const MicroManager* microManager) const
{
	return
		&_microAirToAir == microManager ||
		&_microCarriers == microManager ||
		&_microDetectors == microManager ||
		&_microDarkTemplar == microManager ||
		&_microHighTemplar == microManager ||
		&_microLurkers == microManager ||
		&_microMedics == microManager ||
		&_microMelee == microManager ||
		&_microRanged == microManager ||
		&_microTanks == microManager ||
		&_microTransports == microManager;
}
