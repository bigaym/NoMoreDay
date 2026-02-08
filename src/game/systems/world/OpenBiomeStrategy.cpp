#include "game/systems/world/BiomeStrategies.hpp"
#include <algorithm>
#include <cmath>
#include <random>

namespace {
void ApplyBoundaries(std::vector<Tile> &grid, int width, int height) {
  for (int x = 0; x < width; ++x) {
    grid[x].type = Tile::Type::WALL;
    grid[(height - 1) * width + x].type = Tile::Type::WALL;
  }
  for (int y = 0; y < height; ++y) {
    grid[y * width].type = Tile::Type::WALL;
    grid[y * width + (width - 1)].type = Tile::Type::WALL;
  }
}

void SmoothIteration(const std::vector<Tile> &src, std::vector<Tile> &dst,
                     int width, int height, int birthLimit, int deathLimit) {
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      const int idx = y * width + x;
      int wallCount = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          if (src[(y + dy) * width + (x + dx)].type == Tile::Type::WALL) {
            ++wallCount;
          }
        }
      }

      if (src[idx].type == Tile::Type::WALL) {
        dst[idx].type = (wallCount < deathLimit) ? Tile::Type::FLOOR
                                                 : Tile::Type::WALL;
      } else {
        dst[idx].type = (wallCount > birthLimit) ? Tile::Type::WALL
                                                 : Tile::Type::FLOOR;
      }
    }
  }
}

bool IsOpenCombatBiome(const NoMoreDay::BiomeConfig &config) {
  return config.numericId >= NoMoreDay::BiomeID::SunPrairie &&
         config.numericId <= NoMoreDay::BiomeID::AshPlain;
}

float ComputeWallRatio(const std::vector<Tile> &grid) {
  if (grid.empty()) {
    return 0.0f;
  }

  size_t wallCount = 0;
  for (const Tile &tile : grid) {
    if (tile.type == Tile::Type::WALL) {
      ++wallCount;
    }
  }

  return static_cast<float>(wallCount) / static_cast<float>(grid.size());
}

void EnforceOpenCombatWallBand(std::vector<Tile> &grid, int width, int height,
                               std::mt19937 &gen) {
  constexpr float kMinWallRatio = 0.15f;
  constexpr float kMaxWallRatio = 0.22f;

  const int totalTiles = width * height;
  const int minWalls = static_cast<int>(std::ceil(totalTiles * kMinWallRatio));
  const int maxWalls = static_cast<int>(std::floor(totalTiles * kMaxWallRatio));

  int wallCount = 0;
  std::vector<int> floors;
  std::vector<int> walls;
  floors.reserve(static_cast<size_t>(totalTiles));
  walls.reserve(static_cast<size_t>(totalTiles));

  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      const int idx = y * width + x;
      if (grid[idx].type == Tile::Type::WALL) {
        ++wallCount;
        walls.push_back(idx);
      } else {
        floors.push_back(idx);
      }
    }
  }

  if (wallCount < minWalls) {
    std::shuffle(floors.begin(), floors.end(), gen);
    const int need = std::min(minWalls - wallCount, static_cast<int>(floors.size()));
    for (int i = 0; i < need; ++i) {
      grid[floors[static_cast<size_t>(i)]].type = Tile::Type::WALL;
    }
  } else if (wallCount > maxWalls) {
    std::shuffle(walls.begin(), walls.end(), gen);
    const int need = std::min(wallCount - maxWalls, static_cast<int>(walls.size()));
    for (int i = 0; i < need; ++i) {
      grid[walls[static_cast<size_t>(i)]].type = Tile::Type::FLOOR;
    }
  }
}
} // namespace

namespace NoMoreDay {

void OpenBiomeStrategy::GenerateTerrain(std::vector<Tile> &grid,
                                        const BiomeGenerationParams &params) {
  const int width = params.width;
  const int height = params.height;
  const BiomeConfig &config = *params.config;

  std::mt19937 gen(params.seed);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  const float wallProbability = std::clamp(config.wallProbability, 0.05f, 0.7f);

  for (Tile &tile : grid) {
    tile.type = (dist(gen) < wallProbability) ? Tile::Type::WALL
                                              : Tile::Type::FLOOR;
    tile.isAirWall = false;
    tile.visibility = 0;
  }
  ApplyBoundaries(grid, width, height);

  std::vector<Tile> buffer = grid;
  const int iterations = std::max(1, config.smoothIterations);
  for (int i = 0; i < iterations; ++i) {
    auto &src = (i % 2 == 0) ? grid : buffer;
    auto &dst = (i % 2 == 0) ? buffer : grid;
    SmoothIteration(src, dst, width, height, config.wallBirthLimit,
                    config.wallDeathLimit);
    ApplyBoundaries(dst, width, height);
  }
  if (iterations % 2 != 0) {
    grid = std::move(buffer);
  }
}

void OpenBiomeStrategy::PlaceSpecialStructures(
    std::vector<Tile> &grid, const BiomeGenerationParams &params) {
  const int width = params.width;
  const int height = params.height;
  const BiomeConfig &config = *params.config;
  std::mt19937 gen(params.seed ^ 0x6A09E667u);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  const float sparseBlockChance = 0.01f;
  for (int y = 2; y < height - 2; ++y) {
    for (int x = 2; x < width - 2; ++x) {
      const int idx = y * width + x;
      if (grid[idx].type != Tile::Type::FLOOR || dist(gen) > sparseBlockChance) {
        continue;
      }

      grid[idx].type = Tile::Type::WALL;
      if (dist(gen) < 0.25f) {
        grid[idx + 1].type = Tile::Type::WALL;
      }
      if (dist(gen) < 0.25f) {
        grid[idx - 1].type = Tile::Type::WALL;
      }
      if (dist(gen) < 0.25f) {
        grid[idx + width].type = Tile::Type::WALL;
      }
      if (dist(gen) < 0.25f) {
        grid[idx - width].type = Tile::Type::WALL;
      }
    }
  }

  if (IsOpenCombatBiome(config)) {
    EnforceOpenCombatWallBand(grid, width, height, gen);
  }
}

} // namespace NoMoreDay
