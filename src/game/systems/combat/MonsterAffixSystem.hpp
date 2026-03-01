#pragma once

#include "core/logging/Logger.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/AdvancedAffixComponents.hpp" // For TeleportationComponent, etc
#include "game/components/Buff.hpp"
#include "game/components/Combat.hpp"
#include "game/components/Common.hpp"
#include "game/components/EliteModifierComponents.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/HazardComponents.hpp" // For HazardComponent
#include "game/components/NemesisComponent.hpp" // Added for Tier scaling
#include "core/utils/ScopedTimer.hpp" // Explicit include for ScopedTimer

#include "core/utils/Branchless.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Stats.hpp"
#include "game/data/MonsterAffixRegistry.hpp" // For MonsterAffixComponent
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/CombatEvents.hpp"
#include "game/systems/modifier/MonsterModifierAdapter.hpp"
#include "game/systems/world/MapSystem.hpp"
#include "game/utils/EntityUtils.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include <algorithm>
#include <cmath>
#include <entt/entt.hpp>


namespace NoMoreDay {

/**
 * @brief 熔火路径标签 - 用于伤害区域处理
 */
struct MoltenTrailTag {};

/**
 * @brief 怪物词缀系统 - 处理机制型词缀的运行时逻辑
 *
 * 处理需要 Update/OnHit/OnDeath 回调的词缀：
 * - Molten: 火焰路径
 * - Teleporter: 闪烁
 * - Berserker: 狂暴
 * - Frozen: 减速光环
 * - Nullifier: 驱散 (via CombatEventDispatcher)
 */
class MonsterAffixSystem {
public:
  // Constants moved to MonsterAffixRegistry::Params

  /**
   * @brief 初始化系统 - 注册 CombatEvent 处理器
   */
  static void Init() {
    // Register OnDealDamage handler for Nullifier affix
    CombatEventDispatcher::Register(
        CombatEventType::OnDealDamage,
        [](entt::registry &registry, const CombatEvent &evt) {
          OnEnemyDealDamage(registry, evt);
        },
        -10 // Lower priority to run after damage calculation
    );

    // Register OnTakeDamage handler for StormStrider affix
    CombatEventDispatcher::Register(
        CombatEventType::OnTakeDamage,
        [](entt::registry &registry, const CombatEvent &evt) {
          OnEnemyTakeDamage(registry, evt);
        },
        -10);

    LOG_INFO("MonsterAffixSystem: Initialized");
  }

