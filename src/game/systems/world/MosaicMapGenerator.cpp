#include "game/systems/world/MosaicMapGenerator.hpp"
#include "core/logging/Logger.hpp"
#include "game/components/MapFragmentComponent.hpp"
#include "game/components/Common.hpp"
#include "game/systems/world/MapGeneratorConstants.hpp"
#include <algorithm>
#include <queue>
#include <random>


namespace NoMoreDay {

void MosaicMapGenerator::SetMosaicData(const MosaicGrid &grid,
                                       const ResonanceResult &resonance,
                                       entt::registry *registry) {
  m_grid = grid;
  m_resonance = resonance;
  m_registry = registry;
}

MapGenerator::MapData MosaicMapGenerator::Generate(int width, int height,
                                                   uint32_t seed,
                                                   float wallProb,
                                                   int iterations) {
  MapData map{width, height};
  map.grid.resize(width * height);

  std::mt19937 gen(seed);

  // 1. 计算 9 个区域的边界
  auto zones = CalculateZones(width, height);

  // 2. 为每个区域生成基础地形
  for (int i = 0; i < MosaicGrid::TOTAL_CELLS; ++i) {
    GenerateZone(map.grid, width, height, zones[i], gen());
  }

  // 3. 平滑区域边界
  SmoothZoneBorders(map.grid, width, height);

  // 4. 应用边界
  for (int x = 0; x < width; ++x) {
    map.grid[0 * width + x].type = Tile::Type::WALL;
    map.grid[(height - 1) * width + x].type = Tile::Type::WALL;
  }
  for (int y = 0; y < height; ++y) {
    map.grid[y * width + 0].type = Tile::Type::WALL;
    map.grid[y * width + (width - 1)].type = Tile::Type::WALL;
  }

  // 5. 确保连通性
  EnsureConnectivity(map.grid, width, height);

  // 6. 放置特殊房间 (Boss/商人/宝库)
  PlaceSpecialRooms(map.grid, width, height, gen());

  // 7. 放置出口
  PlaceExits(map.grid, width, height, gen());

  LOG_INFO("MosaicMapGenerator: Generated {}x{} map with {} fragments", width,
           height, m_grid.GetFilledCount());

  return map;
}

std::array<MosaicMapGenerator::ZoneInfo, MosaicGrid::TOTAL_CELLS>
MosaicMapGenerator::CalculateZones(int width, int height) {
  std::array<ZoneInfo, MosaicGrid::TOTAL_CELLS> zones;

  int zoneWidth = width / MosaicGrid::SIZE;
  int zoneHeight = height / MosaicGrid::SIZE;

  for (int gy = 0; gy < MosaicGrid::SIZE; ++gy) {
    for (int gx = 0; gx < MosaicGrid::SIZE; ++gx) {
      int index = MosaicGrid::ToIndex(gx, gy);

      zones[index].startX = gx * zoneWidth;
      zones[index].startY = gy * zoneHeight;
      zones[index].endX = (gx == MosaicGrid::SIZE - 1) ? width : (gx + 1) * zoneWidth;
      zones[index].endY = (gy == MosaicGrid::SIZE - 1) ? height : (gy + 1) * zoneHeight;
      zones[index].resonanceMultiplier = m_grid.resonanceMultipliers[index];

      // 获取碎片组件
      entt::entity fragmentEntity = m_grid.cells[index];
      if (fragmentEntity != entt::null && m_registry &&
          m_registry->valid(fragmentEntity)) {
        zones[index].fragment =
            m_registry->try_get<MapFragmentComponent>(fragmentEntity);
      }
    }
  }

  return zones;
}

void MosaicMapGenerator::GenerateZone(std::vector<Tile> &grid, int mapWidth,
                                      int mapHeight, const ZoneInfo &zone,
                                      uint32_t seed) {
  std::mt19937 gen(seed);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  // 1. 初始化全为地板
  for (int y = zone.startY; y < zone.endY; ++y) {
    for (int x = zone.startX; x < zone.endX; ++x) {
      if (x <= 0 || x >= mapWidth - 1 || y <= 0 || y >= mapHeight - 1) {
        grid[y * mapWidth + x].type = Tile::Type::WALL;
      } else {
        grid[y * mapWidth + x].type = Tile::Type::FLOOR;
      }
    }
  }

  // 2. 使用 BFS "岩石堆积" 算法 (复刻 CaveMapGenerator 逻辑)
  // 参考 Game Design: 生成从 0 到 标准密度 的随机障碍物
  using namespace NoMoreDay::Constants::Generator::Cave;
  
  int zoneW = zone.endX - zone.startX;
  int zoneH = zone.endY - zone.startY;
  int area = zoneW * zoneH;
  
  // 基准最大数量 (与初始地图密度一致)
  int maxRocks = area / ROCK_DENSITY_DIVISOR;

  // 随机密度因子 [0.0, 1.0] (符合"随机生成完全无障碍物到初始地图水平")
  float densityFactor = dist(gen);
  
  // 碎片修正
  if (zone.fragment) {
    if (zone.fragment->type == FragmentType::Terrain) {
       // 地形碎片稍微偏向高密度，但保持随机性
       densityFactor = std::min(1.2f, densityFactor * 1.5f);
    } else if (zone.fragment->type == FragmentType::Unique) {
       // Unique 碎片大幅减少
       densityFactor *= 0.2f;
    }
  }

  int numRocks = static_cast<int>(maxRocks * densityFactor);
  
  // 最小尺寸岩石生成
  if (numRocks > 0) {
      std::uniform_int_distribution<int> xDist(zone.startX + 5, zone.endX - 6);
      std::uniform_int_distribution<int> yDist(zone.startY + 5, zone.endY - 6);
      std::uniform_int_distribution<int> sizeDist(ROCK_SIZE_MIN, ROCK_SIZE_MAX);

      for (int i = 0; i < numRocks; ++i) {
          int cx = xDist(gen);
          int cy = yDist(gen);
          int targetArea = sizeDist(gen);
          int currentArea = 0;

          std::queue<int> q;
          q.push(cy * mapWidth + cx);
          
          while (!q.empty() && currentArea < targetArea) {
              int curr = q.front();
              q.pop();

              if (curr < 0 || curr >= grid.size()) continue; // Safety
              if (grid[curr].type == Tile::Type::WALL) continue;

              grid[curr].type = Tile::Type::WALL;
              currentArea++;

              int curX = curr % mapWidth;
              int curY = curr / mapWidth;

              // 四周扩张
              const int dx[] = {0, 0, 1, -1};
              const int dy[] = {1, -1, 0, 0};
              
              for (int d = 0; d < 4; ++d) {
                  int nx = curX + dx[d];
                  int ny = curY + dy[d];
                  // 严格限制在 Zone 内部 (留 buffer)
                  if (nx >= zone.startX + 1 && nx < zone.endX - 1 &&
                      ny >= zone.startY + 1 && ny < zone.endY - 1) {
                      
                      std::uniform_int_distribution<int> chance(0, 100);
                      if (chance(gen) < ROCK_EXPANSION_CHANCE) {
                          q.push(ny * mapWidth + nx);
                      }
                  }
              }
          }
      }
  }

  // 应用碎片特效 (这里不再做平滑，留给全局 SmoothZoneBorders 做统一风格化)
  ApplyFragmentEffects(grid, mapWidth, zone);
}

void MosaicMapGenerator::ApplyFragmentEffects(std::vector<Tile> &grid,
                                              int mapWidth,
                                              const ZoneInfo &zone) {
  if (!zone.fragment)
    return;

  std::mt19937 gen(static_cast<uint32_t>(zone.startX * 1000 + zone.startY));

  int zoneWidth = zone.endX - zone.startX;
  int zoneHeight = zone.endY - zone.startY;
  int centerX = zone.startX + zoneWidth / 2;
  int centerY = zone.startY + zoneHeight / 2;

  // 特殊碎片效果
  if (zone.fragment->type == FragmentType::Unique) {
    // 创建一个空旷的中心区域
    int clearRadius = std::min(zoneWidth, zoneHeight) / 4;
    for (int dy = -clearRadius; dy <= clearRadius; ++dy) {
      for (int dx = -clearRadius; dx <= clearRadius; ++dx) {
        if (dx * dx + dy * dy <= clearRadius * clearRadius) {
          int x = centerX + dx;
          int y = centerY + dy;
          if (x > 0 && x < mapWidth - 1 && y > 0) {
            grid[y * mapWidth + x].type = Tile::Type::FLOOR;
          }
        }
      }
    }
  }

  // 地形碎片可以覆盖不同的生物群系风格
  // 这里简单处理，实际可以根据 biomeOverride 生成不同的地形
}

void MosaicMapGenerator::SmoothZoneBorders(std::vector<Tile> &grid, int width,
                                           int height) {
  // 多次平滑迭代
  std::vector<Tile> buffer = grid;

  for (int iter = 0; iter < 3; ++iter) {
    auto &src = (iter % 2 == 0) ? grid : buffer;
    auto &dst = (iter % 2 == 0) ? buffer : grid;
    SmoothIteration(src, dst, width, height);
  }

  if (3 % 2 != 0) {
    grid = std::move(buffer);
  }
}

void MosaicMapGenerator::SmoothIteration(const std::vector<Tile> &src,
                                         std::vector<Tile> &dst, int w, int h) {
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      int wallCount = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (src[(y + dy) * w + (x + dx)].type == Tile::Type::WALL) {
            wallCount++;
          }
        }
      }
      // 4-5 规则
      dst[y * w + x].type =
          (wallCount > 4) ? Tile::Type::WALL : Tile::Type::FLOOR;
    }
  }
}

