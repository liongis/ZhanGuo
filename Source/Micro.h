#pragma once

#include <Common.h>
#include <BWAPI.h>

namespace UAlbertaBot
{
namespace Micro
{
	void Stop(BWAPI::Unit unit);
	void AttackUnit(BWAPI::Unit attacker, BWAPI::Unit target);
    void AttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
	void Hold(BWAPI::Unit attacker);
	void Patrol(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
	//void Move(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
	void Move(BWAPI::Unit attacker, const BWAPI::Position & targetPosition, bool ignoredAttack = false);
	void RightClick(BWAPI::Unit unit, BWAPI::Unit target);
	void RightClick(BWAPI::Unit unit, BWAPI::Position pos);
	void Land(BWAPI::Unit unit, BWAPI::TilePosition target);
	void Lift(BWAPI::Unit unit);
	bool EmpShockwave(BWAPI::Unit unit, BWAPI::Position position);
    void LaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos);
    void Repair(BWAPI::Unit unit, BWAPI::Unit target);
	bool Scan(BWAPI::Unit unit, BWAPI::Position targetPosition);
	bool Stim(BWAPI::Unit unit);
	bool Irradiate(BWAPI::Unit unit, BWAPI::Unit target);
	bool DefensiveMatrix(BWAPI::Unit unit, BWAPI::Unit target);
	bool MergeArchon(BWAPI::Unit templar1, BWAPI::Unit templar2);
	void ReturnCargo(BWAPI::Unit worker);
	void KiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target);
    void MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target);
	void MoveAttackLurker(BWAPI::Unit muta, BWAPI::Unit target);

    BWAPI::Position GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target);

    void Rotate(double &x, double &y, double angle);
    void Normalize(double &x, double &y);

	BWAPI::Position GetFleePosition(BWAPI::Position ranged, BWAPI::Position target, double range);
};
}