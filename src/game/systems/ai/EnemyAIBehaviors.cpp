#include "game/systems/ai/EnemyAIBehaviors.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Combat.hpp"
#include "game/components/EliteModifierComponents.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/ai/AIConstants.hpp"
#include "game/systems/world/TilemapCollisionSystem.hpp"
#include <cmath>

namespace NoMoreDay::AI {

// 辅助函数：计算两点之间的距离
static float Distance(const Position &a, const Position &b) {
  float dx = a.x - b.x;
  float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

// 辅助函数：归一化向量
static void Normalize(float &x, float &y) {
  float len = std::sqrt(x * x + y * y);
  using namespace NoMoreDay::Constants::AI;
  if (len > NORMALIZE_THRESHOLD) {
    x /= len;
    y /= len;
  }
}

// ============================================================================
// SUPPORT 支援者行为
// ============================================================================

void UpdateSupportBehavior(entt::registry &registry, entt::entity entity,
                           AIComponent &ai, Position &pos, Velocity &vel,
                           const Position &playerPos, float dt) {
  float distToPlayer = Distance(pos, playerPos);

  // 更新 Buff 冷却
  if (ai.buffCooldownTimer > 0.0f) {
    ai.buffCooldownTimer -= dt;
  }

  // === 1. 危险距离检查：太近则逃跑 ===
  using namespace NoMoreDay::Constants::AI::Support;
  float minSafeDistance = MIN_SAFE_DIST;
  float maxSafeDistance = MAX_SAFE_DIST;

  if (distToPlayer < minSafeDistance) {
    // 紧急逃跑：直接远离玩家
    float dx = pos.x - playerPos.x;
    float dy = pos.y - playerPos.y;
    Normalize(dx, dy);

    float fleeSpeed = ai.speed * FLEE_SPEED_MULT; // 逃跑时加速
    vel.vx = dx * fleeSpeed;
    vel.vy = dy * fleeSpeed;

    // 逃跑时不施放 Buff
    return;
  }

  // === 2. 位置调整：保持合适距离 ===
  using namespace NoMoreDay::Constants::AI::Support;
  if (distToPlayer > maxSafeDistance) {
    // 太远了，稍微靠近一点
    float dx = playerPos.x - pos.x;
    float dy = playerPos.y - pos.y;
    Normalize(dx, dy);

    float approachSpeed = ai.speed * APPROACH_SPEED_MULT;
    vel.vx = dx * approachSpeed;
    vel.vy = dy * approachSpeed;
  } else if (distToPlayer < ai.preferredDistance) {
    // 有点近，慢慢远离
    float dx = pos.x - playerPos.x;
    float dy = pos.y - playerPos.y;
    Normalize(dx, dy);

    float retreatSpeed = ai.speed * RETREAT_SPEED_MULT;
    vel.vx = dx * retreatSpeed;
    vel.vy = dy * retreatSpeed;
  } else {
    // 距离合适，停下来准备施放 Buff
    vel.vx = 0.0f;
    vel.vy = 0.0f;
  }

  // === 3. Buff 施放逻辑 ===
  if (ai.buffCooldownTimer <= 0.0f) {
    ApplyBuffToNearbyAllies(registry, entity, pos, ai.buffRadius);
    ai.buffCooldownTimer = ai.buffCooldown;

    // 视觉效果：发射水蓝色粒子
    for (int i = 0; i < 20; ++i) {
      NoMoreDay::components::GPUParticle p;
      p.position = {pos.x, pos.y};
      float angle =
          NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 360.0f) * DEG2RAD;
      float speed = NoMoreDay::utils::ThreadSafeRandom::GetFloat(50.0f, 150.0f);
      p.velocity = {cosf(angle) * speed, sinf(angle) * speed};
      p.color = {100, 200, 255, 255}; // 水蓝色
      p.lifetime = 0.8f;
      p.maxLifetime = p.lifetime;
      p.scale = 3.0f;
      NoMoreDay::systems::GPUParticleSystem::Get().Emit(p);
    }

    LOG_DEBUG("Support entity {} cast buff to nearby allies",
              static_cast<uint32_t>(entity));
  }
}

void ApplyBuffToNearbyAllies(entt::registry &registry, entt::entity source,
                             const Position &sourcePos, float radius) {
  // 遍历所有敌人，找到范围内的友军
  auto view = registry.view<EnemyTag, Position, ActiveEffectsComponent>();

  int buffedCount = 0;
  for (auto [ally, allyPos, effects] : view.each()) {
    if (ally == source)
      continue; // 跳过自己

    float dist = Distance(sourcePos, allyPos);
    if (dist <= radius) {
      // 创建 Shield Buff
      using namespace NoMoreDay::Constants::AI::Support;
      BuffEffect shieldBuff;
      shieldBuff.id = "support_shield";
      shieldBuff.name = "Support Shield";
      shieldBuff.description = "Damage reduction from support ally";
      shieldBuff.type = BuffType::Shield;
      shieldBuff.duration = BUFF_DURATION;
      shieldBuff.remaining = BUFF_DURATION;
      shieldBuff.is_debuff = false;
      shieldBuff.source = source;

      // 添加防御加成
      NoMoreDay::StatModifier defMod;
      defMod.type = NoMoreDay::StatType::Armor;
      defMod.mode = NoMoreDay::ModifierMode::PercentAdd;
      defMod.value = ARMOR_MOD_VALUE; // +30% 护甲
      shieldBuff.modifiers.push_back(defMod);

      effects.AddOrRefresh(shieldBuff);
      buffedCount++;
    }
  }

  if (buffedCount > 0) {
    LOG_DEBUG("Support buffed {} allies", buffedCount);
  }
}

// ============================================================================
// ASSASSIN 刺客行为
// ============================================================================

void UpdateAssassinBehavior(entt::registry &registry, entt::entity entity,
                            AIComponent &ai, Position &pos, Velocity &vel,
                            const MapSystem &mapSystem,
                            const Position &playerPos, float dt) {
  float distToPlayer = Distance(pos, playerPos);

  // 更新背刺冷却
  if (ai.backstabCooldownTimer > 0.0f) {
    ai.backstabCooldownTimer -= dt;
  }

  // 更新隐身计时
  if (ai.isStealthed) {
    ai.stealthTimer += dt;
  }

  // === 隐身状态管理 ===
  if (!ai.isStealthed && ai.backstabCooldownTimer <= 0.0f) {
    // 进入隐身状态
    ai.isStealthed = true;
    ai.stealthTimer = 0.0f;

    // 添加隐身标记组件
    if (!registry.any_of<StealthedTag>(entity)) {
      registry.emplace<StealthedTag>(entity);
    }

    LOG_DEBUG("Assassin {} entered stealth", static_cast<uint32_t>(entity));
  }

  // === 行为决策 ===
  using namespace NoMoreDay::Constants::AI::Assassin;
  float backstabDistance = BACKSTAB_DIST; // 背刺触发距离
  float lurkerDistance = LURKER_DIST;     // 潜伏距离

  if (ai.isStealthed) {
    if (distToPlayer <= backstabDistance &&
        ai.stealthTimer >= STEALTH_TIMER_THRESHOLD) {
      // 条件满足：执行背刺
      if (ExecuteBackstab(registry, entity, mapSystem, playerPos,
                          ai.backstabMultiplier)) {
        // 背刺成功
        ai.isStealthed = false;
        ai.backstabCooldownTimer = ai.backstabCooldown;
        registry.remove<StealthedTag>(entity);

        vel.vx = 0.0f;
        vel.vy = 0.0f;

        LOG_INFO("Assassin {} executed backstab!",
                 static_cast<uint32_t>(entity));
      }
    } else if (distToPlayer > lurkerDistance) {
      // 太远，慢慢靠近
      float dx = playerPos.x - pos.x;
      float dy = playerPos.y - pos.y;
      Normalize(dx, dy);

      float stalkSpeed = ai.speed * STALK_SPEED_MULT; // 潜行时慢速移动
      vel.vx = dx * stalkSpeed;
      vel.vy = dy * stalkSpeed;
    } else {
      // 在潜伏距离内，停止移动等待时机
      vel.vx = 0.0f;
      vel.vy = 0.0f;
    }
  } else {
    // 非隐身状态（背刺后冷却中）
    if (distToPlayer < FLEE_DIST) {
      // 太近了，逃跑
      float dx = pos.x - playerPos.x;
      float dy = pos.y - playerPos.y;
      Normalize(dx, dy);

      vel.vx = dx * ai.speed;
      vel.vy = dy * ai.speed;
    } else {
      // 等待冷却结束
      vel.vx = 0.0f;
      vel.vy = 0.0f;
    }
  }
}

bool ExecuteBackstab(entt::registry &registry, entt::entity assassin,
                     const MapSystem &mapSystem, const Position &playerPos,
                     float backstabMultiplier) {
  // 获取刺客位置
  auto *assassinPos = registry.try_get<Position>(assassin);
  if (!assassinPos)
    return false;

  // 0. 获取玩家朝向 (基于速度)
  Vector2 playerFacing = {0.0f, -1.0f};
  auto playerView = registry.view<PlayerTag, Velocity>();
  for (auto player : playerView) {
    const auto &pv = playerView.get<Velocity>(player);
    float speedSq = pv.vx * pv.vx + pv.vy * pv.vy;
    if (speedSq > 0.001f) {
      float speed = std::sqrt(speedSq);
      playerFacing = {pv.vx / speed, pv.vy / speed};
    }
    break;
  }

  // 1. 计算背部瞬移点
  using namespace NoMoreDay::Constants::AI::Assassin;
  Vector2 startPos = {playerPos.x, playerPos.y};
  Vector2 direction = {-playerFacing.x, -playerFacing.y};
  Vector2 safePos;

  if (!TilemapCollisionSystem::FindSafeTeleportTarget(
          mapSystem, startPos, direction, TELEPORT_OFFSET, safePos)) {
    // 如果背面完全被堵死，尝试稍微偏移或者取消本次背刺
    LOG_DEBUG("Backstab: No safe teleport target found for assassin {}",
              static_cast<uint32_t>(assassin));
    return false;
  }

  // 2. 射线检测 (Raycast)
  if (!mapSystem.raycast(playerPos, {safePos.x, safePos.y})) {
    return false;
  }

  // 3. 角度检查
  float attackDx = playerPos.x - safePos.x;
  float attackDy = playerPos.y - safePos.y;
  Normalize(attackDx, attackDy);
  float dot = attackDx * playerFacing.x + attackDy * playerFacing.y;

  // 执行瞬移
  assassinPos->x = safePos.x;
  assassinPos->y = safePos.y;

  // 发射背刺粒子效果
  for (int i = 0; i < 30; ++i) {
    NoMoreDay::components::GPUParticle p;
    p.position = {assassinPos->x, assassinPos->y};
    float angle =
        NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 360.0f) * DEG2RAD;
    float speed = NoMoreDay::utils::ThreadSafeRandom::GetFloat(100.0f, 300.0f);
    p.velocity = {cosf(angle) * speed, sinf(angle) * speed};
    p.color = {128, 0, 128, 255};
    p.lifetime = 0.5f;
    p.maxLifetime = p.lifetime;
    p.scale = 4.0f;
    NoMoreDay::systems::GPUParticleSystem::Get().Emit(p);
  }

