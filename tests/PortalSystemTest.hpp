#pragma once
#include "doctest.h"
#include "../src/systems/PortalSystem.hpp"
#include "../src/components/MapComponent.hpp"
#include "../src/components/Common.hpp"
#include "../src/core/SceneManager.hpp"

// Mock SceneManager or use real one with minimal dependencies?
// Real SceneManager needs LevelManager which needs... 
// It's getting integration-heavy.
// Let's just test that PortalSystem calls RequestTransition.
// But SceneManager::RequestTransition changes internal state.

TEST_CASE("PortalSystem - Trigger Transition") {
    using namespace NoMoreDay;
    
    // Setup Registry
    entt::registry registry;
    LevelManager lm;
    SceneManager sm(lm, registry);
    PortalSystem ps(sm);
    
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 100.0f, 100.0f);
    
    auto portal = registry.create();
    registry.emplace<PortalComponent>(portal, "town", 1);
    registry.emplace<Position>(portal, 100.0f, 100.0f);
    
    ps.Update(registry, 0.1f);
    
    CHECK(sm.IsTransitioning() == true);
}