void MosaicMapGenerator::PlaceSpecialRooms(std::vector<Tile> &grid, int width,
                                           int height, uint32_t seed) {
  std::mt19937 gen(seed);

  // Boss 房间
  if (m_resonance.hasBoss) {
    // 在某个区域中心创建 Boss 房间
    for (int i = 0; i < MosaicGrid::TOTAL_CELLS; ++i) {
      entt::entity entity = m_grid.cells[i];
      if (entity != entt::null && m_registry && m_registry->valid(entity)) {
        auto *frag = m_registry->try_get<MapFragmentComponent>(entity);
        if (frag && frag->hasBoss) {
          int zoneW = width / MosaicGrid::SIZE;
          int zoneH = height / MosaicGrid::SIZE;
          int gx, gy;
          MosaicGrid::ToCoord(i, gx, gy);
          int cx = gx * zoneW + zoneW / 2;
          int cy = gy * zoneH + zoneH / 2;

          // 创建 Boss 竞技场 (圆形区域)
          int arenaRadius = std::min(zoneW, zoneH) / 3;
          for (int dy = -arenaRadius; dy <= arenaRadius; ++dy) {
            for (int dx = -arenaRadius; dx <= arenaRadius; ++dx) {
              if (dx * dx + dy * dy <= arenaRadius * arenaRadius) {
                int x = cx + dx;
                int y = cy + dy;
                if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
                  grid[y * width + x].type = Tile::Type::FLOOR;
                }
              }
            }
          }
          LOG_DEBUG("MosaicMapGenerator: Placed Boss arena at ({}, {})", cx,
                    cy);
          break;
        }
      }
    }
  }

  // 商人房间
  if (m_resonance.hasMerchant) {
    // 类似处理，创建商人安全区
    for (int i = 0; i < MosaicGrid::TOTAL_CELLS; ++i) {
      entt::entity entity = m_grid.cells[i];
      if (entity != entt::null && m_registry && m_registry->valid(entity)) {
        auto *frag = m_registry->try_get<MapFragmentComponent>(entity);
        if (frag && frag->hasMerchant) {
          int zoneW = width / MosaicGrid::SIZE;
          int zoneH = height / MosaicGrid::SIZE;
          int gx, gy;
          MosaicGrid::ToCoord(i, gx, gy);
          int cx = gx * zoneW + zoneW / 2;
          int cy = gy * zoneH + zoneH / 2;

          // 创建商人房间 (小矩形)
          int roomW = 6;
          int roomH = 6;
          for (int dy = -roomH / 2; dy <= roomH / 2; ++dy) {
            for (int dx = -roomW / 2; dx <= roomW / 2; ++dx) {
              int x = cx + dx;
              int y = cy + dy;
              if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
                grid[y * width + x].type = Tile::Type::FLOOR;
              }
            }
          }
          LOG_DEBUG("MosaicMapGenerator: Placed Merchant room at ({}, {})", cx,
                    cy);
          break;
        }
      }
    }
  }

  // 宝库
  if (m_resonance.hasTreasure) {
    for (int i = 0; i < MosaicGrid::TOTAL_CELLS; ++i) {
      entt::entity entity = m_grid.cells[i];
      if (entity != entt::null && m_registry && m_registry->valid(entity)) {
        auto *frag = m_registry->try_get<MapFragmentComponent>(entity);
        if (frag && frag->hasTreasure) {
          int zoneW = width / MosaicGrid::SIZE;
          int zoneH = height / MosaicGrid::SIZE;
          int gx, gy;
          MosaicGrid::ToCoord(i, gx, gy);
          int cx = gx * zoneW + zoneW / 2;
          int cy = gy * zoneH + zoneH / 2;

          // 宝库：小房间
          int roomR = 4;
          for (int dy = -roomR; dy <= roomR; ++dy) {
            for (int dx = -roomR; dx <= roomR; ++dx) {
              int x = cx + dx;
              int y = cy + dy;
              if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
                grid[y * width + x].type = Tile::Type::FLOOR;
              }
            }
          }
          LOG_DEBUG("MosaicMapGenerator: Placed Treasure room at ({}, {})", cx,
                    cy);
          break;
        }
      }
    }
  }
}

