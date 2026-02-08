#include "doctest.h"
#include "game/data/BiomeRegistry.hpp"
#include "game/systems/world/MapSystem.hpp"
#include <array>
#include <filesystem>
#include <stdexcept>

namespace {
std::filesystem::path ResolveBiomeJsonPath() {
  constexpr std::array<const char *, 4> kCandidates = {
      "assets/data/biomes.json",
      "../assets/data/biomes.json",
      "../../assets/data/biomes.json",
      "../../../assets/data/biomes.json",
  };

  for (const char *candidate : kCandidates) {
    const std::filesystem::path path(candidate);
    if (std::filesystem::exists(path)) {
      return std::filesystem::absolute(path);
    }
  }
  throw std::runtime_error("Unable to locate assets/data/biomes.json from test cwd");
}
} // namespace

TEST_CASE("[Unit] MapSystem - AirWall Tile Marking") {
  NoMoreDay::BiomeRegistry::Get().LoadFromJSON(ResolveBiomeJsonPath().string());

  SUBCASE("special biome with air wall marks wall tiles") {
    MapSystem mapSystem;
    mapSystem.generateMap(96, 96, "floating_isle");

    const auto &mapData = mapSystem.getMapData();
    bool hasWall = false;
    bool hasAirWall = false;
    for (const Tile &tile : mapData.grid) {
      if (tile.type == Tile::Type::WALL) {
        hasWall = true;
        if (tile.isAirWall) {
          hasAirWall = true;
          break;
        }
      }
    }

    CHECK(hasWall);
    CHECK(hasAirWall);
  }

  SUBCASE("regular biome does not mark air wall") {
    MapSystem mapSystem;
    mapSystem.generateMap(96, 96, "cave");

    const auto &mapData = mapSystem.getMapData();
    bool anyAirWall = false;
    for (const Tile &tile : mapData.grid) {
      if (tile.isAirWall) {
        anyAirWall = true;
        break;
      }
    }

    CHECK_FALSE(anyAirWall);
  }
}

