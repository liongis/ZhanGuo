#include "Config.h"
#include "UABAssert.h"

// Most values here are default values that apply if the configuration entry
// is missing from the config file, or is invalid.

// The ConfigFile record tells where to find the config file, so it's different.

namespace Config
{
    namespace ConfigFile
    {
        bool ConfigFileFound                = false;
        bool ConfigFileParsed               = false;
        std::string ConfigFileLocation      = "bwapi-data/AI/ZhanGuo_1.0.json";
    }

	namespace IO
	{
		std::string ReadDir					= "bwapi-data/read/";
		std::string WriteDir				= "bwapi-data/write/";
		bool ReadOpponentModel				= false;
		bool WriteOpponentModel				= false;
	}

	namespace Strategy
    {
        std::string ProtossStrategyName     = "1ZealotCore";			// default
        std::string TerranStrategyName      = "11Rax";					// default
        std::string ZergStrategyName        = "9PoolSpeed";				// default
        std::string StrategyName            = "9PoolSpeed";
		int maxAttackUnits					= 160;//大于此单位开始进攻
		int minAttackUnits					= 60;//小于此单位则防守
        bool ScoutHarassEnemy               = true;
		bool AutoGasSteal                   = true;
		double RandomGasStealRate           = 0.0;
		bool SurrenderWhenHopeIsLost        = true;
        bool UseEnemySpecificStrategy       = true;
        bool FoundEnemySpecificStrategy     = false;
		bool NeedsToRegroup					= true;						//是否需要重组
    }

    namespace BotInfo
    {
        std::string BotName                 = "ZhanGuo";
        std::string Authors                 = "LionGIS";
        bool PrintInfoOnStart               = false;
    }

	namespace BaseLocations
	{
		extern bool LocationsModel = false;
		extern BWAPI::Position Self = BWAPI::Position(320, 2672);
		extern BWAPI::Position Enemy = BWAPI::Position(3776, 400);
	}

    namespace BWAPIOptions
    {
        int SetLocalSpeed                   = 42;
        int SetFrameSkip                    = 0;
        bool EnableUserInput                = true;
        bool EnableCompleteMapInformation   = false;
    }

    namespace Tournament
    {
        int GameEndFrame                    = 86400;
    }

	namespace Chat
	{
		int ChatFrame = 30 * 24;
		bool OpenChat = true;
	}

    namespace Debug
    {
        bool DrawGameInfo                   = true;
        bool DrawUnitHealthBars             = false;
        bool DrawProductionInfo             = true;
        bool DrawBuildOrderSearchInfo       = false;
        bool DrawScoutInfo                  = false;
        bool DrawResourceInfo               = false;
        bool DrawWorkerInfo                 = false;
        bool DrawModuleTimers               = false;
        bool DrawReservedBuildingTiles      = false;
        bool DrawCombatSimulationInfo       = false;
        bool DrawBuildingInfo               = false;
        bool DrawMouseCursorInfo            = false;
        bool DrawEnemyUnitInfo              = false;
        bool DrawBWTAInfo                   = false;
        bool DrawMapGrid                    = false;
		bool DrawMapDistances				= false;
		bool DrawBaseInfo					= false;
		bool DrawStrategyBossInfo			= false;
		bool DrawUnitTargetInfo				= false;
		bool DrawUnitOrders					= false;
        bool DrawSquadInfo                  = false;
        bool DrawBOSSStateInfo              = false;

        std::string ErrorLogFilename        = "ZhanGuo_ErrorLog.txt";
        bool LogAssertToErrorFile           = false;

        BWAPI::Color ColorLineTarget        = BWAPI::Colors::White;
        BWAPI::Color ColorLineMineral       = BWAPI::Colors::Cyan;
        BWAPI::Color ColorUnitNearEnemy     = BWAPI::Colors::Red;
        BWAPI::Color ColorUnitNotNearEnemy  = BWAPI::Colors::Green;
    }

    namespace Micro
    {
        bool KiteWithRangedUnits            = true;
        std::set<BWAPI::UnitType> KiteLongerRangedUnits;
        bool WorkersDefendRush              = false;
		int RetreatMeleeUnitShields         = 0;
        int RetreatMeleeUnitHP              = 0;
		int CombatSimRadius					= 300;      // radius of units around frontmost unit for combat sim
        int UnitNearEnemyRadius             = 600;      // radius to consider a unit 'near' to an enemy unit
		int ScoutDefenseRadius				= 600;		// radius to chase enemy scout worker
		int DefenseFrame					= 8 * 60 * 24;//开始防守的时间
		int RetreatFrame					= 1 * 24;	//撤退时间
    }

    namespace Macro
    {
        int BOSSFrameLimit                  = 160;
        int WorkersPerRefinery              = 3;
		double WorkersPerPatch              = 3.0;
		int AbsoluteMaxWorkers				= 75;
        int BuildingSpacing                 = 1;
		int BuildingWaiting                 = 6;//修改：建造等待时间
        int PylonSpacing                    = 3;
		int ProductionJamFrameLimit			= 360;
		bool ExpandToIslands				= false;
    }

    namespace Tools
    {
        extern int MAP_GRID_SIZE            = 320;      // size of grid spacing in MapGrid
    }
}
