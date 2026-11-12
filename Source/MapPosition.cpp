#include "Common.h"
#include "MapPosition.h"
#include "InformationManager.h"
#include "Random.h"

using namespace UAlbertaBot;

MapPosition & MapPosition::Instance()
{
	static MapPosition instance;
	return instance;
}

MapPosition::MapPosition() {}

//出生位置
int MapPosition::getStartLocationName(BWAPI::TilePosition baseLocation = BWAPI::Broodwar->self()->getStartLocation()) {
	int x = baseLocation.x;
	int y = baseLocation.y;



	std::string hash = BWAPI::Broodwar->mapHash();

	//if (hash == "c73aa8daade77e62a3dc23bda5b4137bf7ad25bd") {
	if (x == 117 && y == 4) {//117,7 L1
		return 1;
	}
	else if (x == 117 && y == 62) {//117,117 L5
		return 3;
	}
	else if (x == 117 && y == 117) {//7,116 L7
		return 5;
	}
	else if (x == 62 && y == 119) {//7,6 L11
		return 6;
	}
	else if (x == 7 && y == 117) {//7,116 L7
		return 7;
	}
	else if (x == 7 && y == 62) {//7,6 L11
		return 9;
	}
	else if (x == 7 && y == 4) {//7,116 L7
		return 11;
	}
	else if (x == 62 && y == 5) {//7,6 L11
		return 12;
	}

	if (x >= 113 && y <= 15) {
		return 1;
	}
	else if (x >= 113 && y <= 67) {
		return 3;
	}
	else if (x >= 113 && y <= 120) {
		return 5;
	}
	else if (x <= 15 && y <= 15) {
		return 11;
	}
	else if (x <= 15 && y <= 70) {
		return 9;
	}
	else if (x <= 15 && y <= 120) {
		return 7;
	}
	else if (x <= 70 && y <= 15) {
		return 12;
	}
	else if (x <= 70 && y >= 110) {
		return 6;
	}

	return 0;
}

//侦察坐标
BWAPI::Position MapPosition::getReconLocation(BWAPI::TilePosition baseLocation){
	BWAPI::TilePosition position(0, 0);

	int x = baseLocation.x;
	int y = baseLocation.y;
	switch (getStartLocationName(baseLocation))
	{
	case 1:
		position.x = 81;
		position.y = 33;
		break;
	case 5:
		position.x = 87;
		position.y = 79;
		break;
	case 7:
		position.x = 46;
		position.y = 93;
		break;
	case 11:
		position.x = 41;
		position.y = 46;
		break;
	}

	return BWAPI::Position(position);
}

//BT 坐标
std::vector<BWAPI::TilePosition> MapPosition::getMissileTurretPosition() {
	std::vector<BWAPI::TilePosition> missileTurretTilePosition;
	switch (getStartLocationName())
	{
	case 1:
		missileTurretTilePosition.push_back(BWAPI::TilePosition(117, 5));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(117, 13));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(110, 18));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(108, 18));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(87, 11));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(86, 17));

		missileTurretTilePosition.push_back(BWAPI::TilePosition(80, 12));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(80, 15));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(105, 18));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(102, 18));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(115, 5));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(119, 13));
		break;
	case 5:
		missileTurretTilePosition.push_back(BWAPI::TilePosition(117, 115));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(117, 122));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(105, 104));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(105, 105));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(112, 86));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(117, 93));

		missileTurretTilePosition.push_back(BWAPI::TilePosition(116, 113));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(119, 122));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(105, 106));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(105, 101));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(121, 86));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(120, 89));
		break;
	case 7:
		missileTurretTilePosition.push_back(BWAPI::TilePosition(7, 114));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(8, 120));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(16, 108));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(18, 108));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(40, 116));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(40, 110));

		missileTurretTilePosition.push_back(BWAPI::TilePosition(5, 112));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(11, 121));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(20, 108));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(22, 108));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(46, 113));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(46, 116));
		break;
	case 11:
		missileTurretTilePosition.push_back(BWAPI::TilePosition(11, 15));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(12, 42));
		//missileTurretTilePosition.push_back(BWAPI::TilePosition(11, 26));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(11, 35));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(8, 43));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(11, 22));

		missileTurretTilePosition.push_back(BWAPI::TilePosition(7, 4));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(11, 5));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(11, 13));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(11, 24));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(11, 11));
		missileTurretTilePosition.push_back(BWAPI::TilePosition(7, 37));

		break;
	default:
		break;
	}

	return missileTurretTilePosition;
}

