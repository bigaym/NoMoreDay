#include "AISystem.hpp"
#include "../tools/Logger.hpp"
#include <algorithm>
#include <cmath>

float AISystem::distance(const Position& a, const Position& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

entt::entity AISystem::findNearestTarget(entt::registry& registry, 
                                        const Position& sourcePos, 
                                        float maxRange, 
                                        entt::entity exclude) {
    entt::entity nearest = entt::null;
    float nearestDist = maxRange;
    
    // 查找最近的玩家（或敌对实体）
    auto playerView = registry.view<PlayerTag, Position>();
    for (auto entity : playerView) {
        if (entity == exclude) continue;
        
        const auto& pos = playerView.get<Position>(entity);
        float dist = distance(sourcePos, pos);
        
        if (dist < nearestDist) {
            nearest = entity;
            nearestDist = dist;
        }
    }
    
    return nearest;
}

void AISystem::updateAIEntity(entt::registry& registry, 
                             entt::entity entity, 
                             AIComponent& ai, 
                             Position& pos, 
                             Velocity& vel, 
                             const Position& playerPos, 
                             float dt) {
    
    // 更新决策计时器
    ai.lastDecisionTime += dt;
    
    // 基于AI类型执行不同行为
    switch (ai.aiType) {
        case AIType::IDLE: {
            // 闲置状态 - 随机移动或保持不动
            vel.vx = 0.0f;
            vel.vy = 0.0f;
            
            // 检查是否有目标进入检测范围
            if (ai.lastDecisionTime >= ai.decisionInterval) {
                entt::entity target = findNearestTarget(registry, pos, ai.detectionRange, entity);
                if (target != entt::null) {
                    ai.target = target;
                    ai.aiType = AIType::CHASE;
                }
                ai.lastDecisionTime = 0.0f;
            }
            break;
        }
        
        case AIType::PATROL: {
            // 巡逻状态 - 在两个点之间移动
            Position targetPos = ai.patrolDirection ? ai.patrolEnd : ai.patrolStart;
            float distToTarget = distance(pos, targetPos);
            
            if (distToTarget < 10.0f) {
                // 到达目标点，转向另一个点
                ai.patrolDirection = !ai.patrolDirection;
                targetPos = ai.patrolDirection ? ai.patrolEnd : ai.patrolStart;
            }
            
            // 朝目标点移动
            float dx = targetPos.x - pos.x;
            float dy = targetPos.y - pos.y;
            float length = std::sqrt(dx * dx + dy * dy);
            
            if (length > 0.0f) {
                vel.vx = (dx / length) * ai.speed;
                vel.vy = (dy / length) * ai.speed;
            } else {
                vel.vx = 0.0f;
                vel.vy = 0.0f;
            }
            
            // 检查是否有目标进入检测范围
            if (ai.lastDecisionTime >= ai.decisionInterval) {
                entt::entity target = findNearestTarget(registry, pos, ai.detectionRange, entity);
                if (target != entt::null) {
                    ai.target = target;
                    ai.aiType = AIType::CHASE;
                }
                ai.lastDecisionTime = 0.0f;
            }
            break;
        }
        
        case AIType::CHASE: {
            // 追击状态 - 寻找并接近目标
            if (ai.target == entt::null || !registry.valid(ai.target)) {
                // 目标无效，寻找新目标
                ai.target = findNearestTarget(registry, pos, ai.detectionRange, entity);
                if (ai.target == entt::null) {
                    ai.aiType = AIType::IDLE;
                    vel.vx = 0.0f;
                    vel.vy = 0.0f;
                    break;
                }
            }
            
            // 获取目标位置
            if (registry.all_of<Position>(ai.target)) {
                const auto& targetPos = registry.get<Position>(ai.target);
                float distToTarget = distance(pos, targetPos);
                
                if (distToTarget <= ai.attackRange) {
                    // 进入攻击范围，切换到攻击状态
                    ai.aiType = AIType::ATTACK;
                    vel.vx = 0.0f;
                    vel.vy = 0.0f;
                } else {
                    // 朝目标移动
                    float dx = targetPos.x - pos.x;
                    float dy = targetPos.y - pos.y;
                    float length = std::sqrt(dx * dx + dy * dy);
                    
                    if (length > 0.0f) {
                        vel.vx = (dx / length) * ai.speed;
                        vel.vy = (dy / length) * ai.speed;
                    } else {
                        vel.vx = 0.0f;
                        vel.vy = 0.0f;
                    }
                }
            } else {
                // 目标没有位置组件，寻找新目标
                ai.target = findNearestTarget(registry, pos, ai.detectionRange, entity);
                if (ai.target == entt::null) {
                    ai.aiType = AIType::IDLE;
                    vel.vx = 0.0f;
                    vel.vy = 0.0f;
                }
            }
            break;
        }
        
        case AIType::ATTACK: {
            // 攻击状态 - 保持在攻击范围内并尝试攻击
            if (ai.target == entt::null || !registry.valid(ai.target)) {
                // 目标无效，寻找新目标
                ai.target = findNearestTarget(registry, pos, ai.detectionRange, entity);
                if (ai.target == entt::null) {
                    ai.aiType = AIType::IDLE;
                    vel.vx = 0.0f;
                    vel.vy = 0.0f;
                    break;
                }
            }
            
            // 获取目标位置
            if (registry.all_of<Position>(ai.target)) {
                const auto& targetPos = registry.get<Position>(ai.target);
                float distToTarget = distance(pos, targetPos);
                
                if (distToTarget > ai.attackRange) {
                    // 目标超出范围，切换回追击状态
                    ai.aiType = AIType::CHASE;
                } else {
                    // 保持在攻击范围内，可以添加攻击逻辑
                    vel.vx = 0.0f;
                    vel.vy = 0.0f;
                    
                    // 这里可以添加攻击逻辑，比如创建投射物或触发近战攻击
                    // 暂时留空，后续可以扩展
                }
            } else {
                // 目标没有位置组件，寻找新目标
                ai.target = findNearestTarget(registry, pos, ai.detectionRange, entity);
                if (ai.target == entt::null) {
                    ai.aiType = AIType::IDLE;
                    vel.vx = 0.0f;
                    vel.vy = 0.0f;
                }
            }
            break;
        }
        
        case AIType::FLEE: {
            // 逃跑状态 - 远离目标
            if (ai.target == entt::null || !registry.valid(ai.target)) {
                ai.aiType = AIType::IDLE;
                vel.vx = 0.0f;
                vel.vy = 0.0f;
                break;
            }
            
            if (registry.all_of<Position>(ai.target)) {
                const auto& targetPos = registry.get<Position>(ai.target);
                
                // 朝远离目标的方向移动
                float dx = pos.x - targetPos.x;
                float dy = pos.y - targetPos.y;
                float length = std::sqrt(dx * dx + dy * dy);
                
                if (length > 0.0f) {
                    vel.vx = (dx / length) * ai.speed;
                    vel.vy = (dy / length) * ai.speed;
                } else {
                    vel.vx = 0.0f;
                    vel.vy = 0.0f;
                }
            } else {
                ai.aiType = AIType::IDLE;
                vel.vx = 0.0f;
                vel.vy = 0.0f;
            }
            break;
        }
    }
}

void AISystem::update(entt::registry& registry, systems::SpatialHashGrid& grid, const Position& playerPos, float dt) {
    // 更新所有带有AI组件和EnemyTag的实体
    auto aiView = registry.view<AIComponent, Position, Velocity, EnemyTag>();
    
    for (auto entity : aiView) {
        auto& ai = aiView.get<AIComponent>(entity);
        auto& pos = aiView.get<Position>(entity);
        auto& vel = aiView.get<Velocity>(entity);
        
        updateAIEntity(registry, entity, ai, pos, vel, playerPos, dt);
    }
}