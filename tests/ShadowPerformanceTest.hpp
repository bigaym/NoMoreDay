#pragma once
#include "doctest.h"
#include "entt/entt.hpp"
#include "../src/systems/ShadowSystem.hpp"
#include "../src/systems/SkillSystem.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Common.hpp"
#include <chrono>

using namespace NoMoreDay;

TEST_CASE("Shadow System: Performance Stress Test") {
    entt::registry registry;
    
    // Spawn 100 shadows
    for (int i = 0; i < 100; ++i) {
        auto shadow = registry.create();
        SkillSnapshot snapshot;
        snapshot.skill_id = 1;
        registry.emplace<ShadowComponent>(shadow, snapshot, 0.1f, 1.0f);
        registry.emplace<Position>(shadow, (float)i, (float)i);
        registry.emplace<Velocity>(shadow, 0.0f, 0.0f);
    }

    auto start = std::chrono::high_resolution_clock::now();
    
    // Update for 100 frames
    for (int frame = 0; frame < 100; ++frame) {
        ShadowSystem::Update(registry, 0.016f);
        SkillSystem::Update(registry, 0.016f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    double avg_time_per_frame = (double)duration / 100.0;
    
    LOG_INFO("Performance: 100 shadows avg update time: {:.2f}us", avg_time_per_frame);
    
    // 1ms = 1000us. Update should be MUCH faster than 1ms.
    CHECK(avg_time_per_frame < 1000.0);
}
