/* 
 *----------------------------------------------------------------------
 * Steamhammer entry point.
 *----------------------------------------------------------------------
 */

#include "JSONTools.h"

#include "ZhanGuoAIModule.h"

#include "Common.h"
#include "OpponentModel.h"
#include "ParseUtils.h"
#include "UnitUtil.h"
#include "GamePlayer.h"

using namespace UAlbertaBot;

// This gets called when the bot starts.
void ZhanGuoAIModule::onStart()
{
	// Check if this is a replay
	if (BWAPI::Broodwar->isReplay()) return;

    // Initialize BOSS, the Build Order Search System
    //BOSS::init();

	// Parse the bot's configuration file.
	// Change this file path to point to your config file.
    // Any relative path name will be relative to Starcraft installation folder
	// The config depends on the map and must be read after the map is analyzed.
    ParseUtils::ParseConfigFile(Config::ConfigFile::ConfigFileLocation);

    // Set our BWAPI options here    
	if (Config::BWAPIOptions::SetLocalSpeed > -1) {
		BWAPI::Broodwar->setLocalSpeed(Config::BWAPIOptions::SetLocalSpeed);
	}
	
	BWAPI::Broodwar->setFrameSkip(Config::BWAPIOptions::SetFrameSkip);
    
    if (Config::BWAPIOptions::EnableCompleteMapInformation)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);
    }

    if (Config::BWAPIOptions::EnableUserInput)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);
    }

	StrategyManager::Instance().setOpeningGroup();    // may depend on config and/or opponent model
	StrategyManager::Instance().setAggressiveScore();    // may depend on config and/or opponent model

    if (Config::BotInfo::PrintInfoOnStart)
    {
        BWAPI::Broodwar->printf("%s by %s, based on Steamhammer.", Config::BotInfo::BotName.c_str(), Config::BotInfo::Authors.c_str());
	}

	// Call BWTA to read and analyze the current map.
	// Very slow if the map has not been seen before, so that info is not cached.
	BWTA::readMap();
	BWTA::analyze();

	GamePlayer::Instance().onStart();
}

void ZhanGuoAIModule::onEnd(bool isWinner)
{
	// Return if the game is a replay or is paused
	if (BWAPI::Broodwar->isReplay() || BWAPI::Broodwar->isPaused() || !BWAPI::Broodwar->self())
		return;

	OpponentModel::Instance().setWin(isWinner);
	OpponentModel::Instance().write();

	GamePlayer::Instance().onEnd(isWinner);
}

void ZhanGuoAIModule::onFrame()
{
	// Return if the game is a replay or is paused
	if (BWAPI::Broodwar->isReplay() || BWAPI::Broodwar->isPaused() || !BWAPI::Broodwar->self())
		return;
	
	if (!Config::ConfigFile::ConfigFileFound)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Not Found", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without its configuration file", white, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->drawTextScreen(10, 45, "%cCheck that the file below exists. Incomplete paths are relative to Starcraft directory", white);
        BWAPI::Broodwar->drawTextScreen(10, 60, "%cYou can change this file location in Config::ConfigFile::ConfigFileLocation", white);
        BWAPI::Broodwar->drawTextScreen(10, 75, "%cFile Not Found (or is empty): %c %s", white, green, Config::ConfigFile::ConfigFileLocation.c_str());
        return;
    }
    else if (!Config::ConfigFile::ConfigFileParsed)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Parse Error", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without a properly formatted configuration file", white, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->drawTextScreen(10, 45, "%cThe configuration file was found, but could not be parsed. Check that it is valid JSON", white);
        BWAPI::Broodwar->drawTextScreen(10, 60, "%cFile Not Parsed: %c %s", white, green, Config::ConfigFile::ConfigFileLocation.c_str());
        return;
    }

	GameCommander::Instance().update();
}

void ZhanGuoAIModule::onUnitDestroy(BWAPI::Unit unit)
{
	// Return if the game is a replay or is paused
	if (BWAPI::Broodwar->isReplay() || BWAPI::Broodwar->isPaused() || !BWAPI::Broodwar->self())
		return;

	GameCommander::Instance().onUnitDestroy(unit);
}

void ZhanGuoAIModule::onUnitMorph(BWAPI::Unit unit)
{
	// Return if the game is a replay or is paused
	if (BWAPI::Broodwar->isReplay() || BWAPI::Broodwar->isPaused() || !BWAPI::Broodwar->self())
		return;

	GameCommander::Instance().onUnitMorph(unit);
}

void ZhanGuoAIModule::onSendText(std::string text) 
{ 
	ParseUtils::ParseTextCommand(text);
}

void ZhanGuoAIModule::onUnitCreate(BWAPI::Unit unit)
{ 
	// Return if the game is a replay or is paused
	if (BWAPI::Broodwar->isReplay() || BWAPI::Broodwar->isPaused() || !BWAPI::Broodwar->self())
		return;

	GameCommander::Instance().onUnitCreate(unit);
}

void ZhanGuoAIModule::onUnitComplete(BWAPI::Unit unit)
{
	// Return if the game is a replay or is paused
	if (BWAPI::Broodwar->isReplay() || BWAPI::Broodwar->isPaused() || !BWAPI::Broodwar->self())
		return;

	GameCommander::Instance().onUnitComplete(unit);
}

void ZhanGuoAIModule::onUnitShow(BWAPI::Unit unit)
{ 
	// Return if the game is a replay or is paused
	if (BWAPI::Broodwar->isReplay() || BWAPI::Broodwar->isPaused() || !BWAPI::Broodwar->self())
		return;

	GameCommander::Instance().onUnitShow(unit);
}

void ZhanGuoAIModule::onUnitHide(BWAPI::Unit unit)
{ 
	// Return if the game is a replay or is paused
	if (BWAPI::Broodwar->isReplay() || BWAPI::Broodwar->isPaused() || !BWAPI::Broodwar->self())
		return;

	GameCommander::Instance().onUnitHide(unit);
}

void ZhanGuoAIModule::onUnitRenegade(BWAPI::Unit unit)
{ 
	// Return if the game is a replay or is paused
	if (BWAPI::Broodwar->isReplay() || BWAPI::Broodwar->isPaused() || !BWAPI::Broodwar->self())
		return;

	GameCommander::Instance().onUnitRenegade(unit);
}

void ZhanGuoAIModule::onPlayerLeft(BWAPI::Player player)
{
	// Return if the game is a replay or is paused
	if (BWAPI::Broodwar->isReplay() || BWAPI::Broodwar->isPaused() || !BWAPI::Broodwar->self())
		return;

	GameCommander::Instance().onPlayerLeft(player);
}