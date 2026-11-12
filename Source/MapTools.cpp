#include "MapTools.h"
#include "BuildingPlacer.h"
#include "InformationManager.h"

using namespace UAlbertaBot;

MapTools & MapTools::Instance()
{
    static MapTools instance;
    return instance;
}

MapTools::MapTools()
    : _rows(BWAPI::Broodwar->mapHeight())
    , _cols(BWAPI::Broodwar->mapWidth())
{
	UAB_ASSERT(_rows > 0 && _cols > 0, "empty map");

    _map    = std::vector<bool>(_rows*_cols,false);
    _fringe = std::vector<short int>(_rows*_cols,0);

    setBWAPIMapData();

	_hasIslandBases = false;
	for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
	{
		if (base->isIsland())
		{
			_hasIslandBases = true;
			break;
		}
	}
}

// return the index of the 1D array from (row,col)
inline short MapTools::getIndex(int row,int col) const
{
    return row * _cols + col;
}

// Is the tile unexplored?
bool MapTools::unexplored(DistanceMap & dmap, short index) const
{
    return index != -1 &&            // index is valid
		dmap[index] == -1 &&         // tile doesn't have a known distance
		_map[index];                 // the map says it's walkable
}

// Read the map data from BWAPI and remember which 32x32 build tiles are walkable.
// NOTE The game map is walkable at the resolution of 8x8 walk tiles, so this is an approximation.
//      We're asking "Can big units walk here?" Small units may be able to squeeze into more places.
//阅读 BWAPI 的地图数据, 并记住哪些32x32 构建瓷砖是步行。
//注意游戏地图是步行在分辨率的8x8 走瓷砖, 所以这是一个近似。
//我们在问 "大部队能在这里走吗？" 小单位可能会挤进更多的地方。
void MapTools::setBWAPIMapData()
{
	// 1. Check terrain: Is it walkable?
	//1。检查地形: 是步行吗？
    for (int r(0); r < _rows; ++r)
    {
        for (int c(0); c < _cols; ++c)
        {
            bool walkable = true;

            // check each walk tile within this TilePosition
            for (int i=0; i<4 && walkable; ++i)
            {
                for (int j=0; j<4 && walkable; ++j)
                {
                    if (!BWAPI::Broodwar->isWalkable(c*4 + i,r*4 + j))
                    {
                        walkable = false;
                    }
                }
			}

            _map[getIndex(r,c)] = walkable;
        }
    }

	// 2. Check static units: Do they block tiles?
	//2。检查静态单位: 它们是否阻止瓷砖？
	for (const auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
	{
		// The neutral units may include moving critters which do not permanently block tiles.
		// Something immobile blocks tiles it occupies until it is destroyed (exceptions may be possible).
		//中性单位可能包括不永久阻挡瓷砖的移动动物。
		//不动的东西阻止瓷砖它占领, 直到它被摧毁(例外可能是可能的)。
		if (!unit->getType().canMove() && !unit->isFlying())
		{
			BWAPI::TilePosition pos = unit->getTilePosition();
			for (int r = pos.y; r < pos.y + unit->getType().tileHeight(); ++r)
			{
				for (int c = pos.x; c < pos.x + unit->getType().tileWidth(); ++c)
				{
					_map[getIndex(r, c)] = false;
				}
			}
		}
	}
}

void MapTools::resetFringe()
{
    std::fill(_fringe.begin(),_fringe.end(),0);
}

// Ground distance in tiles, -1 if no path exists.
//地面距离在瓦片,-1, 如果没有路径存在。
int MapTools::getGroundTileDistance(BWAPI::Position origin, BWAPI::Position destination)
{
    // if we have too many maps, reset our stored maps in case we run out of memory
	if (_allMaps.size() > allMapsSize)
    {
        _allMaps.clear();

		if (Config::Debug::DrawMapDistances)
		{
			BWAPI::Broodwar->printf("Cleared distance map cache");
		}
    }

    // if we have already computed the distance map to the destination
	if (_allMaps.find(destination) != _allMaps.end())
	{
		return _allMaps[destination][origin];
	}

	// if we have computed the opposite direction, we can use that too
	if (_allMaps.find(origin) != _allMaps.end())
	{
		return _allMaps[origin][destination];
	}

	// add the map and compute it
	_allMaps.insert(std::pair<BWAPI::Position,DistanceMap>(destination,DistanceMap()));
	computeDistance(_allMaps[destination],destination);
	return _allMaps[destination][origin];
}

// Ground distance in pixels (with build tile granularity), -1 if no path exists.
// Build tile granularity means that the distance is a multiple of 32 pixels.
//以像素为单位的地面距离 (具有生成平铺粒度), 如果不存在路径, 则为-1。
//生成平铺粒度意味着距离是32像素的倍数。
int MapTools::getGroundDistance(BWAPI::Position origin, BWAPI::Position destination)
{
	int tiles = getGroundTileDistance(origin, destination);
	if (tiles > 0)
	{
		return 32 * tiles;
	}
	return tiles;    // 0 or -1
}

// computes walk distance from Position P to all other points on the map
//计算从位置 P 到地图上所有其他点的步行距离
void MapTools::computeDistance(DistanceMap & dmap,const BWAPI::Position p)
{
    search(dmap,p.y / 32,p.x / 32);
}

// does the dynamic programming search
//动态编程搜索
void MapTools::search(DistanceMap & dmap,const int sR,const int sC)
{
    // reset the internal variables
    resetFringe();

    // set the starting position for this search
    // dmap.setStartPosition(sR,sC);

    // set the distance of the start cell to zero
    dmap[getIndex(sR,sC)] = 0;

    // set the fringe variables accordingly
    int fringeSize(1);
    int fringeIndex(0);
    _fringe[0] = getIndex(sR,sC);
    dmap.addSorted(getTilePosition(_fringe[0]));

    // temporary variables used in search loop
    int currentIndex,nextIndex;
    int newDist;

    // the size of the map
    int size = _rows*_cols;

    // while we still have things left to expand
    while (fringeIndex < fringeSize)
    {
        // grab the current index to expand from the fringe
        currentIndex = _fringe[fringeIndex++];
        newDist = dmap[currentIndex] + 1;

        // search up
        nextIndex = (currentIndex > _cols) ? (currentIndex - _cols) : -1;
        if (unexplored(dmap,nextIndex))
        {
            // set the distance based on distance to current cell
            dmap.setDistance(nextIndex,newDist);
            dmap.addSorted(getTilePosition(nextIndex));

            // put it in the fringe
            _fringe[fringeSize++] = nextIndex;
        }

        // search down
        nextIndex = (currentIndex + _cols < size) ? (currentIndex + _cols) : -1;
        if (unexplored(dmap,nextIndex))
        {
            // set the distance based on distance to current cell
            dmap.setDistance(nextIndex,newDist);
            dmap.addSorted(getTilePosition(nextIndex));

            // put it in the fringe
            _fringe[fringeSize++] = nextIndex;
        }

        // search left
        nextIndex = (currentIndex % _cols > 0) ? (currentIndex - 1) : -1;
        if (unexplored(dmap,nextIndex))
        {
            // set the distance based on distance to current cell
            dmap.setDistance(nextIndex,newDist);
            dmap.addSorted(getTilePosition(nextIndex));

            // put it in the fringe
            _fringe[fringeSize++] = nextIndex;
        }

        // search right
        nextIndex = (currentIndex % _cols < _cols - 1) ? (currentIndex + 1) : -1;
        if (unexplored(dmap,nextIndex))
        {
            // set the distance based on distance to current cell
            dmap.setDistance(nextIndex,newDist);
            dmap.addSorted(getTilePosition(nextIndex));

            // put it in the fringe
            _fringe[fringeSize++] = nextIndex;
        }
    }
}

const std::vector<BWAPI::TilePosition> & MapTools::getClosestTilesTo(BWAPI::Position pos)
{
	// make sure the distance map is calculated with pos as a destination
	//确保距离图是以 pos 为目标进行计算的
	//    int a = getGroundTileDistance(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()),pos);
	int a = getGroundTileDistance(pos, pos);

	return _allMaps[pos].getSortedTiles();
}

const std::vector<BWAPI::TilePosition> & MapTools::getGroundTilePosition(BWAPI::Position from, BWAPI::Position to)
{
    // make sure the distance map is calculated with pos as a destination
	//确保距离图是以 pos 为目标进行计算的
//    int a = getGroundTileDistance(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()),pos);
	int a = getGroundTileDistance(from, to);

	return _allMaps[from].getSortedTiles();
}

BWAPI::TilePosition MapTools::getTilePosition(short index)
{
    return BWAPI::TilePosition(index % _cols,index / _cols);
}

void MapTools::drawHomeDistanceMap()
{
	if (!Config::Debug::DrawMapDistances)
	{
		return;
	}

	BWAPI::Position homePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

	for (int x = 0; x < BWAPI::Broodwar->mapWidth(); ++x)
	{
		for (int y = 0; y < BWAPI::Broodwar->mapHeight(); ++y)
		{
			BWAPI::Position pos(x * 32, y * 32);

			int dist = getGroundTileDistance(pos, homePosition);

			BWAPI::Broodwar->drawTextMap(pos + BWAPI::Position(16, 16), "%d", dist);
		}
	}
}

//下一个扩展
BWTA::BaseLocation * MapTools::nextExpansion(bool hidden, bool wantMinerals, bool wantGas)
{
	UAB_ASSERT(wantMinerals || wantGas, "unwanted expansion");

	// Abbreviations.
	BWAPI::Player player = BWAPI::Broodwar->self();
	BWAPI::Player enemy = BWAPI::Broodwar->enemy();

	// We'll go through the bases and pick the one with the best score.
	BWTA::BaseLocation * bestBase = nullptr;
	double bestScore = -999999.0;
	
	BWAPI::TilePosition homeTile = InformationManager::Instance().getMyMainBaseLocation()->getTilePosition();
	BWAPI::Position myBasePosition(homeTile);
	BWTA::BaseLocation * enemyBase = InformationManager::Instance().getEnemyMainBaseLocation();  // may be null

    for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
    {
		double score = 0.0;

        // Do we demand a gas base?
		if (wantGas && (base->isMineralOnly() || base->gas() == 0))
		{
			continue;
		}

		// Do we demand a mineral base?
		// The constant is an arbitrary limit "enough minerals to be worth it".
		if (wantMinerals && base->minerals() < 500)
		{
			continue;
		}

		// Don't expand to an existing base.
		if (InformationManager::Instance().getBaseOwner(base) != BWAPI::Broodwar->neutral())
		{
			continue;
		}
        
		// Don't expand to a base already reserved for another expansion.
		if (InformationManager::Instance().isBaseReserved(base))
		{
			continue;
		}

		BWAPI::TilePosition tile = base->getTilePosition();
        bool buildingInTheWay = false;

        for (int x = 0; x < player->getRace().getCenter().tileWidth(); ++x)
        {
			for (int y = 0; y < player->getRace().getCenter().tileHeight(); ++y)
            {
				if (BuildingPlacer::Instance().isReserved(x,y))
				{
					// This happens if we were already planning to expand here. Try somewhere else.
					buildingInTheWay = true;
					break;
				}

				// TODO bug: this doesn't include enemy buildings which are known but out of sight
				for (const auto unit : BWAPI::Broodwar->getUnitsOnTile(BWAPI::TilePosition (tile.x + x, tile.y + y)))
                {
                    if (unit->getType().isBuilding() && !unit->isLifted())
                    {
                        buildingInTheWay = true;
                        break;
                    }
                }
            }
        }
            
        if (buildingInTheWay)
        {
            continue;
        }

        // Want to be close to our own base (unless this is to be a hidden base).
		double distanceFromUs = getGroundTileDistance(BWAPI::Position(tile), myBasePosition);

        // if it is not connected, continue
		if (distanceFromUs < 0)
        {
            continue;
        }

		// Want to be far from the enemy base.
		double distanceFromEnemy = 0.0;
		if (enemyBase) {
			BWAPI::TilePosition enemyTile = enemyBase->getTilePosition();
			distanceFromEnemy = getGroundTileDistance(BWAPI::Position(tile), BWAPI::Position(enemyTile));
			if (distanceFromEnemy < 0)        // no ground connection between the locations
			{
				distanceFromEnemy = 0.0;
			}
		}

		// Add up the score.
		score = hidden ? (distanceFromEnemy + distanceFromUs / 2.0) : (distanceFromEnemy / 2.0 - distanceFromUs);

		// More resources -> better.
		if (wantMinerals)
		{
			score += 0.01 * base->minerals();
		}
		if (wantGas)
		{
			score += 0.02 * base->gas();
		}
		// Big penalty for enemy buildings in the same region.
		if (InformationManager::Instance().isEnemyBuildingInRegion(base->getRegion()))
		{
			score -= 100.0;
		}

		// BWAPI::Broodwar->printf("base score %d, %d -> %f",  tile.x, tile.y, score);
		if (score > bestScore)
        {
            bestBase = base;
			bestScore = score;
		}
    }

    if (bestBase)
    {
        return bestBase;
	}
	if (wantMinerals && wantGas)
	{
		// We wanted a gas base and there isn't one. Try for a mineral-only base.
		return nextExpansion(hidden, true, false);
	}
	return nullptr;
}

//获取下一个扩展
BWAPI::TilePosition MapTools::getNextExpansion(bool hidden, bool wantMinerals, bool wantGas)
{
	BWTA::BaseLocation * base = nextExpansion(hidden, wantMinerals, wantGas);
	if (base)
	{
		// BWAPI::Broodwar->printf("foresee base @ %d, %d", base->getTilePosition().x, base->getTilePosition().y);
		return base->getTilePosition();
	}
	return BWAPI::TilePositions::None;
}

BWAPI::TilePosition MapTools::reserveNextExpansion(bool hidden, bool wantMinerals, bool wantGas)
{
	BWTA::BaseLocation * base = nextExpansion(hidden, wantMinerals, wantGas);
	if (base)
	{
		// BWAPI::Broodwar->printf("reserve base @ %d, %d", base->getTilePosition().x, base->getTilePosition().y);
		InformationManager::Instance().reserveBase(base);
		return base->getTilePosition();
	}
	return BWAPI::TilePositions::None;
}

/*
获取距离起点坐标指定长度的坐标
*/
BWAPI::Position MapTools::getDistancePosition(BWAPI::Position start, BWAPI::Position end, double dist) {
	double distance = sqrt(pow(start.x - end.x, 2) + pow(start.y - end.y, 2));// 两点的坐标距离
	double lenthUnit = distance / 5;// 单位长度
	// 第一步：求得直线方程相关参数y=kx+b
	double k = (start.y - end.y) * 1.0 / (start.x - end.x);// 坐标直线斜率k
	double b = start.y - k * start.x;// 坐标直线b
	// 第二步：求得在直线y=kx+b上，距离当前坐标距离为L的某点
	// 一元二次方程Ax^2+Bx+C=0中,
	// 一元二次方程求根公式：
	// 两根x1,x2= [-B±√(B^2-4AC)]/2A
	// ①(y-y0)^2+(x-x0)^2=L^2;
	// ②y=kx+b;
	// 式中x,y即为根据以上lenthUnit单位长度(这里就是距离L)对应点的坐标
	// 由①②表达式得到:(k^2+1)x^2+2[(b-y0)k-x0]x+[(b-y0)^2+x0^2-L^2]=0
	double A = pow(k, 2) + 1;// A=k^2+1;
	double B = 2 * ((b - start.y) * k - start.x);// B=2[(b-y0)k-x0];
	int m = 1;
	double L = m * dist;
	// C=(b-y0)^2+x0^2-L^2
	double C = pow(b - start.y, 2) + pow(start.x, 2)
		- pow(L, 2);
	// 两根x1,x2= [-B±√(B^2-4AC)]/2A
	double x1 = (-B + sqrt(pow(B, 2) - 4 * A * C)) / (2 * A);
	double x2 = (-B - sqrt(pow(B, 2) - 4 * A * C)) / (2 * A);
	double x = 0;// 最后确定是在已知两点之间的某点
	if (x1 == x2) {
		x = x1;
	}
	else if (start.x <= x1 && x1 <= end.x || end.x <= x1
		&& x1 <= start.x) {
		x = x1;
	}
	else if (start.x <= x2 && x2 <= end.x || end.x <= x2
		&& x2 <= start.x) {
		x = x2;
	}
	double y = k * x + b;

	return BWAPI::Position(x, y);
}

/*
获取距离起点延长指定长度的坐标
*/
BWAPI::Position MapTools::getExtendedPosition(BWAPI::Position start, BWAPI::Position end, double dist) {
	double xab, yab;
	double xbd, ybd;
	double xd, yd;

	xab = start.x - end.x;
	yab = start.y - end.y;

	xbd = sqrt((dist * dist) / ((yab / xab) * (yab / xab) + 1));

	if (xab > 0) {
		xbd = sqrt((dist * dist) / ((yab / xab) * (yab / xab) + 1));
	}
	else {
		xbd = -sqrt((dist * dist) / ((yab / xab) * (yab / xab) + 1));
	}

	xd = start.x + xbd;
	yd = start.y + yab / xab * xbd;

	//printf("xd=%f,yd=%f\n", xd, yd);
	return BWAPI::Position(xd, yd);
}

//获取向量坐标
BWAPI::Position MapTools::getKiteVector(BWAPI::Unit unit, BWAPI::Unit target)
{
	return getKiteVector(unit->getPosition(), target->getPosition());
}

//获取向量坐标
BWAPI::Position MapTools::getKiteVector(BWAPI::Position start, BWAPI::Position target)
{
	BWAPI::Position fleeVec(target - start);
	double fleeAngle = atan2(fleeVec.y, fleeVec.x);
	fleeVec = BWAPI::Position(static_cast<int>(64 * cos(fleeAngle)), static_cast<int>(64 * sin(fleeAngle)));
	return fleeVec;
}

std::vector<BWAPI::Position> MapTools::calculateEnemyRegionVertices(BWTA::BaseLocation * enemyBaseLocation) {
	std::vector<BWAPI::Position> _regionVertices;

	if (!enemyBaseLocation)
	{
		return _regionVertices;
	}

	BWTA::Region * enemyRegion = enemyBaseLocation->getRegion();

	if (!enemyRegion)
	{
		return _regionVertices;
	}

	const BWAPI::Position basePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	const std::vector<BWAPI::TilePosition> & closestTobase = getClosestTilesTo(basePosition);

	std::set<BWAPI::Position> unsortedVertices;

	// check each tile position
	for (size_t i(0); i < closestTobase.size(); ++i)
	{
		const BWAPI::TilePosition & tp = closestTobase[i];

		if (BWTA::getRegion(tp) != enemyRegion)
		{
			continue;
		}

		// a tile is 'on an edge' unless
		// 1) in all 4 directions there's a tile position in the current region
		// 2) in all 4 directions there's a buildable tile
		//瓷砖是 ' 在边缘 ', 除非
		//1) 在所有4个方向在当前区域有一个瓷砖位置
		//2) 在所有4个方向有一个可生成瓷砖
		bool edge =
			BWTA::getRegion(BWAPI::TilePosition(tp.x + 1, tp.y)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x + 1, tp.y))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x, tp.y + 1)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x, tp.y + 1))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x - 1, tp.y)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x - 1, tp.y))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x, tp.y - 1)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x, tp.y - 1));

		// push the tiles that aren't surrounded
		//推开没有被包围的瓷砖
		if (edge && BWAPI::Broodwar->isBuildable(tp))
		{
			if (Config::Debug::DrawScoutInfo)
			{
				int x1 = tp.x * 32 + 2;
				int y1 = tp.y * 32 + 2;
				int x2 = (tp.x + 1) * 32 - 2;
				int y2 = (tp.y + 1) * 32 - 2;

				BWAPI::Broodwar->drawTextMap(x1 + 3, y1 + 2, "%d", getGroundTileDistance(BWAPI::Position(tp), basePosition));
				BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Green, false);
			}

			unsortedVertices.insert(BWAPI::Position(tp) + BWAPI::Position(16, 16));
		}
	}

	std::vector<BWAPI::Position> sortedVertices;
	BWAPI::Position current = *unsortedVertices.begin();

	_regionVertices.push_back(current);
	unsortedVertices.erase(current);

	// while we still have unsorted vertices left, find the closest one remaining to current
	//虽然我们仍然有未排序的顶点左, 找到最接近的一个剩余的当前
	while (!unsortedVertices.empty())
	{
		double bestDist = 1000000;
		BWAPI::Position bestPos;

		for (const BWAPI::Position & pos : unsortedVertices)
		{
			double dist = pos.getDistance(current);

			if (dist < bestDist)
			{
				bestDist = dist;
				bestPos = pos;
			}
		}

		current = bestPos;
		sortedVertices.push_back(bestPos);
		unsortedVertices.erase(bestPos);
	}

	// let's close loops on a threshold, eliminating death grooves
	//让我们关闭循环的门槛, 消除死亡凹槽
	int distanceThreshold = 100;

	while (true)
	{
		// find the largest index difference whose distance is less than the threshold
		//查找距离小于阈值的最大索引差异
		int maxFarthest = 0;
		int maxFarthestStart = 0;
		int maxFarthestEnd = 0;

		// for each starting vertex
		for (int i(0); i < (int)sortedVertices.size(); ++i)
		{
			int farthest = 0;
			int farthestIndex = 0;

			// only test half way around because we'll find the other one on the way back
			for (size_t j(1); j < sortedVertices.size() / 2; ++j)
			{
				int jindex = (i + j) % sortedVertices.size();

				if (sortedVertices[i].getDistance(sortedVertices[jindex]) < distanceThreshold)
				{
					farthest = j;
					farthestIndex = jindex;
				}
			}

			if (farthest > maxFarthest)
			{
				maxFarthest = farthest;
				maxFarthestStart = i;
				maxFarthestEnd = farthestIndex;
			}
		}

		// stop when we have no long chains within the threshold
		if (maxFarthest < 4)
		{
			break;
		}

		std::vector<BWAPI::Position> temp;

		for (size_t s(maxFarthestEnd); s != maxFarthestStart; s = (s + 1) % sortedVertices.size())
		{
			temp.push_back(sortedVertices[s]);
		}

		sortedVertices = temp;
	}

	return sortedVertices;
}

