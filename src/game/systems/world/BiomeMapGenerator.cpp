#include "game/systems/world/BiomeMapGenerator.hpp"
#include "game/foundation/data/BiomeTypes.hpp"
#include <algorithm>
#include <queue>
#include <random>

namespace NoMoreDay {

MapGenerator::MapData BiomeMapGenerator::Generate(int width, int height,
                                                  uint32_t seed, float wallProb,
                                                  int iterations) {
  BiomeConfig fallback;
  fallback.id = "generated_fallback";
  fallback.name = "Generated Fallback";
  fallback.numericId = BiomeID::None;
  fallback.style = BiomeStyle::Open;
  fallback.wallProbability = wallProb;
  fallback.smoothIterations = iterations;
  fallback.wallBirthLimit = 4;
  fallback.wallDeathLimit = 3;

  return GenerateForBiome(width, height, fallback, seed);
}

MapGenerator::MapData
BiomeMapGenerator::GenerateForBiome(int width, int height,
                                    const BiomeConfig &config, uint32_t seed) {
  MapData map{width, height};
  map.grid.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

  auto strategy = CreateStrategy(config.style);
  BiomeGenerationParams params{width, height, &config, seed};
  strategy->GenerateTerrain(map.grid, params);
  strategy->PlaceSpecialStructures(map.grid, params);

  EnsureConnectivity(map.grid, width, height);
  PlaceExits(map.grid, width, height, seed ^ 0x9E3779B9u);
  ApplyFeatures(map.grid, config);

  return map;
}

std::unique_ptr<IBiomeStrategy>
BiomeMapGenerator::CreateStrategy(BiomeStyle style) const {
  switch (style) {
  case BiomeStyle::Town:
  case BiomeStyle::Open:
    return std::make_unique<OpenBiomeStrategy>();
  case BiomeStyle::Maze:
    return std::make_unique<MazeBiomeStrategy>();
  case BiomeStyle::Special:
    return std::make_unique<SpecialBiomeStrategy>();
  default:
    return std::make_unique<OpenBiomeStrategy>();
  }
}

void BiomeMapGenerator::ApplyFeatures(std::vector<Tile> &grid,
                                      const BiomeConfig &config) const {
  const bool enableAirWall = config.hasFeature(BiomeFeature::AirWall);
  for (Tile &tile : grid) {
    tile.isAirWall = enableAirWall && tile.type == Tile::Type::WALL;
  }
}

void BiomeMapGenerator::EnsureConnectivity(std::vector<Tile> &grid, int width,
                                           int height) const {
  std::vector<bool> visited(static_cast<size_t>(width) * static_cast<size_t>(height),
                            false);
  std::vector<int> bestRegion;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int idx = y * width + x;
      if (grid[idx].type == Tile::Type::WALL || visited[idx]) {
        continue;
      }

      std::vector<int> region;
      std::queue<int> queue;
      queue.push(idx);
      visited[idx] = true;

      while (!queue.empty()) {
        const int curr = queue.front();
        queue.pop();
        region.push_back(curr);

        const int cx = curr % width;
        const int cy = curr / width;
        const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto &dir : dirs) {
          const int nx = cx + dir[0];
          const int ny = cy + dir[1];
          if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
            continue;
          }
          const int nIdx = ny * width + nx;
          if (visited[nIdx] || grid[nIdx].type == Tile::Type::WALL) {
            continue;
          }
          visited[nIdx] = true;
          queue.push(nIdx);
        }
      }

      if (region.size() > bestRegion.size()) {
        bestRegion = std::move(region);
      }
    }
  }

  if (bestRegion.empty()) {
    const int cx = width / 2;
    const int cy = height / 2;
    grid[cy * width + cx].type = Tile::Type::FLOOR;
    return;
  }

  std::vector<bool> keep(grid.size(), false);
  for (int idx : bestRegion) {
    keep[idx] = true;
  }
  for (size_t i = 0; i < grid.size(); ++i) {
    if (!keep[i]) {
      grid[i].type = Tile::Type::WALL;
    }
  }
}

void BiomeMapGenerator::PlaceExits(std::vector<Tile> &grid, int width, int height,
                                   uint32_t seed) const {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> xDist(1, width - 2);
  std::uniform_int_distribution<int> yDist(1, height - 2);

  bool startPlaced = false;
  const int cx = width / 2;
  const int cy = height / 2;
  for (int radius = 0; radius < 40 && !startPlaced; ++radius) {
    for (int dx = -radius; dx <= radius && !startPlaced; ++dx) {
      for (int dy = -radius; dy <= radius && !startPlaced; ++dy) {
        const int x = cx + dx;
        const int y = cy + dy;
        if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1) {
          continue;
        }
        if (grid[y * width + x].type == Tile::Type::FLOOR) {
          grid[y * width + x].type = Tile::Type::STAIRS_UP;
          startPlaced = true;
        }
      }
    }
  }

  for (int attempt = 0; attempt < 1200; ++attempt) {
    const int x = xDist(gen);
    const int y = yDist(gen);
    if (grid[y * width + x].type == Tile::Type::FLOOR) {
      grid[y * width + x].type = Tile::Type::STAIRS_DOWN;
      break;
    }
  }
}

} // namespace NoMoreDay