  /**
   * @brief 主更新循环 - 处理需要 Update 的词缀
   */
  static void Update(entt::registry &registry, float dt, const NoMoreDay::systems::SpatialHashGrid& spatialGrid) {
    NoMoreDay::utils::ScopedTimer timer("Affix Update", 100);
    auto view = registry.view<MonsterAffixComponent, Position>(
        entt::exclude<KilledTag>);

    // Get player position for distance checks
    Position playerPos = {0, 0};
    auto playerView = registry.view<PlayerTag, Position>();
    entt::entity playerEntity = entt::null;
    if (playerView.begin() != playerView.end()) {
      playerEntity = playerView.front();
      playerPos = playerView.get<Position>(playerEntity);
    }

    for (auto entity : view) {
      auto &affix = view.get<MonsterAffixComponent>(entity);
      const auto &pos = view.get<Position>(entity);
      const auto eventSet = MonsterModifierAdapter::EvaluateAffixEvents(affix);
      const auto behaviorOps =
          MonsterModifierAdapter::EvaluateBehaviorOps(affix);

      // Skip if no Update-type affixes
      if (!eventSet.HasOnUpdate() && !affix.hasUpdate &&
          !behaviorOps.HasOnUpdate())
        continue;

      // Determine evolution tier
      int tier = 1;
      if (auto *nc = registry.try_get<NemesisComponent>(entity))
        tier = nc->evolution_tier;

      // Process each affix
      if (affix.mirrorCooldown > 0.0f)
        affix.mirrorCooldown -= dt;

      for (size_t i = 0; i < affix.affixes.size(); ++i) {
        affix.timers[i] += dt;
        auto affixType = affix.affixes[i];

        switch (affixType) {
        case MonsterAffixType::Molten:
          if (behaviorOps.HasOnUpdateOpcode(
                  ModifierOpCode::MONSTER_BEHAVIOR_MOLTEN_UPDATE)) {
            ProcessMolten(registry, entity, pos, affix, i, dt, tier);
            break;
          }
          ProcessMolten(registry, entity, pos, affix, i, dt, tier);
          break;
        case MonsterAffixType::Teleporter:
          if (behaviorOps.HasOnUpdateOpcode(
                  ModifierOpCode::MONSTER_BEHAVIOR_TELEPORTER_UPDATE)) {
            ProcessTeleporter(registry, entity, pos, playerPos, affix, i, dt,
                              tier);
            break;
          }
          ProcessTeleporter(registry, entity, pos, playerPos, affix, i, dt,
                            tier);
          break;
        case MonsterAffixType::Berserker:
          if (behaviorOps.HasOnUpdateOpcode(
                  ModifierOpCode::MONSTER_BEHAVIOR_BERSERKER_UPDATE)) {
            ProcessBerserker(registry, entity, affix, tier);
            break;
          }
          if (affix.HasAffix(MonsterAffixType::Berserker)) {
            ProcessBerserker(registry, entity, affix, tier);
          }
          break;
        case MonsterAffixType::Frozen:
          if (behaviorOps.HasOnUpdateOpcode(
                  ModifierOpCode::MONSTER_BEHAVIOR_FROZEN_UPDATE)) {
            ProcessFrozen(registry, entity, pos, playerPos, affix, i, dt, tier);
            break;
          }
          ProcessFrozen(registry, entity, pos, playerPos, affix, i, dt, tier);
          break;
        case MonsterAffixType::VoidZone:
          if (behaviorOps.HasOnUpdateOpcode(
                  ModifierOpCode::MONSTER_BEHAVIOR_VOIDZONE_UPDATE)) {
            ProcessVoidZone(registry, entity, pos, playerPos, affix, i, dt,
                            tier);
            break;
          }
          if (affix.HasAffix(MonsterAffixType::VoidZone)) {
            ProcessVoidZone(registry, entity, pos, playerPos, affix, i, dt,
                            tier);
          }
          break;
        case MonsterAffixType::SoulEater:
          ProcessSoulEater(registry, entity, affix, dt, tier);
          break;
        case MonsterAffixType::Suppressor:
          if (behaviorOps.HasOnUpdateOpcode(
                  ModifierOpCode::MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE)) {
            ProcessSuppressor(registry, entity);
            break;
          }
          if (affix.HasAffix(MonsterAffixType::Suppressor) ||
              registry.all_of<SuppressorComponent>(entity)) {
            ProcessSuppressor(registry, entity);
          }
          break;
        case MonsterAffixType::SoulLink:
          if (behaviorOps.HasOnUpdateOpcode(
                  ModifierOpCode::MONSTER_BEHAVIOR_SOUL_LINK_UPDATE)) {
            ProcessSoulLink(registry, entity);
            break;
          }
          if (affix.HasAffix(MonsterAffixType::SoulLink) ||
              registry.all_of<SoulLinkComponent>(entity)) {
            ProcessSoulLink(registry, entity);
          }
          break;
        case MonsterAffixType::ManaSiphon:
          if (behaviorOps.HasOnUpdateOpcode(
                  ModifierOpCode::MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE)) {
            ProcessManaSiphon(registry, entity, pos, playerEntity, affix, dt,
                              tier);
            break;
          }
          if (affix.HasAffix(MonsterAffixType::ManaSiphon)) {
            ProcessManaSiphon(registry, entity, pos, playerEntity, affix, dt,
                              tier);
          }
          break;
        case MonsterAffixType::Shielding:
          if (behaviorOps.HasOnUpdateOpcode(
                  ModifierOpCode::MONSTER_BEHAVIOR_SHIELDING_UPDATE)) {
            ProcessShielding(registry, entity, pos, spatialGrid, affix, i, dt,
                             tier);
            break;
          }
          if (affix.HasAffix(MonsterAffixType::Shielding)) {
            ProcessShielding(registry, entity, pos, spatialGrid, affix, i, dt,
                             tier);
          }
          break;
        case MonsterAffixType::Vortex:
          if (behaviorOps.HasOnUpdateOpcode(
                  ModifierOpCode::MONSTER_BEHAVIOR_VORTEX_UPDATE)) {
            ProcessVortex(registry, entity, affix, i, dt, tier);
            break;
          }
          if (affix.HasAffix(MonsterAffixType::Vortex)) {
            ProcessVortex(registry, entity, affix, i, dt, tier);
          }
          break;
        case MonsterAffixType::Waller:
          if (behaviorOps.HasOnUpdateOpcode(
                  ModifierOpCode::MONSTER_BEHAVIOR_WALLER_UPDATE)) {
            ProcessWaller(registry, entity, pos, playerPos, affix, i, dt, tier);
            break;
          }
          if (affix.HasAffix(MonsterAffixType::Waller)) {
            ProcessWaller(registry, entity, pos, playerPos, affix, i, dt, tier);
          }
          break;
        default:
          break;
        }
      }
    }

    // Update clone entities and invulnerable components
    EntityUtils::UpdateClones(registry, dt);
    EntityUtils::UpdateInvulnerable(registry, dt);
    EntityUtils::UpdateLinks(registry, dt);
    EntityUtils::UpdateDynamicObstacles(registry, dt);

    // Update active teleportations
    UpdateTeleportation(registry, dt);

    // Update Phase Shields
    auto psView = registry.view<PhaseShieldComponent>();
    psView.each([&](PhaseShieldComponent &ps) {
      if (ps.currentCooldown > 0.0f)
        ps.currentCooldown -= dt;
      if (ps.accumulationTimer > 0.0f) {
        ps.accumulationTimer -= dt;
        if (ps.accumulationTimer <= 0.0f) {
          ps.accumulatedDamage = 0.0f; // Reset if window expires
        }
      }
    });
  }

private:
  /**
   * @brief 更新传送状态 - 处理渐隐渐显
   */
  static void UpdateTeleportation(entt::registry &registry, float dt) {
    auto view =
        registry.view<TeleportationComponent, Position, ColorComponent>();
    std::vector<entt::entity> toRemove;

    view.each([&](entt::entity entity, TeleportationComponent &teleport,
                  Position &pos, ColorComponent &color) {
      teleport.timer += dt;

      switch (teleport.phase) {
      case TeleportationComponent::Phase::FadeOut: {
        float t = teleport.timer / teleport.fadeDuration;
        if (t >= 1.0f) {
          teleport.phase = TeleportationComponent::Phase::Invisible;
          teleport.timer = 0.0f;
          color.color.a = 0;

          // Move to target
          pos.x = teleport.targetX;
          pos.y = teleport.targetY;
        } else {
          // Lerp alpha
          color.color.a =
              static_cast<unsigned char>(teleport.originalColor.a * (1.0f - t));
        }
        break;
      }
      case TeleportationComponent::Phase::Invisible: {
        if (teleport.timer >= teleport.invisibleDuration) {
          teleport.phase = TeleportationComponent::Phase::FadeIn;
          teleport.timer = 0.0f;
          color.color.a = 0;

          // Attack immediately? (Make AI aggressive)
          if (auto *ai = registry.try_get<AIComponent>(entity)) {
            ai->stateTimer = 0.0f; // Reset AI timer
                                   // Force attack state if possible?
            // AI system usually handles this based on distance
          }
        }
        break;
      }
      case TeleportationComponent::Phase::FadeIn: {
        float t = teleport.timer / teleport.fadeDuration;
        if (t >= 1.0f) {
          // Restore
          color.color = teleport.originalColor;
          toRemove.push_back(entity);
        } else {
          color.color.a =
              static_cast<unsigned char>(teleport.originalColor.a * t);
        }
        break;
      }
      }
    });

    for (auto entity : toRemove) {
      registry.remove<TeleportationComponent>(entity);
    }
  }