//直线是否与圆相交
bool MapTools::isIntersect(BWAPI::Position start, BWAPI::Position end, BWAPI::Position center, double radius) {
	double a, b, c, dist1, dist2, angle1, angle2; // ax + by + c = 0;  
	if (start.x == end.x)
		a = 1, b = 0, c = -start.x;//特殊情况判断，分母不能为零  
	else if (start.y == end.y)
		a = 0, b = 1, c = -start.y;//特殊情况判断，分母不能为零  
	else {
		a = start.y - end.y;
		b = end.x - start.x;
		c = start.x * end.y - start.y * end.x;
	}
	dist1 = a * center.x + b * center.y + c;
	dist1 *= dist1;
	dist2 = (a * a + b * b) * radius * radius;
	if (dist1 > dist2) return 0;//点到直线距离大于半径r  
	angle1 = (center.x - start.x) * (end.x - start.x) + (center.y - start.y) * (end.y - start.y);
	angle2 = (center.x - end.x) * (start.x - end.x) + (center.y - end.y) * (start.y - end.y);
	if (angle1 > 0 && angle2 > 0) return true;//余弦都为正，则是锐角  
	return false;
}

// center---圆心坐标， radius---圆半径， sp---圆外一点， rp1,rp2---切点坐标
/*
void MapTools::getCutPoint(BWAPI::Position center, double radius, BWAPI::Position sp, BWAPI::Position & rp1, BWAPI::Position & rp2){
	BWAPI::Position p2;
	p2.x = (center.x + sp.x) / 2;
	p2.y = (center.y + sp.y) / 2;

	double dx2, dy2, r2;
	dx2 = p2.x - center.x;
	dy2 = p2.y - center.y;
	r2 = sqrt(dx2*dx2 + dy2*dy2);
	c2point(center, radius, p2, r2, rp1, rp2);
}
*/

