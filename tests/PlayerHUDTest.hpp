#pragma once
#include "doctest.h"
#include "entt/entt.hpp"
#include "game/systems/ui/PlayerHUD.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/ui/UISystem.hpp"

using namespace NoMoreDay;

TEST_CASE("PlayerHUD: Render Logic") {
    entt::registry registry;
    
    // Setup UISystem state scale
    UISystem::State.scaleFactor = 1.0f;

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& stats = registry.emplace<CombatStats>(player);
    stats.health = 80.0f;
    stats.max_health = 100.0f;
    stats.mana = 50.0f;
    stats.max_mana = 100.0f;
    
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    intent.stacks = 5;
    intent.max_stacks = 10;

    SUBCASE("Draw Call Does Not Crash") {
        // Ensure no crash with various data states
        systems::PlayerHUD::Draw(registry);
    }

    SUBCASE("Missing Components Does Not Crash") {
        auto other = registry.create();
        registry.emplace<PlayerTag>(other);
        // Missing stats and intent
        systems::PlayerHUD::Draw(registry);
    }
}
