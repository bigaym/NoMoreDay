#pragma once
#include <entt/entt.hpp>
#include "../components/Common.hpp"
#include "SpatialGrid.hpp"

class PhysicsSystem {
public:
    // Phase 1: 解决碰撞与计算受力 (线程安全: 读 Position, 写 Velocity)
    static void resolveCollisions(entt::entity entity, const Position& pos, Velocity& vel, 
                                NoMoreDay::systems::SpatialHashGrid& grid, const entt::registry& registry,
                                float dt);

    // Phase 2: 位置积分与边界处理 (线程安全: 读 Velocity, 写 Position)
    static void updatePosition(entt::entity entity, Position& pos, Velocity& vel, 
                             float dt, int worldWidth, int worldHeight);

    // Update all entities sequentially (fallback or simple usage)
    static void updateAll(entt::registry& registry, float dt, int screenWidth, int screenHeight, NoMoreDay::systems::SpatialHashGrid& grid);
};
