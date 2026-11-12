#pragma once

#include "Common.h"
#include "BWTA.h"

#include "Base.h"
#include "UnitData.h"
#include "LocutusMapGrid.h"

//#include "..\..\SparCraft\source\SparCraft.h"

namespace UAlbertaBot
{

class InformationManager 
{
	BWAPI::Player	_self;
    BWAPI::Player	_enemy;

	bool			_enemyProxy;

	bool			_weHaveCombatUnits;
	bool			_enemyHasCombatUnits;
	bool			_enemyHasStaticAntiAir;
	bool			_enemyHasStaticAntiGround;
	bool			_enemyHasAntiAir;
	bool			_enemyHasAirTech;
	bool			_enemyHasCloakTech;
	bool			_enemyHasMobileCloakTech;
	bool			_enemyHasOverlordHunters;
	bool			_enemyHasStaticDetection;
	bool			_enemyHasMobileDetection;
	bool			_enemyHasDarkSwarm;

	int				_zergSpireOverFrame = 0;//飞龙塔完成时间
	int				_psionicStormFrame = 0;//闪电完成时间

	bool			_hasTransports;

	std::map<BWAPI::Unit, int> _attackTargets;//攻击目标及对它的伤害值

	std::map<BWAPI::Unit, int> _hazardousArea;//危险区域

	std::vector<BWAPI::Position> _myRegionVertices;

	BWAPI::Position			_attackPosition;				//进攻点
	BWAPI::Position			_defensivePosition;				//防守点
	int						_lastDefensiveChange;

	BWAPI::Position			_rallyPoint;//集结点
	BWAPI::Position			_dropPoint;//下降点
	BWAPI::Position			_mainAttackTarget;//主要进攻点

	BWAPI::Position			_nearEnemyTank;//最接近敌人的坦克

	BWAPI::Position			_enemyChokePoint;//敌人的阻塞点

	double					_enemyFightScore = 0;//敌人的战斗力总和
	double					_selfFightScore = 0;//我方的战斗力总和

	std::map<BWAPI::Player, UnitData>                   _unitData;
	std::map<BWAPI::Player, BWTA::BaseLocation *>       _mainBaseLocations;
	BWTA::BaseLocation *								_myNaturalBaseLocation;  // whether taken yet or not
	BWTA::BaseLocation *								_enemyNaturalBaseLocation;  // whether taken yet or not
	BWAPI::Position										_myNearestChokepoint;//最近的阻塞点
	BWAPI::Position			_unloadPosition;//空投点
	BWAPI::Position			_myUnloadPosition;//自家空投点

	std::map<BWTA::BaseLocation *, Base *>				_theBases;

	std::map<BWAPI::Player, std::set<BWTA::Region *> >  _occupiedRegions;        // contains any building
	std::map<BWAPI::Position, int>						_reconLocations;		//侦察点

	BWAPI::Unitset										_staticDefense;
	BWAPI::Unitset										_comsatStation;
	BWAPI::Unitset										_factory;
	BWAPI::Unitset										_commandCenter;
	BWAPI::Unitset										_tanks;

	LocutusMapGrid                      _myUnitGrid;

	std::map<BWAPI::WeaponType, int> enemyWeaponDamage;
	std::map<BWAPI::WeaponType, int> enemyWeaponRange;
	std::map<BWAPI::UnitType, int> enemyUnitCooldown;
	std::map<BWAPI::UnitType, double> enemyUnitTopSpeed;
	std::map<BWAPI::UnitType, int> enemyUnitArmor;

	InformationManager();

	void					initializeTheBases();
	void                    initializeRegionInformation();
	void					initializeNaturalBase();
	void					initializeReconLocation();

	int                     getIndex(BWAPI::Player player) const;

	void					baseInferred(BWTA::BaseLocation * base);
	void					baseFound(BWAPI::Unit depot);
	void					baseFound(BWTA::BaseLocation * base, BWAPI::Unit depot);
	void					baseLost(BWAPI::TilePosition basePosition);
	void					baseLost(BWTA::BaseLocation * base);
	void					maybeAddBase(BWAPI::Unit unit);
	bool					closeEnough(BWAPI::TilePosition a, BWAPI::TilePosition b);
	void					chooseNewMainBase();

	void					maybeAddStaticDefense(BWAPI::Unit unit);
	void					maybeAddComsatStation(BWAPI::Unit unit);

	void                    updateUnit(BWAPI::Unit unit);
    void                    updateUnitInfo();
    void                    updateBaseLocationInfo();
	void					enemyBaseLocationFromOverlordSighting();
	void					enemyBaseLocationNatural();
	void					updateTheBases();
	void					updateGoneFromLastPosition();

