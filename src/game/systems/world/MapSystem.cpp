#include "game/systems/world/MapSystem.hpp"
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/Common.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/systems/world/MosaicMapGenerator.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

MapSystem::MapSystem() : m_gen(std::random_device{}()) {}

MapSystem::~MapSystem() {
  if (m_fogTextureValid) {
    UnloadTexture(m_fogTexture);
  }
}

// --- CaveMapGenerator Implementation ---

MapGenerator::MapData CaveMapGenerator::Generate(int width, int height,
                                                 uint32_t seed, float wallProb,
                                                 int iterations) {
  MapData map{width, height};
  map.grid.resize(width * height);

  std::mt19937 gen(seed);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  // 1. 初始化
  using namespace NoMoreDay::Constants::Generator::Cave;
  for (auto &tile : map.grid) {
    tile.type =
        (dist(gen) < INITIAL_WALL_PROB) ? Tile::Type::WALL : Tile::Type::FLOOR;
  }

  // 辅助 buffer
  std::vector<Tile> buffer = map.grid;

  // 2. 平滑迭代 (双缓冲)
  for (int i = 0; i < iterations; ++i) {
    auto &src = (i % 2 == 0) ? map.grid : buffer;
    auto &dst = (i % 2 == 0) ? buffer : map.grid;
    SmoothIteration(src, dst, width, height);
  }

  if (iterations % 2 != 0) {
    map.grid = std::move(buffer);
  }

  // 3. 生成障碍物 (使用种子生长法)
  GenerateObstacles(map.grid, width, height, gen());

  // 4. 边界处理
  ApplyBoundaries(map.grid, width, height);

  // 5. 确保 100% 连通性 (防止大块障碍物切断路径)
  EnsureConnectivity(map.grid, width, height);

  // 6. Place Exits
  PlaceExits(map.grid, width, height, gen());

  return map;
}

void CaveMapGenerator::PlaceExits(std::vector<Tile> &grid, int w, int h,
                                  uint32_t seed) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> xDist(1, w - 2);
  std::uniform_int_distribution<int> yDist(1, h - 2);

  // 1. Place Start point (STAIRS_UP) near center
  using namespace NoMoreDay::Constants::Generator::Cave;
  int cx = w / 2;
  int cy = h / 2;
  bool startPlaced = false;
  for (int r = 0; r < START_SEARCH_RADIUS && !startPlaced; r++) {
    for (int dx = -r; dx <= r; dx++) {
      for (int dy = -r; dy <= r; dy++) {
        int idx = (cy + dy) * w + (cx + dx);
        if (grid[idx].type == Tile::Type::FLOOR) {
          grid[idx].type = Tile::Type::STAIRS_UP;
          startPlaced = true;
          break;
        }
      }
      if (startPlaced)
        break;
    }
  }

  // 2. Place Stairs Down (Exit)
  using namespace NoMoreDay::Constants::Generator::Cave;
  for (int attempt = 0; attempt < EXIT_ATTEMPTS; ++attempt) {
    int x = xDist(gen);
    int y = yDist(gen);
    if (grid[y * w + x].type == Tile::Type::FLOOR) {
      grid[y * w + x].type = Tile::Type::STAIRS_DOWN;
      break;
    }
  }
}

void CaveMapGenerator::SmoothIteration(const std::vector<Tile> &src,
                                       std::vector<Tile> &dst, int w, int h) {
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
        dst[y * w + x].type = Tile::Type::WALL;
        continue;
      }
      int wallCount = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        const int offset = (y + dy) * w + x;
        if (src[offset - 1].type == Tile::Type::WALL)
          wallCount++;
        if (src[offset].type == Tile::Type::WALL)
          wallCount++;
        if (src[offset + 1].type == Tile::Type::WALL)
          wallCount++;
      }
      using namespace NoMoreDay::Constants::Generator::Cave;
      dst[y * w + x].type =
          (wallCount > SMOOTH_THRESHOLD) ? Tile::Type::WALL : Tile::Type::FLOOR;
    }
  }
}

void CaveMapGenerator::ApplyBoundaries(std::vector<Tile> &grid, int w, int h) {
  for (int x = 0; x < w; ++x) {
    grid[0 * w + x].type = Tile::Type::WALL;
    grid[(h - 1) * w + x].type = Tile::Type::WALL;
  }
  for (int y = 0; y < h; ++y) {
    grid[y * w + 0].type = Tile::Type::WALL;
    grid[y * w + (w - 1)].type = Tile::Type::WALL;
  }
}

