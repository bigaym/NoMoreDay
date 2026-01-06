#include "PhysicsSystem.hpp"
#include "../components/Projectile.hpp"
#include "../components/Common.hpp"
#include "../components/EnemyComponent.hpp"
#include "../components/AIComponent.hpp"
#include "raymath.h"
#include <cmath>
#include <algorithm>

void PhysicsSystem::resolveCollisions(entt::entity entity, const Position& pos, Velocity& vel, 
 NoMoreDay::systems::SpatialHashGrid& grid, const entt::registry& registry,
 float dt) {

    // 参数
    const float entityRadius = 5.0f;
    const float separationDist = entityRadius * 2.0f; // 10.0f

    const float searchRadius = separationDist;
    const float repulsionStrength = 200.0f; 

    grid.query(pos, searchRadius, [&](entt::entity neighbor) {
        if (neighbor == entity) return;

        if (!registry.valid(neighbor)) return;
        const auto& nPos = registry.get<Position>(neighbor);

        float dx = pos.x - nPos.x;
        float dy = pos.y - nPos.y;
        float distSq = dx*dx + dy*dy;

        if (distSq > 0.0001f && distSq < separationDist * separationDist) {
            float dist = std::sqrt(distSq);
            float overlap = separationDist - dist;
            
            float forceX = (dx / dist) * overlap * repulsionStrength;
            float forceY = (dy / dist) * overlap * repulsionStrength;
            
            vel.vx += forceX * dt;
            vel.vy += forceY * dt;
        }
    });
}

void PhysicsSystem::updatePosition(entt::entity entity, Position& pos, Velocity& vel, 
 float dt, int worldWidth, int worldHeight) {

    pos.x += vel.vx * dt;
    pos.y += vel.vy * dt;

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

void PhysicsSystem::updateAll(entt::registry& registry, float dt, int screenWidth, int screenHeight, NoMoreDay::systems::SpatialHashGrid& grid) {
    using namespace NoMoreDay;

    auto view = registry.view<Position, Velocity>();
    view.each([dt, screenWidth, screenHeight, &grid, &registry](auto entity, auto& pos, auto& vel) {
        if (registry.any_of<PlayerTag, EnemyTag>(entity)) {
            resolveCollisions(entity, pos, vel, grid, registry, dt);
        }
                updatePosition(entity, pos, vel, dt, screenWidth, screenHeight);
            });
        }
        