  /**
   * @brief 熔火词缀 - 生成火焰路径 (已配置化为 HazardComponent)
   */
  static void ProcessMolten(entt::registry &registry, entt::entity enemy,
                            const Position &pos, MonsterAffixComponent &affix,
                            size_t affixIdx, float dt, int tier) {
    bool ready = affix.timers[affixIdx] >=
                 MonsterAffixRegistry::Params::MOLTEN_TICK_INTERVAL;
    affix.timers[affixIdx] =
        NoMoreDay::utils::SelectF(ready, 0.0f, affix.timers[affixIdx]);

    if (ready) {

      float scaledRadius = MonsterAffixRegistry::GetScaledValue(
          MonsterAffixRegistry::Params::MOLTEN_TRAIL_RADIUS, tier);
      float scaledDamage = MonsterAffixRegistry::GetScaledValue(
          MonsterAffixRegistry::Params::MOLTEN_TRAIL_DAMAGE, tier);

      // Create fire trail entity
      auto fireEntity = registry.create();
      registry.emplace<Position>(fireEntity, pos.x, pos.y);
      registry.emplace<LocalLevelTag>(fireEntity);
      registry.emplace<Radius>(fireEntity, scaledRadius);

      // Hazard 配置 (取代旧的 MoltenTrailTag)
      HazardComponent hazard;
      hazard.damagePerTick =
          scaledDamage * MonsterAffixRegistry::Params::MOLTEN_TICK_INTERVAL;
      hazard.tickInterval = MonsterAffixRegistry::Params::MOLTEN_TICK_INTERVAL;
      hazard.duration = MonsterAffixRegistry::Params::MOLTEN_TRAIL_DURATION;
      hazard.radius = scaledRadius;
      hazard.damageType = DamageType::Fire;
      hazard.isDelayedExplosion = false;
      hazard.hitsPlayers = true;
      hazard.hitsEnemies = false;
      hazard.owner = enemy;
      registry.emplace<HazardComponent>(fireEntity, hazard);

      // 视觉效果
      HazardVisualComponent visual;
      visual.tintColor = Color{255, 80, 0, 180}; // 橙色
      visual.particleEmitInterval = 0.2f;
      visual.particlesPerEmit = 2;
      registry.emplace<HazardVisualComponent>(fireEntity, visual);

      LOG_TRACE("Molten trail (Hazard) spawned at ({:.1f}, {:.1f})", pos.x,
                pos.y);
    }
  }

  /**
   * @brief 闪烁词缀 - 瞬移到玩家附近
   */
  static void ProcessTeleporter(entt::registry &registry, entt::entity enemy,
                                const Position &enemyPos,
                                const Position &playerPos,
                                MonsterAffixComponent &affix, size_t affixIdx,
                                float dt, int tier) {

    // If already teleporting, skip
    if (registry.any_of<TeleportationComponent>(enemy))
      return;

    bool ready = affix.timers[affixIdx] >=
                 MonsterAffixRegistry::Params::TELEPORT_COOLDOWN;

    if (ready) {
      float dx = playerPos.x - enemyPos.x;
      float dy = playerPos.y - enemyPos.y;
      float distSq = dx * dx + dy * dy;

      // Only teleport if far from player
      if (distSq >
          MonsterAffixRegistry::Params::TELEPORT_TRIGGER_DISTANCE *
              MonsterAffixRegistry::Params::TELEPORT_TRIGGER_DISTANCE) {
        affix.timers[affixIdx] = 0.0f; // Reset only on successful trigger

        // Calculate new position behind player
        float dist = std::sqrt(distSq);
        float nx = (dist > 0.001f) ? dx / dist : 1.0f;
        float ny = (dist > 0.001f) ? dy / dist : 0.0f;

        float newX =
            playerPos.x -
            nx * MonsterAffixRegistry::Params::TELEPORT_TARGET_DISTANCE;
        float newY =
            playerPos.y -
            ny * MonsterAffixRegistry::Params::TELEPORT_TARGET_DISTANCE;

        // Start Teleport Sequence
        // Save original color
        Color origColor = WHITE;
        if (auto *c = registry.try_get<ColorComponent>(enemy)) {
          origColor = c->color;
        } else {
          registry.emplace<ColorComponent>(enemy, WHITE);
        }

        auto &tc = registry.emplace<TeleportationComponent>(enemy);
        tc.targetX = newX;
        tc.targetY = newY;
        tc.originalColor = origColor;
        tc.phase = TeleportationComponent::Phase::FadeOut;
        tc.timer = 0.0f;

        LOG_TRACE(
            "Teleporter enemy starting blink sequence to ({:.1f}, {:.1f})",
            newX, newY);
      }
    }
  }

  /**
   * @brief 狂暴词缀 - 低血量时激活
   */
  static void ProcessBerserker(entt::registry &registry, entt::entity enemy,
                               MonsterAffixComponent &affix, int tier) {
    if (affix.isBerserk)
      return; // Already berserk

    auto *hp = registry.try_get<HealthComponent>(enemy);
    if (!hp)
      return;

    float hpRatio = hp->current / hp->max;
    if (hpRatio <= MonsterAffixRegistry::Params::BERSERKER_HP_THRESHOLD) {
      bool wasBerserk = affix.isBerserk;
      affix.isBerserk = true;

      // Trigger recalculation to apply multipliers via StatsSystem
      if (!wasBerserk)
        registry.get_or_emplace<StatsDirty>(enemy);

      // Apply scale multiplier
      if (auto *sprite = registry.try_get<SpriteComponent>(enemy)) {
        sprite->scale *= MonsterAffixRegistry::Params::BERSERKER_SCALE_MULT;
      }

      // Note: Actual damage multiplier should be handled in StatsSystem using
      // tier if needed. But currently it's hardcoded in StatsSystem?
      // MonsterAffixSystem handles activation.
      // If we want to scale the *effect*, we need to store the scaled value on
      // the component or modify StatsSystem. Since StatsSystem likely checks
      // "HasAffix(Berserker) && isBerserk", it applies fixed value. For now, we
      // update signature. Scaling effect would require StatsSystem change.

      // Add red tint
      if (auto *color = registry.try_get<ColorComponent>(enemy)) {
        color->color = Color{255, 50, 50, 255};
      }

      LOG_INFO("Berserker activated for entity {}", (uint32_t)enemy);
    }
  }

