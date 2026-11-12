#include "UnitUtil.h"

using namespace UAlbertaBot;

// Building morphed from another, not constructed.
//建筑从另一个, 而不是构造。
bool UnitUtil::IsMorphedBuildingType(BWAPI::UnitType type)
{
	return
		type == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
		type == BWAPI::UnitTypes::Zerg_Spore_Colony ||
		type == BWAPI::UnitTypes::Zerg_Lair ||
		type == BWAPI::UnitTypes::Zerg_Hive ||
		type == BWAPI::UnitTypes::Zerg_Greater_Spire;
}

// Zerg unit morphed from another, not spawned from a larva.
//虫族单位从另一个进化而来, 不是从幼虫中衍生出来的。
bool UnitUtil::IsMorphedUnitType(BWAPI::UnitType type)
{
	return
		type == BWAPI::UnitTypes::Zerg_Lurker ||
		type == BWAPI::UnitTypes::Zerg_Guardian ||
		type == BWAPI::UnitTypes::Zerg_Devourer;
}

bool UnitUtil::IsStaticDefense(BWAPI::UnitType type)
{
	return
		type == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
		type == BWAPI::UnitTypes::Zerg_Spore_Colony ||
		type == BWAPI::UnitTypes::Terran_Bunker ||
		type == BWAPI::UnitTypes::Terran_Missile_Turret ||
		type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
		type == BWAPI::UnitTypes::Protoss_Shield_Battery;
}

// This is a combat unit for purposes of combat simulation.
//这是一个作战单位为作战仿真目的。
bool UnitUtil::IsCombatSimUnit(BWAPI::Unit unit)
{
	if (!unit->isCompleted() || !unit->isPowered() || unit->getHitPoints() == 0)
	{
		return false;
	}

	// A worker counts as a combat unit if it has been given an order to attack.
	if (unit->getType().isWorker())
	{
		return
			unit->getOrder() == BWAPI::Orders::AttackMove ||
			unit->getOrder() == BWAPI::Orders::AttackTile ||
			unit->getOrder() == BWAPI::Orders::AttackUnit;
	}

	return IsCombatSimUnit(unit->getType());
}

// This type is a combat unit type for purposes of combat simulation.
// Treat workers as non-combat units (overridden above for some workers).
// The combat simulation does not support spells other than medic healing and stim,
// and it does not understand detectors.
// The combat sim treats carriers as the attack unit, not their interceptors (bftjoe).
//这种类型是用于战斗模拟的战斗单元类型。
//将工人视为非战斗单位(某些工人在上面重写)。
//战斗模拟不支持除医疗治疗和stim以外的法术，
//它不理解检测器。
//《战斗模拟》把航母当作攻击单位，而不是拦截机(bftjoe)。
bool UnitUtil::IsCombatSimUnit(BWAPI::UnitType type)
{
	if (type.isWorker())
	{
		return false;
	}

	if (type == BWAPI::UnitTypes::Protoss_Interceptor)
	{
		return false;
	}

	return
		TypeCanAttackAir(type) ||
		TypeCanAttackGround(type) ||
		type == BWAPI::UnitTypes::Terran_Medic;
}

bool UnitUtil::IsCombatUnit(BWAPI::UnitType type)
{
    // No workers, buildings, or carrier interceptors (which are not controllable).
	// Buildings include static defense buildings; they are not put into squads.
	//没有工人、建筑物或航母拦截器(无法控制)。
	//建筑包括静态防御建筑;他们没有被编入小队。
	if (type.isWorker() ||
		type.isBuilding() ||
		type == BWAPI::UnitTypes::Protoss_Interceptor)  // apparently, they canAttack()
    {
        return false;
    }

	return
		(type.canAttack() && type != BWAPI::UnitTypes::Terran_Vulture_Spider_Mine) ||                           // includes carriers and reavers
		type.isDetector() ||
		type == BWAPI::UnitTypes::Zerg_Queen ||
		type == BWAPI::UnitTypes::Zerg_Defiler ||
		type == BWAPI::UnitTypes::Terran_Medic ||
		type == BWAPI::UnitTypes::Terran_Science_Vessel ||
		type == BWAPI::UnitTypes::Protoss_High_Templar ||
		type == BWAPI::UnitTypes::Protoss_Dark_Archon ||
		
		type.isFlyer() && type.spaceProvided() > 0;     // transports
}

