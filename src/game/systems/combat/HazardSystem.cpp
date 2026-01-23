#include "game/systems/combat/HazardSystem.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/EffectSystem.hpp"
#include "raymath.h"
#include <cmath>
#include <vector>


namespace NoMoreDay {

// Forward declaration
static void CreateToxicPool(entt::registry &registry, const Position &pos,
                            entt::entity owner);

// Helper to convert DamageType to Tag
static Tag DamageTypeToTag(DamageType type) {
  switch (type) {
  case DamageType::Physical:
    return Tag::Physical;
  case DamageType::Fire:
    return Tag::Fire;
  case DamageType::Cold:
    return Tag::Cold;
  case DamageType::Lightning:
    return Tag::Lightning;
  case DamageType::Poison:
    return Tag::Poison;
  case DamageType::Shadow:
    return Tag::Shadow;
  default:
    return Tag::None;
  }
}

void HazardSystem::Update(entt::registry &registry, float dt,
                          const systems::SpatialHashGrid &grid) {
  ProcessHazards(registry, dt, grid);
  ProcessFrozenOrbs(registry, dt, grid);
  ProcessVolatileOrbs(registry, dt);
  ProcessLightningGhosts(registry, dt, grid);
  ProcessVoidZones(registry, dt);
  EmitHazardParticles(registry, dt);
}

void HazardSystem::ProcessHazards(entt::registry &registry, float dt,
                                  const systems::SpatialHashGrid &grid) {
  auto view = registry.view<HazardComponent, Position, Radius>();
  std::vector<entt::entity> toDestroy;

  for (auto entity : view) {
    auto &hazard = view.get<HazardComponent>(entity);
    const auto &pos = view.get<Position>(entity);
    const auto &radius = view.get<Radius>(entity);

    // 更新生命周期
    hazard.duration -= dt;
    if (hazard.duration <= 0.0f) {
      // 如果是延迟爆炸，触发爆炸
      if (hazard.isDelayedExplosion) {
        DealAreaDamage(registry, pos, radius.value, hazard.explosionDamage,
                       hazard.damageType, hazard.hitsPlayers,
                       hazard.hitsEnemies, grid, hazard.owner);

        // 发射爆炸粒子
        for (int i = 0; i < 30; i++) {
          components::GPUParticle p;
          p.position = {pos.x, pos.y};
          float angle =
              NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 360.0f) *
              DEG2RAD;
          float speed =
              NoMoreDay::utils::ThreadSafeRandom::GetFloat(100.0f, 300.0f);
          p.velocity = {cosf(angle) * speed, sinf(angle) * speed};

          // 根据伤害类型设置颜色
          if (hazard.damageType == DamageType::Cold) {
            p.color = {100, 200, 255, 220};
          } else if (hazard.damageType == DamageType::Lightning) {
            p.color = {255, 255, 100, 220};
          } else {
            p.color = {255, 100, 100, 220};
          }

          p.lifetime = NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.5f, 1.0f);
          p.maxLifetime = p.lifetime;
          p.scale = NoMoreDay::utils::ThreadSafeRandom::GetFloat(3.0f, 6.0f);
          systems::GPUParticleSystem::Get().Emit(p);
        }
      }
      toDestroy.push_back(entity);
      continue;
    }

    // 处理预警阶段
    if (hazard.hasWarningPhase && hazard.isWarningActive) {
      hazard.warningDuration -= dt;
      if (hazard.warningDuration <= 0.0f) {
        hazard.isWarningActive = false;
      }
      continue; // 预警阶段不造成伤害
    }

    // 如果是延迟爆炸，不造成持续伤害
    if (hazard.isDelayedExplosion) {
      continue;
    }