void CaveMapGenerator::GenerateObstacles(std::vector<Tile> &grid, int w, int h,
                                         uint32_t seed) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> xDist(5, w - 6);
  std::uniform_int_distribution<int> yDist(5, h - 6);
  using namespace NoMoreDay::Constants::Generator::Cave;
  std::uniform_int_distribution<int> sizeDist(
      ROCK_SIZE_MIN, ROCK_SIZE_MAX); // 调整每个岩块的目标面积
  // 1. 确定岩块数量
  using namespace NoMoreDay::Constants::Generator::Cave;
  int numRocks = (w * h) / ROCK_DENSITY_DIVISOR;
  if (numRocks < 10)
    numRocks = ROCK_MIN_COUNT;
  // 2. 生成巨型岩块种子
  for (int i = 0; i < numRocks; ++i) {
    int cx = xDist(gen);
    int cy = yDist(gen);
    int targetArea = sizeDist(gen);
    int currentArea = 0;

    // 使用 BFS 方式扩张“岩体”
    std::queue<int> q;
    q.push(cy * w + cx);
    std::vector<int> rockTiles;

    while (!q.empty() && currentArea < targetArea) {
      int curr = q.front();
      q.pop();

      if (grid[curr].type == Tile::Type::WALL)
        continue;

      grid[curr].type = Tile::Type::WALL;
      rockTiles.push_back(curr);
      currentArea++;

      int x = curr % w;
      int y = curr / w;

      // 向四周扩张，加入一点随机性以产生不规则形状
      const int dx[] = {0, 0, 1, -1};
      const int dy[] = {1, -1, 0, 0};
      for (int d = 0; d < 4; ++d) {
        int nx = x + dx[d];
        int ny = y + dy[d];
        if (nx >= 2 && nx < w - 2 && ny >= 2 && ny < h - 2) {
          using namespace NoMoreDay::Constants::Generator::Cave;
          std::uniform_int_distribution<int> chance(0, 100);
          if (chance(gen) < ROCK_EXPANSION_CHANCE) { // 保证紧凑
            q.push(ny * w + nx);
          }
        }
      }
    }
  }

  // 3. 强力平滑 让岩块边缘圆润且合并临近块
  using namespace NoMoreDay::Constants::Generator::Cave;
  for (int i = 0; i < ROCK_SMOOTH_ITERATIONS; ++i) {
    std::vector<Tile> src = grid;
    SmoothIteration(src, grid, w, h);
  }

  // 4. 清理残留的极小岛屿
  using namespace NoMoreDay::Constants::Generator::Cave;
  RemoveSmallRegions(grid, w, h, REGION_THRESHOLD_WALL, Tile::Type::WALL,
                     Tile::Type::FLOOR);

  // 5. 填充大岩块内部的小孔洞
  RemoveSmallRegions(grid, w, h, REGION_THRESHOLD_FLOOR, Tile::Type::FLOOR,
                     Tile::Type::WALL);
}

void CaveMapGenerator::RemoveSmallRegions(std::vector<Tile> &grid, int w, int h,
                                          int threshold,
                                          Tile::Type typeToRemove,
                                          Tile::Type fillType) {
  std::vector<bool> visited(w * h, false);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      int idx = y * w + x;
      if (grid[idx].type == typeToRemove && !visited[idx]) {
        // BFS 扫描连通区域
        std::vector<int> component;
        std::queue<int> q;
        q.push(idx);
        visited[idx] = true;

        while (!q.empty()) {
          int curr = q.front();
          q.pop();
          component.push_back(curr);

          int cx = curr % w;
          int cy = curr / w;

          const int dx[] = {0, 0, 1, -1};
          const int dy[] = {1, -1, 0, 0};
          for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
              int nIdx = ny * w + nx;
              if (grid[nIdx].type == typeToRemove && !visited[nIdx]) {
                visited[nIdx] = true;
                q.push(nIdx);
              }
            }
          }
        }

        // 如果面积过小，抹平
        if (component.size() < (size_t)threshold) {
          for (int i : component) {
            grid[i].type = fillType;
          }
        }
      }
    }
  }
}

