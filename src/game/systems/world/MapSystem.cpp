#include "game/systems/world/MapSystem.hpp"
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/Common.hpp"
#include "game/systems/world/WorldConstants.hpp"
#include "game/systems/world/MapGeneratorConstants.hpp"
#include "game/components/WorldState.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/systems/world/BiomeMapGenerator.hpp"
#include "game/systems/world/MosaicMapGenerator.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace {
void MarkAirWalls(std::vector<Tile> &grid, bool enableAirWall) {
  for (Tile &tile : grid) {
    tile.isAirWall = enableAirWall && tile.type == Tile::Type::WALL;
  }
}

bool HasFloorNeighbor(const std::vector<Tile> &grid, int width, int height, int x,
                      int y) {
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      const int nx = x + dx;
      const int ny = y + dy;
      if (nx <= 0 || ny <= 0 || nx >= width - 1 || ny >= height - 1) {
        continue;
      }
      if (grid[ny * width + nx].isWalkable()) {
        return true;
      }
    }
  }
  return false;
}
} // namespace

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

  // 3. 生成障碍物(使用种子生长法)
  GenerateObstacles(map.grid, width, height, gen());

  // 4. 边界处理
  ApplyBoundaries(map.grid, width, height);

  // 5. 确保 100% 连通性(防止大块障碍物切断路径)
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

    // 使用 BFS 方式扩张"岩体"
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
  MarkAirWalls(m_mapData.grid,
               biome.hasFeature(NoMoreDay::BiomeFeature::AirWall));

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
  initializeBiomeInteractionLayers(biome);
}

