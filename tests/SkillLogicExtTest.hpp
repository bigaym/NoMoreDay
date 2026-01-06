#pragma once
#include "TestCommon.hpp"
#include "../src/systems/SkillSystem.hpp"
#include "../src/systems/ProjectileSystem.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Projectile.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/AIComponent.hpp"
#include "../src/core/SkillRegistry.hpp"

using namespace NoMoreDay;

TEST_CASE("Skill Logic Extension: Boomerang") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1000, 1000, 50);
    auto owner = registry.create();
    registry.emplace<Position>(owner, 0.0f, 0.0f);
    
    auto proj_ent = registry.create();
    registry.emplace<Position>(proj_ent, 100.0f, 0.0f); 
    auto& vel = registry.emplace<Velocity>(proj_ent, 100.0f, 0.0f); 
    auto& proj = registry.emplace<Projectile>(proj_ent);
    proj.owner = owner;
    proj.speed = 100.0f;

    auto& bc = registry.emplace<BoomerangComponent>(proj_ent);
    bc.owner = owner;
    bc.phase = BoomerangComponent::Outward;
    bc.returnTimer = 0.5f;

    // 1. Update during Outward phase
    ProjectileSystem::Update(registry, grid, 0.1f);
    CHECK(bc.phase == BoomerangComponent::Outward);

    // 2. Trigger return phase
    ProjectileSystem::Update(registry, grid, 0.5f);
    CHECK(bc.phase == BoomerangComponent::Returning);

    // 3. One more update to apply returning velocity
    ProjectileSystem::Update(registry, grid, 0.1f);
    CHECK(vel.vx < 0.0f); 
}

TEST_CASE("Skill Logic Extension: Pull Mechanics") {
    entt::registry registry;
    systems::SpatialHashGrid grid(1000, 1000, 50);
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    
    auto projEnt = registry.create();
    registry.emplace<Position>(projEnt, 0.0f, 0.0f);
    registry.emplace<Velocity>(projEnt, 0.0f, 0.0f);
    auto& proj = registry.emplace<Projectile>(projEnt);
    proj.owner = player;
    proj.hasPull = true;
    proj.pullStrength = 1000.0f;
    proj.radius = 50.0f;

    auto enemy = registry.create();
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<Position>(enemy, 100.0f, 0.0f); 
    auto& eVel = registry.emplace<Velocity>(enemy, 0.0f, 0.0f);

    auto view = registry.view<Position>();
    grid.rebuild(view, registry);

    ProjectileSystem::Update(registry, grid, 0.1f);

    CHECK(eVel.vx < 0.0f);
}

TEST_CASE("Skill Logic Extension: Channeling") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    systems::SpatialHashGrid grid(100, 100, 50);
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player);
    
    auto& chan = registry.emplace<ChannelingComponent>(player);
    chan.skill_id = 5; 
    chan.channel_timer = 1.0f;
    chan.tick_interval = 0.1f;
    chan.tick_timer = 0.1f;

    SkillSystem::Update(registry, grid, 0.15f);
    
    auto exec_view = registry.view<SkillExecution>();
    CHECK(!exec_view.empty());
}

TEST_CASE("Skill Logic Extension: Phantom Flash Riposte") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    auto player = registry.create();
    auto attacker = registry.create();
    
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<Velocity>(player, 0.0f, 0.0f);
    registry.emplace<HealthComponent>(player, 100.0f, 100.0f);
    registry.emplace<CombatStats>(player);
    
    registry.emplace<Position>(attacker, 50.0f, 0.0f); 
    
    auto& pf = registry.emplace<PhantomFlashComponent>(player);
    pf.counter_window = 1.0f;
    pf.triggered = false;

    CombatSystem::ApplyDamage(registry, player, 10.0f, attacker);

    CHECK(pf.triggered == true);
    auto& pPos = registry.get<Position>(player);
    CHECK(pPos.x == doctest::Approx(30.0f));
    
    auto exec_view = registry.view<SkillExecution>();
    CHECK(!exec_view.empty());
    
    auto& hp = registry.get<HealthComponent>(player);
    CHECK(hp.current == 100.0f);
}

TEST_CASE("Skill Logic Extension: Blade Formation Targeting") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    systems::SpatialHashGrid grid(1000, 1000, 50);
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player);
    
    auto& formation = registry.emplace<BladeFormationComponent>(player);
    formation.max_swords = 1;
    formation.current_swords = 1;
    formation.search_radius = 200.0f;
    formation.attack_interval = 0.5f;
    formation.attack_timer = 0.0f;

    auto enemy = registry.create();
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<Position>(enemy, 100.0f, 0.0f);
    registry.emplace<HealthComponent>(enemy, 100.0f, 100.0f);
    
    auto view = registry.view<Position>();
    grid.rebuild(view, registry);

    SkillSystem::Update(registry, grid, 0.1f);

    auto exec_view = registry.view<SkillExecution>();
    CHECK(!exec_view.empty());
}