  // 应用临时的背刺 Buff
  if (auto *effects = registry.try_get<ActiveEffectsComponent>(assassin)) {
    using namespace NoMoreDay::Constants::AI::Assassin;
    BuffEffect backstabBuff;
    backstabBuff.id = "assassin_backstab_boost";
    backstabBuff.name = "Backstab Focus";
    backstabBuff.duration = BUFF_DURATION;
    backstabBuff.remaining = BUFF_DURATION;

    NoMoreDay::StatModifier dmgMod;
    dmgMod.type = NoMoreDay::StatType::PhysicalDamage;
    dmgMod.mode = NoMoreDay::ModifierMode::PercentMult;
    dmgMod.value = (dot >= BACKSTAB_DOT_THRESHOLD)
                       ? backstabMultiplier
                       : (backstabMultiplier * 0.5f);
    backstabBuff.modifiers.push_back(dmgMod);

    effects->AddOrRefresh(backstabBuff);
    LOG_DEBUG("Backstab buff applied (dot: {:.2f})", dot);
  }

  return true;
}

// ============================================================================
// TANK 坦克行为
// ============================================================================

void UpdateTankBehavior(entt::registry &registry, entt::entity entity,
                        AIComponent &ai, Position &pos, Velocity &vel,
                        const Position &playerPos, float dt) {
  // === 1. 查找需要保护的远程友军 ===
  using namespace NoMoreDay::Constants::AI::Tank;
  if (ai.protectTarget == entt::null || !registry.valid(ai.protectTarget)) {
    ai.protectTarget = FindNearestRanger(registry, pos, SEARCH_RADIUS, entity);
  }

  // 如果没有需要保护的目标，切换到普通追击模式
  if (ai.protectTarget == entt::null) {
    // 回退到直接追击玩家
    float dx = playerPos.x - pos.x;
    float dy = playerPos.y - pos.y;
    float dist = Distance(pos, playerPos);

    if (dist > ai.attackRange) {
      Normalize(dx, dy);
      vel.vx = dx * ai.speed;
      vel.vy = dy * ai.speed;
    } else {
      vel.vx = 0.0f;
      vel.vy = 0.0f;
    }
    return;
  }

  // === 2. 计算阻挡位置 ===
  const auto *rangerPos = registry.try_get<Position>(ai.protectTarget);
  if (!rangerPos) {
    ai.protectTarget = entt::null;
    return;
  }

  // 目标位置：玩家与远程友军连线的中点，稍微偏向玩家
  float midX = (playerPos.x + rangerPos->x) / 2.0f;
  float midY = (playerPos.y + rangerPos->y) / 2.0f;

  // 向玩家方向偏移
  using namespace NoMoreDay::Constants::AI::Tank;
  float offsetX = (playerPos.x - midX) * MIDPOINT_OFFSET;
  float offsetY = (playerPos.y - midY) * MIDPOINT_OFFSET;

  Position targetBlockPos = {midX + offsetX, midY + offsetY};

  // === 3. 移动到阻挡位置 ===
  float distToTarget = Distance(pos, targetBlockPos);
  using namespace NoMoreDay::Constants::AI::Tank;

  if (distToTarget > ARRIVAL_DIST) {
    float dx = targetBlockPos.x - pos.x;
    float dy = targetBlockPos.y - pos.y;
    Normalize(dx, dy);

    // 坦克移动较慢但稳定
    float tankSpeed = ai.speed * SPEED_MULT;
    vel.vx = dx * tankSpeed;
    vel.vy = dy * tankSpeed;
  } else {
    vel.vx = 0.0f;
    vel.vy = 0.0f;
  }

  // === 4. 如果玩家在攻击范围内，进行攻击 ===
  float distToPlayer = Distance(pos, playerPos);
  if (distToPlayer <= ai.attackRange) {
    vel.vx = 0.0f;
    vel.vy = 0.0f;

    // 攻击逻辑由 CombatSystem 的距离检测自动处理
    // 坦克在攻击范围内时会被 CombatSystem 识别并触发攻击
  }

  // 确保坦克有阻挡标记
  if (!registry.any_of<TankBlockingTag>(entity)) {
    registry.emplace<TankBlockingTag>(entity);
  }
}

entt::entity FindNearestRanger(entt::registry &registry, const Position &pos,
                               float searchRadius, entt::entity exclude) {
  entt::entity nearest = entt::null;
  float nearestDist = searchRadius;

  auto view = registry.view<EnemyTag, Position, EnemyStateComponent>();

  for (auto [entity, enemyPos, state] : view.each()) {
    if (entity == exclude)
      continue;

    // 只寻找远程类型的敌人
    if (state.archetypeType != EnemyArchetype::RANGER)
      continue;

    float dist = Distance(pos, enemyPos);
    if (dist < nearestDist) {
      nearest = entity;
      nearestDist = dist;
    }
  }

  return nearest;
}

} // namespace NoMoreDay::AI