void CaveMapGenerator::EnsureConnectivity(std::vector<Tile> &grid, int w,
                                          int h) {
  // 找到最大的可行走区域，将其余不可达的可行走区域全部填成墙
  std::vector<bool> visited(w * h, false);
  std::vector<int> bestRegion;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      int idx = y * w + x;
      if (grid[idx].type != Tile::Type::WALL && !visited[idx]) {
        std::vector<int> currentRegion;
        std::queue<int> q;
        q.push(idx);
        visited[idx] = true;

        while (!q.empty()) {
          int curr = q.front();
          q.pop();
          currentRegion.push_back(curr);

          int cx = curr % w;
          int cy = curr / w;
          const int dx[] = {0, 0, 1, -1};
          const int dy[] = {1, -1, 0, 0};
          for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
              int nIdx = ny * w + nx;
              if (grid[nIdx].type != Tile::Type::WALL && !visited[nIdx]) {
                visited[nIdx] = true;
                q.push(nIdx);
              }
            }
          }
        }

        if (currentRegion.size() > bestRegion.size()) {
          bestRegion = std::move(currentRegion);
        }
      }
    }
  }

  // 将非最大区域的 FLOOR 全部填成墙，保证 100% 连通性
  std::vector<bool> isBest(w * h, false);
  for (int idx : bestRegion)
    isBest[idx] = true;

  for (int i = 0; i < w * h; ++i) {
    if (grid[i].type != Tile::Type::WALL && !isBest[i]) {
      grid[i].type = Tile::Type::WALL;
    }
  }
}

// --- MapSystem Implementation ---

void MapSystem::generateCaveMap(int width, int height) {
  // 使用具体的生成器实例
  CaveMapGenerator generator;
  const auto &biome =
      NoMoreDay::BiomeRegistry::Get().GetBiome(m_currentBiomeId);
  auto mapData =
      generator.Generate(width, height, m_gen(), biome.wallProbability,
                         biome.smoothIterations); // 使用随机种子

  m_mapData.width = mapData.width;
  m_mapData.height = mapData.height;
  m_mapData.grid = std::move(mapData.grid);

  // 初始化流场大小
  m_flowField.resize(m_mapData.width * m_mapData.height);
  m_distanceField.resize(m_mapData.width * m_mapData.height);

  // 初始化缓存 CostMap
  using namespace NoMoreDay::Constants::World::Map;
  m_cachedCostMap.resize(m_mapData.grid.size());
  for (size_t i = 0; i < m_mapData.grid.size(); i++) {
    m_cachedCostMap[i] =
        m_mapData.grid[i].isWalkable() ? COST_FLOOR : COST_WALL;
  }
  m_costMapDirty = false;
}

void MapSystem::generateMap(int width, int height, const std::string &biome) {
  m_currentBiomeId = biome;

  // Town gets a special open layout
  if (NoMoreDay::BiomeRegistry::Get().GetBiome(biome).numericId ==
      NoMoreDay::BiomeID::Town) {
    generateTownMap(width, height);
  } else {
    generateCaveMap(width, height);
  }
}

void MapSystem::generateTownMap(int width, int height) {
  m_mapData.width = width;
  m_mapData.height = height;
  m_mapData.grid.resize(width * height);

  // Fill with floor tiles
  for (auto &tile : m_mapData.grid) {
    tile.type = Tile::Type::FLOOR;
    tile.visibility = 1; // Towns are fully explored by default
  }

  // Add walls only at the borders
  for (int x = 0; x < width; ++x) {
    m_mapData.grid[0 * width + x].type = Tile::Type::WALL;
    m_mapData.grid[(height - 1) * width + x].type = Tile::Type::WALL;
  }
  for (int y = 0; y < height; ++y) {
    m_mapData.grid[y * width + 0].type = Tile::Type::WALL;
    m_mapData.grid[y * width + (width - 1)].type = Tile::Type::WALL;
  }

  // Place spawn point (STAIRS_UP) near center
  int cx = width / 2;
  int cy = height / 2;
  m_mapData.grid[cy * width + cx].type = Tile::Type::STAIRS_UP;

  // Place exit portal (STAIRS_DOWN) - leads to dungeon
  // Safe bounds check
  using namespace NoMoreDay::Constants::World::Map;
  int exitX = std::clamp(cx + TOWN_EXIT_OFFSET, 1, width - 2);
  int exitY = std::clamp(cy - TOWN_EXIT_OFFSET, 1, height - 2);
  m_mapData.grid[exitY * width + exitX].type = Tile::Type::STAIRS_DOWN;

  // Initialize flow field
  m_flowField.resize(m_mapData.width * m_mapData.height);
  m_distanceField.resize(m_mapData.width * m_mapData.height);

  // Initialize cached cost map
  using namespace NoMoreDay::Constants::World::Map;
  m_cachedCostMap.resize(m_mapData.grid.size());
  for (size_t i = 0; i < m_mapData.grid.size(); i++) {
    m_cachedCostMap[i] =
        m_mapData.grid[i].isWalkable() ? COST_FLOOR : COST_WALL;
  }
  m_costMapDirty = false;
}

