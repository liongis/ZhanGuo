#pragma once

#include "Common.h"
#include "OpponentPlan.h"
#include "PlayerSnapshot.h"

#include <exception>

// NOTE
// This class does only a little checking of its input file format. Feed it no bad files.

namespace UAlbertaBot
{

class GamePlayer
{
private:
	std::string _filename;

	const std::string fileFormatVersion = "1.4";

	const std::string gameEndMark = "END GAME";

	const std::string currentDate()
	{
		time_t     now = time(0);
		struct tm  tstruct;
		char       buf[80];
		//tstruct = *localtime(&now);
		localtime_s(&tstruct, &now);
		strftime(buf, sizeof(buf), "%Y-%m-%d", &tstruct);

		return buf;
	}

	const std::string currentDateTime()
	{
		time_t     now = time(0);
		struct tm  tstruct;
		char       buf[80];
		//tstruct = *localtime(&now);
		localtime_s(&tstruct, &now);
		strftime(buf, sizeof(buf), "%Y-%m-%d_%X", &tstruct);

		return buf;
	}

public:
	GamePlayer();

	void write(std::ostream & output);
	void onStart();
	void onEnd(bool isWinner);

	static GamePlayer & Instance();
};

}