  /**
   * @brief 极寒词缀 - 生成追踪冰球
   */
  static void ProcessFrozen(entt::registry &registry, entt::entity enemy,
                            const Position &enemyPos, const Position &playerPos,
                            MonsterAffixComponent &affix, size_t affixIdx,
                            float dt, int tier) {
    bool ready = affix.timers[affixIdx] >=
                 MonsterAffixRegistry::Params::FROZEN_ORB_INTERVAL;
    affix.timers[affixIdx] =
        NoMoreDay::utils::SelectF(ready, 0.0f, affix.timers[affixIdx]);

    if (ready) {

      float scaledDamage = MonsterAffixRegistry::GetScaledValue(
          MonsterAffixRegistry::Params::FROZEN_ORB_DAMAGE, tier);

      // 生成冰球实体
      auto orbEntity = registry.create();
      registry.emplace<Position>(orbEntity, enemyPos.x, enemyPos.y);
      registry.emplace<LocalLevelTag>(orbEntity);
      registry.emplace<FrozenOrbTag>(orbEntity);
      registry.emplace<Radius>(orbEntity, 15.0f);

      // 计算朝向玩家的初速度
      float dx = playerPos.x - enemyPos.x;
      float dy = playerPos.y - enemyPos.y;
      float dist = std::sqrt(dx * dx + dy * dy);
      if (dist > 0.1f) {
        float nx = dx / dist;
        float ny = dy / dist;
        registry.emplace<Velocity>(
            orbEntity, nx * MonsterAffixRegistry::Params::FROZEN_ORB_SPEED,
            ny * MonsterAffixRegistry::Params::FROZEN_ORB_SPEED);
      } else {
        registry.emplace<Velocity>(orbEntity, 0.0f, 0.0f);
      }

      // 冰球组件
      FrozenOrbComponent orb;
      orb.travelDuration = 2.0f;
      orb.stopDuration = 1.0f;
      registry.emplace<FrozenOrbComponent>(orbEntity, orb);

      // Hazard 组件（用于爆炸）
      HazardComponent hazard;
      hazard.explosionDamage = scaledDamage;
      hazard.isDelayedExplosion = true;
      hazard.damageType = DamageType::Cold;
      hazard.duration = 3.0f; // 总生命周期
      hazard.owner = enemy;
      registry.emplace<HazardComponent>(orbEntity, hazard);

      // 视觉效果
      registry.emplace<ColorComponent>(orbEntity, Color{150, 220, 255, 220});

      LOG_TRACE("Frozen orb spawned at ({:.1f}, {:.1f})", enemyPos.x,
                enemyPos.y);
    }
  }

  /**
   * @brief 虚空区域词缀 - 在玩家脚下生成虚空区域
   */
  static void ProcessVoidZone(entt::registry &registry, entt::entity enemy,
                              const Position &enemyPos,
                              const Position &playerPos,
                              MonsterAffixComponent &affix, size_t affixIdx,
                              float dt, int tier) {
    // 使用怪物独立的冷却计时器
    if (affix.voidZoneNextSpawnTime <= 0.0f) {
      // 初始化第一次触发时间
      affix.voidZoneNextSpawnTime =
          NoMoreDay::utils::ThreadSafeRandom::GetFloat(
              MonsterAffixRegistry::Params::VOIDZONE_SPAWN_INTERVAL_MIN,
              MonsterAffixRegistry::Params::VOIDZONE_SPAWN_INTERVAL_MAX);
    }

    bool ready = affix.timers[affixIdx] >= affix.voidZoneNextSpawnTime;
    affix.timers[affixIdx] =
        NoMoreDay::utils::SelectF(ready, 0.0f, affix.timers[affixIdx]);

    if (ready) {
      affix.voidZoneNextSpawnTime =
          NoMoreDay::utils::ThreadSafeRandom::GetFloat(
              MonsterAffixRegistry::Params::VOIDZONE_SPAWN_INTERVAL_MIN,
              MonsterAffixRegistry::Params::VOIDZONE_SPAWN_INTERVAL_MAX);

      float scaledDamage = MonsterAffixRegistry::GetScaledValue(
          MonsterAffixRegistry::Params::VOIDZONE_DAMAGE_PER_TICK, tier);

      // 在玩家当前位置生成虚空区域
      auto zoneEntity = registry.create();
      registry.emplace<Position>(zoneEntity, playerPos.x, playerPos.y);
      registry.emplace<LocalLevelTag>(zoneEntity);
      registry.emplace<VoidZoneTag>(zoneEntity);
      registry.emplace<Radius>(zoneEntity, 80.0f);

      // Hazard 组件
      HazardComponent hazard;
      hazard.damagePerTick = scaledDamage;
      hazard.tickInterval =
          MonsterAffixRegistry::Params::VOIDZONE_TICK_INTERVAL;
      hazard.duration =
          MonsterAffixRegistry::Params::VOIDZONE_WARNING_DURATION +
          MonsterAffixRegistry::Params::VOIDZONE_ACTIVE_DURATION;
      hazard.radius = 80.0f;
      hazard.damageType = DamageType::Shadow;
      hazard.isDelayedExplosion = false;
      hazard.hitsPlayers = true;
      hazard.hitsEnemies = false;
      hazard.hasWarningPhase = true;
      hazard.warningDuration =
          MonsterAffixRegistry::Params::VOIDZONE_WARNING_DURATION;
      hazard.isWarningActive = true;
      hazard.owner = enemy;
      registry.emplace<HazardComponent>(zoneEntity, hazard);

      // 视觉效果
      HazardVisualComponent visual;
      visual.tintColor = Color{80, 0, 120, 150}; // 预警阶段暗紫色
      visual.particleEmitInterval = 0.1f;
      visual.particlesPerEmit = 5;
      registry.emplace<HazardVisualComponent>(zoneEntity, visual);

      LOG_TRACE("Void zone spawned at ({:.1f}, {:.1f})", playerPos.x,
                playerPos.y);
    }
  }

  /**
   * @brief 噬魂词缀 - 吸收附近死亡灵魂获得增益
   */
  static void ProcessSoulEater(entt::registry &registry, entt::entity enemy,
                               MonsterAffixComponent &affix, float dt,
                               int tier) {
    // Soul Eater 的层数增加由 OnDeath 事件处理
    // 这里只需要更新视觉效果
    auto *soulEater = registry.try_get<SoulEaterComponent>(enemy);
    if (!soulEater) {
      // 首次添加组件
      auto &se = registry.emplace<SoulEaterComponent>(enemy);
      // Scale per-stack bonuses
      se.sizePerStack =
          MonsterAffixRegistry::GetScaledValue(se.sizePerStack, tier);
      se.damagePerStack =
          MonsterAffixRegistry::GetScaledValue(se.damagePerStack, tier);
      se.attackSpeedPerStack =
          MonsterAffixRegistry::GetScaledValue(se.attackSpeedPerStack, tier);
      return;
    }

    // 根据层数更新体型
    float scaleBonus =
        1.0f + (soulEater->stacks * soulEater->sizePerStack / 100.0f);
    if (auto *sprite = registry.try_get<SpriteComponent>(enemy)) {
      sprite->scale = soulEater->baseScale * scaleBonus;
    }
  }

