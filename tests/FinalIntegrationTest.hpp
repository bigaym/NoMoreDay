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
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<Velocity>(player, 0.0f, 0.0f);
    registry.emplace<AnimationStateComponent>(player);
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    registry.emplace<MovementStanceComponent>(player);
    intent.stacks = 10;
    
    active.slots[0].id = 1;
    active.slots[0].current_charges = 1;

    // Cast
    CHECK(SkillSystem::TryCast(registry, player, 0));
    
    // Update
    SkillSystem::Update(registry, grid, 0.11f); 
    
    auto exec_view = registry.view<SkillExecution>();
    REQUIRE(exec_view.begin() != exec_view.end());
    auto& exec = exec_view.get<SkillExecution>(exec_view.front());
    CHECK(exec.is_empowered == true);
    CHECK(intent.stacks == 0); // Consumed

    // 3. Trigger Shadow Echo
    SkillSnapshot snapshot;
    snapshot.skill_id = 1;
    snapshot.is_empowered = true;
    snapshot.stats = registry.get<CombatStats>(player);
    snapshot.position = {0,0};
    snapshot.target_pos = {100,0};
    
    auto shadow = registry.create();
    registry.emplace<LocalLevelTag>(shadow);
    registry.emplace<ShadowComponent>(shadow, snapshot, 0.1f, 1.0f);
    registry.emplace<Position>(shadow, 0.0f, 0.0f);
    registry.emplace<Velocity>(shadow, 0.0f, 0.0f);

    // 4. Update SkillSystem to trigger shadow
    SkillSystem::Update(registry, grid, 0.15f);
    
    auto shadow_exec_view = registry.view<SkillExecution, ShadowCastTag>();
    REQUIRE(shadow_exec_view.begin() != shadow_exec_view.end());
    auto& shadow_exec = shadow_exec_view.get<SkillExecution>(*shadow_exec_view.begin());
    CHECK(shadow_exec.is_empowered == true); 

    // 6. Cleanup
    CHECK(registry.valid(player));
}