    // 更新 Tick 计时器
    hazard.currentTickTimer += dt;
    if (hazard.currentTickTimer >= hazard.tickInterval) {
      hazard.currentTickTimer = 0.0f;

      // 造成范围伤害
      DealAreaDamage(registry, pos, radius.value, hazard.damagePerTick,
                     hazard.damageType, hazard.hitsPlayers, hazard.hitsEnemies,
                     grid, hazard.owner);
    }
  }

  // 销毁过期实体
  for (auto entity : toDestroy) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
  }
}

void HazardSystem::ProcessFrozenOrbs(entt::registry &registry, float dt,
                                     const systems::SpatialHashGrid &grid) {
  auto view =
      registry.view<FrozenOrbComponent, Position, Velocity, FrozenOrbTag>();
  std::vector<entt::entity> toDestroy;

  for (auto entity : view) {
    auto &orb = view.get<FrozenOrbComponent>(entity);
    auto &pos = view.get<Position>(entity);
    auto &vel = view.get<Velocity>(entity);

    orb.currentTimer += dt;

    if (orb.isTraveling) {
      // 飞行阶段
      if (orb.currentTimer >= orb.travelDuration) {
        orb.isTraveling = false;
        orb.hasStopped = true;
        orb.currentTimer = 0.0f;

        // 停止移动
        vel.vx = 0.0f;
        vel.vy = 0.0f;
      }
    } else if (orb.hasStopped) {
      // 停止阶段，等待爆炸
      if (orb.currentTimer >= orb.stopDuration) {
        // 触发爆炸
        auto *hazard = registry.try_get<HazardComponent>(entity);
        if (hazard) {
          DealAreaDamage(registry, pos, 80.0f, hazard->explosionDamage,
                         DamageType::Cold, true, false, grid, hazard->owner);

          // 对范围内玩家施加 Chill/Freeze
          grid.query(pos, 80.0f,
                     [&](entt::entity target, const Position &targetPos) {
                       if (registry.all_of<PlayerTag>(target)) {
                         ApplyChillDebuff(registry, target, 3.0f, 0.5f);
                       }
                     });

          // 发射冰霜爆炸粒子
          for (int i = 0; i < 40; i++) {
            components::GPUParticle p;
            p.position = {pos.x, pos.y};
            float angle =
                NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 360.0f) *
                DEG2RAD;
            float speed =
                NoMoreDay::utils::ThreadSafeRandom::GetFloat(100.0f, 350.0f);
            p.velocity = {cosf(angle) * speed, sinf(angle) * speed};
            p.color = {150, 220, 255, 220};
            p.lifetime =
                NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.6f, 1.0f);
            p.maxLifetime = p.lifetime;
            p.scale = NoMoreDay::utils::ThreadSafeRandom::GetFloat(4.0f, 7.0f);
            systems::GPUParticleSystem::Get().Emit(p);
          }
        }

        toDestroy.push_back(entity);
      }
    }
  }

  // 销毁爆炸的冰球
  for (auto entity : toDestroy) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
  }
}

void HazardSystem::ProcessVolatileOrbs(entt::registry &registry, float dt) {
  auto view =
      registry.view<VolatileOrbComponent, Position, Velocity, VolatileOrbTag>();
  std::vector<entt::entity> toDestroy;

  // 获取玩家位置
  Position playerPos = {0, 0};
  auto playerView = registry.view<PlayerTag, Position>();
  if (playerView.begin() != playerView.end()) {
    playerPos = playerView.get<Position>(playerView.front());
  }

  for (auto entity : view) {
    auto &orb = view.get<VolatileOrbComponent>(entity);
    auto &pos = view.get<Position>(entity);
    auto &vel = view.get<Velocity>(entity);

    orb.currentLifetime += dt;

    // 超时检查
    if (orb.currentLifetime >= orb.maxLifetime) {
      // 生成毒池
      CreateToxicPool(registry, pos, orb.owner);
      toDestroy.push_back(entity);
      continue;
    }

    // 追踪玩家
    float dx = playerPos.x - pos.x;
    float dy = playerPos.y - pos.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist > 0.1f) {
      float nx = dx / dist;
      float ny = dy / dist;

      // 应用追踪加速度
      vel.vx += nx * orb.homingStrength * dt;
      vel.vy += ny * orb.homingStrength * dt;

      // 限制速度
      float speed = std::sqrt(vel.vx * vel.vx + vel.vy * vel.vy);
      if (speed > orb.speed) {
        vel.vx = (vel.vx / speed) * orb.speed;
        vel.vy = (vel.vy / speed) * orb.speed;
      }
    }

    // 碰撞检测
    if (dist < 30.0f) {
      // 撞击玩家，生成毒池
      CreateToxicPool(registry, pos, orb.owner);
      toDestroy.push_back(entity);
    }
  }

  // 销毁
  for (auto entity : toDestroy) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
  }
}

