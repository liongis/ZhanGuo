#include "Micro.h"
#include "MapGrid.h"
#include "UnitUtil.h"
#include "MapTools.h"

using namespace UAlbertaBot;

size_t TotalCommands = 0;

const int dotRadius = 2;

void Micro::Stop(BWAPI::Unit unit)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
	{
		return;
	}

	// If we already gave this command, don't repeat it.
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Stop)
	{
		return;
	}

	unit->stop();
	TotalCommands++;
}

void Micro::AttackUnit(BWAPI::Unit attacker, BWAPI::Unit target)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
    {
		UAB_ASSERT(false, "bad arg");
        return;
    }

	// Do nothing if we've already issued a command this frame, or the unit is busy attacking.
	// NOTE A lurker attacking a fixed target is ALWAYS on an attack frame.
	//      According to Arrak, sunken colonies behave the same.
	//如果我们已经发出了这个帧的命令, 或者设备正忙于攻击, 则什么也不做。
	//注意攻击固定目标的潜伏者总是在攻击帧上。
	//	根据 Arrak, 下沉的殖民地的行为是一样的。
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() ||
		(attacker->isAttackFrame() && attacker->getType() != BWAPI::UnitTypes::Zerg_Lurker))
    {
		return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to attack this target, ignore this command
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&	currentCommand.getTarget() == target)
    {
		return;
    }

	if (attacker->isAttackFrame() || attacker->isStartingAttack()) {
		return;
	}
	
    // if nothing prevents it, attack the target
    attacker->attack(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::Red, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Red, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), target->getPosition(), BWAPI::Colors::Red);
    }
}

void Micro::AttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self())
    {
		UAB_ASSERT(false, "bad arg");
		return;
    }

	// if we have issued a command to this unit already this frame, ignore this one
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || attacker->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to attack this target, ignore this command
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Move &&	currentCommand.getTargetPosition() == targetPosition)
	{
		return;
	}

	if (attacker->isAttackFrame() || attacker->isStartingAttack()) {
		return;
	}

	// if nothing prevents it, attack the target
	attacker->attack(targetPosition);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::Orange, true);
        BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::Orange, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::Orange);
    }
}

void Micro::Move(BWAPI::Unit attacker, const BWAPI::Position & targetPosition, bool ignoredAttack)
{
	// -- -- TODO temporary extra debugging to solve 2 bugs
	/*
	if (!attacker->exists())
	{
		UAB_ASSERT(false, "SmartMove: nonexistent");
		BWAPI::Broodwar->printf("SM: non-exist %s @ %d, %d", attacker->getType().getName().c_str(), targetPosition.x, targetPosition.y);
		return;
	}
	if (attacker->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "SmartMove: not my unit");
		return;
	}
	if (!targetPosition.isValid())
	{
		UAB_ASSERT(false, "SmartMove: bad position");
		return;
	}
	// -- -- TODO end
	*/

	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() || !targetPosition.isValid())
	{
		// UAB_ASSERT(false, "bad arg");  // TODO restore this after the bugs are solved; can make too many beeps
		return;
	}

    // if we have issued a command to this unit already this frame, ignore this one
	//如果我们已经发布了这个单元的命令已经这个框架, 忽略这一个
    if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
    {
        return;
    }

	if (!ignoredAttack && attacker->isAttackFrame()) {
		return;
	}

    BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
	//如果我们已经告诉这个单位移动到这个位置, 忽略这个命令
    if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Move) && (currentCommand.getTargetPosition() == targetPosition) && attacker->isMoving())
    {
        return;
    }

    // if nothing prevents it, move the target position
	//如果没有阻止它, 移动目标位置
	attacker->move(targetPosition);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo)
    {
        BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::White, true);
        BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::White);
    }
}

