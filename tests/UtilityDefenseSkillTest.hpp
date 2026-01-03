#pragma once
#include "TestCommon.hpp"
#include "../src/systems/SkillSystem.hpp"
#include "../src/systems/ProjectileSystem.hpp"
#include "../src/systems/CombatSystem.hpp"
#include "../src/systems/PhysicsSystem.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Projectile.hpp"
#include "../src/components/EnemyComponent.hpp"
#include "../src/components/AIComponent.hpp"

TEST_CASE("SkillSystem: Blade Ward Interception") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    systems::SpatialHashGrid grid(1000, 1000, 50); // Large enough for 500,500

    auto player = registry.create();
    registry.emplace<Position>(player, 500.0f, 500.0f);
    registry.emplace<HealthComponent>(player, 100.0f, 100.0f);
    auto& ward = registry.emplace<BladeWardComponent>(player);
    ward.interception_chance = 1.0f; // 100%
    ward.sword_count = 1;
    registry.emplace<Velocity>(player, 0.0f, 0.0f); 

    auto proj_ent = registry.create();
    registry.emplace<Position>(proj_ent, 500.0f, 500.0f);
    registry.emplace<Velocity>(proj_ent, 100.0f, 0.0f);
    auto& proj = registry.emplace<Projectile>(proj_ent);
    proj.radius = 10.0f;
    proj.owner = registry.create(); // Monster

    // Add everything to Grid
    auto gridView = registry.view<Position, Velocity>();
    grid.rebuild(gridView, registry);

    // Update Projectile System
    ProjectileSystem::Update(registry, grid, 0.1f);
    ProjectileSystem::Update(registry, grid, 0.1f); 

    // Projectile should be destroyed (hit=true due to interception)
    CHECK_FALSE(registry.valid(proj_ent));
}

TEST_CASE("SkillSystem: Blade Boomerang Pull") {
    LoggerScope scope;
    entt::registry registry;
    systems::SpatialHashGrid grid(1000, 1000, 50);

    auto proj_ent = registry.create();
    registry.emplace<Position>(proj_ent, 500.0f, 500.0f);
    registry.emplace<Velocity>(proj_ent, 0.0f, 0.0f);
    auto& proj = registry.emplace<Projectile>(proj_ent);
    proj.hasPull = true;
    proj.pullStrength = 1000.0f;
    proj.radius = 10.0f;

    auto enemy = registry.create();
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<Position>(enemy, 520.0f, 500.0f);
    auto& vel = registry.emplace<Velocity>(enemy, 0.0f, 0.0f);

    // Manual Rebuild Grid
    auto gridView = registry.view<Position, Velocity>();
    grid.rebuild(gridView, registry);

    // Run Pull Logic
    PhysicsSystem::ProjectilePullLogic(registry, grid, 0.1f);

    // Enemy should have velocity towards projectile (negative vx)
    CHECK(vel.vx < -10.0f);
}

TEST_CASE("SkillSystem: Phantom Flash Riposte") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

    auto player = registry.create();
    registry.emplace<Position>(player, 500.0f, 500.0f);
    registry.emplace<HealthComponent>(player, 100.0f, 100.0f);
    registry.emplace<PhantomFlashComponent>(player);

    auto attacker = registry.create();
    registry.emplace<Position>(attacker, 600.0f, 500.0f);

    // Apply Damage
    bool dead = CombatSystem::ApplyDamage(registry, player, 10.0f, attacker);

    CHECK_FALSE(dead);
    auto& hp = registry.get<HealthComponent>(player);
    CHECK(hp.current == 100.0f); // Blocked

    // Check if Shadow Echo (Riposte) was created
    auto shadow_view = registry.view<ShadowLifetime>();
    CHECK(!shadow_view.empty());
}