//地堡Rush 坐标
std::vector<BWAPI::TilePosition> MapPosition::getRushBunkerPosition() {
	std::vector<BWAPI::TilePosition> rushBunkerPosition;
	rushBunkerPosition.push_back(BWAPI::TilePosition(90, 20));
	rushBunkerPosition.push_back(BWAPI::TilePosition(105, 89));
	rushBunkerPosition.push_back(BWAPI::TilePosition(34, 107));
	rushBunkerPosition.push_back(BWAPI::TilePosition(21, 38));

	return rushBunkerPosition;
}

//运输机空投位置
BWAPI::Position MapPosition::getUnloadPosition() {
	BWAPI::Position position;//= InformationManager::Instance().getMyMainBaseLocation()->getPosition();
	//return position;

	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();

	if (!enemyBaseLocation)
	{
		return position;
	}
	else {
		position = enemyBaseLocation->getPosition();
		for (auto staticMinerals : enemyBaseLocation->getStaticMinerals()) {
			if (staticMinerals->getDistance(position) < 20 * 32) {
				position = staticMinerals->getInitialPosition();
				break;
			}
		}
		/*
		BWAPI::Unit mineral = BWAPI::Broodwar->getClosestUnit(position, BWAPI::Filter::IsMineralField, 10 * 32);
		if (mineral) {
			position = mineral->getInitialPosition();// BWAPI::Position(1 * 32, 0.5 * 32);
		}
		*/

		/*
		switch (getStartLocationName(enemyBaseLocation->getTilePosition()))
		{
		case 1:
			position.x = 3808;
			position.y = 234;
			break;
		case 3:
			position.x = 3808;
			position.y = 2090;
			break;
		case 5:
			position.x = 3808;
			position.y = 3850;
			break;
		case 6:
			position.x = 2048;
			position.y = 3912;
			break;
		case 7:
			position.x = 286;
			position.y = 3850;
			break;
		case 9:
			position.x = 286;
			position.y = 2086;
			break;
		case 11:
			position.x = 286;
			position.y = 230;
			break;
		case 12:
			position.x = 2047;
			position.y = 263;
			break;
		}
		*/
	}

	return position;
}

//获取我方路口
BWAPI::TilePosition MapPosition::getMyDefensivePosition() {
	return getScoutPosition(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition());
}

//获取敌方路口
BWAPI::TilePosition MapPosition::getEnemyDefensivePosition() {
	if (InformationManager::Instance().getEnemyMainBaseLocation()) {
		return getScoutPosition(InformationManager::Instance().getEnemyMainBaseLocation()->getTilePosition());
	}
	return getScoutPosition(InformationManager::Instance().getMyMainBaseLocation()->getTilePosition());
}

//获取路口位置
BWAPI::TilePosition	MapPosition::getScoutPosition(BWAPI::TilePosition baseLocation){
	BWAPI::TilePosition position;

	switch (getStartLocationName(baseLocation))
	{
	case 1:
		position.x = 81;
		position.y = 32;
		break;
	case 5:
		position.x = 86;
		position.y = 79;
		break;
	case 7:
		position.x = 47;
		position.y = 92;
		break;
	case 11:
		position.x = 40;
		position.y = 45;
		break;
	}

	return position;
}
//获取指定坐标，范围内的随机点
BWAPI::Position MapPosition::getRandomPosition(BWAPI::Position baseLocation, int radius)
{
	int i = Random::Instance().index(2);
	radius = Random::Instance().index(radius);
	if (i == 0) {
		baseLocation.x -= radius;
		baseLocation.y -= radius;
	}
	else {
		baseLocation.x += radius;
		baseLocation.y += radius;
	}

	return baseLocation;
}

