#pragma once
#include <entt/entt.hpp>
#include "../components/Common.hpp"
#include "SpatialGrid.hpp"

class PhysicsSystem {
public:
    // Update logic for a single entity (thread-safe, for parallel execution)
    // Added: entity ID (to avoid self-collision) and SpatialGrid (for neighbor query)
    static void updateEntity(entt::entity entity, Position& pos, Velocity& vel, 
                             systems::SpatialHashGrid& grid, const entt::registry& registry,
                             float dt, int screenWidth, int screenHeight);

    // Update all entities sequentially (fallback or simple usage)
    static void updateAll(entt::registry& registry, float dt, int screenWidth, int screenHeight);
};
