#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroRepairs : public MicroManager
{

public:

	MicroRepairs();

	void executeMicro(const BWAPI::Unitset & targets);
	void assignTargets(const BWAPI::Unitset & targets);
	void update(const BWAPI::Position & center);

	int getRepairPriority(BWAPI::Unit attacker, BWAPI::Unit target) const;
	int getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit) const;
	BWAPI::Unit getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);
	bool meleeUnitShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);
};
}