void MapSystem::generateMap(int width, int height, const std::string &biome) {
  m_currentBiomeId = biome;
  const auto &biomeConfig = NoMoreDay::BiomeRegistry::Get().GetBiome(biome);

  // Town biomes keep dedicated safe-zone layout.
  if (biomeConfig.style == NoMoreDay::BiomeStyle::Town) {
    generateTownMap(width, height);
  } else {
    NoMoreDay::BiomeMapGenerator generator;
    auto mapData =
        generator.GenerateForBiome(width, height, biomeConfig, m_gen());

    m_mapData.width = mapData.width;
    m_mapData.height = mapData.height;
    m_mapData.grid = std::move(mapData.grid);
    MarkAirWalls(m_mapData.grid,
                 biomeConfig.hasFeature(NoMoreDay::BiomeFeature::AirWall));

    m_flowField.resize(m_mapData.width * m_mapData.height);
    m_distanceField.resize(m_mapData.width * m_mapData.height);

    using namespace NoMoreDay::Constants::World::Map;
    m_cachedCostMap.resize(m_mapData.grid.size());
    for (size_t i = 0; i < m_mapData.grid.size(); i++) {
      m_cachedCostMap[i] =
          m_mapData.grid[i].isWalkable() ? COST_FLOOR : COST_WALL;
    }
    m_costMapDirty = false;
    initializeBiomeInteractionLayers(biomeConfig);
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
  int exitX = std::clamp(cx + TOWN_EXIT_X_OFFSET, 1, width - 2);
  int exitY = std::clamp(cy + TOWN_EXIT_Y_OFFSET, 1, height - 2);
  m_mapData.grid[exitY * width + exitX].type = Tile::Type::STAIRS_DOWN;

  // Initialize flow field
  m_flowField.resize(m_mapData.width * m_mapData.height);
  m_distanceField.resize(m_mapData.width * m_mapData.height);
  MarkAirWalls(m_mapData.grid, false);

  // Initialize cached cost map
  using namespace NoMoreDay::Constants::World::Map;
  m_cachedCostMap.resize(m_mapData.grid.size());
  for (size_t i = 0; i < m_mapData.grid.size(); i++) {
    m_cachedCostMap[i] =
        m_mapData.grid[i].isWalkable() ? COST_FLOOR : COST_WALL;
  }
  m_costMapDirty = false;
  const auto &biome = NoMoreDay::BiomeRegistry::Get().GetBiome(m_currentBiomeId);
  initializeBiomeInteractionLayers(biome);
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
  uint32_t seed = m_gen();
  if (registry && registry->ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
      const auto& state = registry->ctx().get<NoMoreDay::ActiveDimensionalState>();
      seed = state.seed ^ (state.currentDepth * 0x9e3779b9); // Use depth to vary seed
      LOG_INFO("Using Dimensional Seed: {} (Base: {}, Depth: {})", seed, state.seed, state.currentDepth);
  }

  auto mapData = generator.Generate(
      width, height, seed, biome.wallProbability, biome.smoothIterations);

  m_mapData.width = mapData.width;
  m_mapData.height = mapData.height;
  m_mapData.grid = std::move(mapData.grid);
  MarkAirWalls(m_mapData.grid,
               biome.hasFeature(NoMoreDay::BiomeFeature::AirWall));

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
  initializeBiomeInteractionLayers(biome);
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

bool MapSystem::applyTileDamageAt(int x, int y, float damage) {
  if (damage <= 0.0f || x < 0 || y < 0 || x >= m_mapData.width ||
      y >= m_mapData.height) {
    return false;
  }

  const int idx = y * m_mapData.width + x;
  Tile &tile = m_mapData.grid[idx];
  if (tile.type != Tile::Type::WALL || idx >= (int)m_destructibleTiles.size()) {
    return false;
  }

  auto &destructible = m_destructibleTiles[idx];
  if (destructible.maxHP <= 0.0f || destructible.isDestroyed) {
    return false;
  }

  destructible.currentHP =
      std::max(0.0f, destructible.currentHP - damage);
  if (destructible.currentHP > 0.0f) {
    return false;
  }

  destructible.isDestroyed = true;
  tile.type = destructible.destroyedType;
  tile.isAirWall = false;
  if (idx < (int)m_cachedCostMap.size()) {
    using namespace NoMoreDay::Constants::World::Map;
    m_cachedCostMap[idx] = COST_FLOOR;
  }
  m_costMapDirty = false;

  const float worldX = (x + 0.5f) * NoMoreDay::Constants::World::GRID_TILE_SIZE;
  const float worldY = (y + 0.5f) * NoMoreDay::Constants::World::GRID_TILE_SIZE;
  for (int i = 0; i < 8; ++i) {
    NoMoreDay::components::GPUParticle particle;
    particle.position = {worldX, worldY};
    const float angle = (i / 8.0f) * 6.283185f;
    particle.velocity = {std::cos(angle) * 80.0f, std::sin(angle) * 80.0f};
    particle.color = {150, 130, 100, 200};
    particle.lifetime = 0.25f;
    particle.maxLifetime = 0.25f;
    particle.scale = 2.0f;
    NoMoreDay::systems::GPUParticleSystem::Get().Emit(particle);
  }
  return true;
}

int MapSystem::applyRadialTileDamage(const Position &center, float radius,
                                     float damage) {
  if (radius <= 0.0f || damage <= 0.0f || m_mapData.width <= 0 ||
      m_mapData.height <= 0) {
    return 0;
  }
  using namespace NoMoreDay::Constants::World;
  const int minX = std::max(0, (int)((center.x - radius) / GRID_TILE_SIZE));
  const int minY = std::max(0, (int)((center.y - radius) / GRID_TILE_SIZE));
  const int maxX =
      std::min(m_mapData.width - 1, (int)((center.x + radius) / GRID_TILE_SIZE));
  const int maxY = std::min(m_mapData.height - 1,
                            (int)((center.y + radius) / GRID_TILE_SIZE));
  const float radiusSq = radius * radius;

  int destroyed = 0;
  for (int y = minY; y <= maxY; ++y) {
    for (int x = minX; x <= maxX; ++x) {
      const float worldX = (x + 0.5f) * GRID_TILE_SIZE;
      const float worldY = (y + 0.5f) * GRID_TILE_SIZE;
      const float dx = worldX - center.x;
      const float dy = worldY - center.y;
      if (dx * dx + dy * dy > radiusSq) {
        continue;
      }
      destroyed += applyTileDamageAt(x, y, damage) ? 1 : 0;
    }
  }
  return destroyed;
}

float MapSystem::getSpeedMultiplierAtWorld(float worldX, float worldY) const {
  float multiplier = 1.0f;
  for (const auto &zone : m_speedZones) {
    if (!zone.isActive) {
      continue;
    }
    const float dx = worldX - zone.center.x;
    const float dy = worldY - zone.center.y;
    if (dx * dx + dy * dy <= zone.radius * zone.radius) {
      multiplier = std::max(multiplier, zone.speedMultiplier);
    }
  }
  return multiplier;
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

  // 渲染所有瓦片- GPU FogOfWarSystem 负责在顶层绘制迷雾遮蔽
  for (int y = startY; y < endY; ++y) {
    for (int x = startX; x < endX; ++x) {
      const Tile &tile = m_mapData.grid[y * m_mapData.width + x];
      Tile::Type type = tile.type;
      Color color = BLACK;

      switch (type) {
      case Tile::Type::FLOOR:
      case Tile::Type::STAIRS_DOWN: // Render as Floor (Entity VFX handles visual)
      case Tile::Type::STAIRS_UP:   // Render as Floor (Entity VFX handles visual)
        color = biome.floorColor;
        break;
      case Tile::Type::WALL:
        if (tile.isAirWall) {
          continue;
        }
        color = biome.wallColor;
        if (y * m_mapData.width + x < (int)m_destructibleTiles.size()) {
          const auto &destructible = m_destructibleTiles[y * m_mapData.width + x];
          if (destructible.maxHP > 0.0f && !destructible.isDestroyed) {
            const float hpRatio =
                std::clamp(destructible.currentHP / destructible.maxHP, 0.0f, 1.0f);
            color = ColorLerp({120, 60, 40, 255}, biome.wallColor, hpRatio);
          }
        }
        break;
      default:
        color = biome.floorColor;
        break;
      }

      using namespace NoMoreDay::Constants::World;
      DrawRectangle(x * (int)GRID_TILE_SIZE, y * (int)GRID_TILE_SIZE,
                    (int)GRID_TILE_SIZE, (int)GRID_TILE_SIZE, color);

      for (const auto &spawner : m_spawnerWalls) {
        if (!spawner.isActive || spawner.gridX != x || spawner.gridY != y) {
          continue;
        }
        const float pulse = 0.5f + 0.5f * std::sin((float)GetTime() * 5.0f);
        DrawRectangle(x * (int)GRID_TILE_SIZE, y * (int)GRID_TILE_SIZE,
                      (int)GRID_TILE_SIZE, (int)GRID_TILE_SIZE,
                      ColorAlpha(ORANGE, 0.25f + pulse * 0.35f));
      }
    }
  }

  for (const auto &zone : m_speedZones) {
    if (!zone.isActive) {
      continue;
    }
    DrawCircleLines((int)zone.center.x, (int)zone.center.y, zone.radius,
                    ColorAlpha(SKYBLUE, 0.45f));
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

  // 1. 生成积分场(Integration Field) - Dijkstra / BFS
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
  // 注意：这可能会陷入局部最优，但对于返回出生点通常足够
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

// --- 占位符实现(FogOfWarSystem 已经有自己的实现文件) ---
void MapSystem::renderFog(const Camera2D &camera) const {
  // 此函数在 FogOfWarSystem 中实现，这里只是为了接口完整性
  // 实际调用是在 Game::render 中直接调用 FogOfWarSystem::renderFog
}

void MapSystem::updateVisibility(const Position &playerPos, float viewRadius) {
  // 实际逻辑在 FogOfWarSystem 中
}

void MapSystem::ensureConnectivity(std::vector<Tile> &grid, int width,
                                   int height) {
  // 已经在 CaveMapGenerator 中处理完成
}

void MapSystem::floodFill(int startX, int startY, std::vector<bool> &visited,
                          std::vector<int> &regionMap, int regionId) {
  // 辅助函数
}

void MapSystem::updateFogTexture() {
  // 辅助函数
}


void MapSystem::initializeBiomeInteractionLayers(
    const NoMoreDay::BiomeConfig &biomeConfig) {
  const size_t tileCount = m_mapData.grid.size();
  m_destructibleTiles.assign(tileCount, {});
  m_spawnerWalls.clear();
  m_speedZones.clear();

  if (tileCount == 0) {
    return;
  }

  if (biomeConfig.hasFeature(NoMoreDay::BiomeFeature::Destructible)) {
    std::uniform_real_distribution<float> hpDist(80.0f, 140.0f);
    for (size_t i = 0; i < tileCount; ++i) {
      if (m_mapData.grid[i].type != Tile::Type::WALL) {
        m_destructibleTiles[i].maxHP = 0.0f;
        m_destructibleTiles[i].currentHP = 0.0f;
        continue;
      }
      const float hp = hpDist(m_gen);
      m_destructibleTiles[i].maxHP = hp;
      m_destructibleTiles[i].currentHP = hp;
      m_destructibleTiles[i].debrisType = "stone";
      m_destructibleTiles[i].isDestroyed = false;
      m_destructibleTiles[i].destroyedType = Tile::Type::FLOOR;
    }
  } else {
    for (auto &tile : m_destructibleTiles) {
      tile.maxHP = 0.0f;
      tile.currentHP = 0.0f;
      tile.isDestroyed = true;
    }
  }

  if (biomeConfig.hasFeature(NoMoreDay::BiomeFeature::DynamicSpawner)) {
    const int targetCount =
        std::max(3, (m_mapData.width * m_mapData.height) / 3500);
    std::uniform_int_distribution<int> xDist(1, std::max(1, m_mapData.width - 2));
    std::uniform_int_distribution<int> yDist(1, std::max(1, m_mapData.height - 2));

    int attempts = targetCount * 20;
    while ((int)m_spawnerWalls.size() < targetCount && attempts-- > 0) {
      const int x = xDist(m_gen);
      const int y = yDist(m_gen);
      const int idx = y * m_mapData.width + x;
      if (m_mapData.grid[idx].type != Tile::Type::WALL ||
          !HasFloorNeighbor(m_mapData.grid, m_mapData.width, m_mapData.height, x,
                            y)) {
        continue;
      }

      bool duplicate = false;
      for (const auto &existing : m_spawnerWalls) {
        if (existing.gridX == x && existing.gridY == y) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) {
        continue;
      }

      SpawnerWallComponent spawner;
      spawner.gridX = x;
      spawner.gridY = y;
      spawner.spawnInterval = 3.0f;
      spawner.maxSpawns = 12;
      spawner.spawnPool = biomeConfig.enemyPool.empty()
                              ? std::vector<std::string>{"corrupted", "demon"}
                              : biomeConfig.enemyPool;
      m_spawnerWalls.push_back(std::move(spawner));
    }
  }

  if (biomeConfig.hasFeature(NoMoreDay::BiomeFeature::SpeedZone)) {
    const int zoneCount =
        std::max(2, (m_mapData.width * m_mapData.height) / 4500);
    std::uniform_int_distribution<int> xDist(2, std::max(2, m_mapData.width - 3));
    std::uniform_int_distribution<int> yDist(2, std::max(2, m_mapData.height - 3));

    int attempts = zoneCount * 25;
    while ((int)m_speedZones.size() < zoneCount && attempts-- > 0) {
      const int x = xDist(m_gen);
      const int y = yDist(m_gen);
      const int idx = y * m_mapData.width + x;
      if (!m_mapData.grid[idx].isWalkable()) {
        continue;
      }

      SpeedZoneComponent zone;
      zone.center = {(x + 0.5f) * NoMoreDay::Constants::World::GRID_TILE_SIZE,
                     (y + 0.5f) * NoMoreDay::Constants::World::GRID_TILE_SIZE};
      zone.radius = 45.0f;
      zone.speedMultiplier = 1.3f;
      zone.isActive = true;
      m_speedZones.push_back(zone);
    }
  }
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

