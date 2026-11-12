#include "GamePlayer.h"

#include "GameCommander.h"
#include "InformationManager.h"
#include "Logger.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Constructor for the record of the current game.
// When this object is initialized, the opening and some other items are not yet known.
GamePlayer::GamePlayer()
{
}

void GamePlayer::onStart() {
	_filename = "bwapi-data/read/ZhanGuo_" + currentDate() + ".txt";

	std::stringstream msg;

	msg << "GameStartTime:" << currentDateTime() << '\n';

	msg << "MapName:" << BWAPI::Broodwar->mapFileName() << '\n';
	msg << "MapHash:" << BWAPI::Broodwar->mapHash() << '\n';

	msg << "Strategy:" << Config::Strategy::StrategyName.c_str() << '\n';

	msg << "Allies:\n";
	std::string name = "";
	for (auto ally : BWAPI::Broodwar->allies()) {
		name = ally->getName();
		std::replace(name.begin(), name.end(), ' ', '_');
		msg << name << ":" << ally->getRace() << "\n";
	}

	msg << "Enemies:\n";
	for (auto enemy : BWAPI::Broodwar->enemies()) {
		name = enemy->getName();
		std::replace(name.begin(), name.end(), ' ', '_');
		msg << name << ":" << enemy->getRace() << "\n";
	}

	Logger::LogAppendToFile(_filename, msg.str());
}

void GamePlayer::onEnd(bool isWinner) {
	std::stringstream msg;
	/*
	msg << "Allies:\n";
	std::string name = "";
	for (auto ally : BWAPI::Broodwar->allies()) {
		name = ally->getName();
		std::replace(name.begin(), name.end(), ' ', '_');
		msg << name << ":" << ally->getRace() << "\n";
	}

	msg << "Enemies:\n";
	for (auto enemy : BWAPI::Broodwar->enemies()) {
		name = enemy->getName();
		std::replace(name.begin(), name.end(), ' ', '_');
		msg << name << ":" << enemy->getRace() << "\n";
	}
	*/
	msg << "Result:";

	//小于2分钟不参于计算
	if (BWAPI::Broodwar->getFrameCount() > 2 * 60 * 24) {
		if (isWinner) {
			msg << "Win" << '\n';
		}
		else {
			msg << "Lose" << '\n';
		}
	}

	msg << "GameEndTime:" << currentDateTime() << '\n';
	msg << "\n";

	Logger::LogAppendToFile(_filename, msg.str());
}

void GamePlayer::write(std::ostream & output)
{
	// We only now notice that there was an expected enemy opening plan.
	// Can't initialize this right off, and no point in tracking it in during the game.
	/*
	expectedEnemyPlan = OpponentModel::Instance().getInitialExpectedEnemyPlan();

	output << fileFormatVersion << '\n';
	output <<
		RaceChar(ourRace) <<
		'v' <<
		(enemyIsRandom ? "R" : "") << RaceChar(enemyRace) << '\n';
	output << mapName << '\n';
	output << openingName << '\n';
	output << OpeningPlanString(expectedEnemyPlan) << '\n';
	output << OpeningPlanString(enemyPlan) << '\n';
	output << (win ? '1' : '0') << '\n';
	output << frameScoutSentForGasSteal << '\n';
	output << (gasStealHappened ? '1' : '0') << '\n';
	output << frameEnemyScoutsOurBase << '\n';
	output << frameEnemyGetsCombatUnits << '\n';
	output << frameEnemyGetsAirUnits << '\n';
	output << frameEnemyGetsStaticAntiAir << '\n';
	output << frameEnemyGetsMobileAntiAir << '\n';
	output << frameEnemyGetsCloakedUnits << '\n';
	output << frameEnemyGetsStaticDetection << '\n';
	output << frameEnemyGetsMobileDetection << '\n';
	output << frameGameEnds << '\n';

	// TODO skip the snapshots for now
	// for (const auto & snap : snapshots)
	// {
	// 	writeGameSnapshot(output, snap);
	// }

	output << gameEndMark << '\n';
	*/
}

GamePlayer & GamePlayer::Instance()
{
	static GamePlayer instance;
	return instance;
}
