#pragma once
#include <entt/entt.hpp>
#include "game/components/Common.hpp"
#include "game/systems/physics/SpatialGrid.hpp"

// Fwd decl
namespace tf { class Executor; }

// Forward declaration
// Forward declaration
class MapSystem;

class PhysicsSystem {
public:
    // Phase 1: 解决碰撞与计算受力 (线程安全: 读 Position, 写 Velocity)
    static void resolveCollisions(entt::entity entity, const Position& pos, Velocity& vel, 
                                NoMoreDay::systems::SpatialHashGrid& grid, const entt::registry& registry,
                                float dt);

    // Phase 2: 位置积分与边界处理 (线程安全: 读 Velocity, 写 Position)
    static void updatePosition(entt::registry& registry, entt::entity entity, Position& pos, Velocity& vel, 
                             float dt, int worldWidth, int worldHeight);

    // Apply Force Fields (Vortex, etc.)
    static void applyForceFields(entt::registry& registry, float dt, NoMoreDay::systems::SpatialHashGrid& grid);

    // Update all entities sequentially (fallback or simple usage)
    static void updateAll(entt::registry& registry, float dt, int screenWidth, int screenHeight, NoMoreDay::systems::SpatialHashGrid& grid, tf::Executor* executor = nullptr);

    // Handle Dash Movement with CCD (Continuous Collision Detection)
    static void performDashStep(entt::registry& registry, entt::entity entity, struct DashComponent& dash, Position& pos, Velocity& vel, float dt, const NoMoreDay::systems::SpatialHashGrid& grid, const MapSystem* map = nullptr);
};