void Micro::Hold(BWAPI::Unit attacker)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self())
	{
		// UAB_ASSERT(false, "bad arg");  // TODO restore this after the bugs are solved; can make too many beeps
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	//如果我们已经发布了这个单元的命令已经这个框架, 忽略这一个
	if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		//return;
	}

	BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

	// if we've already told this unit to move to this position, ignore this command
	//如果我们已经告诉这个单位移动到这个位置, 忽略这个命令
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Hold_Position) && attacker->isHoldingPosition())
	{
		return;
	}

	// if nothing prevents it, move the target position
	//如果没有阻止它, 移动目标位置
	attacker->holdPosition();
	TotalCommands++;
}
//巡逻
void Micro::Patrol(BWAPI::Unit attacker, const BWAPI::Position & targetPosition)
{
	if (!attacker || !attacker->exists() || attacker->getPlayer() != BWAPI::Broodwar->self() || !targetPosition.isValid())
	{
		// UAB_ASSERT(false, "bad arg");  // TODO restore this after the bugs are solved; can make too many beeps
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	//如果我们已经发布了这个单元的命令已经这个框架, 忽略这一个
	if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		//return;
	}

	BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

	// if we've already told this unit to move to this position, ignore this command
	//如果我们已经告诉这个单位移动到这个位置, 忽略这个命令
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Patrol) && (currentCommand.getTargetPosition() == targetPosition) && attacker->isPatrolling())
	{
		return;
	}

	// if nothing prevents it, move the target position
	//如果没有阻止它, 移动目标位置
	attacker->patrol(targetPosition);
	TotalCommands++;

	if (Config::Debug::DrawUnitTargetInfo)
	{
		BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::White, true);
		BWAPI::Broodwar->drawCircleMap(targetPosition, dotRadius, BWAPI::Colors::White, true);
		BWAPI::Broodwar->drawLineMap(attacker->getPosition(), targetPosition, BWAPI::Colors::White);
	}
}

void Micro::RightClick(BWAPI::Unit unit, BWAPI::Unit target)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

    // if we have issued a command to this unit already this frame, ignore this one
    if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to right-click this target, ignore this command
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit) && (currentCommand.getTarget() == target) && BWAPI::Broodwar->getFrameCount() < unit->getLastCommandFrame() + 12)
    {
        return;
    }

    // if nothing prevents it, attack the target
    unit->rightClick(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
        BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
        BWAPI::Broodwar->drawLineMap(unit->getPosition(), target->getPosition(), BWAPI::Colors::Cyan);
    }
}


void Micro::RightClick(BWAPI::Unit unit, BWAPI::Position pos)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
	{
		return;
	}

	// get the unit's current command
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// if we've already told this unit to right-click this target, ignore this command
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit) && (currentCommand.getTargetPosition() == pos) && BWAPI::Broodwar->getFrameCount() < unit->getLastCommandFrame() + 12)
	{
		return;
	}

	// if nothing prevents it, attack the target
	unit->rightClick(pos);
	TotalCommands++;

	if (Config::Debug::DrawUnitTargetInfo)
	{
		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
		BWAPI::Broodwar->drawCircleMap(pos, dotRadius, BWAPI::Colors::Cyan, true);
		BWAPI::Broodwar->drawLineMap(unit->getPosition(), pos, BWAPI::Colors::Cyan);
	}
}

void Micro::Land(BWAPI::Unit unit, BWAPI::TilePosition target)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	// get the unit's current command
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		return;
	}

	// if we've already told this unit to right-click this target, ignore this command
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Land) && (currentCommand.getTargetPosition().x == target.x && currentCommand.getTargetPosition().y == target.y))
	{
		return;
	}

	// if nothing prevents it, attack the target
	unit->land(target);
	TotalCommands++;

	if (Config::Debug::DrawUnitTargetInfo)
	{
		BWAPI::Position targetPosition(target);
		BWAPI::Broodwar->drawLineMap(unit->getPosition(), targetPosition, BWAPI::Colors::Cyan);
	}
}

void Micro::Lift(BWAPI::Unit unit)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	// get the unit's current command
	BWAPI::UnitCommand currentCommand(unit->getLastCommand());

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		return;
	}

	// if we've already told this unit to right-click this target, ignore this command
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Lift) && unit->isLifted())
	{
		return;
	}

	// if nothing prevents it, attack the target
	unit->lift();
	TotalCommands++;
}

//埋雷
void Micro::LaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() || !pos.isValid())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	if (!unit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, pos))
    {
        return;
    }

    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
	//如果我们已经告诉这个单位移动到这个位置, 忽略这个命令
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Use_Tech_Position) || currentCommand.getTargetPosition() == pos)
    {
        return;
    }

	unit->useTech(BWAPI::TechTypes::Spider_Mines, pos);
}

