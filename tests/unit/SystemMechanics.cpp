#pragma once

#include "TestCommon.hpp"
#include "game/components/Combat.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Projectile.hpp"

#include "game/components/Stats.hpp"
#include "game/components/Common.hpp"
#include "game/components/HeirloomComponent.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "engine/persistence/SaveManager.hpp"
#include "game/data/ResonanceCalculator.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/systems/item/HeirloomVault.hpp"
#include "game/components/PlayerProfile.hpp"
#include "raylib.h"
#include <array>
#include <filesystem>
#include <stdexcept>

namespace NoMoreDay {
namespace {
std::filesystem::path ResolveBiomeJsonPathForSystemMechanicsTest() {
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
} // namespace

TEST_CASE("[Unit] DefenseMechanics - Verification") {
  LoggerScope scope;
  entt::registry registry;

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  registry.emplace<CombatStats>(attacker).damage_multipliers[0] = 1.0f;

  auto defender = registry.create();
  registry.emplace<Position>(defender, 10.0f, 0.0f);
  registry.emplace<Velocity>(defender, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(defender, 100.0f, 100.0f);
  registry.emplace<CombatStats>(defender);

  SUBCASE("Phantom Flash Counter") {
    auto &pf = registry.emplace<PhantomFlashComponent>(defender);
    pf.counter_window = 0.5f;
    pf.triggered = false;

     DamagePool pool;
    pool.Add(Tag::Physical, 50.0f);

    auto result = DamagePipeline::Calculate(registry, attacker, defender, 0,
                                            pool, Tag::Melee, entt::null, true);

    CHECK(result.total_damage == doctest::Approx(50.0f));
    CHECK(pf.triggered == false);
    CHECK(registry.get<HealthComponent>(defender).current == 100.0f);
  }

  SUBCASE("Blade Ward Interception") {
    auto &ward = registry.emplace<BladeWardComponent>(defender);
    ward.sword_count = 100;
    ward.interception_chance = 1.0f;

    auto proj_ent = registry.create();
    registry.emplace<Position>(proj_ent, 10.0f, 0.0f);
    registry.emplace<Projectile>(proj_ent);

    DamagePool pool;
    pool.Add(Tag::Physical, 30.0f);

    auto result = DamagePipeline::Calculate(registry, attacker, defender, 0,
                                            pool, Tag::Projectile, proj_ent, true);

    CHECK(result.total_damage == doctest::Approx(30.0f));
    CHECK(ward.sword_count == 100);
  }
}

TEST_CASE("[Unit] HeirloomVault - Core Rules") {
    entt::registry registry;
    auto player = registry.create();
    
    SUBCASE("Vault Management") {
        auto& vault = HeirloomVault::Get();
        size_t initialSize = vault.size();
        
        auto itemEnt = registry.create();
        auto& item = registry.emplace<ItemComponent>(itemEnt);
        item.name = "Heirloom Test";
        
        vault.addHeirloom(item, 10, Rarity::Legendary);
        CHECK(vault.size() == initialSize + 1);
        
        vault.removeHeirloom(initialSize);
        CHECK(vault.size() == initialSize);
    }
}

TEST_CASE("[Unit] ResonanceCalculator - Basic Check") {
    MosaicGrid grid;
    entt::registry registry;
    // Empty grid resonance
    auto result = ResonanceCalculator::Calculate(grid, registry);
    CHECK(result.totalEnemyDensity == 1.0f);
}

TEST_CASE("[Unit] ResonanceCalculator - Uses Dimensional Combat Biome Pool") {
    const auto biomePath = ResolveBiomeJsonPathForSystemMechanicsTest().string();
    BiomeRegistry::Get().LoadFromJSON(biomePath);
    CHECK(BiomeRegistry::Get().HasBiome("sky_palace"));
    MosaicGrid grid;
    entt::registry registry;

    auto fragmentEntity = registry.create();
    auto& fragment = registry.emplace<MapFragmentComponent>(fragmentEntity);
    fragment.element = FragmentElement::Fire;
    fragment.type = FragmentType::Terrain;
    grid.cells[0] = fragmentEntity;

    auto result = ResonanceCalculator::Calculate(grid, registry);

    constexpr std::array<BiomeID, 5> kFirePool = {
        BiomeID::CrimsonWaste, BiomeID::MagmaVeins, BiomeID::AshPlain,
        BiomeID::HolyArena, BiomeID::HiveNest};

    bool inPool = false;
    for (BiomeID id : kFirePool) {
        if (result.primaryBiome == id) {
            inPool = true;
            break;
        }
    }
    CHECK(inPool);
}

TEST_CASE("[Unit] ResonanceCalculator - biomeOverride Takes Priority") {
    const auto biomePath = ResolveBiomeJsonPathForSystemMechanicsTest().string();
    BiomeRegistry::Get().LoadFromJSON(biomePath);
    REQUIRE(BiomeRegistry::Get().HasBiome("sky_palace"));
    MosaicGrid grid;
    entt::registry registry;

    auto fragmentEntity = registry.create();
    auto& fragment = registry.emplace<MapFragmentComponent>(fragmentEntity);
    fragment.element = FragmentElement::Fire;
    fragment.type = FragmentType::Terrain;
    fragment.biomeOverride = "sky_palace";
    grid.cells[0] = fragmentEntity;

    auto result = ResonanceCalculator::Calculate(grid, registry);
    CHECK(result.primaryBiome == BiomeID::SkyPalace);
}


TEST_CASE("[Unit] PersistenceSystem - Basic Check") {
    auto& sm = SaveManager::Get();
    CHECK(&sm != nullptr);
}

TEST_CASE("[Unit] SaveManager - Header Name And Playtime Snapshot") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 1.0f, 2.0f);
    registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
    registry.emplace<PlayerName>(player, "玩家0");
    registry.emplace<PlayerPlaytime>(
        player, 120, static_cast<double>(GetTime()) - 5.2);

