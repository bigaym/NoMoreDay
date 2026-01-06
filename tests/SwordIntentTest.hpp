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

TEST_CASE("Sword Intent: Mechanics") {
    entt::registry registry;
    systems::SpatialHashGrid grid(100, 100, 50);
    auto player = registry.create();
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    
    // Setup decay params
    intent.grace_period = 2.0f;
    intent.decay_interval = 0.5f;

    SUBCASE("Gain Logic: Crit") {
        intent.stacks = 0;
        // Basic Hit (No Crit)
        SkillSystem::OnSkillHit(registry, player, entt::null, 1, Tag::Melee, false);
        CHECK(intent.stacks == 1); // Melee gives 1

        // Crit Hit (Non-Melee)
        SkillSystem::OnSkillHit(registry, player, entt::null, 2, Tag::Projectile, true);
        CHECK(intent.stacks == 2); // Crit gives 1
        
        // Non-Crit Non-Melee
        SkillSystem::OnSkillHit(registry, player, entt::null, 2, Tag::Projectile, false);
        CHECK(intent.stacks == 2); // No change
    }

    SUBCASE("Decay Logic: Grace Period") {
        intent.stacks = 5;
        intent.time_since_last_gain = 0.0f;
        
        // Step 1: 1.5s
        SkillSystem::UpdateSwordIntent(registry, 1.5f);
        CHECK(intent.stacks == 5); // Time 1.5 < 2.0
        
        // Step 2: +0.6s (Total 2.1s)
        // Inside Update: time becomes 2.1. >= 2.0.
        // tick becomes 0 + 0.6 = 0.6.
        // 0.6 >= 0.5 -> Decay!
        SkillSystem::UpdateSwordIntent(registry, 0.6f);
        CHECK(intent.stacks == 4);
    }

    SUBCASE("Decay Logic: Rapid Decay") {
        intent.stacks = 5;
        intent.time_since_last_gain = 3.0f; // Already past grace
        intent.decay_tick_timer = 0.0f;

        // Step 0.6s (Interval is 0.5s)
        SkillSystem::UpdateSwordIntent(registry, 0.6f);
        CHECK(intent.stacks == 4);
        CHECK(intent.decay_tick_timer == 0.0f); // Reset after decay

        // Step 0.6s again
        SkillSystem::UpdateSwordIntent(registry, 0.6f);
        CHECK(intent.stacks == 3);
    }
    
    SUBCASE("Reset on Gain") {
        intent.stacks = 5;
        intent.time_since_last_gain = 3.0f;
        
        // Gain stack
        SkillSystem::OnSkillHit(registry, player, entt::null, 1, Tag::Melee, false);
        CHECK(intent.stacks == 6);
        CHECK(intent.time_since_last_gain == 0.0f);
        
        // Should be safe from decay for another grace period
        SkillSystem::UpdateSwordIntent(registry, 1.0f);
        CHECK(intent.stacks == 6);
    }
}