//维修
void Micro::Repair(BWAPI::Unit unit, BWAPI::Unit target)
{
	if (!unit || !unit->exists() || unit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

    // if we have issued a command to this unit already this frame, ignore this one
    if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame())
    {
        return;
    }

    // get the unit's current command
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());

    // if we've already told this unit to move to this position, ignore this command
	if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Repair) && (currentCommand.getTarget() == target) && BWAPI::Broodwar->getFrameCount() < unit->getLastCommandFrame() + 12)
    {
        return;
    }

    // Nothing prevents it, so attack the target.
    unit->repair(target);
    TotalCommands++;

    if (Config::Debug::DrawUnitTargetInfo) 
    {
		if (unit && target) {
			BWAPI::Broodwar->drawCircleMap(unit->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
			BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Cyan, true);
			BWAPI::Broodwar->drawLineMap(unit->getPosition(), target->getPosition(), BWAPI::Colors::Cyan);
		}
    }
}

// Perform a comsat scan at the given position if possible and necessary.
// If it's not possible, or we already scanned there, do nothing.
// Return whether the scan occurred.
//洒雷达
bool Micro::Scan(BWAPI::Unit unit, BWAPI::Position targetPosition)
{
	UAB_ASSERT(targetPosition.isValid(), "bad position");

	int maxEnergy = 50;
	if (unit->getType() != BWAPI::UnitTypes::Terran_Comsat_Station && unit->getEnergy() < maxEnergy) return false;

	// If a scan of this position is still active, don't scan it again.
	if (MapGrid::Instance().scanIsActiveAt(targetPosition))
	{
		return false;
	}

	if (unit)
	{
		MapGrid::Instance().scanAtPosition(targetPosition);
		return unit->useTech(BWAPI::TechTypes::Scanner_Sweep, targetPosition);
	}

	return false;
}

// Stim the given marine or firebat, if possible; otherwise, do nothing.
// Return whether the stim occurred.
//传感给定的海洋或 firebat, 如果可能的话;否则, 什么也不做。
//返回是否发生传感。
//兴奋剂
bool Micro::Stim(BWAPI::Unit unit)
{
	if (!unit ||
		unit->getType() != BWAPI::UnitTypes::Terran_Marine && unit->getType() != BWAPI::UnitTypes::Terran_Firebat ||
		unit->getPlayer() != BWAPI::Broodwar->self())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	if (unit->isStimmed())
	{
		return false;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		return false;
	}

	// Allow a small latency for a previous stim command to take effect.
	// Marines and firebats have only 1 tech to use, so we don't need to check which.
	if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech &&
		BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 8)
	{
		return false;
	}

	// useTech() checks whether stim is researched and any other conditions.
	return unit->useTech(BWAPI::TechTypes::Stim_Packs);
}

//辐射
bool Micro::Irradiate(BWAPI::Unit unit, BWAPI::Unit target)
{
	if (!target || !target->exists()) return false;

	if (!unit ||
		unit->getType() != BWAPI::UnitTypes::Terran_Science_Vessel || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		//UAB_ASSERT(false, "bad unit");
		return false;
	}

	if (!unit->canUseTech(BWAPI::TechTypes::Irradiate, target))
	{
		return false;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		return false;
	}

	// Allow a small latency for a previous stim command to take effect.
	// Marines and firebats have only 1 tech to use, so we don't need to check which.
	if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech &&
		BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 8)
	{
		return false;
	}

	// useTech() checks whether stim is researched and any other conditions.
	return unit->useTech(BWAPI::TechTypes::Irradiate, target);
}

