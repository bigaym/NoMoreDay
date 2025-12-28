#include "PhysicsSystem.hpp"
#include <cmath>
#include <algorithm>

void PhysicsSystem::resolveCollisions(entt::entity entity, const Position& pos, Velocity& vel, 
                                    systems::SpatialHashGrid& grid, const entt::registry& registry,
                                    float dt) {
    // LOG_TRACE("PhysicsSystem: Resolving collisions for entity {}", (uint32_t)entity);  // 日志太频繁

    // Parameters
    const float entityRadius = 5.0f; 
    const float separationDist = entityRadius * 2.0f; // 10.0f
    
    // Optimization: Don't search larger than necessary. 
    // Setting searchRadius = separationDist reduces checked cells/entities.
    const float searchRadius = separationDist; 
    const float repulsionStrength = 200.0f; // Lower strength for better stability with high density

    grid.query(pos, searchRadius, [&](entt::entity neighbor) {
        if (neighbor == entity) return;

        if (!registry.valid(neighbor)) return;
        // 只读访问邻居位置，确保线程安全
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

            // 只更新速度，不修改位置
            vel.vx += forceX * dt;
            vel.vy += forceY * dt;
        }
    });
}

void PhysicsSystem::updatePosition(entt::entity entity, Position& pos, Velocity& vel, 
                                 float dt, int worldWidth, int worldHeight) {
    // LOG_TRACE("PhysicsSystem: Updating position for entity {}", (uint32_t)entity); // 频率太高

    // 1. Movement Integration
    pos.x += vel.vx * dt;
    pos.y += vel.vy * dt;

    // 2. Boundary Collision - Use std::clamp and bounce
    if (pos.x < 0) {
        pos.x = 0;
        vel.vx *= -1;
    } else if (pos.x > (float)worldWidth) {
        pos.x = (float)worldWidth;
        vel.vx *= -1;
    }

    if (pos.y < 0) {
        pos.y = 0;
        vel.vy *= -1;
    } else if (pos.y > (float)worldHeight) {
        pos.y = (float)worldHeight;
        vel.vy *= -1;
    }
}

void PhysicsSystem::updateAll(entt::registry& registry, float dt, int screenWidth, int screenHeight) {
    // Fallback: No collision (grid required)
    auto view = registry.view<Position, Velocity>();
    view.each([dt, screenWidth, screenHeight](auto entity, auto& pos, auto& vel) {
        updatePosition(entity, pos, vel, dt, screenWidth, screenHeight);
    });
}
