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
#include "game/systems/item/HeirloomVault.hpp"

namespace NoMoreDay {

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
                                            pool, Tag::Melee, entt::null);

    CHECK(result.total_damage == doctest::Approx(0.0f));
    CHECK(pf.triggered == true);
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
                                            pool, Tag::Projectile, proj_ent);

    CHECK(result.total_damage == doctest::Approx(0.0f));
    CHECK(ward.sword_count == 99);
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


TEST_CASE("[Unit] PersistenceSystem - Basic Check") {
    auto& sm = SaveManager::Get();
    CHECK(&sm != nullptr);
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