//保护罩
bool Micro::DefensiveMatrix(BWAPI::Unit unit, BWAPI::Unit target)
{
	if (!unit ||
		unit->getType() != BWAPI::UnitTypes::Terran_Science_Vessel || unit->getPlayer() != BWAPI::Broodwar->self() || !target || !target->exists())
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	if (!unit->canUseTech(BWAPI::TechTypes::Defensive_Matrix, target))
	{
		return false;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		return false;
	}

	// Allow a small latency for a previous stim command to take effect.
	// Marines and firebats have only 1 tech to use, so we don't need to check which.
	if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech &&
		BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 8)
	{
		return false;
	}

	// useTech() checks whether stim is researched and any other conditions.
	return unit->useTech(BWAPI::TechTypes::Defensive_Matrix, target);
}
// Merge the 2 given high templar into an archon.
// TODO Check whether the 2 templar can reach each other: Is there a ground path between them?
//把2高的圣殿武士合并成执政官。
//TODO 检查2圣殿武士能否互相联系 : 他们之间有一条路吗？
bool Micro::MergeArchon(BWAPI::Unit templar1, BWAPI::Unit templar2)
{
	if (!templar1 || !templar2 ||
		templar1->getPlayer() != BWAPI::Broodwar->self() ||
		templar2->getPlayer() != BWAPI::Broodwar->self() ||
		templar1->getType() != BWAPI::UnitTypes::Protoss_High_Templar ||
		templar2->getType() != BWAPI::UnitTypes::Protoss_High_Templar ||
		templar1 == templar2)
	{
		UAB_ASSERT(false, "bad unit");
		return false;
	}

	// If we have issued a command already this frame, ignore this one.
	if (templar1->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() ||
		templar2->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		return false;
	}

	// useTech() checks any other conditions.
	return templar1->useTech(BWAPI::TechTypes::Archon_Warp, templar2);
}
//资源返回基地
void Micro::ReturnCargo(BWAPI::Unit worker)
{
	if (!worker || !worker->exists() || worker->getPlayer() != BWAPI::Broodwar->self() ||
		!worker->getType().isWorker())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	// If the worker has no cargo, ignore this command.
	if (!worker->isCarryingMinerals() && !worker->isCarryingGas())
	{
		return;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (worker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || worker->isAttackFrame())
	{
		return;
	}

	// If we've already issued this command, don't issue it again.
	BWAPI::UnitCommand currentCommand(worker->getLastCommand());
	if (currentCommand.getType() == BWAPI::UnitCommandTypes::Return_Cargo)
	{
		return;
	}

	// Nothing prevents it, so return cargo.
	worker->returnCargo();
	TotalCommands++;
}

//风筝攻击
void Micro::KiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target)
{
	if (!rangedUnit || !rangedUnit->exists() || rangedUnit->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	double range(rangedUnit->getType().groundWeapon().maxRange());
	int damage(rangedUnit->getType().groundWeapon().damageAmount());;

	if (rangedUnit->getType() == BWAPI::UnitTypes::Protoss_Dragoon && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge))
	{
		range = 6 * 32;
	}
	else if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines))
	{
		range = 5 * 32;
	}
	else if (rangedUnit->getType() == BWAPI::UnitTypes::Terran_Marine && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::U_238_Shells))
	{
		range = 5 * 32;
	}

	bool kite(true);

	// Kite if configured as a "KiteLonger" unit, or if the enemy's range is shorter than ours.
	// Note: Assumes that the enemy does not have range upgrades.
	//如果将风筝配置为 "KiteLonger" 单位, 或者敌人的射程比我们的短。
	//注: 假设敌人没有范围升级。
    bool kiteLonger = Config::Micro::KiteLongerRangedUnits.find(rangedUnit->getType()) != Config::Micro::KiteLongerRangedUnits.end();
	if (!kiteLonger && (range <= target->getType().groundWeapon().maxRange()))
	{
		kite = false;
	}
    // If the target can't attack back, then don't kite.
	//如果目标不能反击, 那就不要放风筝。
	else if (!UnitUtil::CanAttack(target, rangedUnit))
    {
    	kite = false;
    }

	// Kite if we're not ready yet: Wait for the weapon.
	double dist(rangedUnit->getDistance(target));
	double speed(target->getType().topSpeed());
	double timeToEnter = 0.0;                      // time to reach firing range
	if (speed > .00001)                            // don't even visit the same city as division by zero
	{
		timeToEnter = std::max(0.0, dist - range) / speed;
	}

	//武器冷却时间
	if (timeToEnter >= rangedUnit->getGroundWeaponCooldown() ||
		target->getType().isBuilding())
	{
		kite = false;
	}

	int enemyDamage = 0;//目标的武器伤害
	int enemyRange = 0;//目标的攻击距离

	if (target->canAttack()) {
		if (rangedUnit->getType().isFlyer()) {
			if (target->getType().airWeapon() != BWAPI::WeaponTypes::None) {
				enemyRange = target->getType().airWeapon().maxRange();
				enemyDamage = target->getType().airWeapon().damageAmount();// +target->getType().airWeapon().damageBonus();
			}
		}
		else {
			if (target->getType().groundWeapon() != BWAPI::WeaponTypes::None) {
				enemyRange = target->getType().groundWeapon().maxRange();
				enemyDamage = target->getType().groundWeapon().damageAmount();// +target->getType().groundWeapon().damageBonus();
			}
		}

		int timeOfWeapon = 0;
		if (target->getType().isFlyer()) {
			if (rangedUnit->getType().airWeapon() != BWAPI::WeaponTypes::None) {
				timeOfWeapon = rangedUnit->getAirWeaponCooldown();
			}
		}
		else {
			if (rangedUnit->getType().groundWeapon() != BWAPI::WeaponTypes::None) {
				timeOfWeapon = rangedUnit->getGroundWeaponCooldown();
			}
		}

		if (range < enemyRange) {
			//修改 如果敌人距离小于武器攻击时间，则撤退
			//if (dist / speed  < rangedUnit->getType().groundWeapon().damageCooldown()) {
			if (rangedUnit->isStartingAttack()) {
				timeOfWeapon = rangedUnit->getType().groundWeapon().damageCooldown();

				if (target->getType().isFlyer()) {
					timeOfWeapon = rangedUnit->getType().airWeapon().damageCooldown();
				}
			}
			if ((dist - enemyRange) / speed < timeOfWeapon) {
				kite = true;
			}
		}
	}

	if (dist <= enemyRange && damage < enemyDamage && rangedUnit->getAngle() == target->getAngle()) {
		kite = true;
	}

	BWAPI::Unit needEnemy = rangedUnit->getClosestUnit(BWAPI::Filter::CanAttack && BWAPI::Filter::IsEnemy, 2 * 32);
	if (needEnemy && needEnemy->getAngle() == rangedUnit->getAngle()) {
		kite = true;
	}

	if (kite)
	{
		// Run away.
		//修改 撤退
		//rangedUnit->get
		//BWAPI::Position fleePosition(rangedUnit->getPosition() - target->getPosition() + rangedUnit->getPosition());
		BWAPI::Position fleePosition = MapTools::Instance().getExtendedPosition(rangedUnit->getPosition(), target->getPosition(), range - dist);// - dist
		//dist = rangedUnit->getDistance(fleePosition);

		if (!rangedUnit->hasPath(fleePosition)) {
			fleePosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();
		}

		/*
		if (!target->getType().isBuilding()) {
			//修改 如果敌人距离小于武器攻击时间，则撤退
			//if (dist / speed  < rangedUnit->getType().groundWeapon().damageCooldown()) {
			double timeOfWeapon = rangedUnit->getGroundWeaponCooldown();
			if (rangedUnit->isStartingAttack()) {
				timeOfWeapon = rangedUnit->getType().groundWeapon().damageCooldown();
			}
			if ((dist - target->getType().groundWeapon().maxRange()) / speed < timeOfWeapon) {
				fleePosition = GetFleePosition(fleePosition, rangedUnit->getPosition(), range);
			}
		}
		*/

		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(rangedUnit->getPosition(), fleePosition, BWAPI::Colors::Cyan);
		}

		//rangedUnit->rightClick(fleePosition);
		Micro::Move(rangedUnit, fleePosition);

		/*
		if (dist <= target->getType().groundWeapon().maxRange()) {
			//Micro::Move(rangedUnit, fleePosition, true);
		}
		else {
			Micro::Move(rangedUnit, fleePosition);
		}
		*/
		/*
		if (fleePosition.isValid()) {
			Micro::Move(rangedUnit, fleePosition);
		}
		else {
			rangedUnit->rightClick(fleePosition);
		}
		*/
	}
	else
	{
		// Shoot.
		Micro::AttackUnit(rangedUnit, target);
	}
}

