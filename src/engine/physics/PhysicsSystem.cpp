#include "engine/physics/PhysicsSystem.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/Projectile.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <taskflow/algorithm/for_each.hpp>
#include <taskflow/taskflow.hpp>


// Persistent buffer to avoid per-frame reallocations
static thread_local std::vector<entt::entity> s_entity_buffer;

void PhysicsSystem::resolveCollisions(entt::entity entity, const Position &pos,
                                      Velocity &vel,
                                      NoMoreDay::systems::SpatialHashGrid &grid,
                                      const entt::registry &registry,
                                      float dt) {

  // 参数
  using namespace NoMoreDay::Constants::Physics;
  const float entityRadius = DEFAULT_ENTITY_RADIUS;
  const float separationDist = entityRadius * SEPARATION_DIST_MULT;

  const float searchRadius = separationDist;
  const float repulsionStrength = REPULSION_STRENGTH;

  grid.query(
      pos, searchRadius, [&](entt::entity neighbor, const Position &nPos) {
        if (neighbor == entity)
          return;

        float dx = pos.x - nPos.x;
        float dy = pos.y - nPos.y;
        float distSq = dx * dx + dy * dy;

        using namespace NoMoreDay::Constants::Physics;
        if (distSq > MIN_DIST_SQ_THRESHOLD && distSq < separationDist * separationDist) {
          float dist = std::sqrt(distSq);
          float overlap = separationDist - dist;

          float forceX = (dx / dist) * overlap * repulsionStrength;
          float forceY = (dy / dist) * overlap * repulsionStrength;

          vel.vx += forceX * dt;
          vel.vy += forceY * dt;
        }
      });
}

void PhysicsSystem::updatePosition(entt::entity entity, Position &pos,
                                   Velocity &vel, float dt, int worldWidth,
                                   int worldHeight) {

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

void PhysicsSystem::updateAll(entt::registry &registry, float dt,
                              int screenWidth, int screenHeight,
                              NoMoreDay::systems::SpatialHashGrid &grid,
                              tf::Executor *executor) {
  using namespace NoMoreDay;

  auto view = registry.view<Position, Velocity>();

  // 1. Collect entities for parallel processing using persistent buffer
  s_entity_buffer.clear();
  s_entity_buffer.reserve(view.size_hint());
  for (auto entity : view) {
    s_entity_buffer.push_back(entity);
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

  if (executor && !s_entity_buffer.empty()) {
    tf::Taskflow tf;

    // Phase 1: Collision Resolution (Writes to Velocity)
    auto t1 = tf.for_each(s_entity_buffer.begin(), s_entity_buffer.end(),
                          process_collision);

    // Phase 2: Integration (Reads Velocity, Writes Position)
    auto t2 = tf.for_each(s_entity_buffer.begin(), s_entity_buffer.end(),
                          process_integration);

    t1.precede(t2);

    executor->run(tf).wait();
  } else {
    // Serial Fallback
    for (auto entity : s_entity_buffer) {
      process_collision(entity);
      process_integration(entity);
    }
  }
}
