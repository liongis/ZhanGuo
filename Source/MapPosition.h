#pragma once
#include <Common.h>

namespace UAlbertaBot
{
	class MapPosition
	{
		MapPosition();
	public:
		static MapPosition &	Instance();

		int									getStartLocationName(BWAPI::TilePosition baseLocation);
		BWAPI::Position						getUnloadPosition();
		BWAPI::TilePosition					getScoutPosition(BWAPI::TilePosition baseLocation);
		std::vector<BWAPI::TilePosition>	getMissileTurretPosition();
		std::vector<BWAPI::TilePosition>	getRushBunkerPosition();
		BWAPI::Position						getReconLocation(BWAPI::TilePosition baseLocation);

		BWAPI::TilePosition					getMyDefensivePosition();
		BWAPI::TilePosition					getEnemyDefensivePosition();

		BWAPI::TilePosition		getRandomPosition(BWAPI::TilePosition baseLocation, int radius);
		BWAPI::Position			getRandomPosition(BWAPI::Position baseLocation, int radius);

		std::vector<BWAPI::Position>	getEnemyNextExpansionPosition();//获取敌方下一个扩展点

	};
}

