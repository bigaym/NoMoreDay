#include "game/systems/combat/CombatSystem.hpp"
#include "game/SharedContext.hpp"
#include "core/logging/Logger.hpp"
#include "game/systems/physics/PhysicsUtils.hpp"
#include "core/utils/Branchless.hpp"
#include "game/scene/SceneManager.hpp"
#include "game/data/BiomeTypes.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Combat.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatConstants.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/CombatFormula.hpp" // Added
#include "game/systems/combat/EffectSystem.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/combat/MovementStanceSystem.hpp"
#include <algorithm>
#include <cmath>

// Bring CombatEvent types into scope
using NoMoreDay::CombatEvent;
using NoMoreDay::CombatEventDispatcher;
using NoMoreDay::CombatEventType;
namespace CombatEventFactory = NoMoreDay::CombatEventFactory;

namespace {

NoMoreDay::DamagePool BuildLegacyAttackBasePool(
    const NoMoreDay::CombatStats *stats, float baseDamage) {
  NoMoreDay::DamagePool basePool;
  float physBase = (stats && stats->max_weapon_damage > 0.1f)
                       ? stats->min_weapon_damage
                       : baseDamage;
  if (physBase > 0.0f) {
    basePool.Add(NoMoreDay::Tag::Physical, physBase);
  }
  return basePool;
}

float LegacyDamageFormula(const NoMoreDay::CombatStats &attacker,
                          const NoMoreDay::CombatStats &defender,
                          float baseDamage, NoMoreDay::DamageType type) {
  using namespace NoMoreDay;
  using namespace NoMoreDay::utils;

  float multiplier = attacker.damage_multipliers[(int)type];
  float effectiveMult = SelectF(multiplier > 0.001f, multiplier, 1.0f);
  float damage = baseDamage * effectiveMult;

  float mitigation = 0.0f;
  float effective_armor = defender.armor - attacker.armor_pen;
  int area_level = defender.cached_area_level;
  if (area_level < 1) {
    area_level = 1;
  }

  float multiplier_val =
      NoMoreDay::CombatFormula::CalculateArmorMultiplier(effective_armor,
                                                         area_level);
  float physMitigation = 1.0f - multiplier_val;

  float res = defender.resistances[(int)type];
  using namespace NoMoreDay::Constants::Combat;
  float elemMitigation = SelectF(res > Cap::RESISTANCE, Cap::RESISTANCE, res);

  bool isPhysical = (type == DamageType::Physical);
  mitigation = SelectF(isPhysical, physMitigation, elemMitigation);
  damage *= (1.0f - mitigation);

  float reduction = defender.damage_reduction;
  reduction = SelectF(reduction > Cap::DR, Cap::DR, reduction);
  float effective_dr = SelectF(reduction > 0.0f, reduction, 0.0f);
  damage *= (1.0f - effective_dr);
  return SelectF(damage > 0.0f, damage, 0.0f);
}

} // namespace

// Static member initialization
// (Assuming any static members are here or removed if not needed)