//两圆交点：   
// 两圆已经相交（相切） 
void MapTools::c2point(BWAPI::Position p1, double r1, BWAPI::Position p2, double r2, BWAPI::Position &rp1, BWAPI::Position &rp2)
{
	double a, b, r;
	a = p2.x - p1.x;
	b = p2.y - p1.y;
	r = (a*a + b*b + r1*r1 - r2*r2) / 2;
	if (a == 0 && b != 0)
	{
		rp1.y = rp2.y = r / b;
		rp1.x = sqrt(r1*r1 - rp1.y*rp1.y);
		rp2.x = -rp1.x;
	}
	else if (a != 0 && b == 0)
	{
		rp1.x = rp2.x = r / a;
		rp1.y = sqrt(r1*r1 - rp1.x*rp2.x);
		rp2.y = -rp1.y;
	}
	else if (a != 0 && b != 0)
	{
		double delta;
		delta = b*b*r*r - (a*a + b*b)*(r*r - r1*r1*a*a);
		rp1.y = (b*r + sqrt(delta)) / (a*a + b*b);
		rp2.y = (b*r - sqrt(delta)) / (a*a + b*b);
		rp1.x = (r - b*rp1.y) / a;
		rp2.x = (r - b*rp2.y) / a;
	}

	rp1.x += p1.x;
	rp1.y += p1.y;
	rp2.x += p1.x;
	rp2.y += p1.y;
}

