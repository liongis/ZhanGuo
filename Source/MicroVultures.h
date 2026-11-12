#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroVultures : public MicroManager
{
	int								_lastFrameChange = 0;
	std::map<BWAPI::Unit, int>		_spiderMineUnits;
public:

	MicroVultures();

	void executeMicro(const BWAPI::Unitset & targets);
	void assignTargets(const BWAPI::Unitset & targets);

	int getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target);
	BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);

	bool stayHomeUntilReady(const BWAPI::Unit u) const;

	bool canLaySpiderMine(BWAPI::Unit unit, BWAPI::Position position, int dist, int num = 1, bool random = true);
};
}