void CombatSystem::update(entt::registry &registry,
                          NoMoreDay::systems::SpatialHashGrid &grid,
                          const Camera2D &camera, float dt) {
  // LOG_TRACE("CombatSystem::update: 处理战斗逻辑");

  // 不再强制要求 WeaponComponent
  auto view = registry.view<PlayerTag, InputComponent, Position>();

  // 遍历所有玩家（通常只有一个）
  for (auto entity : view) {
    auto &input = view.get<InputComponent>(entity);
    const auto &pos = view.get<Position>(entity);

    // 获取组件指针
    auto *weapon = registry.try_get<WeaponComponent>(entity);
    auto *attackState = registry.try_get<NoMoreDay::AttackState>(entity);
    const auto *stats = registry.try_get<NoMoreDay::CombatStats>(entity);

    // 确定战斗参数
    using namespace NoMoreDay::Constants::Combat;
    float currentCooldownTimer = 0.0f;
    float maxCooldown = System::DEFAULT_ATTACK_COOLDOWN;
    float range = System::DEFAULT_ATTACK_RANGE;
    float knockback = 0.0f;
    float baseDamage = 0.0f;

    if (attackState) {
      // 新系统路径
      currentCooldownTimer = attackState->cooldownTimer;
      maxCooldown = attackState->baseAttackInterval;
      if (stats) {
        if (stats->attack_speed > 0.01f)
          maxCooldown /= stats->attack_speed;
        range = (stats->cast_range > 0.1f)
                    ? stats->cast_range
                    : System::DEFAULT_ATTACK_RANGE; // 默认范围
        knockback = stats->knockback;
      }
    } else if (weapon) {
      // 遗留系统路径
      currentCooldownTimer = weapon->cooldownTimer;
      maxCooldown = weapon->cooldown;
      if (stats && stats->attack_speed > 0.01f)
        maxCooldown /= stats->attack_speed;
      range = weapon->range;
      knockback = weapon->knockback;
      baseDamage = weapon->damage;
    } else {
      // 无法攻击
      continue;
    }

    // 更新冷却
    if (currentCooldownTimer > 0.0f)
      currentCooldownTimer -= dt;

    // 2. 处理攻击
    if (input.attack && currentCooldownTimer <= 0.0f) {
      // 重置冷却时间（使用计算出的有效冷却时间）
      LOG_TRACE("玩家 {} 发起攻击。有效冷却时间: {:.2f}s", (uint32_t)entity,
                maxCooldown);

      currentCooldownTimer = maxCooldown;

      // 计算瞄准方向（玩家 -> 鼠标）
      Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);
      float dx = mouseWorld.x - pos.x;
      float dy = mouseWorld.y - pos.y;
      float len = std::sqrt(dx * dx + dy * dy);

      // 归一化方向
      float dirX = (len > 0) ? dx / len : 1.0f;
      float dirY = (len > 0) ? dy / len : 0.0f;

      // --- 生成攻击特效 (挥剑轨迹) ---
      auto effectEntity = registry.create();
      registry.emplace<Position>(effectEntity, pos.x,
                                 pos.y); // 特效跟随玩家位置(或固定在挥剑处)
      float angleDeg = std::atan2(dirY, dirX) * (180.0f / PI);

      using namespace NoMoreDay::Constants::Combat;
      AttackEffect effect;
      effect.timer = 0.0f;
      effect.lifeTime = System::ATTACK_EFFECT_LIFETIME; // 持续0.2秒
      effect.rotation = angleDeg;
      effect.range = range;
      effect.arcAngle = System::DEFAULT_ATTACK_ARC;
      effect.color = GOLD; // 剑光颜色
      registry.emplace<AttackEffect>(effectEntity, effect);

      // 3. 查询网格中的目标
      // 以玩家为中心，攻击距离为半径进行查询
      bool hitAny = false;
      grid.query(
          {pos.x, pos.y}, range,
          [&](entt::entity target, const Position &tPos) {
            if (target == entity)
              return; // 不要击中自己

            // Validate Target (must be valid)
            if (!registry.valid(target))
              return;

            float dx = tPos.x - pos.x;
            float dy = tPos.y - pos.y;
            float distSq = dx * dx + dy * dy;

            // 1. 距离检查
            if (distSq <= range * range) {
              // 2. 扇形角度检查 (120度)
              float angleToTarget = std::atan2(dy, dx);
              float aimAngle = std::atan2(dirY, dirX);
              float angleDiff = std::abs(angleToTarget - aimAngle);
              if (angleDiff > PI)
                angleDiff = 2.0f * PI - angleDiff;

              using namespace NoMoreDay::Constants::Combat;
              if (angleDiff <=
                  (System::DEFAULT_ATTACK_ARC * 0.5f) * (PI / 180.0f)) {
                // HIT CONFIRMED
                hitAny = true;

                // --- 命中判定 (Accuracy/Miss Check) ---
                // 如果命中率 < 1.0，则有几率未命中
                if (stats && stats->accuracy < 1.0f) {
                  float roll = (float)GetRandomValue(0, 1000) / 1000.0f;
                  if (roll > stats->accuracy) {
                    LOG_DEBUG("Attack missed target {}", (uint32_t)target);
                    return; // 未命中，跳过后续所有判定
                  }
                }

                LOG_DEBUG("Hit confirmed on target {}", (uint32_t)target);

                NoMoreDay::Tag hitTags =
                    NoMoreDay::Tag::Melee | NoMoreDay::Tag::Hit;
                constexpr uint32_t skillId = 0;

                float finalDamage = 0.0f;
                bool isCrit = false;
                bool wasDodged = false;
                bool wasBlocked = false;

#if COMBAT_LEGACY_CALC_ENABLED
                float totalDamage = 0.0f;
                float physBase = (stats && stats->max_weapon_damage > 0.1f)
                                     ? stats->min_weapon_damage
                                     : baseDamage;
                if (stats) {
                  physBase +=
                      stats->flat_damage[(int)NoMoreDay::DamageType::Physical];
                }
                totalDamage += LegacyDamageFormula(
                    stats ? *stats : NoMoreDay::CombatStats{},
                    registry.get_or_emplace<NoMoreDay::CombatStats>(target),
                    physBase, NoMoreDay::DamageType::Physical);
                if (stats) {
                  using namespace NoMoreDay::Constants::Combat::Pipeline;
                  for (int i = 1; i < ELEMENTAL_TYPE_COUNT; ++i) {
                    if (stats->flat_damage[i] > 0.01f) {
                      totalDamage += LegacyDamageFormula(
                          *stats,
                          registry.get_or_emplace<NoMoreDay::CombatStats>(
                              target),
                          stats->flat_damage[i], (NoMoreDay::DamageType)i);
                    }
                  }
                }
                finalDamage = totalDamage;
#else
                NoMoreDay::DamageRequest damageReq;
                damageReq.attacker = entity;
                damageReq.defender = target;
                damageReq.skill_id = skillId;
                damageReq.base_pool = BuildLegacyAttackBasePool(stats, baseDamage);
                damageReq.additional_tags = hitTags;
                auto execution =
                    NoMoreDay::DamagePipeline::Execute(registry, damageReq, entity);
                finalDamage = execution.damage.total_damage;
                isCrit = execution.damage.is_crit;
                wasDodged = execution.damage.was_dodged;
                wasBlocked = execution.damage.was_blocked;
                if (wasDodged) {
                  if (registry.all_of<Position>(target)) {
                    NoMoreDay::systems::EffectSystem::EmitStatusPopup(
                        registry, {tPos.x, tPos.y}, "闪避", WHITE);
                  }
                  return;
                }
                if (wasBlocked && registry.all_of<Position>(target)) {
                  NoMoreDay::systems::EffectSystem::EmitStatusPopup(
                      registry, {tPos.x, tPos.y}, "格挡", SKYBLUE);
                }
                LOG_TRACE("Combat(Pipeline): BasePhys={:.1f}, FinalDmg={:.1f}, Target={}",
                          damageReq.base_pool.Get(NoMoreDay::Tag::Physical),
                          finalDamage, (uint32_t)target);
#endif

#if COMBAT_LEGACY_CALC_ENABLED
                // 3. 暴击判定 (作用于最终总伤害)
                if (stats && registry.all_of<NoMoreDay::CombatStats>(target)) {
                  float roll = (float)GetRandomValue(0, 1000) / 1000.0f;
                  // 应用暴击率上限
                  using namespace NoMoreDay::Constants::Combat;
                  float effectiveCrit =
                      (std::min)(stats->crit_chance, Cap::CRIT_CHANCE);
                  if (roll < effectiveCrit) {
                    isCrit = true;
                    using namespace NoMoreDay::Constants::Combat;
                    finalDamage *= (stats->crit_damage > 0.1f
                                        ? stats->crit_damage
                                        : System::CRIT_DAMAGE_FALLBACK);
                  }
                }

                // --- Event System: OnSkillHit (Delayed) ---
                CombatEvent hit_evt = CombatEventFactory::CreateSkillHit(
                    entity, target, skillId, hitTags, isCrit);
                CombatEventDispatcher::Dispatch(registry, hit_evt);
#endif

                // Apply Knockback after defense order confirms the hit was not
                // dodged.
                NoMoreDay::Utils::ApplyKnockback(registry, target,
                                                 {pos.x, pos.y}, knockback);

                // Apply Damage
                if (registry.all_of<HealthComponent>(target)) {
                  // 应用伤害逻辑（这会处理生命值减少和死亡并生成飘字）
#if COMBAT_LEGACY_CALC_ENABLED
                  bool targetDead = ApplyDamage(registry, target, finalDamage,
                                                entity, isCrit);
#else
                  bool targetDead = execution.target_killed;
#endif
                  LOG_DEBUG("对 {} 造成 {:.1f} 伤害 (暴击: {}, 死亡: {})",
                            (uint32_t)target, finalDamage, isCrit, targetDead);

                  // --- 荆棘伤害 (Thorns) ---
                  if (!targetDead &&
                      registry.all_of<NoMoreDay::CombatStats>(target)) {
                    const auto &tStats =
                        registry.get<NoMoreDay::CombatStats>(target);
                    if (tStats.thorns > 0.0f) {
#if COMBAT_LEGACY_CALC_ENABLED
                      // 反伤给攻击者
                      ApplyDamage(registry, entity, tStats.thorns, target,
                                  false);
#else
                      NoMoreDay::DamagePool thornsPool;
                      thornsPool.Add(NoMoreDay::Tag::Physical, tStats.thorns);
                      NoMoreDay::DamageRequest thornsReq;
                      thornsReq.attacker = target;
                      thornsReq.defender = entity;
                      thornsReq.skill_id = 0;
                      thornsReq.base_pool = thornsPool;
                      thornsReq.skip_mitigation = true;
                      thornsReq.thorns_like_damage = true;
                      (void)NoMoreDay::DamagePipeline::Execute(registry, thornsReq,
                                                               target);
#endif
                      LOG_TRACE("Thorns: Entity {} took {:.1f} damage",
                                (uint32_t)entity, tStats.thorns);
                    }
                  }

                  // --- 生命偷取 & 击中回复 ---
                  if (stats && registry.all_of<HealthComponent>(entity)) {
                    float healAmount = 0.0f;
                    if (stats->life_steal > 0.0f)
                      healAmount += finalDamage * stats->life_steal;

                    if (healAmount > 0.0f) {
                      auto &attackerHp = registry.get<HealthComponent>(entity);
                      attackerHp.current += healAmount;
                      if (attackerHp.current > attackerHp.max)
                        attackerHp.current = attackerHp.max;
                      // 可选：在这里添加治疗飘字或特效
                    }
                  }
                } else { // 如果目标没有生命值组件
                  LOG_LIMITED_WARN(1.0f,
                                   "Target {} hit but has no HealthComponent",
                                   (uint32_t)target);
                  // For particles/props without health, maybe just destroy or
                  // knockback? For now, let's just knock them back hard.
                }
              }
            } // Close angleDiff
          });

      if (!hitAny) {
        LOG_TRACE("攻击未命中任何目标（查询半径: {:.1f}）", range);
      }
    }

    // 写回冷却时间
    if (attackState)
      attackState->cooldownTimer = currentCooldownTimer;
    if (weapon)
      weapon->cooldownTimer = currentCooldownTimer;
  }

  // --- Enemy Attack Logic ---
  auto enemyView =
      registry.view<EnemyTag, AIComponent, Position, NoMoreDay::AttackState,
                    NoMoreDay::CombatStats>();
  for (auto enemy : enemyView) {
    auto &ai = enemyView.get<AIComponent>(enemy);
    auto &ePos = enemyView.get<Position>(enemy);
    auto &eAttack = enemyView.get<NoMoreDay::AttackState>(enemy);
    const auto &eStats = enemyView.get<NoMoreDay::CombatStats>(enemy);

    // Update Cooldown
    if (eAttack.cooldownTimer > 0.0f) {
      eAttack.cooldownTimer -= dt;
    }

    // Only attack if in ATTACK state and cooldown is ready
    if (ai.aiType == AIType::ATTACK && eAttack.cooldownTimer <= 0.0f) {
      if (registry.valid(ai.target) && registry.all_of<Position>(ai.target)) {
        const auto &tPos = registry.get<Position>(ai.target);
        float dx = tPos.x - ePos.x;
        float dy = tPos.y - ePos.y;
        float distSq = dx * dx + dy * dy;

        // Precision range check
        if (distSq <= ai.attackRange * ai.attackRange) {
          // Start Attack
          float interval = eAttack.baseAttackInterval;
          if (eStats.attack_speed > 0.01f)
            interval /= eStats.attack_speed;
          eAttack.cooldownTimer = interval;

          // Calculate Damage
          float basePhys =
              eStats.min_weapon_damage +
              (eStats.max_weapon_damage - eStats.min_weapon_damage) *
                  ((float)GetRandomValue(0, 1000) / 1000.0f);

          float finalDamage = 0.0f;
          bool isCrit = false;
          bool wasDodged = false;
          bool wasBlocked = false;
#if COMBAT_LEGACY_CALC_ENABLED
          finalDamage = LegacyDamageFormula(
              eStats, registry.get_or_emplace<NoMoreDay::CombatStats>(ai.target),
              basePhys, NoMoreDay::DamageType::Physical);
#else
          NoMoreDay::DamagePool basePool;
          basePool.Add(NoMoreDay::Tag::Physical, basePhys);
          NoMoreDay::DamageRequest damageReq;
          damageReq.attacker = enemy;
          damageReq.defender = ai.target;
          damageReq.skill_id = 0;
          damageReq.base_pool = basePool;
          damageReq.additional_tags = NoMoreDay::Tag::Melee | NoMoreDay::Tag::Hit;
          auto execution =
              NoMoreDay::DamagePipeline::Execute(registry, damageReq, enemy);
          finalDamage = execution.damage.total_damage;
          isCrit = execution.damage.is_crit;
          wasDodged = execution.damage.was_dodged;
          wasBlocked = execution.damage.was_blocked;
#endif

          if (wasDodged) {
            if (registry.all_of<Position>(ai.target)) {
              NoMoreDay::systems::EffectSystem::EmitStatusPopup(
                  registry, {tPos.x, tPos.y}, "闪避", WHITE);
            }
            continue;
          }

          if (wasBlocked) {
            if (registry.all_of<Position>(ai.target)) {
              NoMoreDay::systems::EffectSystem::EmitStatusPopup(
                  registry, {tPos.x, tPos.y}, "格挡", SKYBLUE);
            }
          }

          LOG_TRACE("Monster {} attacked {} for {:.1f} damage",
                    (uint32_t)enemy, (uint32_t)ai.target, finalDamage);
        }
      }
    }
  }
}

