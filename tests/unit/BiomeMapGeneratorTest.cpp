#include "doctest.h"
#include "game/data/BiomeRegistry.hpp"
#include "game/systems/world/BiomeMapGenerator.hpp"
#include <array>
#include <filesystem>
#include <queue>
#include <stdexcept>

namespace {
std::filesystem::path ResolveBiomeJsonPathForGeneratorTest() {
  constexpr std::array<const char *, 4> kCandidates = {
      "assets/data/biomes.json",
      "../assets/data/biomes.json",
      "../../assets/data/biomes.json",
      "../../../assets/data/biomes.json",
  };

  for (const char *candidate : kCandidates) {
    const auto path = std::filesystem::path(candidate);
    if (std::filesystem::exists(path)) {
      return std::filesystem::absolute(path);
    }
  }
  throw std::runtime_error("Unable to locate assets/data/biomes.json from test cwd");
}

float ComputeWalkableRatio(const MapGenerator::MapData &map) {
  size_t walkableCount = 0;
  for (const Tile &tile : map.grid) {
    if (tile.isWalkable()) {
      ++walkableCount;
    }
  }
  return static_cast<float>(walkableCount) / static_cast<float>(map.grid.size());
}

float ComputeWallRatio(const MapGenerator::MapData &map) {
  size_t wallCount = 0;
  for (const Tile &tile : map.grid) {
    if (tile.type == Tile::Type::WALL) {
      ++wallCount;
    }
  }
  return static_cast<float>(wallCount) / static_cast<float>(map.grid.size());
}

bool HasConnectedExits(const MapGenerator::MapData &map) {
  int start = -1;
  int goal = -1;
  for (int y = 0; y < map.height; ++y) {
    for (int x = 0; x < map.width; ++x) {
      const int idx = y * map.width + x;
      if (map.grid[idx].type == Tile::Type::STAIRS_UP) {
        start = idx;
      } else if (map.grid[idx].type == Tile::Type::STAIRS_DOWN) {
        goal = idx;
      }
    }
  }
  if (start < 0 || goal < 0) {
    return false;
  }

  std::vector<bool> visited(map.grid.size(), false);
  std::queue<int> queue;
  queue.push(start);
  visited[start] = true;

  const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  while (!queue.empty()) {
    const int curr = queue.front();
    queue.pop();
    if (curr == goal) {
      return true;
    }

    const int cx = curr % map.width;
    const int cy = curr / map.width;
    for (const auto &dir : dirs) {
      const int nx = cx + dir[0];
      const int ny = cy + dir[1];
      if (nx < 0 || nx >= map.width || ny < 0 || ny >= map.height) {
        continue;
      }
      const int nIdx = ny * map.width + nx;
      if (visited[nIdx] || !map.grid[nIdx].isWalkable()) {
        continue;
      }
      visited[nIdx] = true;
      queue.push(nIdx);
    }
  }
  return false;
}

bool IsWalkableRegionConnected(const MapGenerator::MapData &map) {
  int start = -1;
  int walkableCount = 0;
  for (int y = 0; y < map.height; ++y) {
    for (int x = 0; x < map.width; ++x) {
      const int idx = y * map.width + x;
      if (!map.grid[idx].isWalkable()) {
        continue;
      }
      ++walkableCount;
      if (start < 0) {
        start = idx;
      }
    }
  }
  if (start < 0) {
    return true;
  }

  std::vector<bool> visited(map.grid.size(), false);
  std::queue<int> queue;
  queue.push(start);
  visited[start] = true;
  int reached = 0;

  const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  while (!queue.empty()) {
    const int curr = queue.front();
    queue.pop();
    ++reached;

    const int cx = curr % map.width;
    const int cy = curr / map.width;
    for (const auto &dir : dirs) {
      const int nx = cx + dir[0];
      const int ny = cy + dir[1];
      if (nx < 0 || nx >= map.width || ny < 0 || ny >= map.height) {
        continue;
      }
      const int nIdx = ny * map.width + nx;
      if (visited[nIdx] || !map.grid[nIdx].isWalkable()) {
        continue;
      }
      visited[nIdx] = true;
      queue.push(nIdx);
    }
  }

  return reached == walkableCount;
}
} // namespace