bool UnitUtil::IsCombatUnit(BWAPI::Unit unit)
{
	UAB_ASSERT(unit != nullptr, "Unit was null");
	if (!unit)
	{
		return false;
	}

	return IsCombatUnit(unit->getType());
}

// Check whether a unit variable points to a unit we control.
// This is called only on units that we believe are ours.
bool UnitUtil::IsValidUnit(BWAPI::Unit unit)
{
	return unit
		&& unit->exists()
		&& (unit->isCompleted() || IsMorphedBuildingType(unit->getType()))
		&& unit->getHitPoints() > 0
		&& unit->getType() != BWAPI::UnitTypes::Unknown
		&& (unit->getPosition().isValid() || unit->isLoaded())     // position is invalid if loaded in transport or bunker
		&& unit->getPlayer() == BWAPI::Broodwar->self();           // catches mind controlled units
}

bool UnitUtil::CanAttack(BWAPI::Unit attacker, BWAPI::Unit target)
{
	return target->isFlying() ? TypeCanAttackAir(attacker->getType()) : TypeCanAttackGround(attacker->getType());
}

// Accounts for cases where units can attack without a weapon of their own.
// Ignores spellcasters, which have limitations on their attacks.
// For example, high templar can attack air or ground mobile units, but can't attack buildings.
bool UnitUtil::CanAttack(BWAPI::UnitType attacker, BWAPI::UnitType target)
{
	return target.isFlyer() ? TypeCanAttackAir(attacker) : TypeCanAttackGround(attacker);
}

bool UnitUtil::CanAttackAir(BWAPI::Unit attacker)
{
	return TypeCanAttackAir(attacker->getType());
}

// Assume that a bunker is loaded and can shoot at air.
//假设一个地堡被装载并且能射击在空气。
bool UnitUtil::TypeCanAttackAir(BWAPI::UnitType attacker)
{
	return attacker.airWeapon() != BWAPI::WeaponTypes::None ||
		attacker == BWAPI::UnitTypes::Terran_Bunker ||
		attacker == BWAPI::UnitTypes::Protoss_Carrier;
}

// NOTE surrenderMonkey() checks CanAttackGround() to see whether the enemy can destroy buildings.
//      Adding spellcasters to it would break that.
//      If you do that, make CanAttackBuildings() and have surrenderMonkey() call that.
//注意 surrenderMonkey () 检查 CanAttackGround (), 看看敌人是否能摧毁建筑物。
//添加施法者将打破这一点。
//如果你这样做, 做 CanAttackBuildings(), 并有 surrenderMonkey() 调用它。
bool UnitUtil::CanAttackGround(BWAPI::Unit attacker)
{
	return TypeCanAttackGround(attacker->getType());
}

// Assume that a bunker is loaded and can shoot at ground.
//假设一个地堡被装载并且能射击在地面。
bool UnitUtil::TypeCanAttackGround(BWAPI::UnitType attacker)
{
	return attacker.groundWeapon() != BWAPI::WeaponTypes::None ||
		attacker == BWAPI::UnitTypes::Terran_Bunker ||
		attacker == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
		attacker == BWAPI::UnitTypes::Protoss_Carrier ||
		attacker == BWAPI::UnitTypes::Protoss_Reaver;
}

// NOTE Unused but potentially useful.
double UnitUtil::CalculateLTD(BWAPI::Unit attacker, BWAPI::Unit target)
{
	BWAPI::WeaponType weapon = GetWeapon(attacker, target);

	if (weapon == BWAPI::WeaponTypes::None || weapon.damageCooldown() <= 0)
	{
		return 0;
	}

	return double(weapon.damageAmount()) / weapon.damageCooldown();
}

