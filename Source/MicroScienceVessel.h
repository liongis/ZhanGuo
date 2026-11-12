#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
	class MicroManager;

	class MicroScienceVessel : public MicroManager
	{

		// The cloakedUnitMap is unused, but code exists to fill in its values.
		// For each enemy cloaked unit, it keeps a flag "have we assigned a detector to watch it?"
		// No code exists to assign detectors or move them toward their assigned positions.
		std::map<BWAPI::Unit, bool>	cloakedUnitMap;

		BWAPI::Unit unitClosestToEnemy;

	public:

		MicroScienceVessel();
		~MicroScienceVessel() {}

		void setUnitClosestToEnemy(BWAPI::Unit unit) { unitClosestToEnemy = unit; }
		void executeMicro(const BWAPI::Unitset & targets);

		int getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit) const;
		BWAPI::Unit getIrradiateTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);
		BWAPI::Unit getEmpTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);

		BWAPI::Unit closestCloakedUnit(const BWAPI::Unitset & cloakedUnits, BWAPI::Unit detectorUnit);
	};
}