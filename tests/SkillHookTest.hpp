#pragma once
#include "TestCommon.hpp"
#include "../src/systems/SkillSystem.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Stats.hpp"
#include "../src/core/SkillRegistry.hpp"

TEST_CASE("SkillSystem: Logic Hooks") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::ClearHooks();

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    auto& stats = registry.emplace<CombatStats>(player);
    stats.mana = 100.0f;
    active.slots[0].id = 1; // Flowing Thrust
    active.slots[0].current_charges = 1;

    SUBCASE("PreCast Hook Trigger") {
        bool pre_called = false;
        SkillSystem::AddPreCastHook([&](entt::registry&, entt::entity, SkillExecution& exec) {
            pre_called = true;
            CHECK(exec.skill_id == 1);
            CHECK(exec.state == SkillState::Preparing);
        });

        CHECK(SkillSystem::TryCast(registry, player, 0));
        
        // Update to trigger transition from Preparing to Casting
        SkillSystem::Update(registry, 0.11f); 
        CHECK(pre_called);
    }

    SUBCASE("PostCast Hook Trigger") {
        bool post_called = false;
        SkillSystem::AddPostCastHook([&](entt::registry&, entt::entity, SkillExecution& exec) {
            post_called = true;
            CHECK(exec.skill_id == 1);
            CHECK(exec.state == SkillState::Settle);
        });

        CHECK(SkillSystem::TryCast(registry, player, 0));
        
        // Preparing (0.1s) -> Casting (0.05s) -> Settle
        SkillSystem::Update(registry, 0.11f); // To Casting
        SkillSystem::Update(registry, 0.06f); // To Settle
        
        CHECK(post_called);
    }

    SUBCASE("Multiple Hooks") {
        int count = 0;
        SkillSystem::AddPreCastHook([&](entt::registry&, entt::entity, SkillExecution&) { count++; });
        SkillSystem::AddPreCastHook([&](entt::registry&, entt::entity, SkillExecution&) { count++; });

        CHECK(SkillSystem::TryCast(registry, player, 0));
        SkillSystem::Update(registry, 0.11f);
        CHECK(count == 2);
    }
}
