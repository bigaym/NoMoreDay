#include "TestCommon.hpp"
#include "game/systems/combat/HazardSystem.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/components/HazardComponents.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/physics/SpatialGrid.hpp"

using namespace NoMoreDay;

TEST_CASE("[Unit] HazardSystem - Duration and Cleanup") {
    entt::registry registry;
    systems::SpatialHashGrid grid(100, 100, 50.0f);
    
    auto hazardEntity = registry.create();
    registry.emplace<Position>(hazardEntity, 100.0f, 100.0f);
    registry.emplace<Radius>(hazardEntity, 50.0f);
    
    auto& hazard = registry.emplace<HazardComponent>(hazardEntity);
    hazard.duration = 1.0f;
    hazard.damagePerTick = 10.0f;
    hazard.tickInterval = 0.5f;
    
    // Update with 0.6s
    HazardSystem::Update(registry, 0.6f, grid);
    
    CHECK(registry.valid(hazardEntity));
    CHECK(hazard.duration == doctest::Approx(0.4f));
    
    // Update with 0.5s (total 1.1s)
    HazardSystem::Update(registry, 0.5f, grid);
    
    // Entity should be destroyed
    CHECK(!registry.valid(hazardEntity));
}

TEST_CASE("[Unit] HazardSystem - Damage Application") {
    entt::registry registry;
    systems::SpatialHashGrid grid(100, 100, 50.0f);
    
    // Create player
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 100.0f, 100.0f);
    auto& playerHP = registry.emplace<HealthComponent>(player, 100.0f, 100.0f);
    registry.emplace<CombatStats>(player);
    
    // Rebuild grid to include player
    grid.rebuild(registry.view<Position>(), registry);
    
    // Create hazard
    auto hazardEntity = registry.create();
    registry.emplace<Position>(hazardEntity, 105.0f, 105.0f); // Close to player
    registry.emplace<Radius>(hazardEntity, 20.0f);
    registry.emplace<CombatStats>(hazardEntity); // 给伤害源也加个 stats 避免潜在问题
    
    auto& hazard = registry.emplace<HazardComponent>(hazardEntity);
    hazard.owner = hazardEntity; // 设为自身作为 owner 进行测试
    hazard.duration = 5.0f;
    hazard.damagePerTick = 10.0f;
    hazard.tickInterval = 0.5f;
    hazard.hitsPlayers = true;
    hazard.hitsEnemies = false;
    hazard.damageType = DamageType::Fire;
    
    // Update to trigger a tick
    HazardSystem::Update(registry, 0.6f, grid);
    
    // Player should have taken damage
    CHECK(playerHP.current < 100.0f);
}

TEST_CASE("[Unit] HazardSystem - Delayed Explosion") {
    entt::registry registry;
    systems::SpatialHashGrid grid(100, 100, 50.0f);
    
    // Create player
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 100.0f, 100.0f);
    auto& playerHP = registry.emplace<HealthComponent>(player, 100.0f, 100.0f);
    registry.emplace<CombatStats>(player);
    
    // Rebuild grid to include player
    grid.rebuild(registry.view<Position>(), registry);
    
    // Create delayed explosion hazard
    auto hazardEntity = registry.create();
    registry.emplace<Position>(hazardEntity, 100.0f, 100.0f);
    registry.emplace<Radius>(hazardEntity, 50.0f);
    registry.emplace<CombatStats>(hazardEntity);
    
    auto& hazard = registry.emplace<HazardComponent>(hazardEntity);
    hazard.owner = hazardEntity;
    hazard.duration = 1.0f;
    hazard.isDelayedExplosion = true;
    hazard.explosionDamage = 50.0f;
    hazard.hitsPlayers = true;
    hazard.damageType = DamageType::Fire;
    
    // Update before duration ends
    HazardSystem::Update(registry, 0.5f, grid);
    CHECK(playerHP.current == 100.0f); // No damage yet
    
    // Update to end duration
    HazardSystem::Update(registry, 0.6f, grid);
    
    // Player should have taken explosion damage
    CHECK(playerHP.current < 100.0f);
    CHECK(!registry.valid(hazardEntity));
}