	double					getUnitFightScore(BWAPI::Unit unit);//获取单位的战斗分数
public:

    void                    update();

	void					setEnemy(BWAPI::Player player);
	BWAPI::Player			getEnemy(){ return _enemy; };

	bool					isDukou = false;//

    // event driven stuff
	void					onUnitShow(BWAPI::Unit unit)        { updateUnit(unit); maybeAddBase(unit); }
	void					onUnitHide(BWAPI::Unit unit)        { updateUnit(unit); removeAttackTargets(unit); }
	void					onUnitCreate(BWAPI::Unit unit)		{ updateUnit(unit); maybeAddBase(unit); }
	void					onUnitComplete(BWAPI::Unit unit)    { updateUnit(unit); maybeAddStaticDefense(unit); maybeAddComsatStation(unit); }
	void					onUnitMorph(BWAPI::Unit unit)       { updateUnit(unit); maybeAddBase(unit); }
    void					onUnitRenegade(BWAPI::Unit unit)    { updateUnit(unit); }
    void					onUnitDestroy(BWAPI::Unit unit);

	void                    updateOccupiedRegions(BWTA::Region * region, BWAPI::Player player);//更新占领区域

	bool					isEnemyBuildingInRegion(BWTA::Region * region);
    int						getNumUnits(BWAPI::UnitType type, BWAPI::Player player) const;
    bool					nearbyForceHasCloaked(BWAPI::Position p,BWAPI::Player player,int radius);

    void                    getNearbyForce(std::vector<UnitInfo> & unitInfo,BWAPI::Position p,BWAPI::Player player,int radius);

    const UIMap &           getUnitInfo(BWAPI::Player player) const;
	BWAPI::Unitset			getUnits(BWAPI::Player player, BWAPI::UnitType type);

	std::set<BWTA::Region *> &  getOccupiedRegions(BWAPI::Player player);

    BWTA::BaseLocation *    getMainBaseLocation(BWAPI::Player player);
	BWTA::BaseLocation *	getMyMainBaseLocation();
	BWTA::BaseLocation *	getEnemyMainBaseLocation();

	Base*					baseAt(BWAPI::TilePosition baseTilePosition);

	void					setEnemyMainBaseLocation(std::string baseName);
	void					setEnemyMainBaseLocation(BWTA::BaseLocation * baseLocation);

	void					clearEnemyMainBaseLocation(){ _mainBaseLocations[_enemy] = BWTA::getStartLocation(_enemy); };

	BWAPI::Player			getBaseOwner(BWTA::BaseLocation * base);
	BWAPI::Unit 			getBaseDepot(BWTA::BaseLocation * base);
	BWTA::BaseLocation *	getMyNaturalLocation();

	BWAPI::Position			getMyNearestChokepoint() { return _myNearestChokepoint; };

	BWAPI::Unitset			getStaticDefense(){ return _staticDefense; };
	BWAPI::Unitset			getComsatStation(){ return _comsatStation; };
	BWAPI::Unitset			getFactory(){ return _factory; };
	BWAPI::Unitset			getCommandCenter(){ return _commandCenter; };
	BWAPI::Unitset			getTankUnits(){ return _tanks; };

	int						getTotalNumBases() const;
	int						getNumBases(BWAPI::Player player);
	int						getNumFreeLandBases();
	int						getMyNumMineralPatches();
	int						getMyNumGeysers();
	void					getMyGasCounts(int & nRefineries, int & nFreeGeysers);

	BWTA::BaseLocation *	getEnemyNaturalLocation();

	bool					getEnemyProxy() { return _enemyProxy; };

	void					maybeChooseNewMainBase();

	bool					isFindEnemyMainBase();//是否所有的敌人基地都找到

	bool					isBaseReserved(BWTA::BaseLocation * base);
	void					reserveBase(BWTA::BaseLocation * base);
	void					unreserveBase(BWTA::BaseLocation * base);
	void					unreserveBase(BWAPI::TilePosition baseTilePosition);

	int						getAir2GroundSupply(BWAPI::Player player) const;

	bool					weHaveCombatUnits();
	bool					myHasTransports();

	bool					enemyHasCombatUnits();
	bool					enemyHasStaticAntiAir();
	bool					enemyHasStaticAntiGround();
	bool					enemyHasAntiAir();
	bool					enemyHasAirTech();
	bool                    enemyHasCloakTech();
	bool                    enemyHasMobileCloakTech();
	bool					enemyHasOverlordHunters();
	bool					enemyHasStaticDetection();
	bool					enemyHasMobileDetection();
	bool					enemyHasDarkSwarm();//敌人是否有黄雾