BWAPI::WeaponType UnitUtil::GetWeapon(BWAPI::Unit attacker, BWAPI::Unit target)
{
	return GetWeapon(attacker->getType(), target);
}

// Handle carriers and reavers correctly in the case of floating buildings.
// We have to check unit->isFlying() because unitType->isFlyer() is not useful
// for a lifted terran building.
BWAPI::WeaponType UnitUtil::GetWeapon(BWAPI::UnitType attacker, BWAPI::Unit target)
{
	// We pretend that a bunker has marines in it. It's only a guess.
	if (attacker == BWAPI::UnitTypes::Terran_Bunker)
	{
		return GetWeapon(BWAPI::UnitTypes::Terran_Marine, target);
	}
	if (attacker == BWAPI::UnitTypes::Protoss_Carrier)
	{
		return GetWeapon(BWAPI::UnitTypes::Protoss_Interceptor, target);
	}
	if (attacker == BWAPI::UnitTypes::Protoss_Reaver)
	{
		return GetWeapon(BWAPI::UnitTypes::Protoss_Scarab, target);
	}
	return target->isFlying() ? attacker.airWeapon() : attacker.groundWeapon();
}

BWAPI::WeaponType UnitUtil::GetWeapon(BWAPI::UnitType attacker, BWAPI::UnitType target)
{
	// We pretend that a bunker has marines in it. It's only a guess.
	if (attacker == BWAPI::UnitTypes::Terran_Bunker)
	{
		return GetWeapon(BWAPI::UnitTypes::Terran_Marine, target);
	}
	if (attacker == BWAPI::UnitTypes::Protoss_Carrier)
	{
		return GetWeapon(BWAPI::UnitTypes::Protoss_Interceptor, target);
	}
	if (attacker == BWAPI::UnitTypes::Protoss_Reaver)
	{
		return GetWeapon(BWAPI::UnitTypes::Protoss_Scarab, target);
	}
	return target.isFlyer() ? attacker.airWeapon() : attacker.groundWeapon();
}

// Tries to take possible range upgrades into account, making pessimistic assumptions about the enemy.
// Returns 0 if the attacker does not have a way to attack the target.
// NOTE Does not check whether our reaver, carrier, or bunker has units inside that can attack.
//试图把可能的范围升级考虑在内, 对敌人做出悲观的假设。
//如果攻击者没有攻击目标的方法, 则返回0。
//注意不要检查我们的掠夺者, 承运人, 或地堡是否有单位内, 可以攻击。
int UnitUtil::GetAttackRange(BWAPI::Unit attacker, BWAPI::Unit target)
{
	// Reavers, carriers, and bunkers have "no weapon" but still have an attack range.
	if (attacker->getType() == BWAPI::UnitTypes::Protoss_Reaver && !target->isFlying())
	{
		return 8;
	}
	if (attacker->getType() == BWAPI::UnitTypes::Protoss_Carrier)
	{
		return 8;
	}
	if (attacker->getType() == BWAPI::UnitTypes::Terran_Bunker)
	{
		if (attacker->getPlayer() == BWAPI::Broodwar->enemy() ||
			BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::U_238_Shells))
		{
			return 6 * 32;
		}
		return 5 * 32;
	}

	const BWAPI::WeaponType weapon = GetWeapon(attacker, target);

	if (weapon == BWAPI::WeaponTypes::None)
	{
		return 0;
	}

	int range = weapon.maxRange();

	// Count range upgrades,
	// for ourselves if we have researched it,
	// for the enemy always (by pessimistic assumption).
	if (attacker->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
	{
		if (attacker->getPlayer() == BWAPI::Broodwar->enemy() ||
			BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge))
		{
			range = 6 * 32;
		}
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Terran_Marine)
	{
		if (attacker->getPlayer() == BWAPI::Broodwar->enemy() ||
			BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::U_238_Shells))
		{
			range = 5 * 32;
		}
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Terran_Goliath && target->isFlying())
	{
		if (attacker->getPlayer() == BWAPI::Broodwar->enemy() ||
			BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Charon_Boosters))
		{
			range = 8 * 32;
		}
	}
	else if (attacker->getType() == BWAPI::UnitTypes::Zerg_Hydralisk)
	{
		if (attacker->getPlayer() == BWAPI::Broodwar->enemy() ||
			BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines))
		{
			range = 5 * 32;
		}
	}

    return range;
}

