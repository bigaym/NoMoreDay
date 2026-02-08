#include "game/systems/world/BiomeStrategies.hpp"
#include <algorithm>
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
} // namespace

namespace NoMoreDay {

void SpecialBiomeStrategy::GenerateTerrain(std::vector<Tile> &grid,
                                           const BiomeGenerationParams &params) {
  const int width = params.width;
  const int height = params.height;
  const BiomeConfig &config = *params.config;

  std::mt19937 gen(params.seed);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  const float wallProbability = std::clamp(config.wallProbability, 0.2f, 0.75f);

  for (Tile &tile : grid) {
    tile.type = (dist(gen) < wallProbability) ? Tile::Type::WALL
                                              : Tile::Type::FLOOR;
    tile.isAirWall = false;
    tile.visibility = 0;
  }
  ApplyBoundaries(grid, width, height);

  std::vector<Tile> buffer = grid;
  const int iterations = std::max(2, config.smoothIterations);
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

void SpecialBiomeStrategy::PlaceSpecialStructures(
    std::vector<Tile> &grid, const BiomeGenerationParams &params) {
  const int width = params.width;
  const int height = params.height;
  const BiomeConfig &config = *params.config;

  switch (config.numericId) {
  case BiomeID::FloatingIsle:
  case BiomeID::SkyPalace:
    GenerateFloatingPlatforms(grid, width, height, params.seed ^ 0xA54FF53Au);
    GenerateBridges(grid, width, height, params.seed ^ 0x510E527Fu);
    break;
  case BiomeID::HolyArena:
    GenerateHolyArena(grid, width, height);
    break;
  case BiomeID::HiveNest:
    GenerateHiveNest(grid, width, height, params.seed ^ 0x1F83D9ABu);
    break;
  default:
    // Keep CA result for other special biomes.
    break;
  }
}

void SpecialBiomeStrategy::GenerateFloatingPlatforms(std::vector<Tile> &grid,
                                                     int width, int height,
                                                     uint32_t seed) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> centerX(6, width - 7);
  std::uniform_int_distribution<int> centerY(6, height - 7);
  std::uniform_int_distribution<int> radiusDist(2, 5);

  const int islandCount = std::max(6, (width * height) / 2200);
  for (int i = 0; i < islandCount; ++i) {
    const int cx = centerX(gen);
    const int cy = centerY(gen);
    const int radius = radiusDist(gen);
    for (int y = cy - radius; y <= cy + radius; ++y) {
      for (int x = cx - radius; x <= cx + radius; ++x) {
        if (x <= 1 || x >= width - 2 || y <= 1 || y >= height - 2) {
          continue;
        }
        const int dx = x - cx;
        const int dy = y - cy;
        if (dx * dx + dy * dy <= radius * radius) {
          grid[y * width + x].type = Tile::Type::FLOOR;
        }
      }
    }
  }
}

void SpecialBiomeStrategy::GenerateBridges(std::vector<Tile> &grid, int width,
                                           int height, uint32_t seed) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> xDist(2, width - 3);
  std::uniform_int_distribution<int> yDist(2, height - 3);

  const int bridgeCount = std::max(4, (width * height) / 3000);
  for (int i = 0; i < bridgeCount; ++i) {
    int x = xDist(gen);
    int y = yDist(gen);
    const bool horizontal = (i % 2 == 0);
    const int length = std::max(8, (horizontal ? width : height) / 5);
    for (int step = 0; step < length; ++step) {
      if (x <= 1 || x >= width - 2 || y <= 1 || y >= height - 2) {
        break;
      }
      grid[y * width + x].type = Tile::Type::FLOOR;
      if (horizontal) {
        ++x;
      } else {
        ++y;
      }
    }
  }
}

void SpecialBiomeStrategy::GenerateHolyArena(std::vector<Tile> &grid, int width,
                                             int height) {
  const int cx = width / 2;
  const int cy = height / 2;
  const int radius = std::max(6, std::min(width, height) / 6);
  for (int y = cy - radius; y <= cy + radius; ++y) {
    for (int x = cx - radius; x <= cx + radius; ++x) {
      if (x <= 1 || x >= width - 2 || y <= 1 || y >= height - 2) {
        continue;
      }
      const int dx = x - cx;
      const int dy = y - cy;
      if (dx * dx + dy * dy <= radius * radius) {
        grid[y * width + x].type = Tile::Type::FLOOR;
      }
    }
  }
}

void SpecialBiomeStrategy::GenerateHiveNest(std::vector<Tile> &grid, int width,
                                            int height, uint32_t seed) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> centerX(5, width - 6);
  std::uniform_int_distribution<int> centerY(5, height - 6);
  std::uniform_int_distribution<int> radiusDist(2, 4);

  const int chamberCount = std::max(8, (width * height) / 1800);
  for (int i = 0; i < chamberCount; ++i) {
    const int cx = centerX(gen);
    const int cy = centerY(gen);
    const int radius = radiusDist(gen);

    for (int y = cy - radius; y <= cy + radius; ++y) {
      for (int x = cx - radius; x <= cx + radius; ++x) {
        if (x <= 1 || x >= width - 2 || y <= 1 || y >= height - 2) {
          continue;
        }
        const int dx = x - cx;
        const int dy = y - cy;
        if (dx * dx + dy * dy <= radius * radius) {
          grid[y * width + x].type = Tile::Type::FLOOR;
        }
      }
    }
  }
}

} // namespace NoMoreDay