BWAPI::Position MapTools::getCutPoint(BWAPI::Position center, double radius, BWAPI::Position sp, BWAPI::Position ep) {
	return BWAPI::Position(-1, -1);
}

//求点到圆的切点
void MapTools::getCutPoint(BWAPI::Position center, double radius, BWAPI::Position sp, BWAPI::Position & rp1, BWAPI::Position & rp2)
{
	double m = sp.x, n = sp.y;
	double a = center.x, b = center.y;
	double r = radius;

	double dx = center.x - sp.x;
	double dy = center.y - sp.y;
	//计算点击处与圆心相对于X轴的夹角
	double r1 = atan2(dy, dx);
	//计算点击处与圆心、点击处与切点1这两条线段间的夹角
	double d1 = sqrt(dx*dx + dy*dy);
	double r2 = asin(radius / d1);
	//计算从切点1向圆的垂直直径做垂线形成的直角三角形的一个角
	double r3 = r1 - r2;
	//计算坐标系中的角度
	double r4 = r3 - 3.1415926 / 2;
	//计算切点1相对于圆心的x、y坐标
	double x1 = radius * cos(r4);
	double y1 = radius * sin(r4);

	//计算点击处与切线2相对于X轴的夹角
	double r5 = 3.1415926 / 2 - r1 - r2;
	//计算坐标系中的角度
	double r6 = -r5;
	//计算切点2相对于圆心的x、y坐标
	double x2 = radius * cos(r6);
	double y2 = radius * sin(r6);

	rp1.x = center.x + x1;
	rp1.y = center.y + y1;

	rp2.x = center.x - x2;
	rp2.y = center.y - y2;

	/*
	cout << "请输入圆心座标：";
	cin >> a >> b;
	cout << "请输入半径：";
	cin >> r;
	cout << "请输入点座标：";
	cin >> m >> n;
	*/
	/*
	// 点到圆心距离的平方
	double d2 = (m - a) * (m - a) + (n - b) * (n - b);
	// 点到圆心距离
	double d = sqrt(d2);
	// 半径的平方
	double r2 = r * r;
	if (d2 < r2)
	{
		//cout << "点在圆内，无切点" << endl;
		rp1.x = 0;
		rp1.y = 0;
	}
	else if (d2 == r2)
	{
		//cout << "点在圆上，切点为给定点：(" << m << ", " << n << ")" << endl;
		rp1.x = m;
		rp1.y = n;
	}
	else
	{
		// 点到切点距离
		double l = sqrt(d2 - r2);
		// 点->圆心的单位向量
		double x0 = (a - m) / d;
		double y0 = (b - n) / d;
		// 计算切线与点心连线的夹角
		double f = asin(r / d);
		// 向正反两个方向旋转单位向量
		double x1 = x0 * cos(f) - y0 * sin(f);
		double y1 = x0 * sin(f) + y0 * cos(f);
		double x2 = x0 * cos(-f) - y0 * sin(-f);
		double y2 = x0 * sin(-f) + y0 * cos(-f);
		// 得到新座标
		x1 = (x1 + m) * l;
		y1 = (y1 + n) * l;
		x2 = (x2 + m) * l;
		y2 = (y2 + n) * l;
		//cout << "点在圆外，切点有两个：(" << x1 << ", " << y1 << ")和(" << x2 << ", " << y2 << ")" << endl;

		rp1.x = x1;
		rp1.y = y1;

		rp2.x = x2;
		rp2.y = y2;
	}

	*/

	//return BWAPI::Position(-1, -1);
}

