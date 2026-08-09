#include "doctest.h"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/systems/world/EnemySpawnSystem.hpp"
#include "game/systems/world/MapSystem.hpp"
#include "game/systems/world/WorldConstants.hpp"
#include <array>
#include <filesystem>
#include <stdexcept>

namespace {
std::filesystem::path ResolveBiomeJsonPathForIntegrationTest() {
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

size_t CountEnemies(entt::registry &registry) {
  size_t count = 0;
  auto view = registry.view<EnemyTag>();
  for (const auto entity : view) {
    (void)entity;
    ++count;
  }
  return count;
}

bool HasTileType(const MapSystem::MapData &mapData, Tile::Type type) {
  for (const Tile &tile : mapData.grid) {
    if (tile.type == type) {
      return true;
    }
  }
  return false;
}
} // namespace

TEST_CASE("[Integration] Biome Traversal - Map Generation And Spawn Pipeline") {
  using namespace NoMoreDay;
  BiomeRegistry::Get().LoadFromJSON(
      ResolveBiomeJsonPathForIntegrationTest().string());

  constexpr std::array<BiomeID, 29> kAllBiomes = {
      BiomeID::Town,               BiomeID::Town_SwordImmortal,
      BiomeID::Town_Mage,          BiomeID::Town_Mech,
      BiomeID::Town_Shadow,        BiomeID::Town_Beast,
      BiomeID::Town_Radiant,       BiomeID::Cave,
      BiomeID::SunPrairie,         BiomeID::IceTundra,
      BiomeID::CrimsonWaste,       BiomeID::DustSea,
      BiomeID::VoidFlats,          BiomeID::EmeraldWet,
      BiomeID::AshPlain,           BiomeID::GloomSpire,
      BiomeID::MagmaVeins,         BiomeID::JadeMine,
      BiomeID::DrownedLib,         BiomeID::ClockCore,
      BiomeID::AncientCrypt,       BiomeID::CrystalLab,
      BiomeID::FloatingIsle,       BiomeID::CoralRuin,
      BiomeID::WhisperWood,        BiomeID::HolyArena,
      BiomeID::HiveNest,           BiomeID::SkyPalace,
      BiomeID::AbyssalGap,
  };

  for (const BiomeID biomeId : kAllBiomes) {
    const auto &config = BiomeRegistry::Get().GetBiome(biomeId);
    CAPTURE(config.id);

    MapSystem mapSystem;
    mapSystem.generateMap(96, 96, config.id);
    const auto &mapData = mapSystem.getMapData();

    CHECK(mapData.width == 96);
    CHECK(mapData.height == 96);
    CHECK(mapData.grid.size() == static_cast<size_t>(96 * 96));
    CHECK(HasTileType(mapData, Tile::Type::STAIRS_UP));
    CHECK(HasTileType(mapData, Tile::Type::STAIRS_DOWN));

    EnemySpawnSystem spawnSystem;
    entt::registry registry;
    spawnSystem.initData(96, 96, 1, mapSystem, biomeId);

    const Position center = {
        96.0f * NoMoreDay::Constants::World::GRID_TILE_SIZE * 0.5f,
        96.0f * NoMoreDay::Constants::World::GRID_TILE_SIZE * 0.5f,
    };
    spawnSystem.updateEnemySpawning(center, registry, 0.016f, &mapSystem);

    const size_t enemyCount = CountEnemies(registry);
    if (config.isSafeZone) {
      CHECK(enemyCount == 0);
    } else {
      CHECK(enemyCount > 0);
    }
  }
}
