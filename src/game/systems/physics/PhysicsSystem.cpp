#include "game/systems/physics/PhysicsSystem.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/systems/physics/PhysicsConstants.hpp"
#include "game/systems/world/WorldConstants.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/PlayerState.hpp"
#include "game/systems/world/MapSystem.hpp" // Added
#include "game/systems/world/TilemapCollisionSystem.hpp" // Added
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <taskflow/algorithm/for_each.hpp>
#include <taskflow/taskflow.hpp>


// Persistent buffer to avoid per-frame reallocations
static thread_local std::vector<entt::entity> s_entity_buffer;

void PhysicsSystem::performDashStep(entt::registry& registry, entt::entity entity, DashComponent& dash, Position& pos, Velocity& vel, float dt, const NoMoreDay::systems::SpatialHashGrid& grid, const MapSystem* map) {
    // 1. Calculate intended movement
    float moveX = vel.vx * dt;
    float moveY = vel.vy * dt;
    float distSq = moveX * moveX + moveY * moveY;
    
    using namespace NoMoreDay::Constants::Physics;
    if (distSq < MIN_DIST_SQ_THRESHOLD) return;
    
    float dist = std::sqrt(distSq);
    
    // 2. Stepping for CCD
    // Use a step size smaller than player size/wall thickness
    // Assuming player radius approx 15.0f.
    using namespace NoMoreDay::Constants::Physics;
    int steps = static_cast<int>(std::ceil(dist / CCD_STEP_SIZE));
    if (steps == 0) steps = 1;
    
    float stepX = moveX / static_cast<float>(steps);
    float stepY = moveY / static_cast<float>(steps);
    
    // Current test position
    float currX = pos.x;
    float currY = pos.y;
    
    using namespace NoMoreDay::Constants::Physics;
    using namespace NoMoreDay::Constants::Physics;
    float entityRadius = DEFAULT_ENTITY_RADIUS; 
    if (registry.all_of<Radius>(entity)) {
        entityRadius = registry.get<Radius>(entity).value;
    }
    // Reduced radius for map collision to match GameplayState logic and prevent sticking
    float collisionRadius = std::max(1.0f, entityRadius * MAP_COLLISION_RADIUS_FACTOR); 
    const bool isPhasing = registry.any_of<PhaseTag>(entity);
    
    for (int i = 0; i < steps; ++i) {
        float nextX = currX + stepX;
        float nextY = currY + stepY;
        
        bool hit = false;
        
        if (!isPhasing) {
            // A. Check Tilemap (Static World)
            if (map) {
                 // Use IsAreaWalkable to check the full body of the entity against the map area
                 // This prevents tunneling through corners or "staircase" tiles
                 if (!NoMoreDay::TilemapCollisionSystem::IsAreaWalkable(*map, {nextX, nextY}, collisionRadius)) {
                     // LOG_INFO("Dash Collision: Hit Wall at ({:.2f}, {:.2f})", nextX, nextY);
                     hit = true;
                 }
            }

            // B. Check Dynamic Entities (via Grid)
            if (!hit) {
                // Query around next position
                // Search radius needs to include the entity radius + check margin
                grid.query({nextX, nextY}, entityRadius + 20.0f, [&](entt::entity neighbor, const Position& nPos) {
                    if (hit) return; // Already hit in this step
                    if (neighbor == entity) return;
                    
                    if (registry.any_of<ColliderComponent>(neighbor)) {
                        const auto& col = registry.get<ColliderComponent>(neighbor);
                        if (col.type == ColliderType::Static) {
                             // Circle-AABB Check
                             float hw = col.width * 0.5f;
                             float hh = col.height * 0.5f;
                             
                             float closestX = std::clamp(nextX, nPos.x - hw, nPos.x + hw);
                             float closestY = std::clamp(nextY, nPos.y - hh, nPos.y + hh);
                             
                             float cdx = nextX - closestX;
                             float cdy = nextY - closestY;
                             float dSq = cdx*cdx + cdy*cdy;
                             
                             if (dSq < entityRadius * entityRadius) {
                                 hit = true;
                             }
                        }
                    }
                });
            }
        }
        
        if (hit) {
            // Stop dash
            dash.isDashing = false;
            dash.dashTimer = 0.0f; 
            vel.vx = 0;
            vel.vy = 0;
            registry.remove<PhaseTag>(entity);
            
            // Stop at current position (before hitting wall)
            break; 
        } else {
            currX = nextX;
            currY = nextY;
        }
    }
    
    // 3. Commit position
    pos.x = currX;
    pos.y = currY;
}

