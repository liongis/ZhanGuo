#include "Common.h"
#include "Chat.h"
//#include "InformationManager.h"
#include "Random.h"

using namespace UAlbertaBot;

Chat & Chat::Instance()
{
	static Chat instance;
	return instance;
}

Chat::Chat() {
	if (!isInit) {
		initChatString();
	}
}

void Chat::initChatString() {
	if (!isInit && chatString.size() == 0) {
		isInit = true;

		chatString.push_back("6 fenzhong bu yao da wo, bu ran wo jiu GG!");
		chatString.push_back("AI QQ Qun: 656186623");
		chatString.push_back("Can you speak Chinese?");
		chatString.push_back("Wo shi AI, qing dian nue.");
	}
}

void Chat::initChatString(std::vector<std::string> cString)
{
	isInit = true;
	chatString = cString;
}

void Chat::update()
{
	if (!Config::Chat::OpenChat) return;

	int frame = BWAPI::Broodwar->getFrameCount();

	if (frame < lastFrame) return;

	if (frame < 4 * 60 * 24 && frame > 500) {
		int index = Random::Instance().index(chatString.size());
		std::string text = chatString.at(index);
		sendText(text, 30 * 24);
		return;
	}

	/*
	if (frame < 6 * 60 * 24) {
		BWAPI::Position chokePoint = InformationManager::Instance().getEnemyChokePoint();
		if (chokePoint.isValid() && chokePoint.x != 0 && chokePoint.y != 0) {
			if (BWAPI::Broodwar->getUnitsInRadius(chokePoint, 3 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::IsBuilding).size() > 1) {
				sendText("ni you du kou, you yi shi ma?", 20 * 24);
				return;
			}
		}
	}
	*/

	/*
	if (frame < 6 * 60 * 24) {
		for each (BWAPI::Unit unit in BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->isUnderAttack())
			{
				sendText("hai mei 6 fenzhong ni you da wo, wo yao GG!", 1 * 24);
				return;
			}
		}
	}
	*/

	if (frame > 4 * 60 * 24 && frame < 10 * 60 * 24 && (frame % ( 60 * 24) == 0)) {
		BWAPI::Broodwar->sendText("%d fenzhong.", frame / ( 60 * 24));

		lastFrame = BWAPI::Broodwar->getFrameCount() + 1 * 24;
	}
}

void Chat::sendText(std::string text) {
	sendText(text, Config::Chat::ChatFrame);
}

void Chat::sendText(std::string text, int nextFrame) {
	BWAPI::Broodwar->sendText(text.c_str());

	lastFrame = BWAPI::Broodwar->getFrameCount() + nextFrame;
}