void MosaicMapGenerator::EnsureConnectivity(std::vector<Tile> &grid, int width,
                                            int height) {
  // 找到最大的可行走区域
  std::vector<bool> visited(width * height, false);
  std::vector<int> bestRegion;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int idx = y * width + x;
      if (grid[idx].type != Tile::Type::WALL && !visited[idx]) {
        std::vector<int> currentRegion;
        std::queue<int> q;
        q.push(idx);
        visited[idx] = true;

        while (!q.empty()) {
          int curr = q.front();
          q.pop();
          currentRegion.push_back(curr);

          int cx = curr % width;
          int cy = curr / width;
          const int dx[] = {0, 0, 1, -1};
          const int dy[] = {1, -1, 0, 0};
          for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
              int nIdx = ny * width + nx;
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

  // 填充非连通区域
  std::vector<bool> isBest(width * height, false);
  for (int idx : bestRegion)
    isBest[idx] = true;

  for (int i = 0; i < width * height; ++i) {
    if (grid[i].type != Tile::Type::WALL && !isBest[i]) {
      grid[i].type = Tile::Type::WALL;
    }
  }
}

void MosaicMapGenerator::PlaceExits(std::vector<Tile> &grid, int width,
                                    int height, uint32_t seed) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> xDist(5, width - 6);
  std::uniform_int_distribution<int> yDist(5, height - 6);

  // 出生点 (中心附近)
  int cx = width / 2;
  int cy = height / 2;
  bool startPlaced = false;
  for (int r = 0; r < 30 && !startPlaced; r++) {
    for (int dx = -r; dx <= r && !startPlaced; dx++) {
      for (int dy = -r; dy <= r && !startPlaced; dy++) {
        int x = cx + dx;
        int y = cy + dy;
        if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
          if (grid[y * width + x].type == Tile::Type::FLOOR) {
            grid[y * width + x].type = Tile::Type::STAIRS_UP;
            startPlaced = true;
          }
        }
      }
    }
  }

  // 出口 (随机位置)
  for (int attempt = 0; attempt < 1000; ++attempt) {
    int x = xDist(gen);
    int y = yDist(gen);
    if (grid[y * width + x].type == Tile::Type::FLOOR) {
      grid[y * width + x].type = Tile::Type::STAIRS_DOWN;
      break;
    }
  }
}

} // namespace NoMoreDay
