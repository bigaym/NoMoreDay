#include "AISystem.hpp"
#include "../tools/Logger.hpp"
#include "../components/EnemyComponent.hpp"
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
                             const MapSystem& mapSystem,
                             const Position& playerPos, 
                             float dt) {
    
    // 更新决策计时器
    ai.lastDecisionTime += dt;

    // --- 全局状态管理 (Leashing / Reset) ---
    // 检查是否有关联的 EnemyStateComponent (用于获取脱战范围等)
    const EnemyStateComponent* stateComp = registry.try_get<EnemyStateComponent>(entity);
    HealthComponent* health = registry.try_get<HealthComponent>(entity);
    
    if (stateComp) {
        float dx = pos.x - playerPos.x;
        float dy = pos.y - playerPos.y;
        float distSq = dx*dx + dy*dy;
        
        float leashRangeSq = stateComp->deactivationRange * stateComp->deactivationRange;
        float hardResetRangeSq = leashRangeSq * 4.0f;
        float wakeUpRangeSq = stateComp->activationRange * stateComp->activationRange;

        // 1. 强制传送逻辑
        if (distSq > hardResetRangeSq) {
            pos = ai.patrolStart; // 传送回出生点
            ai.aiType = AIType::IDLE;
            ai.target = entt::null;
            vel.vx = 0.0f; vel.vy = 0.0f;
            if (health) health->current = health->max;
            return; // 传送后本帧不移动
        }
        // 2. 脱战逻辑
        else if (distSq > leashRangeSq) {
             if (ai.aiType == AIType::CHASE || ai.aiType == AIType::ATTACK) {
                LOG_DEBUG("AI entity {} leashing: switching to PATROL", (uint32_t)entity);
                ai.aiType = AIType::PATROL;
                ai.target = entt::null;
            }
            // 回血
            if (health && health->current < health->max * 0.95f) {
                health->current += health->max * 0.10f * dt;
                if (health->current > health->max) health->current = health->max;
            }
        }
        // 3. 激活逻辑
        else if (distSq < wakeUpRangeSq) {
             if (ai.aiType == AIType::IDLE) {
                ai.aiType = AIType::PATROL;
            }
        }
    }
    
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
                    LOG_DEBUG("AI entity {} found target {} in IDLE, switching to CHASE", (uint32_t)entity, (uint32_t)target);
                    ai.target = target;
                    ai.aiType = AIType::CHASE;
                }
                ai.lastDecisionTime = 0.0f;
            }
            break;
        }
        
        case AIType::PATROL: {
            // 巡逻状态 - 使用 A* 路径或简单直线返回巡逻点
            Position targetPos = ai.patrolDirection ? ai.patrolEnd : ai.patrolStart;
            
            // 使用 MapSystem 的简单寻路获取下一步
            Position nextStep = mapSystem.getPathNextStep(pos, targetPos);
            
            float dx = nextStep.x - pos.x;
            float dy = nextStep.y - pos.y;
            float distToNext = std::sqrt(dx*dx + dy*dy);
            
            // 检查是否到达当前巡逻目标点
            float distToTarget = distance(pos, targetPos);
            if (distToTarget < 10.0f) {
                // LOG_TRACE("AI entity {} reached patrol point, switching direction", (uint32_t)entity);
                ai.patrolDirection = !ai.patrolDirection;
                vel.vx = 0; vel.vy = 0;
            } else if (distToNext > 0.1f) {
                // 移动向下一步
                vel.vx = (dx / distToNext) * ai.speed * 0.5f; // 巡逻速度慢
                vel.vy = (dy / distToNext) * ai.speed * 0.5f;
            }

            // 检查是否有目标进入检测范围
            if (ai.lastDecisionTime >= ai.decisionInterval) {
                entt::entity target = findNearestTarget(registry, pos, ai.detectionRange, entity);
                if (target != entt::null) {
                    // LOG_DEBUG("AI entity {} found target {} during PATROL, switching to CHASE", (uint32_t)entity, (uint32_t)target);
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
                LOG_DEBUG("AI entity {} target lost or invalid, searching for new target", (uint32_t)entity);
                ai.target = findNearestTarget(registry, pos, ai.detectionRange, entity);
                if (ai.target == entt::null) {
                    ai.aiType = AIType::IDLE;
                    LOG_DEBUG("AI entity {} no target found, returning to IDLE", (uint32_t)entity);
                    vel.vx = 0.0f; vel.vy = 0.0f;
                    break;
                }
            }
            
            // 获取目标位置
            if (registry.all_of<Position>(ai.target)) {
                const auto& targetPos = registry.get<Position>(ai.target);
                float distToTarget = distance(pos, targetPos);
                
                if (distToTarget <= ai.attackRange) {
                    // 进入攻击范围，切换到攻击状态
                    // LOG_DEBUG("AI entity {} in range of target {}, switching to ATTACK", (uint32_t)entity, (uint32_t)ai.target);
                    ai.aiType = AIType::ATTACK;
                    vel.vx = 0.0f;
                    vel.vy = 0.0f;
                } else {
                    // --- 核心修改：使用 MapSystem 的流场进行智能寻路 ---
                    Vector2 flow = mapSystem.getFlowDirection(pos);
                    
                    if (flow.x != 0 || flow.y != 0) {
                        vel.vx = flow.x * ai.speed;
                        vel.vy = flow.y * ai.speed;
                    } else {
                        // 流场无效（例如目标在墙里或距离太远），回退到直线追踪
                        LOG_LIMITED_WARN(2.0f, "AI entity {} flow field invalid at ({:.1f}, {:.1f}), falling back to direct chase", (uint32_t)entity, pos.x, pos.y);
                        
                        float dx = targetPos.x - pos.x;
                        float dy = targetPos.y - pos.y;
                        float length = std::sqrt(dx * dx + dy * dy);
                        
                        if (length > 0.0f) {
                            vel.vx = (dx / length) * ai.speed;
                            vel.vy = (dy / length) * ai.speed;
                        }
                    }
                }
            } else {
                ai.target = entt::null;
            }
            break;
        }
        
        case AIType::ATTACK: {
            // 攻击状态
            if (ai.target == entt::null || !registry.valid(ai.target)) {
                ai.aiType = AIType::CHASE;
                break;
            }
            
            if (registry.all_of<Position>(ai.target)) {
                const auto& targetPos = registry.get<Position>(ai.target);
                float distToTarget = distance(pos, targetPos);
                
                if (distToTarget > ai.attackRange * 1.2f) { // 稍微增加一点缓冲防止抖动
                    // LOG_DEBUG("AI entity {} target moved out of range, switching back to CHASE", (uint32_t)entity);
                    ai.aiType = AIType::CHASE;
                } else {
                    vel.vx = 0.0f;
                    vel.vy = 0.0f;
                    // 实际攻击逻辑由 CombatSystem 处理，这里只负责状态和移动
                }
            } else {
                ai.aiType = AIType::CHASE;
            }
            break;
        }
        
        case AIType::FLEE: {
            // 逃跑状态 (简化为直线远离)
            if (ai.target != entt::null && registry.valid(ai.target) && registry.all_of<Position>(ai.target)) {
                const auto& targetPos = registry.get<Position>(ai.target);
                float dx = pos.x - targetPos.x;
                float dy = pos.y - targetPos.y;
                float length = std::sqrt(dx * dx + dy * dy);
                
                if (length > 0.0f) {
                    vel.vx = (dx / length) * ai.speed;
                    vel.vy = (dy / length) * ai.speed;
                }
            } else {
                ai.aiType = AIType::IDLE;
            }
            break;
        }
    }
}

void AISystem::update(entt::registry& registry, systems::SpatialHashGrid& grid, const MapSystem& mapSystem, const Position& playerPos, float dt) {
    // LOG_TRACE("AISystem::update: processing AI logic for frame");
    
    auto aiView = registry.view<AIComponent, Position, Velocity, EnemyTag>();
    
    for (auto entity : aiView) {
        auto& ai = aiView.get<AIComponent>(entity);
        auto& pos = aiView.get<Position>(entity);
        auto& vel = aiView.get<Velocity>(entity);
        
        updateAIEntity(registry, entity, ai, pos, vel, mapSystem, playerPos, dt);
    }
}