float CombatSystem::CalculateDamage(const NoMoreDay::CombatStats &attacker,
                                    const NoMoreDay::CombatStats &defender,
                                    float baseDamage,
                                    NoMoreDay::DamageType type) {
#if COMBAT_LEGACY_CALC_ENABLED
  return LegacyDamageFormula(attacker, defender, baseDamage, type);
#else
  (void)attacker;
  (void)defender;
  (void)baseDamage;
  (void)type;
  return 0.0f;
#endif
}

bool CombatSystem::ApplyDamage(entt::registry &registry, entt::entity target,
                               float amount, entt::entity attacker, bool isCrit,
                               bool showVFX,
                               DamageApplyResult *applyResult) {
  auto commitApplyResult = [&](float healthApplied, float barrierAbsorbed,
                               bool wasPrevented) {
    if (!applyResult) {
      return;
    }
    applyResult->requested_damage = amount;
    applyResult->health_applied = healthApplied;
    applyResult->barrier_absorbed = barrierAbsorbed;
    applyResult->was_prevented = wasPrevented;
  };

  if (!registry.valid(target) || !registry.all_of<HealthComponent>(target)) {
    commitApplyResult(0.0f, 0.0f, true);
    return false;
  }

  // --- Phantom Flash Riposte ---
  if (auto *pf = registry.try_get<NoMoreDay::PhantomFlashComponent>(target)) {
    if (!pf->triggered) {
      pf->triggered = true;
      LOG_INFO("Phantom Flash triggered! Riposte on entity {}",
               (uint32_t)attacker);

      if (registry.valid(attacker) && registry.all_of<Position>(attacker)) {
        const auto &aPos = registry.get<Position>(attacker);
        const auto &tPos = registry.get<Position>(target);

        // Teleport behind attacker (approx)
        Vector2 dir = Vector2Normalize(
            Vector2Subtract({tPos.x, tPos.y}, {aPos.x, aPos.y}));
        Vector2 ripostePos =
            Vector2Add({aPos.x, aPos.y}, Vector2Scale(dir, 20.0f));

        auto &targetPosComp = registry.get<Position>(target);
        targetPosComp.x = ripostePos.x;
        targetPosComp.y = ripostePos.y;

        // Shadow Riposte (Cast Flowing Thrust ID 1 as riposte)
        NoMoreDay::SkillSystem::ShadowCast(registry, target, 1, ripostePos,
                                           {aPos.x, aPos.y});

        // Apply Stealth (Invisibility / Aggro Drop)
        auto &effects =
            registry.get_or_emplace<NoMoreDay::ActiveEffectsComponent>(target);
        NoMoreDay::BuffEffect stealth;
        stealth.id = "stealth";
        stealth.name = "Stealth";
        stealth.description = "Invisible to enemies";
        stealth.type = NoMoreDay::BuffType::None;
        stealth.duration = 2.0f;
        stealth.remaining = 2.0f;
        // Optionally add a visual modifier or tag
        effects.AddOrRefresh(stealth);

        LOG_INFO("Phantom Flash: Entity {} entered Stealth", (uint32_t)target);
      }

      commitApplyResult(0.0f, 0.0f, true);
      return false; // Damage blocked
    }
  }

  // Interrupt movement stance on damage
  NoMoreDay::MovementStanceSystem::OnTakeDamage(registry, target);

  auto &hp = registry.get<HealthComponent>(target);

  // 如果已经打上了死亡标记，直接返回（防止重复结算和回血复活后的逻辑干扰）
  if (registry.all_of<KilledTag>(target)) {
    hp.current = 0.0f; // 强制锁定
    commitApplyResult(0.0f, 0.0f, true);
    return true;
  }

  const float healthBeforeDamage = hp.current;
  float remainingDamage = amount;
  float barrierDamage = 0.0f;

  // --- Hybrid Barrier: Damage Absorption ---
  // Priority: Barrier absorbs damage before Health
  // Note: Chaos/True damage bypass is currently handled at DamagePipeline level
  if (auto *barrier = registry.try_get<BarrierComponent>(target)) {
    auto *stats = registry.try_get<NoMoreDay::CombatStats>(target);
    if (stats && stats->barrier > 0.0f) {
      // Update last_damage_time to reset ES regeneration delay
      barrier->last_damage_time = static_cast<float>(GetTime());

      // Calculate barrier absorption
      if (stats->barrier >= remainingDamage) {
        // Barrier absorbs all damage
        barrierDamage = remainingDamage;
        stats->barrier -= remainingDamage;
        remainingDamage = 0.0f;
      } else {
        // Barrier absorbs partial damage
        barrierDamage = stats->barrier;
        remainingDamage -= stats->barrier;
        stats->barrier = 0.0f;
      }

      LOG_TRACE("Barrier absorbed {:.1f} damage, remaining: {:.1f}, barrier "
                "left: {:.1f}",
                barrierDamage, remainingDamage, stats->barrier);
    }
  }

  // Apply remaining damage to health
  hp.current -= remainingDamage;
  const float healthDamageApplied =
      (healthBeforeDamage > 0.0f)
          ? (std::min)(healthBeforeDamage, (std::max)(0.0f, remainingDamage))
          : 0.0f;

  // --- Unified Damage Popup (Gated by showVFX for performance) ---
  if (showVFX && registry.all_of<Position>(target)) {
    const auto &tPos = registry.get<Position>(target);
    NoMoreDay::systems::EffectSystem::EmitDamagePopup(
        registry, {tPos.x, tPos.y}, amount, isCrit);

    // Screen Shake for heavy damage
    using namespace NoMoreDay::Constants::Combat;
    if (amount > System::SCREEN_SHAKE_THRESHOLD) {
      RenderSystem::AddScreenShake(0.15f);
    }
  }

  if (hp.current <= 0) {
    // --- Blade Formation: Immortality (Node 322) ---
    if (auto *formation =
            registry.try_get<NoMoreDay::BladeFormationComponent>(target)) {
      if (formation->immortality_ready) {
        formation->immortality_ready = false;
        float heal = hp.max * 0.3f;
        hp.current = heal;
        LOG_INFO("Blade Formation Immortality (322) triggered for entity {}! "
                 "Restored {:.1f} HP",
                 (uint32_t)target, heal);

        // Visual Effect for Immortality
        if (registry.all_of<Position>(target)) {
          const auto &tPos = registry.get<Position>(target);
          NoMoreDay::systems::EffectSystem::EmitStatusPopup(
              registry, {tPos.x, tPos.y}, "不灭剑魂", GOLD);
          auto &particleSys = NoMoreDay::systems::GPUParticleSystem::Get();
          auto splash = NoMoreDay::systems::InkEffectHelper::CreateInkSplash(
              {tPos.x, tPos.y}, 20, 15.0f, 200.0f);
          for (auto &p : splash) {
            p.color = GOLD;
            particleSys.Emit(p);
          }
          RenderSystem::AddScreenShake(0.3f);
        }
        commitApplyResult(healthDamageApplied, barrierDamage, true);
        return false; // Death prevented
      }
    }

    // Calculate overkill before zeroing HP
    float overkill = (hp.current < 0.0f) ? -hp.current : 0.0f;

    hp.current = 0.0f; // 锁定生命值为0

    // --- OPTIMIZATION: Immediate Logical and Visual Removal ---
    // 立即移除战斗标签，使其无法被后续技能搜寻到
    if (registry.all_of<EnemyTag>(target))
      registry.remove<EnemyTag>(target);
    if (registry.all_of<AIComponent>(target))
      registry.remove<AIComponent>(target);
    if (registry.all_of<SpriteComponent>(target))
      registry.remove<SpriteComponent>(target);

    // Player death flow: recover resources and return to town instead of
    // entering the generic KilledTag GC pipeline.
    if (registry.all_of<PlayerTag>(target)) {
      hp.current = hp.max;

      if (auto *stats = registry.try_get<NoMoreDay::CombatStats>(target)) {
        stats->health = stats->max_health;
        stats->mana = stats->max_mana;
        stats->barrier = stats->max_barrier;
      }

      if (auto *playerStats = registry.try_get<PlayerStats>(target)) {
        playerStats->deathCount++;
      }

      if (registry.ctx().contains<NoMoreDay::SharedContext *>()) {
        NoMoreDay::SharedContext *ctx =
            registry.ctx().get<NoMoreDay::SharedContext *>();
        if (ctx && ctx->sceneManager) {
          ctx->sceneManager->ClearOriginInfo();
          ctx->sceneManager->RequestTransition(NoMoreDay::BiomeID::Town, 1,
                                               "player_death_return");
        }
      }

      LOG_INFO("Player {} died and was returned to town with full HP/MP.",
               static_cast<uint32_t>(target));
      commitApplyResult(healthDamageApplied, barrierDamage, false);
      return true;
    }

    registry.emplace<KilledTag>(target, attacker);

    // --- Event System: OnKill ---
    CombatEvent kill_evt =
        CombatEventFactory::CreateOnKill(attacker, target, overkill);
    CombatEventDispatcher::Dispatch(registry, kill_evt);

    // --- Monster Affix System: OnDeath ---
    NoMoreDay::MonsterAffixSystem::OnEnemyDeath(registry, target);

    // --- Event System: OnOverkill (if significant overkill damage) ---
    if (overkill > 1.0f) {
      CombatEvent overkill_evt = CombatEventFactory::CreateOnOverkill(
          attacker, target, overkill, amount);
      CombatEventDispatcher::Dispatch(registry, overkill_evt);
    }

    // 处理击杀奖励 (Moved relevant parts to XPAwardingSystem)
    // Note: Actual item dropping is handled by DropSystem
    if (registry.valid(attacker) && registry.all_of<PlayerStats>(attacker)) {
      auto &playerStats = registry.get<PlayerStats>(attacker);
      playerStats.killCount++;
    }

    commitApplyResult(healthDamageApplied, barrierDamage, false);
    return true;
  }

  commitApplyResult(healthDamageApplied, barrierDamage, false);
  return false;
}