//获取切点
/*
BWAPI::Position MapTools::getCutPoint(BWAPI::Position center, double radius, BWAPI::Position sp, BWAPI::Position ep)
{
	int xa = sp.x;
	int ya = sp.y;
	int xb = ep.x;
	int yb = ep.y;
	int xo = center.x;
	int yo = center.y;
	double r = radius;

	float a, b, c, d;
	float aerf;

	a = sqrt((xa - xo) * (xa - xo) + (ya - yo) * (ya - yo));
	b = sqrt((xb - xo) * (xb - xo) + (yb - yo) * (yb - yo));
	c = sqrt((xb - xa) * (xb - xa) + (yb - ya) * (yb - ya));

	aerf = acos((b * b + c * c - a * a) / (2 * b * c));
	d = b * sin(aerf);

	float k = (yb - ya) / (xb - xa);

	float x0 = -ya / k + xa;
	float y0 = -k * xa + ya;

	float xd = 1 - (sqrt(x0 * x0 + y0 * y0)) / sqrt(a * a - d * d) * x0;
	float yd = sqrt(a * a - d * d) / (sqrt(x0 * x0 + y0 * y0)) * y0;

	float k1 = (yd - yo) / (xd - xo);
	float y1 = -xo * k1 + yo;
	float x1 = -yo / k1 + yo;
	float thet = atan((yb - ya) / (xb - xa));
	float tmp = sqrt(x1 * x1 + y1 * y1) - d - xd * cos(thet);
	if (d > r)
	{
		tmp = sqrt(x1 * x1 + y1 * y1) - d - xd * cos(thet);
	}
	if (d == r)
	{
		tmp = sqrt(x1 * x1 + y1 * y1) - xd * cos(thet);
	}
	if (d < r)
	{
		tmp = sqrt(x1 * x1 + y1 * y1) + d - xd * cos(thet);
	}
	float yOut = tmp * cos(thet) * yo;
	float xOut = x1 - yo * tan(yOut);

	return BWAPI::Position(xOut, yOut);
}
*/