// Range is zero if the attacker cannot attack the target at all.
//如果攻击者根本无法攻击目标, 则范围为零。
int UnitUtil::GetAttackRangeAssumingUpgrades(BWAPI::UnitType attacker, BWAPI::UnitType target)
{
	// Reavers, carriers, and bunkers have "no weapon" but still have an attack range.
	if (attacker == BWAPI::UnitTypes::Protoss_Reaver && !target.isFlyer())
	{
		return 8;
	}
	if (attacker == BWAPI::UnitTypes::Protoss_Carrier)
	{
		return 8;
	}
	if (attacker == BWAPI::UnitTypes::Terran_Bunker)
	{
		return 6 * 32;
	}

	BWAPI::WeaponType weapon = GetWeapon(attacker, target);
	if (weapon == BWAPI::WeaponTypes::None)
    {
        return 0;
    }

    int range = weapon.maxRange();

	// Assume that any upgrades are researched.
	if (attacker == BWAPI::UnitTypes::Protoss_Dragoon)
	{
		range = 6 * 32;
	}
	else if (attacker == BWAPI::UnitTypes::Terran_Marine)
	{
		range = 5 * 32;
	}
	else if (attacker == BWAPI::UnitTypes::Terran_Goliath && target.isFlyer())
	{
		range = 8 * 32;
	}
	else if (attacker == BWAPI::UnitTypes::Zerg_Hydralisk)
	{
		range = 5 * 32;
	}

	return range;
}

// The longest range at which the unit type is able to make a regular attack, assuming upgrades.
// This ignores spells--for example, yamato cannon has a longer range.
// Used in selecting enemy units for the combat sim.
//设备类型能够进行常规攻击的最长范围, 假设升级。
//这忽略咒语 - 例如, 大和大炮有一个更长的范围。
//用于为作战 sim 选择敌方单位。
int UnitUtil::GetMaxAttackRange(BWAPI::UnitType type)
{
	return std::max(
		GetAttackRangeAssumingUpgrades(type, BWAPI::UnitTypes::Terran_Marine),   // range vs. ground
		GetAttackRangeAssumingUpgrades(type, BWAPI::UnitTypes::Terran_Wraith)    // range vs. air
	);
}

// The damage the attacker's weapon will do to a worker. It's good for any small unit.
// Ignores:
// - the attacker's weapon upgrades (easy to include)
// - the defender's armor/shield upgrades (a bit complicated for probes)
// Used in worker self-defense. It's usually good enough for that.
//攻击者的武器会对工人造成的伤害。这对任何一个小单位都有好处。
//忽略:
//-攻击者的武器升级(易于包括)
//- 后卫的装甲 / 防护罩升级(探头有点复杂)
//用于工人自卫。这通常是足够好的。
int UnitUtil::GetWeaponDamageToWorker(BWAPI::Unit attacker)
{
	// Workers will be the same, so use an SCV as a representative worker.
	const BWAPI::UnitType workerType = BWAPI::UnitTypes::Terran_SCV;

	BWAPI::WeaponType weapon = UnitUtil::GetWeapon(attacker->getType(), workerType);

	if (weapon == BWAPI::WeaponTypes::None)
	{
		return 0;
	}

	int damage = weapon.damageAmount();

	if (weapon.damageType() == BWAPI::DamageTypes::Explosive)
	{
		return damage / 2;
	}

	// Assume it is Normal or Concussive damage, though there are other possibilities.
	return damage;
}