TEST_CASE("[Unit] BiomeMapGenerator - Open Maze Special Profiles") {
  using namespace NoMoreDay;
  BiomeRegistry::Get().LoadFromJSON(ResolveBiomeJsonPathForGeneratorTest().string());
  BiomeMapGenerator generator;

  SUBCASE("open biome keeps high walkable ratio and connected exits") {
    const auto &config = BiomeRegistry::Get().GetBiome("sun_prairie");
    auto map = generator.GenerateForBiome(96, 96, config, 1337u);
    const float wallRatio = ComputeWallRatio(map);

    CHECK(config.style == BiomeStyle::Open);
    CHECK(wallRatio >= 0.15f);
    CHECK(wallRatio <= 0.22f);
    INFO("Open biome wall ratio: " << wallRatio);
    CHECK(ComputeWalkableRatio(map) > 0.70f);
    CHECK(IsWalkableRegionConnected(map));
    CHECK(HasConnectedExits(map));
  }

  SUBCASE("maze biome keeps strong wall ratio and connected exits") {
    const auto &config = BiomeRegistry::Get().GetBiome("gloom_spire");
    auto map = generator.GenerateForBiome(96, 96, config, 7331u);

    CHECK(config.style == BiomeStyle::Maze);
    CHECK(ComputeWallRatio(map) > 0.35f);
    CHECK(IsWalkableRegionConnected(map));
    CHECK(HasConnectedExits(map));
  }

  SUBCASE("special floating biome marks air walls and keeps exits connected") {
    const auto &config = BiomeRegistry::Get().GetBiome("floating_isle");
    auto map = generator.GenerateForBiome(96, 96, config, 2026u);

    bool hasAirWall = false;
    for (const Tile &tile : map.grid) {
      if (tile.isAirWall) {
        hasAirWall = true;
        break;
      }
    }

    CHECK(config.style == BiomeStyle::Special);
    CHECK(hasAirWall);
    CHECK(IsWalkableRegionConnected(map));
    CHECK(HasConnectedExits(map));
  }
}

TEST_CASE("[Unit] BiomeMapGenerator - Connectivity For All Combat Biomes") {
  using namespace NoMoreDay;
  BiomeRegistry::Get().LoadFromJSON(ResolveBiomeJsonPathForGeneratorTest().string());
  BiomeMapGenerator generator;

  constexpr std::array<BiomeID, 22> kCombatBiomes = {
      BiomeID::Cave,         BiomeID::SunPrairie,  BiomeID::IceTundra,
      BiomeID::CrimsonWaste, BiomeID::DustSea,     BiomeID::VoidFlats,
      BiomeID::EmeraldWet,   BiomeID::AshPlain,    BiomeID::GloomSpire,
      BiomeID::MagmaVeins,   BiomeID::JadeMine,    BiomeID::DrownedLib,
      BiomeID::ClockCore,    BiomeID::AncientCrypt, BiomeID::CrystalLab,
      BiomeID::FloatingIsle, BiomeID::CoralRuin,   BiomeID::WhisperWood,
      BiomeID::HolyArena,    BiomeID::HiveNest,    BiomeID::SkyPalace,
      BiomeID::AbyssalGap,
  };

  uint32_t seed = 9001u;
  for (BiomeID biomeId : kCombatBiomes) {
    const auto &config = BiomeRegistry::Get().GetBiome(biomeId);
    CAPTURE(config.id);
    auto map = generator.GenerateForBiome(96, 96, config, seed++);

    CHECK(IsWalkableRegionConnected(map));
    CHECK(HasConnectedExits(map));
  }
}
