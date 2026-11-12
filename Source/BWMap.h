#pragma once
#include <Common.h>

namespace UAlbertaBot
{
	class BWMap
	{
		BWMap();
	public:
		static BWMap &	Instance();

		std::map<std::string, BWAPI::TilePosition> StartLocations;//开始点位
		std::map<std::string, BWAPI::Position> UnloadPoints;//空投点
		std::map<std::string, BWAPI::Position> ChokePoints;//阻塞点

		std::string getStartLocationName(BWAPI::TilePosition baseLocation = BWAPI::Broodwar->self()->getStartLocation());
	};
}