// 生成拼图地图
void MapSystem::generateMosaicMap(int width, int height,
                                  const NoMoreDay::MosaicGrid &grid,
                                  const NoMoreDay::ResonanceResult &resonance,
                                  entt::registry *registry) {
  NoMoreDay::MosaicMapGenerator generator;
  generator.SetMosaicData(grid, resonance, registry);

  // 假设 biome 由 resonance 结果决定，或者暂时默认 cave
  m_currentBiomeId =
      (resonance.primaryBiome == NoMoreDay::BiomeID::None)
          ? "cave"
          : NoMoreDay::BiomeRegistry::Get().GetBiome(resonance.primaryBiome).id;
  const auto &biome =
      NoMoreDay::BiomeRegistry::Get().GetBiome(m_currentBiomeId);

  // 生成地图
  auto mapData = generator.Generate(
      width, height, m_gen(), biome.wallProbability, biome.smoothIterations);

  m_mapData.width = mapData.width;
  m_mapData.height = mapData.height;
  m_mapData.grid = std::move(mapData.grid);

  // 初始化流场大小
  m_flowField.resize(m_mapData.width * m_mapData.height);
  m_distanceField.resize(m_mapData.width * m_mapData.height);

  // 初始化缓存 CostMap
  using namespace NoMoreDay::Constants::World::Map;
  m_cachedCostMap.resize(m_mapData.grid.size());
  for (size_t i = 0; i < m_mapData.grid.size(); i++) {
    m_cachedCostMap[i] =
        m_mapData.grid[i].isWalkable() ? COST_FLOOR : COST_WALL;
  }
  m_costMapDirty = false;
}

bool MapSystem::raycast(const Position &start, const Position &end) const {
  using namespace NoMoreDay::Constants::World;
  float dx = end.x - start.x;
  float dy = end.y - start.y;
  float distance = std::sqrt(dx * dx + dy * dy);
  if (distance < 1.0f)
    return true;

  int steps = static_cast<int>(std::ceil(distance / (GRID_TILE_SIZE * 0.5f)));
  float stepX = dx / steps;
  float stepY = dy / steps;

  for (int i = 1; i <= steps; ++i) {
    float px = start.x + stepX * i;
    float py = start.y + stepY * i;
    int tx = static_cast<int>(px / GRID_TILE_SIZE);
    int ty = static_cast<int>(py / GRID_TILE_SIZE);
    if (!isWalkable(tx, ty))
      return false;
  }
  return true;
}

bool MapSystem::isWalkable(int x, int y) const {
  return m_mapData.isWalkable(x, y);
}

Tile::Type MapSystem::getTileType(int x, int y) const {
  return m_mapData.getTile(x, y);
}