void HazardSystem::ProcessLightningGhosts(
    entt::registry &registry, float dt, const systems::SpatialHashGrid &grid) {
  auto view =
      registry.view<LightningGhostComponent, Position, LightningGhostTag>();
  std::vector<entt::entity> toDestroy;

  for (auto entity : view) {
    auto &ghost = view.get<LightningGhostComponent>(entity);
    const auto &pos = view.get<Position>(entity);

    ghost.currentTimer += dt;

    if (ghost.currentTimer >= ghost.explosionDelay) {
      // 触发雷电爆炸
      auto *hazard = registry.try_get<HazardComponent>(entity);
      entt::entity owner = hazard ? hazard->owner : entt::null;
      DealAreaDamage(registry, pos, 50.0f, 80.0f, DamageType::Lightning, true,
                     false, grid, owner);

      // 发射雷电粒子
      for (int i = 0; i < 25; i++) {
        components::GPUParticle p;
        p.position = {pos.x, pos.y};
        float angle =
            NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 360.0f) *
            DEG2RAD;
        float speed =
            NoMoreDay::utils::ThreadSafeRandom::GetFloat(80.0f, 260.0f);
        p.velocity = {cosf(angle) * speed, sinf(angle) * speed};
        p.color = {255, 255, 100, 220};
        p.lifetime = NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.4f, 0.7f);
        p.maxLifetime = p.lifetime;
        p.scale = NoMoreDay::utils::ThreadSafeRandom::GetFloat(3.0f, 5.5f);
        systems::GPUParticleSystem::Get().Emit(p);
      }

      toDestroy.push_back(entity);
    }
  }

  // 销毁
  for (auto entity : toDestroy) {
    if (registry.valid(entity)) {
      registry.destroy(entity);
    }
  }
}

void HazardSystem::ProcessVoidZones(entt::registry &registry, float dt) {
  auto view =
      registry.view<VoidZoneTag, HazardComponent, HazardVisualComponent>();

  for (auto entity : view) {
    auto &hazard = view.get<HazardComponent>(entity);
    auto &visual = view.get<HazardVisualComponent>(entity);

    if (hazard.isWarningActive) {
      visual.tintColor = Color{80, 0, 120, 150};
    } else {
      visual.tintColor = Color{40, 0, 80, 200};
    }
  }
}

void HazardSystem::EmitHazardParticles(entt::registry &registry, float dt) {
  auto view = registry.view<HazardVisualComponent, Position>();

  for (auto entity : view) {
    auto &visual = view.get<HazardVisualComponent>(entity);
    const auto &pos = view.get<Position>(entity);

    visual.particleEmitTimer += dt;

    if (visual.particleEmitTimer >= visual.particleEmitInterval) {
      visual.particleEmitTimer = 0.0f;

      for (int i = 0; i < visual.particlesPerEmit; i++) {
        components::GPUParticle p;
        p.position = {
            pos.x + NoMoreDay::utils::ThreadSafeRandom::GetFloat(-20.0f, 20.0f),
            pos.y +
                NoMoreDay::utils::ThreadSafeRandom::GetFloat(-20.0f, 20.0f)};
        p.velocity = {
            NoMoreDay::utils::ThreadSafeRandom::GetFloat(-10.0f, 10.0f),
            -30.0f - NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 20.0f)};
        p.color = visual.tintColor;
        p.lifetime = NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.5f, 0.8f);
        p.maxLifetime = p.lifetime;
        p.scale = 2.5f * visual.visualScale +
                  NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 1.5f);
        systems::GPUParticleSystem::Get().Emit(p);
      }
    }
  }
}