  /**
   * @brief 虹吸词缀 - 剥夺玩家法力
   */
  static void ProcessManaSiphon(entt::registry &registry, entt::entity enemy,
                                const Position &enemyPos, entt::entity player,
                                MonsterAffixComponent &affix, float dt,
                                int tier) {
    if (!registry.valid(player))
      return;

    // 确保有 ResourceDrainComponent
    auto *drain = registry.try_get<ResourceDrainComponent>(enemy);
    if (!drain) {
      auto &newDrain = registry.emplace<ResourceDrainComponent>(enemy);
      newDrain.radius = 200.0f;
      newDrain.drainRate = MonsterAffixRegistry::GetScaledValue(10.0f, tier);
      newDrain.resource = ResourceType::Mana;
      newDrain.safeZoneInside = true;
      newDrain.innerRadius = 50.0f;
      newDrain.effectColor = PURPLE;
      drain = &newDrain;
    }

    // 检查玩家是否在范围内
    auto *playerPos = registry.try_get<Position>(player);
    if (!playerPos)
      return;

    float dx = playerPos->x - enemyPos.x;
    float dy = playerPos->y - enemyPos.y;
    float distSq = dx * dx + dy * dy;
    float radiusSq = drain->radius * drain->radius;

    // 甜甜圈模式：内圈安全
    bool inRange = false;
    if (drain->safeZoneInside) {
      float innerRadiusSq = drain->innerRadius * drain->innerRadius;
      inRange = (distSq > innerRadiusSq && distSq <= radiusSq);
    } else {
      inRange = (distSq <= radiusSq);
    }

    // 剥夺法力
    if (auto *stats = registry.try_get<CombatStats>(player)) {
      float drainAmount =
          NoMoreDay::utils::SelectF(inRange, drain->drainRate * dt, 0.0f);
      // Branchless max(0, x): SelectF(x>0, x, 0)
      float resultingMana = stats->mana - drainAmount;
      stats->mana =
          NoMoreDay::utils::SelectF(resultingMana > 0.0f, resultingMana, 0.0f);
    }
  }

  /**
   * @brief 护盾词缀 - 给附近友军施加无敌护盾
   */
  static void ProcessShielding(entt::registry &registry, entt::entity enemy,
                               const Position &enemyPos,
                               const NoMoreDay::systems::SpatialHashGrid& spatialGrid,
                               MonsterAffixComponent &affix, size_t affixIdx,
                               float dt, int tier) {
    // Phase Shield Logic (Self)
    if (!registry.all_of<PhaseShieldComponent>(enemy)) {
      auto &ps = registry.emplace<PhaseShieldComponent>(enemy);
      // Scale threshold? Maybe not needed, ratio is usually consistent.
    }

    // 每 3 秒检查一次附近友军
    static constexpr float SHIELDING_COOLDOWN = 3.0f;
    static constexpr float SHIELDING_RANGE = 250.0f;
    float shieldingDuration = MonsterAffixRegistry::GetScaledValue(2.0f, tier);

    if (affix.timers[affixIdx] < SHIELDING_COOLDOWN)
      return;
    // Delay reset until actually searching to avoid spinning?
    // Actually Logic here is: Check CD -> Search Allies -> Apply.
    // We can make CD check branchless for timer reset but early return is
    // better for perf. The Plan asked for branchless. Let's do partial.
    affix.timers[affixIdx] = 0.0f;

    // [FIX] Use Spatial Grid Query instead of O(N) iteration
    std::vector<entt::entity> targets;
    spatialGrid.query(enemyPos, SHIELDING_RANGE, [&](entt::entity ally, const Position& allyPos) {
        if (ally == enemy) return;
        
        // Filter: Only shield enemies (Shielding doesn't apply to projectiles or dead things)
        if (registry.all_of<EnemyTag>(ally) && !registry.any_of<KilledTag>(ally)) {
            // Distance Check (Spatial grid query gives approximate cells, need fine check)
            float dx = allyPos.x - enemyPos.x;
            float dy = allyPos.y - enemyPos.y;
            if (dx*dx + dy*dy <= SHIELDING_RANGE * SHIELDING_RANGE) {
                targets.push_back(ally);
            }
        }
    });

    // Apply shields deferred
    for (auto ally : targets) {
      // 给友军添加无敌状态
      if (!registry.all_of<InvulnerableComponent>(ally)) {
        registry.emplace<InvulnerableComponent>(
            ally,
            shieldingDuration,        // duration
            0.0f,                     // elapsed
            enemy,                    // source
            Color{255, 200, 50, 150}, // shieldColor (金色)
            0.0f                      // shieldRadius
        );

        // 添加连线组件
        registry.emplace<LinkComponent>(enemy,
                                        ally,                // target
                                        LinkType::Shielding, // type
                                        2.0f,                // visualWidth
                                        GOLD,                // color
                                        shieldingDuration,   // lifetime
                                        true                 // isActive
        );
      }
    }
  }

  /**
   * @brief 漩涡词缀 - 周期性吸引力场
   */
  static void ProcessVortex(entt::registry &registry, entt::entity enemy,
                            MonsterAffixComponent &affix, size_t affixIdx,
                            float dt, int tier) {
    // Init logic: ensure ForceFieldComponent exists
    if (!registry.all_of<ForceFieldComponent>(enemy)) {
      registry.emplace<ForceFieldComponent>(
          enemy,
          MonsterAffixRegistry::GetScaledValue(
              MonsterAffixRegistry::Params::VORTEX_STRENGTH, tier),
          MonsterAffixRegistry::Params::VORTEX_RADIUS,
          0.0f, // Initial active duration
          0.0f, // unused
          0.0f, // unused
          false // not always on
      );
    }

    auto &ff = registry.get<ForceFieldComponent>(enemy);

    // Timer logic
    bool ready =
        affix.timers[affixIdx] >= MonsterAffixRegistry::Params::VORTEX_INTERVAL;
    affix.timers[affixIdx] =
        NoMoreDay::utils::SelectF(ready, 0.0f, affix.timers[affixIdx]);

    if (ready) {
      ff.activeDuration = MonsterAffixRegistry::Params::VORTEX_DURATION;
      // Visual cue could be added here
      LOG_TRACE("Vortex activated for entity {}", (uint32_t)enemy);
    }

    // Update force field duration
    if (ff.activeDuration > 0.0f) {
      ff.activeDuration -= dt;
    }
  }