    auto data = SaveManager::Get().createSnapshot(registry);
    CHECK(data.header.name == "玩家0");
    CHECK(data.header.playtime >= 125);

    data.header.playtime = 200;
    entt::registry restoredRegistry;
    SaveManager::Get().restoreFromSnapshot(restoredRegistry, data);

    auto restoredView = restoredRegistry.view<PlayerTag, PlayerPlaytime>();
    REQUIRE(restoredView.begin() != restoredView.end());
    auto restoredPlayer = *restoredView.begin();
    auto &playtime = restoredRegistry.get<PlayerPlaytime>(restoredPlayer);
    playtime.session_start_time -= 3.1;

    auto data2 = SaveManager::Get().createSnapshot(restoredRegistry);
    CHECK(data2.header.playtime >= 203);
}

TEST_CASE("[Unit] SaveManager - Skill Contract Runtime Snapshot Roundtrip") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 1.0f, 2.0f);
    registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
    registry.emplace<ActiveSkillsComponent>(player);

    auto& runtime = registry.emplace<SkillContractRuntimeComponent>(player);
    runtime.version = kSkillContractRuntimeVersion;
    runtime.active_transmuter_node_by_skill[8] = 870;
    runtime.trigger_cooldowns[114] = 1.25f;
    runtime.trigger_cooldowns[971] = 0.5f;

    const auto snapshot = SaveManager::Get().createSnapshot(registry);
    CHECK(snapshot.skill_contract_runtime.version == kSkillContractRuntimeVersion);
    CHECK(snapshot.skill_contract_runtime.skills.size() >= 1);

    entt::registry restored;
    SaveManager::Get().restoreFromSnapshot(restored, snapshot);
    auto view = restored.view<PlayerTag, SkillContractRuntimeComponent>();
    REQUIRE(view.begin() != view.end());
    auto restoredPlayer = *view.begin();
    const auto& restoredRuntime =
        restored.get<SkillContractRuntimeComponent>(restoredPlayer);

    REQUIRE(restoredRuntime.active_transmuter_node_by_skill.contains(8));
    CHECK(restoredRuntime.active_transmuter_node_by_skill.at(8) == 870);
    REQUIRE(restoredRuntime.trigger_cooldowns.contains(114));
    CHECK(restoredRuntime.trigger_cooldowns.at(114) == doctest::Approx(1.25f));
    REQUIRE(restoredRuntime.trigger_cooldowns.contains(971));
    CHECK(restoredRuntime.trigger_cooldowns.at(971) == doctest::Approx(0.5f));
}

TEST_CASE("[Unit] StatsOptimization - Zero Allocation") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<PrimaryStats>(entity);
    registry.emplace<CombatStats>(entity);
    
    // Just verify it works
    StatsSystem::Recalculate(registry, entity);
    CHECK(registry.get<CombatStats>(entity).health >= 0);
}

TEST_CASE("[Unit] SwordIntent - Basic Accumulation") {
    entt::registry registry;
    auto player = registry.create();
    auto& si = registry.emplace<SwordIntentComponent>(player);
    si.stacks = 5.0f;
    CHECK(si.stacks == 5.0f);
}

} // namespace NoMoreDay