void MapSystem::render(const Camera2D &camera) const {
  // 简单的视锥剔除
  using namespace NoMoreDay::Constants::World;
  using namespace NoMoreDay::Constants::World::Map;
  int startX =
      static_cast<int>((camera.target.x - camera.offset.x / camera.zoom) /
                       GRID_TILE_SIZE) -
      2;
  int startY =
      static_cast<int>((camera.target.y - camera.offset.y / camera.zoom) /
                       GRID_TILE_SIZE) -
      2;
  int endX =
      startX +
      static_cast<int>((GetScreenWidth() / camera.zoom) / GRID_TILE_SIZE) +
      (int)RENDER_PADDING;
  int endY =
      startY +
      static_cast<int>((GetScreenHeight() / camera.zoom) / GRID_TILE_SIZE) +
      (int)RENDER_PADDING;

  startX = std::max(0, startX);
  startY = std::max(0, startY);
  endX = std::min(m_mapData.width, endX);
  endY = std::min(m_mapData.height, endY);

  const auto &biome =
      NoMoreDay::BiomeRegistry::Get().GetBiome(m_currentBiomeId);

  // 渲染所有瓦片 - GPU FogOfWarSystem 负责在顶层绘制迷雾遮罩
  for (int y = startY; y < endY; ++y) {
    for (int x = startX; x < endX; ++x) {
      Tile::Type type = getTileType(x, y);
      Color color = BLACK;

      switch (type) {
      case Tile::Type::FLOOR:
        color = biome.floorColor;
        break;
      case Tile::Type::WALL:
        color = biome.wallColor;
        break;
      case Tile::Type::STAIRS_DOWN:
        color = RED;
        break;
      case Tile::Type::STAIRS_UP:
        color = GREEN; // Make stairs up visible
        break;
      default:
        color = biome.floorColor;
        break;
      }

      using namespace NoMoreDay::Constants::World;
      DrawRectangle(x * (int)GRID_TILE_SIZE, y * (int)GRID_TILE_SIZE,
                    (int)GRID_TILE_SIZE, (int)GRID_TILE_SIZE, color);
    }
  }
}

// --- 寻路算法实现 (The "Black Magic") ---

