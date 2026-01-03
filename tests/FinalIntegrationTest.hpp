#pragma once
#include "doctest.h"
#include "entt/entt.hpp"
#include "../src/systems/SkillSystem.hpp"
#include "../src/systems/ShadowSystem.hpp"
#include "../src/systems/MovementStanceSystem.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/core/SkillRegistry.hpp"

using namespace NoMoreDay;

TEST_CASE("Final Integration: Sword Cultivator Full Flow") {
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();

    // 1. Setup Player
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<Velocity>(player, 0.0f, 0.0f);
    registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<SwordIntentComponent>(player);
    registry.emplace<MovementStanceComponent>(player);
    registry.emplace<ColorComponent>(player, WHITE);
    
    auto& active = registry.get<ActiveSkillsComponent>(player);
    active.slots[0].id = 1; // Flowing Thrust
    active.slots[0].current_charges = 1;

    // 2. Accumulate Sword Intent and Cast
    auto& intent = registry.get<SwordIntentComponent>(player);
    intent.stacks = 10;

    CHECK(SkillSystem::TryCast(registry, player, 0));
    
    // Skill is Preparing. Trigger PreCastHook.
    SkillSystem::Update(registry, 0.11f); 
    
    auto exec_view = registry.view<SkillExecution>();
    auto& exec = exec_view.get<SkillExecution>(exec_view.front());
    CHECK(exec.is_empowered == true);
    CHECK(intent.stacks == 0); // Consumed

    // 3. Trigger Shadow Echo (Manually for integration test simulation)
    // In a real scenario, this might be another hook or talent node.
    SkillSnapshot snapshot;
    snapshot.skill_id = 1;
    snapshot.is_empowered = exec.is_empowered; // Echo the empowered state
    snapshot.stats = registry.get<CombatStats>(player);
    
    auto shadow = registry.create();
    registry.emplace<ShadowComponent>(shadow, snapshot, 0.5f, 2.0f); // 0.5s delay
    registry.emplace<Position>(shadow, 0.0f, 0.0f);

    // 4. Player enters Sword Riding
    auto& vel = registry.get<Velocity>(player);
    vel.vx = 300.0f;
    MovementStanceSystem::Update(registry, 2.1f); // 2.1s later
    
    auto& stance = registry.get<MovementStanceComponent>(player);
    CHECK(stance.stance == MovementStance::SwordRiding);
    CHECK(registry.get<ColorComponent>(player).color.r == SKYBLUE.r);

    // 5. Update systems to process shadow
    // After 0.5s, the shadow should trigger its skill
    ShadowSystem::Update(registry, 0.6f);
    
    auto shadow_exec_view = registry.view<SkillExecution, ShadowCastTag>();
    CHECK(shadow_exec_view.begin() != shadow_exec_view.end());
    auto& shadow_exec = shadow_exec_view.get<SkillExecution>(shadow_exec_view.front());
    CHECK(shadow_exec.is_empowered == true); // Shadow echoed the empowerment!
    CHECK(shadow_exec.owner == shadow);

    // 6. Cleanup
    // Final check on entity count
    CHECK(registry.valid(player));
    CHECK(registry.valid(shadow));
}
