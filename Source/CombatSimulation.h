#pragma once

#include "Common.h"
#include "MapGrid.h"

#include "InformationManager.h"

namespace UAlbertaBot
{
class CombatSimulation
{
public:

	CombatSimulation();

	int simulateScores = 0;

	void setCombatUnits(const BWAPI::Position & center, const int radius, bool visibleOnly);
	//计算两个部队资源结果
	int simulateCombat(const BWAPI::Unitset & ourCombatUnits, const BWAPI::Unitset & enemyCombatUnits);
	double simulateCombat();
};
}