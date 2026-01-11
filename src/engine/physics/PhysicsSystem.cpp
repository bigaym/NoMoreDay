#include "engine/physics/PhysicsSystem.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/AIComponent.hpp"
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

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/for_each.hpp>

void PhysicsSystem::updateAll(entt::registry& registry, float dt, int screenWidth, int screenHeight, NoMoreDay::systems::SpatialHashGrid& grid, tf::Executor* executor) {
    using namespace NoMoreDay;

    auto view = registry.view<Position, Velocity>();
    
    // 1. Collect entities for parallel processing
    // EnTT views are not random access, so we can't directly partition them for Taskflow efficiently without extraction.
    std::vector<entt::entity> entities;
    entities.reserve(view.size_hint()); 
    for(auto entity : view) {
        entities.push_back(entity);
    }

    auto process_collision = [&](entt::entity entity) {
        if (registry.any_of<PlayerTag, EnemyTag>(entity)) {
            auto [pos, vel] = view.get<Position, Velocity>(entity);
            resolveCollisions(entity, pos, vel, grid, registry, dt);
        }
    };

    auto process_integration = [&](entt::entity entity) {
        auto [pos, vel] = view.get<Position, Velocity>(entity);
        updatePosition(entity, pos, vel, dt, screenWidth, screenHeight);
    };

    if (executor && !entities.empty()) {
        tf::Taskflow tf;
        
        // Phase 1: Collision Resolution (Writes to Velocity)
        auto t1 = tf.for_each(entities.begin(), entities.end(), process_collision);
        
        // Phase 2: Integration (Reads Velocity, Writes Position)
        auto t2 = tf.for_each(entities.begin(), entities.end(), process_integration);
        
        t1.precede(t2);
        
        executor->run(tf).wait();
    } else {
        // Serial Fallback
        for(auto entity : entities) {
            process_collision(entity);
            process_integration(entity);
        }
    }
}
        