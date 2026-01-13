#include "game/systems/world/MapSystem.hpp"
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

  // 1. 初始化：进一步降低环境噪音 (0.10 -> 0.05)，确保留白更多
  for (auto &tile : map.grid) {
    tile.type = (dist(gen) < 0.05f) ? Tile::Type::WALL : Tile::Type::FLOOR;
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
  int cx = w / 2;
  int cy = h / 2;
  bool startPlaced = false;
  for (int r = 0; r < 20 && !startPlaced; r++) {
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
  for (int attempt = 0; attempt < 1000; ++attempt) {
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
      dst[y * w + x].type =
          (wallCount > 4) ? Tile::Type::WALL : Tile::Type::FLOOR;
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
  std::uniform_int_distribution<int> sizeDist(
      100, 400); // 调整每个岩块的目标面积为 100~400

  // 1. 确定岩块数量 (减少为原来的一半)
  int numRocks = (w * h) / 1200; // 原来是 600
  if (numRocks < 10)
    numRocks = 12; // 原来是 25

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
          std::uniform_int_distribution<int> chance(0, 100);
          if (chance(gen) < 85) { // 85% 扩张概率，保证紧凑
            q.push(ny * w + nx);
          }
        }
      }
    }
  }

  // 3. 强力平滑 (4次) 让岩块边缘圆润且合并临近块
  for (int i = 0; i < 4; ++i) {
    std::vector<Tile> src = grid;
    SmoothIteration(src, grid, w, h);
  }

  // 4. 清理残留的极小岛屿 (面积小于 80 的碎片直接移除)
  RemoveSmallRegions(grid, w, h, 80, Tile::Type::WALL, Tile::Type::FLOOR);

  // 5. 填充大岩块内部的小孔洞
  RemoveSmallRegions(grid, w, h, 40, Tile::Type::FLOOR, Tile::Type::WALL);
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
  m_cachedCostMap.resize(m_mapData.grid.size());
  for (size_t i = 0; i < m_mapData.grid.size(); i++) {
    m_cachedCostMap[i] = m_mapData.grid[i].isWalkable() ? 1 : 255;
  }
  m_costMapDirty = false;
}

void MapSystem::generateMap(int width, int height, const std::string &biome) {
  m_currentBiomeId = biome;
  generateCaveMap(width, height);
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
      (resonance.primaryBiome.empty()) ? "cave" : resonance.primaryBiome;
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
  m_cachedCostMap.resize(m_mapData.grid.size());
  for (size_t i = 0; i < m_mapData.grid.size(); i++) {
    m_cachedCostMap[i] = m_mapData.grid[i].isWalkable() ? 1 : 255;
  }
  m_costMapDirty = false;
}

bool MapSystem::isWalkable(int x, int y) const {
  return m_mapData.isWalkable(x, y);
}

Tile::Type MapSystem::getTileType(int x, int y) const {
  return m_mapData.getTile(x, y);
}

void MapSystem::render(const Camera2D &camera) const {
  // 简单的视锥剔除
  int startX = static_cast<int>(
                   (camera.target.x - camera.offset.x / camera.zoom) / 10.0f) -
               2;
  int startY = static_cast<int>(
                   (camera.target.y - camera.offset.y / camera.zoom) / 10.0f) -
               2;
  int endX =
      startX + static_cast<int>((GetScreenWidth() / camera.zoom) / 10.0f) + 4;
  int endY =
      startY + static_cast<int>((GetScreenHeight() / camera.zoom) / 10.0f) + 4;

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

      DrawRectangle(x * 10, y * 10, 10, 10, color);
    }
  }
}

// --- 寻路算法实现 (The "Black Magic") ---

void MapSystem::updateFlowField(const Position &targetPos) {
  int targetX = static_cast<int>(targetPos.x / 10.0f);
  int targetY = static_cast<int>(targetPos.y / 10.0f);

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

  // 限制搜索深度以优化性能 (例如只计算玩家周围 50 格)
  // 对于全图追踪，可以移除此限制或分帧计算
  const int MAX_DEPTH = 100;

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
  int x = static_cast<int>(pos.x / 10.0f);
  int y = static_cast<int>(pos.y / 10.0f);

  if (x >= 0 && x < m_mapData.width && y >= 0 && y < m_mapData.height) {
    return m_flowField[y * m_mapData.width + x];
  }
  return {0, 0};
}

Position MapSystem::getPathNextStep(const Position &start,
                                    const Position &end) const {
  // 简单的 A* 实现，只返回下一步的位置
  // 为了性能，这里使用简化的贪心搜索或小范围 A*

  int startX = static_cast<int>(start.x / 10.0f);
  int startY = static_cast<int>(start.y / 10.0f);
  int endX = static_cast<int>(end.x / 10.0f);
  int endY = static_cast<int>(end.y / 10.0f);

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
        nextStep = {nx * 10.0f + 5.0f, ny * 10.0f + 5.0f};
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