//获取指定坐标，范围内的随机点
BWAPI::TilePosition MapPosition::getRandomPosition(BWAPI::TilePosition baseLocation, int radius)
{
	BWAPI::Position position(baseLocation);
	position = getRandomPosition(position, radius);

	return BWAPI::TilePosition(position);
}

//敌人的扩张点
std::vector<BWAPI::Position> MapPosition::getEnemyNextExpansionPosition(){
	std::vector<BWAPI::Position> tilePosition;

	if (InformationManager::Instance().getEnemyMainBaseLocation()) {
		//我方位置
		BWAPI::Position playerPosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();
		//敌方位置
		BWAPI::Position enemyPosition = InformationManager::Instance().getEnemyMainBaseLocation()->getPosition();

		int playerNum = getStartLocationName();
		int enemyNum = getStartLocationName(InformationManager::Instance().getEnemyMainBaseLocation()->getTilePosition());

		bool isDown = false;
		int numOffset = 14 * 32;

		for (BWTA::BaseLocation * baseLocation : BWTA::getBaseLocations())
		{
			BWAPI::Position basePosition = baseLocation->getPosition();
			switch (playerNum)
			{
			case 1:
				if (enemyNum != 5) {
					if (basePosition.x < abs(playerPosition.x - enemyPosition.x) / 2 - numOffset) {
						tilePosition.push_back(basePosition);
					}
				}
				else {
					if (basePosition.y > abs(playerPosition.y - enemyPosition.y) / 2 + numOffset) {
						tilePosition.push_back(basePosition);
					}
				}
				break;
			case 5:
				if (enemyNum != 1) {
					if (basePosition.x < abs(playerPosition.x - enemyPosition.x) / 2 - numOffset) {
						tilePosition.push_back(basePosition);
					}
				}
				else {
					if (basePosition.y < abs(playerPosition.y - enemyPosition.y) / 2 - numOffset) {
						tilePosition.push_back(basePosition);
					}
				}
				break;
			case 7:
				if (enemyNum != 11) {
					if (basePosition.x > abs(playerPosition.x - enemyPosition.x) / 2 + numOffset) {
						tilePosition.push_back(basePosition);
					}
				}
				else {
					if (basePosition.y < abs(playerPosition.y - enemyPosition.y) / 2 - numOffset) {
						tilePosition.push_back(basePosition);
					}
				}
				break;
			case 11:
				if (enemyNum != 7) {
					if (basePosition.x > abs(playerPosition.x - enemyPosition.x) / 2 - numOffset) {
						tilePosition.push_back(basePosition);
					}
				}
				else {
					if (basePosition.y > abs(playerPosition.y - enemyPosition.y) / 2 + numOffset) {
						tilePosition.push_back(basePosition);
					}
				}
				break;
			}
		}

		/*
		for (BWTA::BaseLocation * baseLocation : BWTA::getBaseLocations())
		{
			BWAPI::TilePosition basePosition = baseLocation->getTilePosition();
			//同则
			if (playerPosition.y == enemyPosition.y || playerPosition.x == enemyPosition.x) {
				//上
				if (playerPosition.x < abs(playerPosition.x - enemyPosition.x) / 2 || playerPosition.y < abs(playerPosition.y - enemyPosition.y) / 2) {
					isDown = true;
				}
			}
			else {
				//对点位置，我方在上边
				if (playerPosition.x < abs(playerPosition.x - enemyPosition.x) / 2) {
					if (playerPosition.y < abs(playerPosition.y - enemyPosition.y) / 2) {
						isDown = true;
					}
				}
			}

			if (isDown) {
				if (basePosition.y > abs(playerPosition.y - enemyPosition.y) / 2) {
					tilePosition.push_back(basePosition);
				}
			}
			else {
				if (basePosition.y < abs(playerPosition.y - enemyPosition.y) / 2 || (basePosition.y != playerPosition.y)) {
					tilePosition.push_back(basePosition);
				}
			}
		}
		*/
	}

	return tilePosition;
}