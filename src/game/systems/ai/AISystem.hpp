#pragma once
#include <entt/entt.hpp>
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/world/MapSystem.hpp"
#include <cmath>
#include <vector>
#include "raylib.h"

class AISystem {
public:
    // 更新所有AI实体
    static void update(entt::registry& registry, NoMoreDay::systems::SpatialHashGrid& grid, const MapSystem& mapSystem, const Position& playerPos, float dt);
    
private:
    static float distance(const Position& a, const Position& b);
    // 计算两点之间的距离
    
    // 查找最近的玩家或目标
    static entt::entity findNearestTarget(entt::registry& registry, 
                                         const Position& sourcePos, 
                                         float maxRange, 
                                         entt::entity exclude = entt::null);
    
    // 更新单个AI实体
    static void updateAIEntity(entt::registry& registry, 
                              entt::entity entity, 
                              AIComponent& ai, 
                              Position& pos, 
                              Velocity& vel,
                              const NoMoreDay::systems::SpatialHashGrid& grid,
                              const MapSystem& mapSystem,
                              const Position& playerPos, 
                              const std::vector<Vector2>& flowField,
                              Vector2 gridOrigin,
                              int gridW, int gridH,
                              float cellSize,
                              float dt);
};