	int                     getWeaponDamage(BWAPI::Player player, BWAPI::WeaponType wpn);
	int                     getWeaponRange(BWAPI::Player player, BWAPI::WeaponType wpn);
	int                     getUnitCooldown(BWAPI::Player player, BWAPI::UnitType type);
	double                  getUnitTopSpeed(BWAPI::Player player, BWAPI::UnitType type);
	int                     getUnitArmor(BWAPI::Player player, BWAPI::UnitType type);

	// BWAPI::Unit				nearestGroundStaticDefense(BWAPI::Position pos) const;
	// BWAPI::Unit				nearestAirStaticDefense(BWAPI::Position pos) const;
	BWAPI::Unit				nearestShieldBattery(BWAPI::Position pos) const;

	int						nScourgeNeeded();           // zerg specific

    void                    drawExtendedInterface();
    void                    drawUnitInformation(int x,int y);
    void                    drawMapInformation();
	void					drawBaseInformation(int x, int y);

    const UnitData &        getUnitData(BWAPI::Player player) const;
	const UnitData &        getUnitData(BWAPI::Player player, BWAPI::UnitType type) const;

	const double 			getEnemyFightScore() const { return _enemyFightScore; };
	const double 			getSelfFightScore() const { return _selfFightScore; };

	const int				getPlayerLost(BWAPI::Player player) { return _unitData[player].getMineralsLost() + _unitData[player].getGasLost(); };
	void					setPlayerLost(BWAPI::Player player, int mineralsLost = 0, int gasLost = 0) { _unitData[player].setMineralsLost(mineralsLost); _unitData[player].setGasLost(gasLost); }

	const std::vector<BWAPI::Position> getMyRegionVertices() { return _myRegionVertices; };

	void					setDefensivePosition(BWAPI::Position position);
	void					setDefensivePosition() { _defensivePosition = getRallyPoint(); };
	BWAPI::Position			getDefensivePosition() { return _defensivePosition; };//防御点

	void					setNearEnemyTankPosition(BWAPI::Position position) { _nearEnemyTank = position; };
	BWAPI::Position			getNearEnemyTankPosition() { return _nearEnemyTank; };
	
	void					setAttackPosition(BWAPI::Position position) { _attackPosition = position; };//设置攻击点
	BWAPI::Position			getAttackPosition() { return _attackPosition; };

	BWAPI::Position			getChokepoint(bool isNatural = true);//最近的防守路口,默认为分矿口
	BWAPI::Position			getUnloadPosition();//敌人空投点
	BWAPI::Position			getMyUnloadPosition();//自家空投点
	BWAPI::Position			checkUnloadPosition();//敌人空投点

	BWAPI::Position			getRallyPoint();//出兵集合点
	void					setRallyPoint(BWAPI::Position position) { _rallyPoint = position; };

	
	void					setMainAttackPoint(BWAPI::Position position) { _mainAttackTarget = position; }
	BWAPI::Position			getMainAttackPoint() { return _mainAttackTarget; };

	void					setDropPoint(BWAPI::Position position) { _dropPoint = position; }
	BWAPI::Position			getDropPoint() { return _dropPoint; };

	BWAPI::Position			getEnemyChokePoint() { return _enemyChokePoint; }

	BWAPI::Position			getReconLocation(bool isStartLocation = false);//获取侦察点
	void					setReconLocation(BWAPI::Position position);//设置侦察点权重
	void					setReconLocation(int weight);//设置侦察点权重
	void					setReconLocation(BWAPI::Position position, int weight);//设置侦察点权重

	int						getZergSpireOverFrame() { return _zergSpireOverFrame; };//获取飞龙塔建造完成时间
	void					setZergSpireOverFrame(int frame) { _zergSpireOverFrame = frame; };

	int						getPsionicStormFrame() { return _psionicStormFrame; };//获取闪电升级完成时间
	void					setPsionicStormFrame(int frame) { _psionicStormFrame = frame; };

	BWAPI::TilePosition		workerScoutTarget;

	void					setAttackTargets(BWAPI::Unit unit, int damage) { _attackTargets[unit] += damage; };
	int						getAttackTargets(BWAPI::Unit unit) { if (_attackTargets[unit]) { return _attackTargets[unit]; } else { return 0; } };
	void					clearAttackTargets() { _attackTargets.clear(); }
	void					removeAttackTargets(BWAPI::Unit unit) { _attackTargets.erase(unit); }

	std::map<BWAPI::Unit, int>					getHazardousArea() { return _hazardousArea; }
	void					setHazardousArea(BWAPI::Unit unit, int radius);
	void					removeHazardousArea(BWAPI::Unit unit);

	LocutusMapGrid&         getMyUnitGrid() { return _myUnitGrid; };

	// yay for singletons!
	static InformationManager & Instance();
};
}