// Used for both mutalisks and vultures--fast units with short range.
//用于飞龙和秃鹰--快速的单位与短的范围。
void Micro::MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target)
{
	if (!muta || !muta->exists() || muta->getPlayer() != BWAPI::Broodwar->self() ||
		(muta->getType() != BWAPI::UnitTypes::Zerg_Mutalisk && 
		muta->getType() != BWAPI::UnitTypes::Terran_Vulture && 
		muta->getType() != BWAPI::UnitTypes::Terran_Wraith && 
		muta->getType() != BWAPI::UnitTypes::Protoss_Scout &&
		muta->getType() != BWAPI::UnitTypes::Protoss_Carrier) ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

    const int cooldown                  = muta->getType().groundWeapon().damageCooldown();
    const int latency                   = BWAPI::Broodwar->getLatency();
    const double speed                  = muta->getType().topSpeed();   // known to be non-zero :-)
    const double range                  = muta->getType().groundWeapon().maxRange();
    const double distanceToTarget       = muta->getDistance(target);
	const double distanceToFiringRange  = std::max(distanceToTarget - range,0.0);
	const double timeToEnterFiringRange = distanceToFiringRange / speed;
	const int framesToAttack			= static_cast<int>(timeToEnterFiringRange) + 2 * latency;

	if (target->isFlying()) {
		if (muta->getType() == BWAPI::UnitTypes::Zerg_Mutalisk || muta->getType() != BWAPI::UnitTypes::Terran_Wraith || muta->getType() != BWAPI::UnitTypes::Protoss_Scout) {
			const int cooldown = muta->getType().airWeapon().damageCooldown();
			const double range = muta->getType().airWeapon().maxRange();
		}
	}

	// How many frames are left before we can attack?
	const int currentCooldown = muta->isStartingAttack() ? cooldown : muta->getGroundWeaponCooldown();

	// If we can attack by the time we reach our firing range
	if(currentCooldown <= framesToAttack)
	{
		// Move towards and attack the target
		muta->attack(target);
	}
	else // Otherwise we cannot attack and should temporarily back off
	{
		BWAPI::Position fleeVector = GetKiteVector(target, muta);
		BWAPI::Position moveToPosition(muta->getPosition() + fleeVector);
		if (moveToPosition.isValid())
		{
			muta->rightClick(moveToPosition);
		}
	}
}

// Used for both mutalisks and vultures--fast units with short range.
//用于飞龙和秃鹰--快速的单位与短的范围。
void Micro::MoveAttackLurker(BWAPI::Unit muta, BWAPI::Unit target)
{
	if (!muta || !muta->exists() || muta->getPlayer() != BWAPI::Broodwar->self() ||
		!target || !target->exists())
	{
		UAB_ASSERT(false, "bad arg");
		return;
	}

	const int cooldown = muta->getType().groundWeapon().damageCooldown();
	const int latency = BWAPI::Broodwar->getLatency();
	const double speed = muta->getType().topSpeed();   // known to be non-zero :-)
	const double range = muta->getType().groundWeapon().maxRange();
	const double distanceToTarget = muta->getDistance(target);
	const double distanceToFiringRange = std::max(distanceToTarget - range, 0.0);
	const double timeToEnterFiringRange = distanceToFiringRange / speed;
	const int framesToAttack = static_cast<int>(timeToEnterFiringRange)+2 * latency;

	if (target->isFlying()) {
		if (muta->getType() == BWAPI::UnitTypes::Zerg_Mutalisk || muta->getType() != BWAPI::UnitTypes::Terran_Wraith || muta->getType() != BWAPI::UnitTypes::Protoss_Scout) {
			const int cooldown = muta->getType().airWeapon().damageCooldown();
			const double range = muta->getType().airWeapon().maxRange();
		}
	}

	// How many frames are left before we can attack?
	const int currentCooldown = muta->isStartingAttack() ? cooldown : muta->getGroundWeaponCooldown();

	// If we can attack by the time we reach our firing range
	if (currentCooldown <= framesToAttack)
	{
		// Move towards and attack the target
		muta->attack(target);
	}
	else // Otherwise we cannot attack and should temporarily back off
	{
		BWAPI::Position fleeVector = GetKiteVector(target, muta);
		BWAPI::Position moveToPosition(muta->getPosition() + fleeVector);
		if (moveToPosition.isValid())
		{
			muta->rightClick(moveToPosition);
		}
	}
}

//震荡波
bool Micro::EmpShockwave(BWAPI::Unit unit, BWAPI::Position position){
	if (!unit ||
		unit->getType() != BWAPI::UnitTypes::Terran_Science_Vessel || unit->getPlayer() != BWAPI::Broodwar->self())
	{
		//UAB_ASSERT(false, "bad unit");
		return false;
	}

	if (!unit->canUseTech(BWAPI::TechTypes::EMP_Shockwave, position))
	{
		return false;
	}

	// if we have issued a command to this unit already this frame, ignore this one
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount())
	{
		return false;
	}

	// Allow a small latency for a previous stim command to take effect.
	// Marines and firebats have only 1 tech to use, so we don't need to check which.
	if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech &&
		BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < 8)
	{
		return false;
	}

	// useTech() checks whether stim is researched and any other conditions.
	return unit->useTech(BWAPI::TechTypes::EMP_Shockwave, position);
}

BWAPI::Position Micro::GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target)
{
    BWAPI::Position fleeVec(target->getPosition() - unit->getPosition());
	if (target->getDistance(target) < 2 * 32) {
		fleeVec.y += 32;
		fleeVec.x += 32;
	}
    double fleeAngle = atan2(fleeVec.y, fleeVec.x);
    fleeVec = BWAPI::Position(static_cast<int>(64 * cos(fleeAngle)), static_cast<int>(64 * sin(fleeAngle)));
    return fleeVec;
}