// All our units, whether completed or not.
//我们所有的单位, 无论完成与否。
int UnitUtil::GetAllUnitCount(BWAPI::UnitType type)
{
	return GetAllUnitCount(type, BWAPI::Broodwar->self());
}

int UnitUtil::GetAllUnitCount(BWAPI::UnitType type, BWAPI::Player player = BWAPI::Broodwar->self()) {
	int count = 0;
	for (const auto unit : player->getUnits())
	{
		if (unit->getType() == type)
		{
			++count;
		}

		// Units in the egg.
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg && unit->getBuildType() == type)
		{
			count += type.isTwoUnitsInOneEgg() ? 2 : 1;
		}

		// Lurkers in the egg.
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker_Egg && type == BWAPI::UnitTypes::Zerg_Lurker)
		{
			++count;
		}

		// Guardians or devourers in the cocoon.
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Cocoon && unit->getBuildType() == type)
		{
			++count;
		}

		// case where a building has started constructing a unit but it doesn't yet have a unit associated with it
		else if (unit->getRemainingTrainTime() > 0)
		{
			BWAPI::UnitType trainType = unit->getLastCommand().getUnitType();

			// NOTE Comparing the time like this could lead to miscounts if units start simultaneously.
			//      But the original UAlbertaBot production system does not start units simultaneously.
			if (trainType == type && unit->getRemainingTrainTime() == trainType.buildTime())
			{
				++count;
			}
		}
	}

	return count;
}

// Only our completed units.
int UnitUtil::GetCompletedUnitCount(BWAPI::UnitType type)
{
	int count = 0;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == type && unit->isCompleted())
		{
			++count;
		}
	}

	return count;
}

// Only our incomplete units.
int UnitUtil::GetUncompletedUnitCount(BWAPI::UnitType type)
{
	int count = 0;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		// Units in the egg.
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg && unit->getBuildType() == type)
		{
			count += type.isTwoUnitsInOneEgg() ? 2 : 1;
		}

		// Lurkers in the egg.
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker_Egg && type == BWAPI::UnitTypes::Zerg_Lurker)
		{
			++count;
		}

		// Guardians or devourers in the cocoon.
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Cocoon && unit->getBuildType() == type)
		{
			++count;
		}

		// case where a building has started constructing a unit but it doesn't yet have a unit associated with it
		else if (unit->getRemainingTrainTime() > 0)
		{
			BWAPI::UnitType trainType = unit->getLastCommand().getUnitType();

			// NOTE Comparing the time like this could lead to miscounts if units start simultaneously.
			//      But the original UAlbertaBot production system does not start units simultaneously.
			if (trainType == type && unit->getRemainingTrainTime() == trainType.buildTime())
			{
				++count;
			}
		}

		// The basic case.
		else if (unit->getType() == type && !unit->isCompleted())
		{
			++count;
		}
	}

	return count;
}

BWAPI::UnitType UnitUtil::GetUnitTypeByName(std::string name) {
	
	std::transform(name.begin(), name.end(), name.begin(), ::tolower);

	for (const BWAPI::UnitType & unitType : BWAPI::UnitTypes::allUnitTypes())
	{
		std::string raceName = unitType.getRace().getName();
		std::string typeName = unitType.getName();
		std::replace(typeName.begin(), typeName.end(), '_', ' ');
		std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
		if ((typeName.length() > raceName.length()) && (typeName.compare(raceName.length() + 1, typeName.length(), name) == 0))
		{
			return unitType;
		}
	}

	return BWAPI::UnitTypes::None;
}


BWAPI::Unit UnitUtil::GetNextCompletedBuildingOfType(BWAPI::UnitType type)
{
	int bestFrame = INT_MAX;
	BWAPI::Unit bestUnit = nullptr;
	for (const auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == type && unit->isBeingConstructed() && unit->getRemainingBuildTime() < bestFrame)
			bestFrame = unit->getRemainingBuildTime(), bestUnit = unit;
	}

	return bestUnit;
}
