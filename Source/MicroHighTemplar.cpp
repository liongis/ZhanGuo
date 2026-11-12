#include "MicroHighTemplar.h"
#include "UnitUtil.h"
#include "MapPosition.h"
#include "MapTools.h"

using namespace UAlbertaBot;

// For now, all this does is immediately merge high templar into archons.

MicroHighTemplar::MicroHighTemplar()
{ 
}

void MicroHighTemplar::executeMicro(const BWAPI::Unitset & targets)
{
	assignTargets(targets);
}

void MicroHighTemplar::assignTargets(const BWAPI::Unitset & targets)
{

}

void MicroHighTemplar::update(const BWAPI::Position & center)
{
	const BWAPI::Unitset & units = getUnits();

	int frame = BWAPI::Broodwar->getFrameCount();

	for (auto unit : units) {
		if (unit->getEnergy() < 40) {
			BWAPI::Unit closest2 = unit->getClosestUnit(BWAPI::Filter::IsOwned && BWAPI::Filter::IsCompleted && BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_High_Templar && BWAPI::Filter::Energy < 40 && !BWAPI::Filter::IsLoaded, 10 * 32);
			if (closest2) {
				(void)Micro::MergeArchon(unit, closest2);//全白球
				continue;
			}
			else {
				BWAPI::Position fleeTo(InformationManager::Instance().getMyMainBaseLocation()->getPosition());//
				Micro::Move(unit, fleeTo);
				continue;
			}
		}
		else {
			BWAPI::Position unloadPosition = MapPosition::Instance().getUnloadPosition();
			if (unloadPosition.isValid() && unit->getDistance(unloadPosition) < 10 * 32) {
				BWAPI::Unitset targets = BWAPI::Broodwar->getUnitsInRadius(unloadPosition, 4 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::IsWorker);
				if (targets.size() > 0) {
					unit->useTech(BWAPI::TechTypes::Psionic_Storm, unloadPosition);
					continue;
				}
			} else {
				int maxNum = 0;
				int num = 0;
				BWAPI::Unit enemy = nullptr;
				BWAPI::Unitset targets = unit->getUnitsInRadius(9 * 32, BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBuilding && BWAPI::Filter::CanAttack);
				BWAPI::Position stormPosition;

				for (auto target : targets) {
					num = target->getUnitsInRadius(3 * 32, BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBuilding && BWAPI::Filter::CanAttack).size();
					if (num > maxNum) {
						stormPosition = target->getPosition();
						maxNum = num;
					}
				}

				if (stormPosition.isValid() && stormPosition.x > 0 && stormPosition.y > 0) {
					//重新计算一下附近是否有更多的敌人
					BWAPI::Position newStormPosition;
					for (int x = -2; x < 2; x++) {
						for (int y = -2; y < 2; y++) {
							newStormPosition = stormPosition + BWAPI::Position(x * 32, y * 32);
							num = BWAPI::Broodwar->getUnitsInRadius(stormPosition, 3 * 32, BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBuilding && BWAPI::Filter::CanAttack).size();
							if (num > maxNum) {
								stormPosition = newStormPosition;
								maxNum = num;
							}
						}
					}
				}

				if (stormPosition.isValid() && stormPosition.x > 0 && stormPosition.y > 0 && unit->getEnergy() >= 75 && BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Psionic_Storm)) {
					stormPosition = stormPosition + BWAPI::Position(1 * 32, 1 * 32);
					//判断放电的位置是否有我方部队
					int numOwned = BWAPI::Broodwar->getUnitsInRadius(stormPosition, 3 * 32, BWAPI::Filter::IsOwned).size();
					if (numOwned < maxNum) {
						unit->useTech(BWAPI::TechTypes::Psionic_Storm, stormPosition);
						continue;
					}
				}
			}
		}

		//if (unit->getDistance(center) > 10 * 32 && unit->getEnergy() > 60 && BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Psionic_Storm)) {
		if (unit->getDistance(center) > 10 * 32) {
			BWAPI::Position pos = center;
			if (unit->getEnergy() < 60 || !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Psionic_Storm)) {
				pos = InformationManager::Instance().getMyMainBaseLocation()->getPosition();
			} else {
				pos = MapTools::Instance().getDistancePosition(center, unit->getPosition(), 9 * 32);
			}

			Micro::Move(unit, pos);
			continue;
		}

		//如果周围有敌人，则躲开
		BWAPI::Unit enemy = unit->getClosestUnit(BWAPI::Filter::IsEnemy && BWAPI::Filter::CanAttack);
		if (enemy) {
			BWAPI::Position fleePosition = getFleePosition(unit->getPosition(), enemy->getPosition(), 6 * 32); //getFleePosition(meleeUnit, target);
			if (fleePosition.isValid() && unit->hasPath(fleePosition)) {
				Micro::RightClick(unit, fleePosition);
			}
		}
	}
}