  /**
   * @brief 筑墙词缀 - 在玩家周围生成U型墙
   */
  static void ProcessWaller(entt::registry &registry, entt::entity enemy,
                            const Position &enemyPos, const Position &playerPos,
                            MonsterAffixComponent &affix, size_t affixIdx,
                            float dt, int tier) {
    bool ready =
        affix.timers[affixIdx] >= MonsterAffixRegistry::Params::WALLER_COOLDOWN;

    if (ready) {
      float dx = playerPos.x - enemyPos.x;
      float dy = playerPos.y - enemyPos.y;
      float distSq = dx * dx + dy * dy;

      if (distSq < 600.0f * 600.0f) { // Only if reasonably close
        // Reset timer only if triggered
        affix.timers[affixIdx] = 0.0f;

        // Calculate direction to player
        float dist = std::sqrt(distSq);
        float nx = (dist > 0.1f) ? dx / dist : 1.0f;
        float ny = (dist > 0.1f) ? dy / dist : 0.0f;

        // Perpendicular vector
        float perpX = -ny;
        float perpY = nx;

        // Wall dimensions
        float segmentLen = 60.0f;
        float segmentThick = 20.0f;

        // Center point slightly behind player (relative to monster)
        float backDist = 50.0f;
        float cx = playerPos.x + nx * backDist;
        float cy = playerPos.y + ny * backDist;

        float scaledDuration = MonsterAffixRegistry::GetScaledValue(
            MonsterAffixRegistry::Params::WALLER_DURATION, tier);

        // Helper to spawn wall
        auto SpawnWall = [&](float x, float y, float w, float h) {
          auto entity = MapSystem::spawnDynamicObstacle(
              registry, Rectangle{x - w * 0.5f, y - h * 0.5f, w, h},
              scaledDuration);
          registry.emplace<ColorComponent>(entity,
                                           Color{139, 69, 19, 255}); // Brown
        };

        SpawnWall(cx, cy, segmentLen, segmentThick); // Center
        SpawnWall(cx - perpX * 40 - nx * 30, cy - perpY * 40 - ny * 30,
                  segmentThick, segmentLen); // Left
        SpawnWall(cx + perpX * 40 - nx * 30, cy + perpY * 40 - ny * 30,
                  segmentThick, segmentLen); // Right

        LOG_TRACE("Waller cast walls around player");
      }
    }
  }

  [[nodiscard]] static float GetVampiricLifeStealRatio() {
    const auto &def =
        MonsterAffixRegistry::GetAffixDef(MonsterAffixType::Vampiric);
    for (int i = 0; i < def.statModCount; ++i) {
      const auto &statMod = def.statMods[i];
      if (statMod.type == StatType::LifeSteal) {
        return statMod.value / 100.0f;
      }
    }
    return 0.0f;
  }

  static void ProcessSuppressor(entt::registry &registry, entt::entity enemy) {
    (void)registry.get_or_emplace<SuppressorComponent>(enemy);
  }

  static void ProcessSoulLink(entt::registry &registry, entt::entity enemy) {
    (void)registry.get_or_emplace<SoulLinkComponent>(enemy);
    (void)registry.get_or_emplace<SoulLinkTag>(enemy);
  }

  static void ProcessAvengerOnNearbyDeath(entt::registry &registry,
                                          entt::entity avengerEntity,
                                          const Position &avengerPos,
                                          const Position &victimPos) {
    auto &avenger = registry.get_or_emplace<AvengerComponent>(avengerEntity);
    const float dx = victimPos.x - avengerPos.x;
    const float dy = victimPos.y - avengerPos.y;
    const float distSq = dx * dx + dy * dy;
    const float rangeSq = avenger.stackRadius * avenger.stackRadius;
    if (distSq <= rangeSq) {
      avenger.AddStack(1);
    }
  }

  static void ApplyEntanglerOnHit(entt::registry &registry,
                                  const CombatEvent &evt) {
    if (registry.valid(evt.target) && registry.any_of<PlayerTag>(evt.target)) {
      if (NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 1.0f) <
          MonsterAffixRegistry::Params::ENTANGLER_CHANCE) {
        if (auto *effects = registry.try_get<ActiveEffectsComponent>(evt.target)) {
          BuffEffect rootBuff;
          rootBuff.id = "root";
          rootBuff.name = "Rooted";
          rootBuff.description = "Cannot move.";
          rootBuff.type = BuffType::Root;
          rootBuff.duration = MonsterAffixRegistry::Params::ENTANGLER_ROOT_DURATION;
          rootBuff.remaining =
              MonsterAffixRegistry::Params::ENTANGLER_ROOT_DURATION;
          rootBuff.is_debuff = true;
          effects->AddOrRefresh(rootBuff);

          if (auto *pState = registry.try_get<PlayerStats>(evt.target)) {
            pState->isRooted = true;
          }

          LOG_INFO("Entangler rooted player");
        }
      }
    }
  }

  static void ApplyNullifierOnHit(entt::registry &registry,
                                  const CombatEvent &evt) {
    if (!registry.valid(evt.target)) {
      return;
    }

    if (auto *effects = registry.try_get<ActiveEffectsComponent>(evt.target)) {
      effects->effects.erase(
          std::remove_if(effects->effects.begin(), effects->effects.end(),
                         [](const BuffEffect &e) { return !e.is_debuff; }),
          effects->effects.end());

      LOG_INFO("Nullifier dispelled buffs from entity {}", (uint32_t)evt.target);
    }
  }