void PhysicsSystem::resolveCollisions(entt::entity entity, const Position &pos,
                                      Velocity &vel,
                                      NoMoreDay::systems::SpatialHashGrid &grid,
                                      const entt::registry &registry,
                                      float dt) {
  if (registry.any_of<PhaseTag>(entity))
    return;

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
        
        // Check for Static Collider (e.g. Walls)
        if (registry.any_of<ColliderComponent>(neighbor)) {
            const auto& collider = registry.get<ColliderComponent>(neighbor);
            if (collider.type == ColliderType::Static) {
                // Circle-AABB Collision
                float hw = collider.width * 0.5f;
                float hh = collider.height * 0.5f;
                // nPos is center
                float closestX = std::clamp(pos.x, nPos.x - hw, nPos.x + hw);
                float closestY = std::clamp(pos.y, nPos.y - hh, nPos.y + hh);
                
                float cdx = pos.x - closestX;
                float cdy = pos.y - closestY;
                float distSq = cdx*cdx + cdy*cdy;
                
                // Hard Push (Approximate)
                using namespace NoMoreDay::Constants::Physics;
                if (distSq < entityRadius * entityRadius && distSq > MIN_DIST_SQ_THRESHOLD) {
                    float dist = std::sqrt(distSq);
                    float overlap = entityRadius - dist;
                    // Stronger repulsion for walls
                    vel.vx += (cdx / dist) * overlap * repulsionStrength * WALL_REPULSION_FACTOR * dt;
                    vel.vy += (cdy / dist) * overlap * repulsionStrength * WALL_REPULSION_FACTOR * dt;
                }
                return;
            }
        }
        
        // Standard Boid Separation
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

void PhysicsSystem::updatePosition(entt::registry& registry, entt::entity entity, Position &pos,
                                   Velocity &vel, float dt, int worldWidth,
                                   int worldHeight) {

  // [SAFETY] Clamp velocity to prevent physics explosion (mimic GPU shader)
  float speedSq = vel.vx * vel.vx + vel.vy * vel.vy;
  using namespace NoMoreDay::Constants::Physics;
  if (speedSq > MAX_VELOCITY * MAX_VELOCITY) {
    float speed = std::sqrt(speedSq);
    vel.vx = (vel.vx / speed) * MAX_VELOCITY;
    vel.vy = (vel.vy / speed) * MAX_VELOCITY;
  }

  pos.x += vel.vx * dt;
  pos.y += vel.vy * dt;

  // [DAMPING] Apply velocity damping ONLY to gameplay entities (Enemies/Players)
  // Projectiles should maintain their speed.
  if (registry.any_of<EnemyTag, PlayerTag>(entity)) {
    using namespace NoMoreDay::Constants::Physics;
    float damping = std::pow(ENTITY_DAMPING_FACTOR, dt * 60.0f);
    vel.vx *= damping;
    vel.vy *= damping;
  }

  // [BOUNDARY] World boundary clamping with generous fallback
  // Use Constants if passed values are suspiciously small/zero
  float limitX = (worldWidth > 100) ? (float)worldWidth : NoMoreDay::Constants::World::MAP_BOUNDARY;
  float limitY = (worldHeight > 100) ? (float)worldHeight : NoMoreDay::Constants::World::MAP_BOUNDARY;

  if (pos.x < 0) {
    pos.x = 0;
    vel.vx *= -0.5f;
  } else if (pos.x > limitX) {
    pos.x = limitX;
    vel.vx *= -0.5f;
  }

  if (pos.y < 0) {
    pos.y = 0;
    vel.vy *= -0.5f;
  } else if (pos.y > limitY) {
    pos.y = limitY;
    vel.vy *= -0.5f;
  }
}

void PhysicsSystem::applyForceFields(entt::registry& registry, float dt, NoMoreDay::systems::SpatialHashGrid& grid) {
    using namespace NoMoreDay;
    auto view = registry.view<Position, ForceFieldComponent>();
    view.each([&](entt::entity entity, const Position& pos, ForceFieldComponent& ff) {
        if (!ff.isAlwaysOn && ff.activeDuration > 0 && ff.currentCooldown > 0) return; // Simple check

        float r = ff.radius;
        grid.query(pos, r, [&](entt::entity target, const Position& tPos) {
            if (target == entity) return;
            if (!registry.any_of<Velocity>(target)) return;
            
            // Ignore other walls/static objects
            if (registry.any_of<ColliderComponent>(target)) {
                 const auto& col = registry.get<ColliderComponent>(target);
                 if (col.type == ColliderType::Static) return;
            }

            float dx = tPos.x - pos.x;
            float dy = tPos.y - pos.y;
            float distSq = dx*dx + dy*dy;
            using namespace NoMoreDay::Constants::Physics;
            if (distSq < r*r && distSq > MIN_DIST_SQ_THRESHOLD) {
                float dist = std::sqrt(distSq);
                float factor = 1.0f - (dist / r); // Linear Falloff
                
                // Force vector
                float fx = (dx / dist) * ff.strength * factor;
                float fy = (dy / dist) * ff.strength * factor;
                
                auto& tVel = registry.get<Velocity>(target);
                tVel.vx += fx * dt;
                tVel.vy += fy * dt;
            }
        });
    });
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
    updatePosition(registry, entity, pos, vel, dt, screenWidth, screenHeight);
  };

  if (executor && !s_entity_buffer.empty()) {
    tf::Taskflow tf;

    // Phase 0: Force Fields
    applyForceFields(registry, dt, grid);

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
      auto [pos, vel] = view.get<Position, Velocity>(entity);
      updatePosition(registry, entity, pos, vel, dt, screenWidth, screenHeight);
    }
  }
}
