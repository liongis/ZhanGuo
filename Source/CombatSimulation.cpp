#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

CombatSimulation::CombatSimulation()
{
}

int CombatSimulation::simulateCombat(const BWAPI::Unitset & ourCombatUnits, const BWAPI::Unitset & enemyCombatUnits)
{
	int ourScores = 0;
	for (auto ourUnit : ourCombatUnits) {
		BWAPI::UnitType type = ourUnit->getType();
		if (type.groundWeapon() == BWAPI::WeaponTypes::None && type.airWeapon() == BWAPI::WeaponTypes::None) continue;
		if (type.maxHitPoints() + type.maxShields() == 0) continue;

		ourScores += (type.mineralPrice() + type.gasPrice() + type.supplyRequired() * 12.5);//矿，气，人口和
		ourScores += (type.groundWeapon().maxRange() * 0.1);//武器
		ourScores += (ourUnit->getHitPoints() + ourUnit->getShields()) * 0.3;//血量

		//ourScores *= ((ourUnit->getHitPoints() + ourUnit->getShields()) / (type.maxHitPoints() + type.maxShields()));//健康度
	}

	int enemyScores = 0;
	for (auto enemyUnit : enemyCombatUnits) {
		BWAPI::UnitType type = enemyUnit->getType();
		if (type.groundWeapon() == BWAPI::WeaponTypes::None && type.airWeapon() == BWAPI::WeaponTypes::None) continue;
		if (type.maxHitPoints() + type.maxShields() == 0) continue;

		enemyScores += type.mineralPrice() + type.gasPrice() + type.supplyRequired() * 12.5;
		enemyScores += (type.groundWeapon().maxRange() * 0.1);
		enemyScores += (enemyUnit->getHitPoints() + enemyUnit->getShields()) * 0.3;//血量

		//enemyScores *= ((enemyUnit->getHitPoints() + enemyUnit->getShields()) / (type.maxHitPoints() + type.maxShields()));
	}

	return ourScores - enemyScores;
}

// sets the starting states based on the combat units within a radius of a given position
// this center will most likely be the position of the forwardmost combat unit we control
//根据给定位置半径内的战斗单位设置起始状态
//这个中心很可能是我们控制的 forwardmost 作战单位的位置
void CombatSimulation::setCombatUnits(const BWAPI::Position & center, int radius, bool visibleOnly)
{
	fap.clearState();

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, 6, BWAPI::Colors::Red, true);
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, radius, BWAPI::Colors::Red);
	}

	// Work around a bug in mutalisks versus spore colony: It believes that any 2 mutalisks
	// can beat a spore. 6 is a better estimate. So for each spore in the fight, we compensate
	// by dropping 5 mutalisks.
	// TODO fix the bug and remove the workaround
	// Compensation only applies when visibleOnly is false.
	int compensatoryMutalisks = 0;

	BWAPI::Unitset ourUnits;
	BWAPI::Unitset enemyUnits;

	// Add enemy units.
	if (visibleOnly)
	{
		// Only units that we can see right now.
		BWAPI::Unitset enemyCombatUnits;
		MapGrid::Instance().getUnits(enemyCombatUnits, center, radius, false, true);
		for (const auto unit : enemyCombatUnits)
		{
			if (unit->getHitPoints() > 0 && UnitUtil::IsCombatSimUnit(unit))
			{
				fap.addIfCombatUnitPlayer2(unit);
				if (!enemyUnits.contains(unit)) {
					enemyUnits.insert(unit);
				}
				if (Config::Debug::DrawCombatSimulationInfo)
				{
					BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 3, BWAPI::Colors::Orange, true);
				}
			}
		}

		// Also static defense that is out of sight.
		std::vector<UnitInfo> enemyStaticDefense;
		for (auto enemy : BWAPI::Broodwar->enemies()) {
			InformationManager::Instance().getNearbyForce(enemyStaticDefense, center, enemy, radius);
		}
		for (const UnitInfo & ui : enemyStaticDefense)
		{
			// If it wasn't completed when we saw it, pessimistically assume that it is now.
			if (ui.type.isBuilding() && 
				ui.lastHealth > 0 &&
				!ui.unit->isVisible() &&
				UnitUtil::IsCombatSimUnit(ui.type))
			{
				fap.addIfCombatUnitPlayer2(ui);
				if (!enemyUnits.contains(ui.unit)) {
					enemyUnits.insert(ui.unit);
				}
				if (Config::Debug::DrawCombatSimulationInfo)
				{
					BWAPI::Broodwar->drawCircleMap(ui.lastPosition, 3, BWAPI::Colors::Orange, true);
				}
			}
		}
	}
	else
	{
		// All known enemy units, according to their most recently seen position.
		// Skip if goneFromLastPosition, which means the last position was seen and the unit wasn't there.
		std::vector<UnitInfo> enemyCombatUnits;
		for (auto enemy : BWAPI::Broodwar->enemies()) {
			InformationManager::Instance().getNearbyForce(enemyCombatUnits, center, enemy, radius);
		}

		for (const UnitInfo & ui : enemyCombatUnits)
		{
			// The check is careful about seen units and assumes that unseen units are completed and powered.
			if (ui.lastHealth > 0 &&
				(ui.unit->exists() || ui.lastPosition.isValid() && !ui.goneFromLastPosition) &&
				(ui.unit->exists() ? UnitUtil::IsCombatSimUnit(ui.unit) : UnitUtil::IsCombatSimUnit(ui.type)))
			{
				fap.addIfCombatUnitPlayer2(ui);
				if (!enemyUnits.contains(ui.unit)) {
					enemyUnits.insert(ui.unit);
				}
				if (ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)
				{
					compensatoryMutalisks += 5;
				}
				if (Config::Debug::DrawCombatSimulationInfo)
				{
					BWAPI::Broodwar->drawCircleMap(ui.lastPosition, 3, BWAPI::Colors::Red, true);
				}
			}
		}
	}

	// Add our units.

	BWAPI::Unitset ourCombatUnits;
	MapGrid::Instance().getUnits(ourCombatUnits, center, radius, true, false);
	for (const auto unit : ourCombatUnits)
	{
		if (UnitUtil::IsCombatSimUnit(unit))
		{
			if (compensatoryMutalisks > 0 && unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
			{
				--compensatoryMutalisks;
				continue;
			}
			fap.addIfCombatUnitPlayer1(unit);
			if (!ourUnits.contains(unit)) {
				ourUnits.insert(unit);
			}
			if (Config::Debug::DrawCombatSimulationInfo)
			{
				BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 3, BWAPI::Colors::Green, true);
			}
		}
	}

	simulateScores = simulateCombat(ourUnits, enemyUnits);
}

//模拟战斗
double CombatSimulation::simulateCombat()
{
	//return double(simulateScores);

	fap.simulate();
	std::pair<int, int> scores = fap.playerScores();

	int score = scores.first - scores.second;

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawTextScreen(150, 200, "%cCombat sim: us %c%d %c- them %c%d %c= %c%d",
			white, orange, scores.first, white, orange, scores.second, white,
			score >= 0 ? green : red, score);
	}

	return double(score);
}
