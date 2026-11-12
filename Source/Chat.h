#pragma once
#include <Common.h>
#include <Config.h>

namespace UAlbertaBot
{
	class Chat
	{
		Chat();

		std::vector<std::string> chatString;
		bool			isInit = false;
		int				lastFrame;//下一次播放时间

	public:
		static Chat &	Instance();


		void			update();

		void			initChatString();
		void			initChatString(std::vector<std::string> chatString);

		void			sendText(std::string text);
		void			sendText(std::string text, int nextFrame);
	};
}

