#pragma once

#include "Common.h"

namespace UAlbertaBot
{

enum class MacroCommandType
	{ None
	, Scout
	, ScoutIfNeeded
	, ScoutLocation
	, ScoutMyLocation
	, ScoutOnceOnly
	, ScoutWhileSafe   // TODO unimplemented
	, StealGas
	, StartGas
	, StopGas
	, GasUntil
	, ExtractorTrickDrone
	, ExtractorTrickZergling
	, Aggressive
	, Defensive
	, Recon
	, Squad	//增加部队
	, PullWorkers
	, PullWorkersLeaving
	, ReleaseWorkers
	, Nonadaptive
	, Lift	//飞建筑去某个位置
	, Land	//飞建筑下降到某个位置
	};

class MacroCommand
{
	MacroCommandType	_type;
    int                 _amount;
	std::string			_squadName;
	BWAPI::UnitType		_unitType;
	BWAPI::TilePosition	_pos;

public:

	static const std::list<MacroCommandType> allCommandTypes()
	{
		return std::list<MacroCommandType>
			{ MacroCommandType::Scout
			, MacroCommandType::ScoutIfNeeded
			, MacroCommandType::ScoutLocation
			, MacroCommandType::ScoutMyLocation
			, MacroCommandType::ScoutOnceOnly
			, MacroCommandType::ScoutWhileSafe
			, MacroCommandType::StartGas
			, MacroCommandType::StopGas
			, MacroCommandType::GasUntil
			, MacroCommandType::StealGas
			, MacroCommandType::ExtractorTrickDrone
			, MacroCommandType::ExtractorTrickZergling
			, MacroCommandType::Aggressive
			, MacroCommandType::Defensive
			, MacroCommandType::Recon
			, MacroCommandType::Squad
			, MacroCommandType::PullWorkers
			, MacroCommandType::PullWorkersLeaving
			, MacroCommandType::ReleaseWorkers
			, MacroCommandType::Nonadaptive
			, MacroCommandType::Lift
			, MacroCommandType::Land
		};
	}

	// Default constructor for when the value doesn't matter.
	MacroCommand()
		: _type(MacroCommandType::None)
		, _amount(0)
	{
	}

	MacroCommand(MacroCommandType type)
		: _type(type)
        , _amount(0)
	{
		UAB_ASSERT(!hasArgument(type),"missing MacroCommand argument");
	}

	MacroCommand(MacroCommandType type, int amount)
		: _type(type)
		, _amount(amount)
	{
		UAB_ASSERT(hasArgument(type), "extra MacroCommand argument");
	}

	MacroCommand(MacroCommandType type, int amount, BWAPI::UnitType unitType)
		: _type(type)
		, _amount(amount)
		, _unitType(unitType)
	{
		UAB_ASSERT(hasArgument(type), "extra MacroCommand argument");
	}

	MacroCommand(MacroCommandType type, BWAPI::UnitType unitType, BWAPI::TilePosition pos)
		: _type(type)
		, _pos(pos)
		, _unitType(unitType)
	{
	}

	MacroCommand(MacroCommandType type, std::string squadName, BWAPI::UnitType unitType, int num, BWAPI::TilePosition pos)
		: _type(type)
		, _pos(pos)
		, _unitType(unitType)
		, _amount(num)
		, _squadName(squadName)
	{
	}

    const int getAmount() const
    {
        return _amount;
    }

	const MacroCommandType & getType() const
    {
        return _type;
    }

	const BWAPI::UnitType & getUnitType() const
	{
		return _unitType;
	}

	const BWAPI::TilePosition & getPosition() const
	{
		return _pos;
	}

	const std::string & getSquadName() const
	{
		return _squadName;
	}

	// The command has a numeric argument, the _amount.
	//该命令有一个数值参数, 即 _amount。
	static const bool hasArgument(MacroCommandType t)
	{
		return
			t == MacroCommandType::GasUntil ||
			t == MacroCommandType::PullWorkers || 
			t == MacroCommandType::PullWorkersLeaving ||
			t == MacroCommandType::Recon;
	}

	static const bool hasUnitType(MacroCommandType t)
	{
		return
			t == MacroCommandType::Recon ||
			t == MacroCommandType::Lift ||
			t == MacroCommandType::Land;
	}

	static const bool hasPosition(MacroCommandType t)
	{
		return
			t == MacroCommandType::Lift ||
			t == MacroCommandType::Land;
	}

	static const std::string getName(MacroCommandType t)
	{
		if (t == MacroCommandType::Scout)
		{
			return "go scout";
		}
		if (t == MacroCommandType::ScoutIfNeeded)
		{
			return "go scout if needed";
		}
		if (t == MacroCommandType::ScoutLocation)
		{
			return "go scout location";
		}
		if (t == MacroCommandType::ScoutMyLocation)
		{
			return "go scout my location";
		}
		if (t == MacroCommandType::ScoutOnceOnly)
		{
			return "go scout once around";
		}
		if (t == MacroCommandType::ScoutWhileSafe)
		{
			return "go scout while safe";
		}
		if (t == MacroCommandType::StartGas)
		{
			return "go start gas";
		}
		if (t == MacroCommandType::StopGas)
		{
			return "go stop gas";
		}
		if (t == MacroCommandType::GasUntil)
		{
			return "go gas until";
		}
		if (t == MacroCommandType::StealGas)
		{
			return "go steal gas";
		}
		if (t == MacroCommandType::ExtractorTrickDrone)
		{
			return "go extractor trick drone";
		}
		if (t == MacroCommandType::ExtractorTrickZergling)
		{
			return "go extractor trick zergling";
		}
		if (t == MacroCommandType::Aggressive)
		{
			return "go aggressive";
		}
		if (t == MacroCommandType::Defensive)
		{
			return "go defensive";
		}
		if (t == MacroCommandType::Recon)
		{
			return "set recon";
		}
		if (t == MacroCommandType::Squad)
		{
			return "set squad";
		}
		if (t == MacroCommandType::PullWorkers)
		{
			return "go pull workers";
		}
		if (t == MacroCommandType::PullWorkersLeaving)
		{
			return "go pull workers leaving";
		}
		if (t == MacroCommandType::ReleaseWorkers)
		{
			return "go release workers";
		}
		if (t == MacroCommandType::Nonadaptive)
		{
			return "go nonadaptive";
		}
		if (t == MacroCommandType::Lift)
		{
			return "lift";
		}
		if (t == MacroCommandType::Land)
		{
			return "land";
		}

		UAB_ASSERT(t == MacroCommandType::None, "unrecognized MacroCommandType");
		return "go none";
	}

	const std::string getName() const
	{
		std::string name = getName(_type);
		if (hasArgument(_type))
		{
			name += " " + std::to_string(_amount);
		}

		if (hasUnitType(_type))
		{
			name += " " + NiceMacroActName(_unitType.toString());
		}

		if (hasPosition(_type))
		{
			name += " [" + std::to_string(_pos.x) + "," + std::to_string(_pos.y) + "]";
		}

		return name;
	}
};
}