  static void ApplyToxicOnDeath(entt::registry &registry, entt::entity enemy,
                                const Position &enemyPos) {
    Position playerPos = {0, 0};
    auto playerView = registry.view<PlayerTag, Position>();
    if (playerView.begin() != playerView.end()) {
      playerPos = playerView.get<Position>(playerView.front());
    }

    for (int i = 0; i < 3; i++) {
      auto orbEntity = registry.create();

      float offsetX = NoMoreDay::utils::ThreadSafeRandom::GetFloat(-20.0f, 20.0f);
      float offsetY = NoMoreDay::utils::ThreadSafeRandom::GetFloat(-20.0f, 20.0f);
      registry.emplace<Position>(orbEntity, enemyPos.x + offsetX,
                                 enemyPos.y + offsetY);
      registry.emplace<LocalLevelTag>(orbEntity);
      registry.emplace<VolatileOrbTag>(orbEntity);
      registry.emplace<Radius>(orbEntity, 12.0f);

      float dx = playerPos.x - enemyPos.x;
      float dy = playerPos.y - enemyPos.y;
      float dist = std::sqrt(dx * dx + dy * dy);
      if (dist > 0.1f) {
        float nx = dx / dist;
        float ny = dy / dist;
        registry.emplace<Velocity>(orbEntity, nx * 80.0f, ny * 80.0f);
      } else {
        registry.emplace<Velocity>(orbEntity, 0.0f, 0.0f);
      }

      VolatileOrbComponent orb;
      orb.maxLifetime = 3.0f;
      orb.homingStrength = 200.0f;
      orb.speed = 150.0f;
      orb.owner = enemy;
      registry.emplace<VolatileOrbComponent>(orbEntity, orb);

      registry.emplace<ColorComponent>(orbEntity, Color{100, 255, 100, 200});
    }

    LOG_TRACE("Toxic volatile orbs spawned at ({:.1f}, {:.1f})", enemyPos.x,
              enemyPos.y);
  }

public:
  /**
   * @brief OnDealDamage 回调 - 处理 Nullifier 和 MirrorImage 词缀
   */
  static void OnEnemyDealDamage(entt::registry &registry,
                                const CombatEvent &evt) {
    // Check if attacker has OnHit affixes
    auto *affix = registry.try_get<MonsterAffixComponent>(evt.source);
    if (!affix)
      return;

    const auto eventSet = MonsterModifierAdapter::EvaluateAffixEvents(*affix);
    const auto behaviorOps = MonsterModifierAdapter::EvaluateBehaviorOps(*affix);
    if (!eventSet.HasOnHit() && !affix->hasOnHit && !behaviorOps.HasOnHit())
      return;

    const bool hasVampiricBehaviorOp = behaviorOps.HasOnHitOpcode(
        ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT);
    if (affix->HasAffix(MonsterAffixType::Vampiric) &&
        registry.all_of<HealthComponent>(evt.source) &&
        (hasVampiricBehaviorOp || !behaviorOps.HasOnHit())) {
      const float dealtDamage = CombatEventFactory::GetFinalAppliedDamage(evt);
      const float lifeStealRatio = GetVampiricLifeStealRatio();
      const float healAmount = dealtDamage * lifeStealRatio;
      if (healAmount > 0.0f) {
        auto &attackerHp = registry.get<HealthComponent>(evt.source);
        attackerHp.current = (std::min)(attackerHp.current + healAmount,
                                        attackerHp.max);
      }
    }

    const bool hasEntanglerBehaviorOp = behaviorOps.HasOnHitOpcode(
        ModifierOpCode::MONSTER_BEHAVIOR_ENTANGLER_ON_HIT);
    if (hasEntanglerBehaviorOp) {
      ApplyEntanglerOnHit(registry, evt);
    } else if (affix->HasAffix(MonsterAffixType::Entangler)) {
      ApplyEntanglerOnHit(registry, evt);
    }

    const bool hasNullifierBehaviorOp = behaviorOps.HasOnHitOpcode(
        ModifierOpCode::MONSTER_BEHAVIOR_NULLIFIER_ON_HIT);
    if (hasNullifierBehaviorOp) {
      ApplyNullifierOnHit(registry, evt);
    } else if (affix->HasAffix(MonsterAffixType::Nullifier)) {
      ApplyNullifierOnHit(registry, evt);
    }
  }

  /**
   * @brief OnTakeDamage 回调 - 处理 StormStrider 和 MirrorImage 词缀
   */
  static void OnEnemyTakeDamage(entt::registry &registry,
                                const CombatEvent &evt) {
    const entt::entity defender = evt.source;
    if (!registry.valid(defender)) {
      return;
    }

    // Check Phase Shield
    if (auto *ps = registry.try_get<PhaseShieldComponent>(defender)) {
      if (ps->currentCooldown <= 0.0f) {
        ps->accumulatedDamage +=
            CombatEventFactory::GetFinalAppliedDamage(evt);
        ps->accumulationTimer = ps->accumulationWindow; // Reset timer

        // Check Threshold
        if (auto *hp = registry.try_get<HealthComponent>(defender)) {
          if (ps->accumulatedDamage >= hp->max * ps->triggerThresholdRatio) {
            // Trigger Invulnerability
            if (!registry.all_of<InvulnerableComponent>(defender)) {
              registry.emplace<InvulnerableComponent>(
                  defender, ps->invulnDuration, 0.0f,
                  defender,                 // Self source
                  Color{200, 200, 255, 150}, // Cyan/White shield
                  0.0f);
              LOG_INFO("Phase Shield Triggered for Entity {}! (Burst: {:.1f})",
                       (uint32_t)defender, ps->accumulatedDamage);
            }

            // Reset and Cooldown
            ps->accumulatedDamage = 0.0f;
            ps->currentCooldown = ps->cooldown;
          }
        }
      }
    }

    // Check if defender has OnHit affixes
    auto *affix = registry.try_get<MonsterAffixComponent>(defender);
    if (!affix)
      return;

    const auto eventSet = MonsterModifierAdapter::EvaluateAffixEvents(*affix);
    const auto behaviorOps = MonsterModifierAdapter::EvaluateBehaviorOps(*affix);
    if (!eventSet.HasOnHit() && !affix->hasOnHit && !behaviorOps.HasOnHit())
      return;

    // MirrorImage: Spawn clones on crit or low HP
    const bool hasMirrorImageBehaviorOp = behaviorOps.HasOnHitOpcode(
        ModifierOpCode::MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE);
    bool dispatchMirrorImage = false;
    if (hasMirrorImageBehaviorOp) {
      dispatchMirrorImage = true;
    } else if (affix->HasAffix(MonsterAffixType::MirrorImage)) {
      dispatchMirrorImage = true;
    }

    if (dispatchMirrorImage) {
      static constexpr float MIRROR_COOLDOWN = 10.0f;
      static constexpr float MIRROR_HP_THRESHOLD = 0.5f;

      bool shouldTrigger = false;
      bool triggeredByHpThreshold = false;

      // Trigger on crit (5% chance)
      if (evt.isCrit &&
          NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 1.0f) < 0.05f) {
        shouldTrigger = true;
      }

      // Trigger on HP threshold (once)
      if (!shouldTrigger) {
        auto *hp = registry.try_get<HealthComponent>(defender);
        if (hp && hp->current / hp->max <= MIRROR_HP_THRESHOLD) {
          // Check if already triggered (use mirrorTriggered flag)
          if (!affix->mirrorTriggered) {
            shouldTrigger = true;
            triggeredByHpThreshold = true;
          }
        }
      }

      if (shouldTrigger && affix->mirrorCooldown <= 0.0f) {
        if (triggeredByHpThreshold) {
          affix->mirrorTriggered = true;
        }
        affix->mirrorCooldown = MIRROR_COOLDOWN;

        // Spawn 2 clones
        for (int i = 0; i < 2; ++i) {
          EntityUtils::CloneEntity(registry, defender, 0.5f, 0.1f, 10.0f);
        }

        LOG_INFO("MirrorImage: Entity {} spawned 2 clones",
                 static_cast<uint32_t>(defender));
      }
    }