void HazardSystem::ApplyChillDebuff(entt::registry &registry,
                                    entt::entity target, float duration,
                                    float slowAmount) {
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);

  BuffEffect chill;
  chill.id = "frozen_chill";
  chill.name = "冰冻减速";
  chill.description = "被冰霜减速";
  chill.type = BuffType::SpeedDown;
  chill.is_debuff = true;
  chill.duration = duration;
  chill.remaining = duration;
  chill.modifiers.push_back({.value = -slowAmount * 100.0f,
                             .type = StatType::MoveSpeed,
                             .mode = ModifierMode::PercentAdd});

  effects.AddOrRefresh(chill);
}

void HazardSystem::DealAreaDamage(entt::registry &registry, Position center,
                                  float radius, float damage, DamageType type,
                                  bool hitsPlayers, bool hitsEnemies,
                                  const systems::SpatialHashGrid &grid,
                                  entt::entity owner) {
  if (damage <= 0.0f)
    return;

  DamagePool base_pool;
  base_pool.Add(DamageTypeToTag(type), damage);

  float radiusSq = radius * radius;

  grid.query(
      center, radius, [&](entt::entity target, const Position &targetPos) {
        float dx = targetPos.x - center.x;
        float dy = targetPos.y - center.y;
        float distSq = dx * dx + dy * dy;
        if (distSq > radiusSq)
          return;

        bool isPlayer = registry.all_of<PlayerTag>(target);
        bool isEnemy = registry.all_of<EnemyTag>(target) &&
                       !registry.all_of<KilledTag>(target);

        if ((hitsPlayers && isPlayer) || (hitsEnemies && isEnemy)) {
          auto result = DamagePipeline::Calculate(registry, owner, target, 0,
                                                  base_pool, Tag::None);

          // 修复：必须实际应用伤害
          CombatSystem::ApplyDamage(registry, target, result.total_damage,
                                    owner, result.is_crit, false);

          Tag damageTag = DamageTypeToTag(type);
          systems::EffectSystem::EmitDamagePopup(
              registry, {targetPos.x, targetPos.y - 20.0f}, result.total_damage,
              result.is_crit, damageTag);
        }
      });
}

static void CreateToxicPool(entt::registry &registry, const Position &pos,
                            entt::entity owner) {
  auto poolEntity = registry.create();
  registry.emplace<Position>(poolEntity, pos.x, pos.y);
  registry.emplace<LocalLevelTag>(poolEntity);
  registry.emplace<ToxicPoolTag>(poolEntity);
  registry.emplace<Radius>(poolEntity, 60.0f);

  HazardComponent hazard;
  hazard.damagePerTick = 20.0f;
  hazard.tickInterval = 0.5f;
  hazard.duration = 5.0f;
  hazard.radius = 60.0f;
  hazard.damageType = DamageType::Poison;
  hazard.isDelayedExplosion = false;
  hazard.hitsPlayers = true;
  hazard.hitsEnemies = false;
  hazard.owner = owner;
  registry.emplace<HazardComponent>(poolEntity, hazard);

  HazardVisualComponent visual;
  visual.tintColor = Color{100, 255, 100, 180};
  visual.particleEmitInterval = 0.15f;
  visual.particlesPerEmit = 4;
  registry.emplace<HazardVisualComponent>(poolEntity, visual);

  LOG_TRACE("Toxic pool created at ({:.1f}, {:.1f})", pos.x, pos.y);
}

} // namespace NoMoreDay