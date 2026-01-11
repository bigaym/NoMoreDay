#include "game/systems/ai/AISystem.hpp"
#include "core/logging/Logger.hpp"
#include "game/components/EnemyComponent.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
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
                             const NoMoreDay::systems::SpatialHashGrid& grid,
                             const MapSystem& mapSystem,
                             const Position& playerPos, 
                             const std::vector<Vector2>& flowField,
                             Vector2 gridOrigin,
                             int gridW, int gridH,
                             float cellSize,
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
            // 回回血
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
                // 移动向下一步 - 游荡速度固定为 20
                float patrolSpeed = 20.0f;
                vel.vx = (dx / distToNext) * patrolSpeed;
                vel.vy = (dy / distToNext) * patrolSpeed;
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
                    // --- 核心修改：使用 GPUFlowFieldSystem 的流场进行寻路 ---
                    Vector2 flow = { 0, 0 };
                    
                    int gx = (int)((pos.x - gridOrigin.x) / cellSize);
                    int gy = (int)((pos.y - gridOrigin.y) / cellSize);
                    
                    if (gx >= 0 && gx < gridW && gy >= 0 && gy < gridH) {
                        flow = flowField[gy * gridW + gx];
                    }

                    // --- 局部回避 (Separation) ---
                    Vector2 separation = { 0, 0 };
                    float searchRadius = 35.0f;
                    int count = 0;

                    const_cast<NoMoreDay::systems::SpatialHashGrid&>(grid).query(pos, searchRadius, [&](entt::entity neighbor) {
                        if (neighbor == entity) return;
                        
                        const auto& nPos = registry.get<Position>(neighbor);
                        float dx = pos.x - nPos.x;
                        float dy = pos.y - nPos.y;
                        float dSq = dx*dx + dy*dy;
                        
                        if (dSq > 0.01f && dSq < searchRadius * searchRadius) {
                            float d = std::sqrt(dSq);
                            separation.x += dx / d;
                            separation.y += dy / d;
                            count++;
                        }
                    });

                    float chaseSpeed = 50.0f; // Default speed (1/2 of Player Base Speed 100.0f)
                    if (stateComp) {
                        chaseSpeed = stateComp->speed;
                    }

                    if (flow.x != 0 || flow.y != 0) {
                        // 融合流场与回避
                        Vector2 finalDir = flow;
                        if (count > 0) {
                            separation.x /= count;
                            separation.y /= count;
                            // 权重：流场 0.7, 回避 0.3
                            finalDir.x = flow.x * 0.7f + separation.x * 0.3f;
                            finalDir.y = flow.y * 0.7f + separation.y * 0.3f;
                            
                            float mag = std::sqrt(finalDir.x*finalDir.x + finalDir.y*finalDir.y);
                            if (mag > 0.01f) {
                                finalDir.x /= mag;
                                finalDir.y /= mag;
                            }
                        }

                        vel.vx = finalDir.x * chaseSpeed;
                        vel.vy = finalDir.y * chaseSpeed;
                    } else {
                        // 流场无效（例如在网格外部），回退到直线追踪
                        float dx = targetPos.x - pos.x;
                        float dy = targetPos.y - pos.y;
                        float length = std::sqrt(dx * dx + dy * dy);
                        
                        if (length > 0.0f) {
                            Vector2 finalDir = { dx / length, dy / length };
                            if (count > 0) {
                                separation.x /= count;
                                separation.y /= count;
                                finalDir.x = finalDir.x * 0.7f + separation.x * 0.3f;
                                finalDir.y = finalDir.y * 0.7f + separation.y * 0.3f;
                                
                                float mag = std::sqrt(finalDir.x*finalDir.x + finalDir.y*finalDir.y);
                                if (mag > 0.01f) {
                                    finalDir.x /= mag;
                                    finalDir.y /= mag;
                                }
                            }
                            vel.vx = finalDir.x * chaseSpeed;
                            vel.vy = finalDir.y * chaseSpeed;
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

void AISystem::update(entt::registry& registry, NoMoreDay::systems::SpatialHashGrid& grid, const MapSystem& mapSystem, const Position& playerPos, float dt) {
    auto& flowSystem = NoMoreDay::systems::GPUFlowFieldSystem::Get();
    std::vector<Vector2> field = flowSystem.DownloadFlowField();
    Vector2 origin = flowSystem.GetGridOrigin();
    int gridW = flowSystem.GetWidth();
    int gridH = flowSystem.GetHeight();
    float cellSize = 10.0f; 

    auto aiView = registry.view<AIComponent, Position, Velocity, EnemyTag>(entt::exclude<KilledTag>);

    for (auto entity : aiView) {
        auto& ai = aiView.get<AIComponent>(entity);
        auto& pos = aiView.get<Position>(entity);
        auto& vel = aiView.get<Velocity>(entity);
        
        // --- OPTIMIZATION: AI Throttling based on distance ---
        float dx = pos.x - playerPos.x;
        float dy = pos.y - playerPos.y;
        float distSq = dx*dx + dy*dy;

        // 1. Culling: Very far enemies stop AI completely (e.g., > 1200 units)
        if (distSq > 1200.0f * 1200.0f) {
            vel.vx = 0; vel.vy = 0;
            continue;
        }

        // 2. Frame-rate independent throttling using time accumulator:
        // - Close (< 400): Every frame (~0s interval)
        // - Medium (400 - 800): ~0.033s (2 frames at 60 FPS)
        // - Far (> 800): ~0.083s (5 frames at 60 FPS)
        float updateInterval = 0.0f;
        if (distSq > 800.0f * 800.0f) {
            updateInterval = 0.083f;  // ~5 frames at 60 FPS
        } else if (distSq > 400.0f * 400.0f) {
            updateInterval = 0.033f;  // ~2 frames at 60 FPS
        }
        
        ai.updateAccumulator += dt;
        bool shouldUpdate = (ai.updateAccumulator >= updateInterval);
        if (shouldUpdate) {
            ai.updateAccumulator = 0.0f;
        }

        if (shouldUpdate) {
            updateAIEntity(registry, entity, ai, pos, vel, grid, mapSystem, playerPos, field, origin, gridW, gridH, cellSize, dt);
        }
    }
}