void MapSystem::updateFlowField(const Position &targetPos) {
  using namespace NoMoreDay::Constants::World;
  int targetX = static_cast<int>(targetPos.x / GRID_TILE_SIZE);
  int targetY = static_cast<int>(targetPos.y / GRID_TILE_SIZE);

  // 优化：如果目标瓦片没有变化，不重新计算
  if (targetX == static_cast<int>(m_lastFlowTarget.x) &&
      targetY == static_cast<int>(m_lastFlowTarget.y)) {
    return;
  }
  m_lastFlowTarget = {(float)targetX, (float)targetY};

  // 1. 生成积分场 (Integration Field) - Dijkstra / BFS
  std::fill(m_distanceField.begin(), m_distanceField.end(),
            std::numeric_limits<int>::max());
  std::queue<int> queue;

  if (isWalkable(targetX, targetY)) {
    int targetIdx = targetY * m_mapData.width + targetX;
    m_distanceField[targetIdx] = 0;
    queue.push(targetIdx);
  }

  // 限制搜索深度以优化性能
  using namespace NoMoreDay::Constants::World::Map;
  const int MAX_DEPTH = FLOW_FIELD_MAX_DEPTH;

  while (!queue.empty()) {
    int currIdx = queue.front();
    queue.pop();

    int cx = currIdx % m_mapData.width;
    int cy = currIdx / m_mapData.width;
    int currentDist = m_distanceField[currIdx];

    if (currentDist >= MAX_DEPTH)
      continue;

    // 检查 4 个邻居
    const int dirs[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    for (auto &dir : dirs) {
      int nx = cx + dir[0];
      int ny = cy + dir[1];

      if (nx >= 0 && nx < m_mapData.width && ny >= 0 && ny < m_mapData.height) {
        int nIdx = ny * m_mapData.width + nx;
        if (m_distanceField[nIdx] == std::numeric_limits<int>::max() &&
            isWalkable(nx, ny)) {
          m_distanceField[nIdx] = currentDist + 1;
          queue.push(nIdx);
        }
      }
    }
  }

  // 2. 生成流场 (Flow Field)
  // 对每个格子，指向距离最小的邻居
  for (int y = 0; y < m_mapData.height; ++y) {
    for (int x = 0; x < m_mapData.width; ++x) {
      int idx = y * m_mapData.width + x;
      if (m_distanceField[idx] == std::numeric_limits<int>::max()) {
        m_flowField[idx] = {0, 0};
        continue;
      }

      int minDist = m_distanceField[idx];
      Vector2 flow = {0, 0};

      const int dirs[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
      for (auto &dir : dirs) {
        int nx = x + dir[0];
        int ny = y + dir[1];
        if (nx >= 0 && nx < m_mapData.width && ny >= 0 &&
            ny < m_mapData.height) {
          int nIdx = ny * m_mapData.width + nx;
          if (m_distanceField[nIdx] < minDist) {
            minDist = m_distanceField[nIdx];
            flow = {(float)dir[0], (float)dir[1]};
          }
        }
      }
      m_flowField[idx] = flow;
    }
  }
}

Vector2 MapSystem::getFlowDirection(const Position &pos) const {
  using namespace NoMoreDay::Constants::World;
  int x = static_cast<int>(pos.x / GRID_TILE_SIZE);
  int y = static_cast<int>(pos.y / GRID_TILE_SIZE);

  if (x >= 0 && x < m_mapData.width && y >= 0 && y < m_mapData.height) {
    return m_flowField[y * m_mapData.width + x];
  }
  return {0, 0};
}

Position MapSystem::getPathNextStep(const Position &start,
                                    const Position &end) const {
  // 简单的 A* 实现，只返回下一步的位置
  // 为了性能，这里使用简化的贪心搜索或小范围 A*

  using namespace NoMoreDay::Constants::World;
  int startX = static_cast<int>(start.x / GRID_TILE_SIZE);
  int startY = static_cast<int>(start.y / GRID_TILE_SIZE);
  int endX = static_cast<int>(end.x / GRID_TILE_SIZE);
  int endY = static_cast<int>(end.y / GRID_TILE_SIZE);

  if (startX == endX && startY == endY)
    return end;

  // 简单的贪心：检查哪个邻居离终点更近且可行走
  // 注意：这可能会陷入局部最优，但对于"返回出生点"通常足够，
  // 如果需要更强壮的逻辑，可以替换为完整的 A*

  float minCost = std::numeric_limits<float>::max();
  Position nextStep = start;
  bool found = false;

  const int dirs[8][2] = {{0, 1}, {0, -1}, {1, 0},  {-1, 0},
                          {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

  for (auto &dir : dirs) {
    int nx = startX + dir[0];
    int ny = startY + dir[1];

    if (isWalkable(nx, ny)) {
      float dx = (float)(nx - endX);
      float dy = (float)(ny - endY);
      float distSq = dx * dx + dy * dy;

      if (distSq < minCost) {
        minCost = distSq;
        using namespace NoMoreDay::Constants::World;
        nextStep = {nx * GRID_TILE_SIZE + (GRID_TILE_SIZE / 2.0f),
                    ny * GRID_TILE_SIZE + (GRID_TILE_SIZE / 2.0f)};
        found = true;
      }
    }
  }

  return found ? nextStep : start;
}

// --- 占位符实现 (FogOfWarSystem 已经有自己的实现文件) ---
void MapSystem::renderFog(const Camera2D &camera) const {
  // 此函数在 FogOfWarSystem 中实现，这里只是为了接口完整性
  // 实际调用是在 Game::render 中直接调用 FogOfWarSystem::renderFog
}

void MapSystem::updateVisibility(const Position &playerPos, float viewRadius) {
  // 实际逻辑在 FogOfWarSystem 中
}

void MapSystem::ensureConnectivity(std::vector<Tile> &grid, int width,
                                   int height) {
  // 已经在 CaveMapGenerator 中处理
}

void MapSystem::floodFill(int startX, int startY, std::vector<bool> &visited,
                          std::vector<int> &regionMap, int regionId) {
  // 辅助函数
}

void MapSystem::updateFogTexture() {
  // 辅助函数
}

void MapSystem::initializeFogTexture(int width, int height) {
  // 辅助函数
}

entt::entity MapSystem::spawnDynamicObstacle(entt::registry &registry,
                                             const Rectangle &bounds,
                                             float duration) {
  auto entity = registry.create();
  // Position at center
  registry.emplace<Position>(entity, bounds.x + bounds.width * 0.5f,
                             bounds.y + bounds.height * 0.5f);
  registry.emplace<LocalLevelTag>(entity);
  // Static collider
  ColliderComponent collider;
  collider.width = bounds.width;
  collider.height = bounds.height;
  collider.type = ColliderType::Static;
  registry.emplace<ColliderComponent>(entity, collider);

  // Dynamic obstacle logic (lifetime)
  registry.emplace<NoMoreDay::DynamicObstacleComponent>(entity, duration,
                                                        (uint8_t)1);

  // Optional: Add visual component here if needed, or handle in VisualFXSystem
  // For now, PhysicsSystem will handle the improved collision

  return entity;
}