    // StormStrider: Spawn lightning ghost
    const bool hasStormStriderBehaviorOp = behaviorOps.HasOnHitOpcode(
        ModifierOpCode::MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE);
    if (!hasStormStriderBehaviorOp &&
        !affix->HasAffix(MonsterAffixType::StormStrider))
      return;

    // 概率触发
    if (NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 1.0f) >
        MonsterAffixRegistry::Params::STORMSTRIDER_TRIGGER_CHANCE)
      return;

    // 获取怪物位置
    auto *pos = registry.try_get<Position>(defender);
    if (!pos)
      return;

    // 生成雷电残影
    auto ghostEntity = registry.create();
    registry.emplace<Position>(ghostEntity, pos->x, pos->y);
    registry.emplace<LocalLevelTag>(ghostEntity);
    registry.emplace<LightningGhostTag>(ghostEntity);
    registry.emplace<Radius>(ghostEntity, 15.0f);

    // 雷电残影组件
    LightningGhostComponent ghost;
    ghost.explosionDelay =
        MonsterAffixRegistry::Params::STORMSTRIDER_GHOST_DELAY;
    registry.emplace<LightningGhostComponent>(ghostEntity, ghost);

    // Hazard 组件 (用于 owner 追踪)
    HazardComponent hazard;
    hazard.owner = defender;
    registry.emplace<HazardComponent>(ghostEntity, hazard);

    // 视觉效果（半透明黄色）
    registry.emplace<ColorComponent>(ghostEntity, Color{255, 255, 100, 150});

    LOG_TRACE("Lightning ghost spawned at ({:.1f}, {:.1f})", pos->x, pos->y);
  }

  /**
   * @brief OnDeath 回调 - 处理 Toxic 和 SoulEater 词缀
   */
  static void OnEnemyDeath(entt::registry &registry, entt::entity enemy) {
    auto *affix = registry.try_get<MonsterAffixComponent>(enemy);

    // === SoulEater / Avenger: 全局监听所有敌人死亡 ===
    auto *enemyPos = registry.try_get<Position>(enemy);
    if (enemyPos) {
      // 查找附近的 SoulEater 怪物
      auto soulEaterView =
          registry.view<MonsterAffixComponent, SoulEaterComponent, Position>(
              entt::exclude<KilledTag>);

      for (auto eater : soulEaterView) {
        if (eater == enemy) {
          continue;
        }

        const auto &eaterAffix =
            soulEaterView.get<MonsterAffixComponent>(eater);
        const auto behaviorOps =
            MonsterModifierAdapter::EvaluateBehaviorOps(eaterAffix);
        const bool hasSoulEaterBehaviorOp = behaviorOps.HasOnDeathOpcode(
            ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH);
        bool dispatchSoulEater = false;
        if (hasSoulEaterBehaviorOp) {
          dispatchSoulEater = true;
        } else if (eaterAffix.HasAffix(MonsterAffixType::SoulEater)) {
          dispatchSoulEater = true;
        }
        if (!dispatchSoulEater) {
          continue;
        }

        auto &soulEater = soulEaterView.get<SoulEaterComponent>(eater);
        const auto &eaterPos = soulEaterView.get<Position>(eater);

        float dx = eaterPos.x - enemyPos->x;
        float dy = eaterPos.y - enemyPos->y;
        float distSq = dx * dx + dy * dy;

        if (distSq <= soulEater.stackRadius * soulEater.stackRadius) {
          // 在范围内,增加层数
          if (soulEater.stacks < soulEater.maxStacks) {
            soulEater.stacks++;

            // 触发属性重算
            registry.get_or_emplace<StatsDirty>(eater);

            LOG_INFO("SoulEater: Entity {} gained stack (now {})",
                     static_cast<uint32_t>(eater), soulEater.stacks);
          }
        }
      }

      auto avengerView =
          registry.view<MonsterAffixComponent, Position>(entt::exclude<KilledTag>);

      for (auto avengerEntity : avengerView) {
        if (avengerEntity == enemy) {
          continue;
        }

        const auto &avengerAffix =
            avengerView.get<MonsterAffixComponent>(avengerEntity);
        const auto behaviorOps =
            MonsterModifierAdapter::EvaluateBehaviorOps(avengerAffix);
        const bool hasAvengerBehaviorOp = behaviorOps.HasOnDeathOpcode(
            ModifierOpCode::MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH);
        const bool hasAvengerFallback =
            avengerAffix.HasAffix(MonsterAffixType::Avenger) ||
            registry.all_of<AvengerComponent>(avengerEntity);
        bool dispatchAvenger = false;
        if (hasAvengerBehaviorOp) {
          dispatchAvenger = true;
        } else if (hasAvengerFallback) {
          dispatchAvenger = true;
        }
        if (!dispatchAvenger) {
          continue;
        }

        const auto &avengerPos = avengerView.get<Position>(avengerEntity);
        ProcessAvengerOnNearbyDeath(registry, avengerEntity, avengerPos,
                                    *enemyPos);
      }
    }

    // === Toxic: 死亡时生成毒球 ===
    if (!affix)
      return;

    const auto behaviorOps = MonsterModifierAdapter::EvaluateBehaviorOps(*affix);
    const auto eventSet = MonsterModifierAdapter::EvaluateAffixEvents(*affix);
    if (!eventSet.HasOnDeath() && !affix->hasOnDeath &&
        !behaviorOps.HasOnDeath())
      return;

    const bool hasToxicBehaviorOp = behaviorOps.HasOnDeathOpcode(
        ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH);
    if (!hasToxicBehaviorOp && !affix->HasAffix(MonsterAffixType::Toxic))
      return;

    auto *pos = registry.try_get<Position>(enemy);
    if (!pos)
      return;

    ApplyToxicOnDeath(registry, enemy, *pos);
  }
};

} // namespace NoMoreDay
