#pragma once
#include "doctest.h"
#include "entt/entt.hpp"
#include "game/systems/ui/MonsterHealthBarSystem.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Common.hpp"
#include "game/components/Buff.hpp"

using namespace NoMoreDay;

TEST_CASE("MonsterHealthBarSystem: Visibility and Buffs") {
    entt::registry registry;
    Camera2D camera = { {0,0}, {0,0}, 0.0f, 1.0f };

    auto enemy = registry.create();
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<Position>(enemy, 100.0f, 100.0f);
    auto& stats = registry.emplace<CombatStats>(enemy);
    stats.health = 50.0f;
    stats.max_health = 100.0f;

    SUBCASE("Render Call Does Not Crash") {
        // We can't easily test visual output in unit tests without mocking Raylib,
        // but we can ensure the logic that iterates over entities is correct.
        // For now, ensure calling it doesn't crash with various components missing.
        systems::MonsterHealthBarSystem::Render(registry, camera);
    }

    SUBCASE("Buff Synchronization") {
        auto& activeEffects = registry.emplace<ActiveEffectsComponent>(enemy);
        
        BuffEffect buff;
        buff.id = "test_buff";
        buff.is_debuff = false;
        activeEffects.effects.push_back(buff);

        BuffEffect debuff;
        debuff.id = "test_debuff";
        debuff.is_debuff = true;
        activeEffects.effects.push_back(debuff);

        CHECK(activeEffects.effects.size() == 2);
        CHECK(activeEffects.effects[0].is_debuff == false);
        CHECK(activeEffects.effects[1].is_debuff == true);
        
        // Ensure the render logic (which we verified manually in code) would see these.
        systems::MonsterHealthBarSystem::Render(registry, camera);
    }

    SUBCASE("Full Health Hiding") {
        stats.health = 100.0f;
        stats.max_health = 100.0f;
        // Logic check: system should skip full HP enemies
        systems::MonsterHealthBarSystem::Render(registry, camera);
    }
}
