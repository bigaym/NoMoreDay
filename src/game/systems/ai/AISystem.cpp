#include "game/systems/ai/AISystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/EliteModifierComponents.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/NemesisComponent.hpp"
#include "game/registry/GroupRegistry.hpp"
#include "game/systems/ai/EnemyAIBehaviors.hpp"
#include <algorithm>
#include <cmath>

float AISystem::distance(const Position &a, const Position &b) {
  float dx = a.x - b.x;
  float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

entt::entity AISystem::findNearestTarget(entt::registry &registry,
                                         const Position &sourcePos,
                                         float maxRange, entt::entity exclude) {
  entt::entity nearest = entt::null;
  float nearestDist = maxRange;

  // 查找最近的玩家（或敌对实体）
  auto playerView = registry.view<PlayerTag, Position>();
  for (auto entity : playerView) {
    if (entity == exclude)
      continue;

    const auto &pos = playerView.get<Position>(entity);
    float dist = distance(sourcePos, pos);

    if (dist < nearestDist) {
      nearest = entity;
      nearestDist = dist;
    }
  }

  return nearest;
}

// 辅助函数：快速获取符号
inline float sgn(float x) { return (x > 0) ? 1.0f : ((x < 0) ? -1.0f : 0.0f); }

void AISystem::updateAIEntity(entt::registry &registry, entt::entity entity,
                              AIComponent &ai, Position &pos, Velocity &vel,
                              const NoMoreDay::systems::SpatialHashGrid &grid,
                              const MapSystem &mapSystem,
                              const Position &playerPos,
                              const std::vector<Vector2> &flowField,
                              Vector2 gridOrigin, int gridW, int gridH,
                              float cellSize, float dt) {
  using namespace NoMoreDay::Constants::AI;

  // 更新决策计时器
  ai.lastDecisionTime += dt;

  // --- 全局状态管理 (Leashing / Reset) ---
  const EnemyStateComponent *stateComp =
      registry.try_get<EnemyStateComponent>(entity);
  HealthComponent *health = registry.try_get<HealthComponent>(entity);

  if (stateComp) {
    float dx = pos.x - playerPos.x;
    float dy = pos.y - playerPos.y;
    float distSq = dx * dx + dy * dy;

    // TODO: Remove debug logging after fixing aggro
    if (ai.aiType == AIType::CHASE && distSq > 400000.0f) { // > 630 pixels
      LOG_DEBUG("DEBUG AGGRO: Entity {} sticking in CHASE at dist {:.1f}. "
                "limit: {:.1f}",
                (uint32_t)entity, std::sqrt(distSq),
                stateComp->deactivationRange);
    }

    // 脱战与重置逻辑
    using namespace NoMoreDay::Constants::AI;
    float leashRangeSq =
        stateComp->deactivationRange * stateComp->deactivationRange;
    float hardResetRangeSq = leashRangeSq * LEASH_RESET_MULTIPLIER;
    float wakeUpRangeSq =
        stateComp->activationRange * stateComp->activationRange;

    // 1. 强制传送逻辑 (Hard Reset)
    if (distSq > hardResetRangeSq) {
      if (ai.aiType != AIType::NEMESIS_HUNTER) {
        // LOG_DEBUG("Entity {} Hard Reset (Dist: {:.1f})", (uint32_t)entity,
        // std::sqrt(distSq));
        pos = ai.patrolStart;
        ai.aiType = AIType::IDLE;
        ai.target = entt::null;
        vel.vx = 0.0f;
        vel.vy = 0.0f;
        if (health)
          health->current = health->max;
        return;
      }
    }
    // 2. 脱战逻辑 (Leashing)
    else if (distSq > leashRangeSq) {
      if (ai.aiType == AIType::CHASE || ai.aiType == AIType::ATTACK ||
          ai.aiType == AIType::NEMESIS_HUNTER ||
          ai.aiType == AIType::TANK_BLOCK ||
          ai.aiType == AIType::ASSASSIN_STEALTH ||
          ai.aiType == AIType::SUPPORT_FLEE_BUFF) {
        if (ai.aiType != AIType::NEMESIS_HUNTER) { // Nemesis 不脱战
          // LOG_DEBUG("Entity {} Leashed (Dist: {:.1f})", (uint32_t)entity,
          // std::sqrt(distSq));
          ai.aiType = AIType::PATROL;
          ai.target = entt::null;
          // Reset specialized states if needed
          if (ai.aiType == AIType::ASSASSIN_STEALTH) {
            registry.remove<NoMoreDay::StealthedTag>(entity);
            ai.isStealthed = false;
          }
        }
      }
      if (health && health->current < health->max * HEALTH_REGEN_THRESHOLD) {
        health->current += health->max * HEALTH_REGEN_PER_SEC * dt;
        if (health->current > health->max)
          health->current = health->max;
      }
    }
    // 3. 激活逻辑 (Wake Up)
    else if (distSq < wakeUpRangeSq) {
      if (ai.aiType == AIType::IDLE) {
        ai.aiType = AIType::PATROL;
      }
    }
  }

  // Common Chase Logic using Flow Field
  auto applyFlowFieldCheck = [&](float speed) {
    // 2.1 坐标映射 (Coordinate Mapping)
    int gx = (int)((pos.x - gridOrigin.x) / cellSize);
    int gy = (int)((pos.y - gridOrigin.y) / cellSize);

    bool hasFlow = false;
    Vector2 flow = {0.0f, 0.0f};

    if (gx >= 0 && gx < gridW && gy >= 0 && gy < gridH) {
      int index = gy * gridW + gx;
      if (index < (int)flowField.size()) {
        flow = flowField[index];
        // 若采样到的 Vector2 模长 > Threshold (有效指引)
        if (std::abs(flow.x) > NORMALIZE_THRESHOLD ||
            std::abs(flow.y) > NORMALIZE_THRESHOLD) {
          hasFlow = true;
        }
      }
    }

    if (hasFlow) {
      // 采样模式 (Exclusive): 直接读取 SSBO，不计算避障
      vel.vx = flow.x * speed;
      vel.vy = flow.y * speed;
    } else {
      // 若采样到的模长为 0 (障碍物或无效区)
      // 实体保持当前速度衰减 (Friction)
      vel.vx *= FRICTION; // Rapid validation friction
      vel.vy *= FRICTION;
    }
  };

  // 基于AI类型执行不同行为
  switch (ai.aiType) {
  case AIType::IDLE: {
    vel.vx = 0.0f;
    vel.vy = 0.0f;
    if (ai.lastDecisionTime >= ai.decisionInterval) {
      ai.target = entt::null;
      entt::entity target =
          findNearestTarget(registry, pos, ai.detectionRange, entity);
      if (target != entt::null) {
        ai.target = target;
        // Determine state based on archetype
        if (stateComp) {
          switch (stateComp->archetypeType) {
          case EnemyArchetype::TANK:
            ai.aiType = AIType::TANK_BLOCK;
            break;
          case EnemyArchetype::ASSASSIN:
            ai.aiType = AIType::ASSASSIN_STEALTH;
            break;
          case EnemyArchetype::SUPPORT:
            ai.aiType = AIType::SUPPORT_FLEE_BUFF;
            break;
          default:
            ai.aiType = AIType::CHASE;
            break;
          }
        } else {
          ai.aiType =
              AIType::PATROL; // Fallback to PATROL if no state component
        }
      }
      ai.lastDecisionTime = 0.0f;
    }
    break;
  }

  case AIType::PATROL: {
    // 0. Wait Logic
    if (ai.stateTimer > 0.0f) {
      ai.stateTimer -= dt;
      vel.vx = 0.0f;
      vel.vy = 0.0f;
      break;
    }

    // 巡逻逻辑保持不变 (MapSystem A* or Simple)
    Position targetPos = ai.patrolDirection ? ai.patrolEnd : ai.patrolStart;
    Position nextStep = mapSystem.getPathNextStep(pos, targetPos);
    float dx = nextStep.x - pos.x;
    float dy = nextStep.y - pos.y;
    float distToNext = std::sqrt(dx * dx + dy * dy);
    float distToTarget = distance(pos, targetPos);

    if (distToTarget < NoMoreDay::Constants::AI::Patrol::ARRIVAL_DIST) {
      ai.patrolDirection = !ai.patrolDirection;
      ai.stateTimer = NoMoreDay::Constants::AI::Patrol::WAIT_TIME;
      vel.vx = 0;
      vel.vy = 0;
    } else if (distToNext > NoMoreDay::Constants::AI::Patrol::MIN_STEP_DIST) {
      float patrolSpeed = NoMoreDay::Constants::AI::Patrol::SPEED;
      vel.vx = (dx / distToNext) * patrolSpeed;
      vel.vy = (dy / distToNext) * patrolSpeed;
    }

    if (ai.lastDecisionTime >= ai.decisionInterval) {
      ai.target = entt::null;
      entt::entity target =
          findNearestTarget(registry, pos, ai.detectionRange, entity);
      if (target != entt::null) {
        ai.target = target;
        // Determine state based on archetype
        if (stateComp) {
          switch (stateComp->archetypeType) {
          case EnemyArchetype::TANK:
            ai.aiType = AIType::TANK_BLOCK;
            break;
          case EnemyArchetype::ASSASSIN:
            ai.aiType = AIType::ASSASSIN_STEALTH;
            break;
          case EnemyArchetype::SUPPORT:
            ai.aiType = AIType::SUPPORT_FLEE_BUFF;
            break;
          default:
            ai.aiType = AIType::CHASE;
            break;
          }
        } else {
          ai.aiType =
              AIType::PATROL; // Fallback to PATROL if no state component
        }
      }
      ai.lastDecisionTime = 0.0f;
    }
    break;
  }

  case AIType::CHASE: {
    if (ai.target == entt::null || !registry.valid(ai.target)) {
      ai.target = findNearestTarget(registry, pos, ai.detectionRange, entity);
      if (ai.target == entt::null) {
        ai.aiType = AIType::IDLE;
        vel.vx = 0.0f;
        vel.vy = 0.0f;
        break;
      }
    }

    // Check attack range
    if (registry.all_of<Position>(ai.target)) {
      const auto &targetPos = registry.get<Position>(ai.target);
      if (distance(pos, targetPos) <= ai.attackRange) {
        ai.aiType = AIType::ATTACK;
        vel.vx = 0.0f;
        vel.vy = 0.0f;
        break;
      }
    }

    // Spec 2.2: Flow Field Drive (No Separation)
    using namespace NoMoreDay::Constants::AI;
    float chaseSpeed = stateComp ? stateComp->speed : Chase::SPEED_FALLBACK;
    applyFlowFieldCheck(chaseSpeed);
    break;
  }

  case AIType::ATTACK: {
    if (ai.target == entt::null || !registry.valid(ai.target)) {
      ai.aiType = AIType::CHASE;
      break;
    }
    if (registry.all_of<Position>(ai.target)) {
      const auto &targetPos = registry.get<Position>(ai.target);
      if (distance(pos, targetPos) > ai.attackRange * Chase::ATTACK_EXIT_MULT) {
        ai.aiType = AIType::CHASE;
      } else {
        vel.vx = 0.0f;
        vel.vy = 0.0f;
      }
    } else {
      ai.aiType = AIType::CHASE;
    }
    break;
  }

  case AIType::FLEE: {
    // Flee remains simple vector based for now, or could use negative flow?
    // For now, keep existing logic but simplified
    if (registry.valid(ai.target) && registry.all_of<Position>(ai.target)) {
      const auto &targetPos = registry.get<Position>(ai.target);
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

  case AIType::NEMESIS_HUNTER: {
    using namespace NoMoreDay::Constants::AI;
    float hunterSpeed = ai.speed * Chase::HUNTER_SPEED_MULT;
    // Check attack range
    if (distance(pos, playerPos) <= ai.attackRange) {
      vel.vx = 0.0f;
      vel.vy = 0.0f;
    } else {
      // Spec 2.2: Flow Field Drive (No Separation)
      applyFlowFieldCheck(hunterSpeed);
    }
    break;
  }

  case AIType::SUPPORT_FLEE_BUFF:
    NoMoreDay::AI::UpdateSupportBehavior(registry, entity, ai, pos, vel,
                                         playerPos, dt);
    break;
  case AIType::ASSASSIN_STEALTH:
    NoMoreDay::AI::UpdateAssassinBehavior(registry, entity, ai, pos, vel,
                                          mapSystem, playerPos, dt);
    break;
  case AIType::TANK_BLOCK:
    NoMoreDay::AI::UpdateTankBehavior(registry, entity, ai, pos, vel, playerPos,
                                      dt);
    break;
  }
}

void AISystem::update(entt::registry &registry,
                      NoMoreDay::systems::SpatialHashGrid &grid,
                      const MapSystem &mapSystem, const Position &playerPos,
                      float dt) {
  auto &flowSystem = NoMoreDay::systems::GPUFlowFieldSystem::Get();

  // Spec 1.0: CPU Time Constraint - Sync from GPU
  flowSystem.SyncToCPU();
  const std::vector<Vector2> &field = flowSystem.GetFlowFieldCPU();

  Vector2 origin = flowSystem.GetGridOrigin();
  int gridW = flowSystem.GetWidth();
  int gridH = flowSystem.GetHeight();
  float cellSize =
      NoMoreDay::Constants::AI::FLOW_CELL_SIZE; // Should match
                                                // flowSystem.m_cellSize

  // Exclude DormantTag (Spec 3.0) and KilledTag
  // Phase 2 Optimization: Use AIGroup for linear memory access
  auto group =
      registry.group<AIComponent>(entt::get<Position, Velocity, EnemyTag>);

  for (auto entity : group) {
    if (registry.any_of<KilledTag, DormantTag>(entity))
      continue;

    auto &ai = group.get<AIComponent>(entity);
    auto &pos = group.get<Position>(entity);
    auto &vel = group.get<Velocity>(entity);

    // --- OPTIMIZATION: AI Throttling based on distance ---
    float dx = pos.x - playerPos.x;
    float dy = pos.y - playerPos.y;
    float distSq = dx * dx + dy * dy;

    // 1. Culling & Dormancy (Spec 2.3)
    using namespace NoMoreDay::Constants::AI;
    bool isNemesis = registry.any_of<NoMoreDay::NemesisTag>(entity);
    if (!isNemesis && distSq > DORMANCY_THRESHOLD * DORMANCY_THRESHOLD) {
      // Enter Dormancy
      registry.emplace_or_replace<DormantTag>(entity);
      registry.remove<Velocity>(entity);
      // Teleport to holding area
      pos.x = DORMANCY_TELEPORT_COORD;
      pos.y = DORMANCY_TELEPORT_COORD;
      continue;
    }

    // 2. Frame-rate independent throttling using time accumulator:
    using namespace NoMoreDay::Constants::AI;
    float updateInterval = 0.0f;
    if (distSq > UPDATE_DIST_FAR * UPDATE_DIST_FAR) {
      updateInterval = UPDATE_INTERVAL_FAR;
    } else if (distSq > UPDATE_DIST_MEDIUM * UPDATE_DIST_MEDIUM) {
      updateInterval = UPDATE_INTERVAL_MEDIUM;
    }

    ai.updateAccumulator += dt;
    if (ai.updateAccumulator >= updateInterval) {
      // Pass dt as accumulator if we want to simulate jumped time,
      // but usually for movement, we just update current state.
      // But here we'll pass dt.
      updateAIEntity(registry, entity, ai, pos, vel, grid, mapSystem, playerPos,
                     field, origin, gridW, gridH, cellSize, dt);
      ai.updateAccumulator = 0.0f;
    }
  }
}
