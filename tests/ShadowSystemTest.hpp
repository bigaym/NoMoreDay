#pragma once
#include "doctest.h"
#include "entt/entt.hpp"
#include "../src/systems/ShadowSystem.hpp"
#include "../src/systems/SkillSystem.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/Stats.hpp"
#include "../src/core/SkillRegistry.hpp"

using namespace NoMoreDay;

TEST_CASE("Shadow System: Basic Lifecycle") {
    entt::registry registry;
    
    // Setup Skill Registry
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    auto& stats = registry.emplace<CombatStats>(player);
    stats.damage_multipliers[0] = 2.0f; // 2x damage

    // 1. Create a Shadow with snapshot
    SkillSnapshot snapshot;
    snapshot.skill_id = 1;
    snapshot.position = {10.0f, 10.0f};
    snapshot.target_pos = {20.0f, 20.0f};
    snapshot.stats = stats;

    auto shadow = registry.create();
    registry.emplace<ShadowComponent>(shadow, snapshot, 0.1f, 1.0f);
    registry.emplace<Position>(shadow, 10.0f, 10.0f);

    // 2. Update before delay
    ShadowSystem::Update(registry, 0.05f);
    
    auto& sc = registry.get<ShadowComponent>(shadow);
    CHECK(sc.triggered == false);
    CHECK(sc.delay > 0.0f);

    // 3. Update after delay
    ShadowSystem::Update(registry, 0.1f);
    CHECK(sc.triggered == true);
    
    // Check if SkillExecution was created
    bool exec_found = false;
    auto exec_view = registry.view<SkillExecution>();
    for(auto exec_ent : exec_view) {
        auto& exec = exec_view.get<SkillExecution>(exec_ent);
        if (exec.owner == shadow) {
            exec_found = true;
            CHECK(exec.skill_id == 1);
            CHECK(exec.has_snapshot == true);
            CHECK(exec.snapshot.stats.damage_multipliers[0] == 2.0f);
        }
    }
    CHECK(exec_found == true);

    SkillSystem::InitHooks();
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player2 = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player2);
    registry.emplace<Position>(player2, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player2);
    
    // Equipping skill 1
    active.slots[0].id = 1;
    active.slots[0].current_charges = 1;

    // Trigger shadow cast
    SkillSystem::ShadowCast(registry, player2, 1, {100, 100}, {200, 200});

    // Run skill system update
    SkillSystem::Update(registry, grid, 0.1f); // Preparing -> Casting
    SkillSystem::Update(registry, grid, 0.1f); // Casting -> Settle
    SkillSystem::Update(registry, grid, 0.1f); // Settle -> Removed

    // 5. Update ShadowSystem for cleanup
    ShadowSystem::Update(registry, 1.0f); // Lifetime finished
    
    CHECK(registry.valid(shadow) == false);
}

TEST_CASE("Shadow System: Snapshot Independence") {
    entt::registry registry;
    
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

    auto player = registry.create();
    auto& stats = registry.emplace<CombatStats>(player);
    stats.damage_multipliers[0] = 1.5f;

    // Create shadow
    SkillSnapshot snapshot;
    snapshot.skill_id = 1;
    snapshot.stats = stats;

    auto shadow = registry.create();
    registry.emplace<ShadowComponent>(shadow, snapshot, 0.0f, 1.0f);

    // Change player stats after shadow creation
    stats.damage_multipliers[0] = 5.0f;

    // Trigger shadow
    ShadowSystem::Update(registry, 0.01f);
    
    auto exec_view = registry.view<SkillExecution>();
    auto exec_ent = exec_view.front();
    auto& exec = exec_view.get<SkillExecution>(exec_ent);
    
    // Shadow should still have 1.5 multiplier, not 5.0
    CHECK(exec.snapshot.stats.damage_multipliers[0] == 1.5f);
    
    // Check CombatStats attached to shadow entity
    auto& shadow_stats = registry.get<CombatStats>(shadow);
    CHECK(shadow_stats.damage_multipliers[0] == 1.5f);
}
