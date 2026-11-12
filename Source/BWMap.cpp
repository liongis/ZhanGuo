#include "Common.h"
#include "BWMap.h"
#include "InformationManager.h"
#include "Random.h"

using namespace UAlbertaBot;

std::string BWMap::getStartLocationName(BWAPI::TilePosition baseLocation) {
	for (const auto & startLocation : StartLocations)
	{
		const BWAPI::TilePosition & pos = startLocation.second;

		if (baseLocation == pos)
		{
			return startLocation.first;
		}
	}

	return "";
}

BWMap & BWMap::Instance()
{
	static BWMap instance;
	return instance;
}

BWMap::BWMap() {}
