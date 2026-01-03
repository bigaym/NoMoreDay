#pragma once
#include "doctest.h"
#include "entt/entt.hpp"
#include "../src/systems/SkillSystem.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/Stats.hpp"
#include "../src/core/SkillRegistry.hpp"

using namespace NoMoreDay;

TEST_CASE("Sword Intent: Empowered Logic") {
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    
    active.slots[0].id = 1; // Flowing Thrust
    active.slots[0].current_charges = 1;

    SUBCASE("Stacks < 10: Not Empowered") {
        intent.stacks = 5;
        CHECK(SkillSystem::TryCast(registry, player, 0));
        
        auto exec_view = registry.view<SkillExecution>();
        auto& exec = exec_view.get<SkillExecution>(exec_view.front());
        
        // Before PreCastHook (Still Preparing)
        CHECK(exec.is_empowered == false);
        
        // Trigger PreCastHook
        SkillSystem::Update(registry, grid, 0.11f); 
        CHECK(exec.is_empowered == false);
        CHECK(intent.stacks == 5);
    }

    SUBCASE("Stacks == 10: Trigger Empowered") {
        intent.stacks = 10;
        CHECK(SkillSystem::TryCast(registry, player, 0));
        
        // Trigger PreCastHook
        SkillSystem::Update(registry, grid, 0.11f); 
        
        auto exec_view = registry.view<SkillExecution>();
        auto& exec = exec_view.get<SkillExecution>(exec_view.front());
        
        CHECK(exec.is_empowered == true);
        CHECK(intent.stacks == 0);
    }

    SUBCASE("Shadows Do Not Consume Stacks") {
        intent.stacks = 10;
        
        // Trigger a shadow cast directly
        SkillSystem::ShadowCast(registry, player, 1, {0,0}, {0,0});
        
        // SkillSystem::Update will trigger PreCastHook
        SkillSystem::Update(registry, grid, 0.1f);
        
        CHECK(intent.stacks == 10); // Should still be 10
        
        auto exec_view = registry.view<SkillExecution>();
        bool found = false;
        for(auto entity : exec_view) {
            auto& exec = exec_view.get<SkillExecution>(entity);
            if (registry.any_of<ShadowCastTag>(entity)) {
                CHECK(exec.is_empowered == false); // Unless snapshot says so
                found = true;
            }
        }
        CHECK(found);
    }
}
