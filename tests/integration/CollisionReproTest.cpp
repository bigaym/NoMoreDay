#pragma once

#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/physics/PhysicsSystem.hpp"
#include <cmath>

namespace NoMoreDay {

TEST_CASE("[Bugfix] Dash - Dash Tunneling Reproduction") {
    // Setup
    entt::registry registry;
    systems::SpatialHashGrid grid(200, 200, 50);

    // 1. Create Wall (Static Collider)
    // Wall at x=100, width=10. Center at 100,50. X range: [95, 105]
    auto wall = registry.create();
    registry.emplace<Position>(wall, 100.0f, 50.0f);
    registry.emplace<ColliderComponent>(wall, 10.0f, 100.0f, ColliderType::Static);

    // 2. Create Player
    // Player at x=80, radius=10. Right edge at 90.
    auto player = registry.create();
    registry.emplace<Position>(player, 80.0f, 50.0f);
    registry.emplace<Velocity>(player, 0.0f, 0.0f);
    registry.emplace<DashComponent>(player);

    // Populate Grid
    auto view = registry.view<Position>();
    grid.rebuild(view, registry);

    // 3. Simulate Dash (Fixed Logic)
    auto& pos = registry.get<Position>(player);
    auto& vel = registry.get<Velocity>(player);
    auto& dash = registry.get<DashComponent>(player);

    // Dash Params
    dash.isDashing = true;
    dash.dashSpeed = 1000.0f; // Very fast
    dash.dirX = 1.0f;
    dash.dirY = 0.0f;
    dash.dashTimer = 0.1f;

    float dt = 0.05f; // Frame time

    // Fixed Logic
    vel.vx = dash.dirX * dash.dashSpeed;
    vel.vy = dash.dirY * dash.dashSpeed;
    
    PhysicsSystem::performDashStep(registry, player, dash, pos, vel, dt, grid);

    // Check
    // Wall is at 100 (95-105). Player started at 80.
    // It should stop before 95 (approx 90 + radius check margin).
    // The physics system check uses radius 15 + 20 margin in query? No, the check itself uses radius 15.
    // So player center should stop at approx 95 - 15 = 80? Or just not pass 95.
    
    CHECK_MESSAGE(pos.x < 95.0f, "Player should stop before the wall! Final Pos X: ", pos.x);
    CHECK_MESSAGE(dash.isDashing == false, "Dash should be interrupted");
}

} // namespace NoMoreDay
