#include "game/systems/world/BiomeStrategies.hpp"
#include <algorithm>
#include <random>
#include <vector>

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

void MazeBiomeStrategy::GenerateTerrain(std::vector<Tile> &grid,
                                        const BiomeGenerationParams &params) {
  const int width = params.width;
  const int height = params.height;
  const BiomeConfig &config = *params.config;

  std::mt19937 gen(params.seed);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  const float wallProbability = std::clamp(config.wallProbability, 0.25f, 0.8f);

  for (Tile &tile : grid) {
    tile.type = (dist(gen) < wallProbability) ? Tile::Type::WALL
                                              : Tile::Type::FLOOR;
    tile.isAirWall = false;
    tile.visibility = 0;
  }
  ApplyBoundaries(grid, width, height);

  std::vector<Tile> buffer = grid;
  const int iterations = std::max(3, config.smoothIterations);
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

  CarveCorridors(grid, width, height, params.seed ^ 0xBB67AE85u);
}

void MazeBiomeStrategy::PlaceSpecialStructures(
    std::vector<Tile> &grid, const BiomeGenerationParams &params) {
  ProcessDeadEnds(grid, params.width, params.height, params.seed ^ 0x3C6EF372u);
}

void MazeBiomeStrategy::CarveCorridors(std::vector<Tile> &grid, int width,
                                       int height, uint32_t seed) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> xDist(2, width - 3);
  std::uniform_int_distribution<int> yDist(2, height - 3);
  std::uniform_int_distribution<int> dirDist(0, 3);
  std::uniform_int_distribution<int> widthDist(2, 3);
  std::uniform_int_distribution<int> stepDist(10, 30);

  const int corridorCount = std::max(8, (width * height) / 1800);
  for (int i = 0; i < corridorCount; ++i) {
    int x = (i == 0) ? (width / 2) : xDist(gen);
    int y = (i == 0) ? (height / 2) : yDist(gen);
    int corridorWidth = widthDist(gen);
    int steps = stepDist(gen);

    for (int step = 0; step < steps; ++step) {
      for (int oy = 0; oy < corridorWidth; ++oy) {
        for (int ox = 0; ox < corridorWidth; ++ox) {
          const int tx = std::clamp(x + ox, 1, width - 2);
          const int ty = std::clamp(y + oy, 1, height - 2);
          grid[ty * width + tx].type = Tile::Type::FLOOR;
        }
      }

      switch (dirDist(gen)) {
      case 0:
        ++x;
        break;
      case 1:
        --x;
        break;
      case 2:
        ++y;
        break;
      default:
        --y;
        break;
      }
      x = std::clamp(x, 2, width - 3);
      y = std::clamp(y, 2, height - 3);
    }
  }
}

void MazeBiomeStrategy::ProcessDeadEnds(std::vector<Tile> &grid, int width,
                                        int height, uint32_t seed) {
  std::mt19937 gen(seed);
  std::uniform_real_distribution<float> chance(0.0f, 1.0f);
  std::vector<int> deadEnds;
  deadEnds.reserve((width * height) / 32);

  const int offsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      const int idx = y * width + x;
      if (grid[idx].type != Tile::Type::FLOOR) {
        continue;
      }

      int openNeighbors = 0;
      for (const auto &offset : offsets) {
        const int nx = x + offset[0];
        const int ny = y + offset[1];
        if (grid[ny * width + nx].type != Tile::Type::WALL) {
          ++openNeighbors;
        }
      }
      if (openNeighbors <= 1) {
        deadEnds.push_back(idx);
      }
    }
  }

  for (int idx : deadEnds) {
    if (chance(gen) > 0.35f) {
      continue;
    }
    const int x = idx % width;
    const int y = idx / width;
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        const int nx = std::clamp(x + dx, 1, width - 2);
        const int ny = std::clamp(y + dy, 1, height - 2);
        grid[ny * width + nx].type = Tile::Type::FLOOR;
      }
    }
  }
}

} // namespace NoMoreDay
