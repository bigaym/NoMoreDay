#include "PhysicsSystem.hpp"
#include <cmath>

void PhysicsSystem::updateEntity(entt::entity entity, Position& pos, Velocity& vel, 
                                 systems::SpatialHashGrid& grid, const entt::registry& registry,
                                 float dt, int screenWidth, int screenHeight) {
    // 1. Movement Integration
    pos.x += vel.vx * dt;
    pos.y += vel.vy * dt;

    // 2. Boundary Collision
    if (pos.x < 0) {
        pos.x = 0;
        vel.vx *= -1;
    } else if (pos.x > (float)screenWidth) {
        pos.x = (float)screenWidth;
        vel.vx *= -1;
    }

    if (pos.y < 0) {
        pos.y = 0;
        vel.vy *= -1;
    } else if (pos.y > (float)screenHeight) {
        pos.y = (float)screenHeight;
        vel.vy *= -1;
    }

    // 3. Separation (Soft Collision) via Spatial Grid
    // Parameters
    const float entityRadius = 5.0f; 
    const float separationDist = entityRadius * 2.0f; // 10.0f
    
    // Optimization: Don't search larger than necessary. 
    // Setting searchRadius = separationDist reduces checked cells/entities.
    const float searchRadius = separationDist; 
    const float repulsionStrength = 200.0f; // Lower strength for better stability with high density

    grid.query(pos, searchRadius, [&](entt::entity neighbor) {
        if (neighbor == entity) return;

        // Note: For extreme performance, we might want to store positions in the grid 
        // or use a SoA layout, but getting from registry is standard ECS.
        if (!registry.valid(neighbor)) return;
        const auto& nPos = registry.get<Position>(neighbor);

        float dx = pos.x - nPos.x;
        float dy = pos.y - nPos.y;
        float distSq = dx*dx + dy*dy;

        // Avoid division by zero and only repel if within radius
        if (distSq > 0.0001f && distSq < separationDist * separationDist) {
            float dist = std::sqrt(distSq);
            float overlap = separationDist - dist;
            
            // Force vector (normalized * overlap * strength)
            float forceX = (dx / dist) * overlap * repulsionStrength;
            float forceY = (dy / dist) * overlap * repulsionStrength;

            // Apply to velocity (mass = 1)
            vel.vx += forceX * dt;
            vel.vy += forceY * dt;
        }
    });
}

void PhysicsSystem::updateAll(entt::registry& registry, float dt, int screenWidth, int screenHeight) {
    // Fallback: No collision (grid required)
    auto view = registry.view<Position, Velocity>();
    view.each([dt, screenWidth, screenHeight](auto& pos, auto& vel) {
        // Just move
        pos.x += vel.vx * dt;
        pos.y += vel.vy * dt;
        // ... boundary checks skipped for brevity in fallback
    });
}
