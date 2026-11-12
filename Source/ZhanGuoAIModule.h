#pragma once

#include <BWAPI.h>
#include "GameCommander.h"
#include <iostream>
#include <fstream>
#include "Logger.h"
#include "MapTools.h"
#include "Config.h"

#include "rapidjson\document.h"
/*
#include "../../BWEM/source/bwem.h"
using namespace BWEM;
namespace { auto & theMap = BWEM::Map::Instance(); }
*/
namespace UAlbertaBot
{

class ZhanGuoAIModule : public BWAPI::AIModule
{

public:

	void	onStart();
	void	onFrame();
	void	onEnd(bool isWinner);
	void	onUnitDestroy(BWAPI::Unit unit);
	void	onUnitMorph(BWAPI::Unit unit);
	void	onSendText(std::string text);
	void	onUnitCreate(BWAPI::Unit unit);
	void	onUnitComplete(BWAPI::Unit unit);
	void	onUnitShow(BWAPI::Unit unit);
	void	onUnitHide(BWAPI::Unit unit);
	void	onUnitRenegade(BWAPI::Unit unit);

	void	onPlayerLeft(BWAPI::Player player);
};

}