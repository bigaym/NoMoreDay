#include "game/systems/skill/BoomerangDeliverySystem.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/skill/ElementPathSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/StunApplication.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "engine/render/GPUParticleSystem.hpp"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace NoMoreDay {
namespace {

// 回旋体统一接刃阈值：全工程唯一权威判定，避免双状态机各执一套
constexpr float kReturnCatchThreshold = 32.0f;
// 870/872 元素路径沿轨迹生成的节流间隔
constexpr float kElementPathSpawnInterval = 0.05f;
// 810 滞空切割节拍：每 0.2s 一次
constexpr float kHoverCutInterval = 0.2f;
// 873 接刃电磁爆发的默认半径（未配置牵引半径时）
constexpr float kElectromagneticBurstRadius = 80.0f;

// 轻型/中型敌人筛选：Boss 与重甲(TANK)视为重型，不受 850/851 折返牵引
bool IsHeavyEnemy(entt::registry &registry, entt::entity e) {
  if (registry.any_of<BossBattleComponent>(e)) {
    return true;
  }
  if (const auto *state = registry.try_get<EnemyStateComponent>(e)) {
    return state->archetypeType == EnemyArchetype::TANK;
  }
  return false;
}

// 通用百分比增益/减益附加：刷新叠加交给 AddOrRefresh
void AddPercentBuff(entt::registry &registry, entt::entity owner,
                    std::string_view id, const char *name, BuffType type,
                    StatType stat, float value, float duration, int maxStacks,
                    bool isDebuff) {
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
  BuffEffect buff;
  buff.id.assign(id);
  buff.name = name;
  buff.type = type;
  buff.duration = duration;
  buff.remaining = duration;
  buff.max_stacks = std::max(1, maxStacks);
  buff.is_debuff = isDebuff;
  buff.modifiers.push_back(
      {.value = value, .type = stat, .mode = ModifierMode::PercentAdd});
  effects.AddOrRefresh(buff);
  registry.get_or_emplace<StatsDirty>(owner);
}

// 折返阶段击杀标记：接刃时供 835 判定使用
void MarkReturningKills(entt::registry &registry, entt::entity blade,
                        BoomerangComponent &bc) {
  const auto *proj = registry.try_get<Projectile>(blade);
  if (proj == nullptr) {
    return;
  }
  const auto killed = [&](entt::entity hit) {
    return hit != entt::null && registry.valid(hit) &&
           registry.all_of<KilledTag>(hit);
  };
  // 折返起点已 ClearHits（hit_count 归零），只统计折返阶段新命中
  const uint16_t inlineCount = std::min<uint16_t>(
      proj->hit_count, static_cast<uint16_t>(proj->hit_cache.size()));
  for (uint16_t i = 0; i < inlineCount; ++i) {
    if (killed(proj->hit_cache[i])) {
      bc.returning_kill_occurred = true;
      return;
    }
  }
  for (auto hit : proj->overflow_hits) {
    if (killed(hit)) {
      bc.returning_kill_occurred = true;
      return;
    }
  }
}

// 810 滞空切割：对半径内敌人派发一次技能命中，使 811/813/852 等节点自然生效
void ApplyHoverCut(entt::registry &registry, systems::SpatialHashGrid &grid,
                   entt::entity blade, const BoomerangComponent &bc,
                   const Vector2 &bladePos) {
  if (!registry.valid(bc.owner)) {
    return;
  }
  const auto hit = SkillBehaviorRegistry::GetHit(bc.skill_id);
  if (hit == nullptr) {
    return;
  }
  const auto *proj = registry.try_get<Projectile>(blade);
  const float radius =
      (proj != nullptr && proj->radius > 0.0f) ? proj->radius : 40.0f;
  grid.query({bladePos.x, bladePos.y}, radius,
             [&](entt::entity target, const Position &) {
               if (!registry.any_of<EnemyTag>(target) ||
                   registry.any_of<KilledTag>(target)) {
                 return;
               }
               hit(registry, bc.owner, target, Tag::Hit, false);
             });
}

// 850/851 折返路径牵引：沿路径把轻/中型敌人拉向施法者，范围由组件字段共同决定
void ApplyReturnPull(entt::registry &registry, systems::SpatialHashGrid &grid,
                     const BoomerangComponent &bc, const Vector2 &bladePos,
                     const Vector2 &targetPos, float dt) {
  const float radius =
      bc.pull_radius * (bc.pull_radius_mult > 0.0f ? bc.pull_radius_mult : 1.0f);
  if (radius <= 0.0f || bc.pull_strength <= 0.0f) {
    return;
  }
  grid.query({bladePos.x, bladePos.y}, radius,
             [&](entt::entity target, const Position &tpos) {
               if (!registry.any_of<EnemyTag>(target) ||
                   registry.any_of<KilledTag>(target)) {
                 return;
               }
               if (IsHeavyEnemy(registry, target)) {
                 return;
               }
               auto *tvel = registry.try_get<Velocity>(target);
               if (tvel == nullptr) {
                 return;
               }
               Vector2 toward = {targetPos.x - tpos.x, targetPos.y - tpos.y};
               const float dist = Vector2Length(toward);
               if (dist <= 1.0f) {
                 return;
               }
               const Vector2 dir = Vector2Scale(toward, 1.0f / dist);
               tvel->vx += dir.x * bc.pull_strength * dt;
               tvel->vy += dir.y * bc.pull_strength * dt;
             });
}

// 870/872 元素路径：按节流沿上一帧到当前帧的飞行线段生成残留
void EmitElementPath(entt::registry &registry, entt::entity blade,
                     BoomerangComponent &bc, const Position &pos,
                     const Velocity &vel, float dt) {
  if (bc.element_path_tag == Tag::None) {
    return;
  }
  bc.path_spawn_timer -= dt;
  if (bc.path_spawn_timer > 0.0f) {
    return;
  }
  bc.path_spawn_timer = kElementPathSpawnInterval;

  const auto *proj = registry.try_get<Projectile>(blade);
  const float baseHalfWidth =
      (proj != nullptr && proj->radius > 0.0f) ? proj->radius * 0.5f : 12.0f;

  element_path::SpawnParams params{};
  params.owner = bc.owner;
  params.cast_id = bc.cast_id;
  params.skill_id = bc.skill_id;
  params.element_tag = bc.element_path_tag;
  params.start = {pos.x - vel.vx * dt, pos.y - vel.vy * dt};
  params.end = {pos.x, pos.y};
  params.half_width = baseHalfWidth * bc.path_width_mult;
  params.duration = 3.0f * bc.path_duration_mult;
  params.amp = bc.path_amp;
  params.penetration = bc.path_pen;
  params.arc_freq_mult = bc.arc_freq_mult;
  element_path::Spawn(registry, params);
}

// 接刃统一结算入口：832/833/834/815/835/873 全部在此处理
void HandleCatch(entt::registry &registry, entt::entity blade,
                 BoomerangComponent &bc) {
  const entt::entity owner = bc.owner;
  if (!registry.valid(owner)) {
    return;
  }

  // 832 接剑：同一 cast 的多把侧刃只结算一次回蓝
  if (bc.catch_mana > 0.0f) {
    if (auto *stats = registry.try_get<CombatStats>(owner)) {
      stats->mana = std::min(stats->max_mana, stats->mana + bc.catch_mana);
      registry.get_or_emplace<StatsDirty>(owner);
    }
    for (auto other : registry.view<BoomerangComponent>()) {
      auto &otherBc = registry.get<BoomerangComponent>(other);
      if (otherBc.owner == owner && otherBc.cast_id == bc.cast_id) {
        otherBc.catch_mana = 0.0f;
      }
    }
  }

  // 833 连环劲：接剑后攻速提升，2s 内无限叠加刷新
  if (bc.combo_attack_speed > 0.0f) {
    AddPercentBuff(registry, owner,
                   BuffIdToString(BuffId::BladeBoomerangCombo),
                   "Blade Boomerang Combo", BuffType::AttackUp,
                   StatType::AttackSpeed, bc.combo_attack_speed, 2.0f, 99,
                   false);
  }

  // 834 御剑接踵：处于剑步时延长剑步，并施加下次投掷免蓝
  if (bc.step_extend_sec > 0.0f) {
    if (auto *step = skills::seven_star_shared::FindBuff(
            registry, owner, BuffIdToString(BuffId::SwordStep))) {
      step->remaining += bc.step_extend_sec;
      step->duration += bc.step_extend_sec;
      registry.get_or_emplace<StatsDirty>(owner);

      auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
      BuffEffect freeCast;
      freeCast.id.assign(BuffIdToString(BuffId::BladeBoomerangFreeCast));
      freeCast.name = "Blade Boomerang Free Cast";
      freeCast.type = BuffType::PowerBoost;
      freeCast.kind = BuffKind::FreeCast;
      freeCast.source_skill_id = bc.skill_id;
      freeCast.duration = 3.0f;
      freeCast.remaining = 3.0f;
      effects.AddOrRefresh(freeCast);
    }
  }

  // 815 风眼：本次飞行累计流血伤害按比例转化为治疗
  if (bc.heal_bleed_pct > 0.0f && bc.bleed_damage_pool > 0.0f) {
    const float heal = bc.heal_bleed_pct * bc.bleed_damage_pool;
    if (heal > 0.0f) {
      if (auto *hp = registry.try_get<HealthComponent>(owner)) {
        hp->current = std::min(hp->max, hp->current + heal);
      }
    }
  }

  // 835 回旋游步：折返期间有击杀时，接剑净化减速/定身并给予短暂移速
  if (bc.returning_kill_occurred) {
    if (auto *effects = registry.try_get<ActiveEffectsComponent>(owner)) {
      std::erase_if(effects->effects, [](const BuffEffect &b) {
        // 净化范围以 BuffType 整数比较：减速/冻结/定身（含技能5 Chill 的 SpeedDown 载体）
        return b.type == BuffType::SpeedDown || b.type == BuffType::Freeze ||
               b.type == BuffType::Root;
      });
    }
    AddPercentBuff(registry, owner,
                   BuffIdToString(BuffId::BladeBoomerangSwift),
                   "Blade Boomerang Swift", BuffType::SpeedUp,
                   StatType::MoveSpeed, 20.0f, 2.0f, 1, false);
  }

  // 873 接刃电磁爆发：仅闪电路径（872）在接刃时触发
  if (bc.element_path_tag == Tag::Lightning &&
      registry.all_of<Position>(owner)) {
    const auto &ownerPos = registry.get<Position>(owner);
    const float radius =
        bc.pull_radius > 0.0f ? bc.pull_radius : kElectromagneticBurstRadius;
    element_path::Detonate(registry, owner, bc.element_path_tag,
                           {ownerPos.x, ownerPos.y}, radius);
  }
}

} // namespace

void BoomerangDeliverySystem::Update(entt::registry &registry,
                                     systems::SpatialHashGrid &grid, float dt) {
  static thread_local std::vector<entt::entity> s_to_destroy;
  s_to_destroy.clear();

  // 统一进入折返：清空命中缓存并一次性应用折返伤害倍率
  const auto enterReturning = [&](entt::entity e, BoomerangComponent &b,
                                  Velocity &v) {
    b.phase = BoomerangPhase::Returning;
    v.vx = 0.0f;
    v.vy = 0.0f;
    if (auto *proj = registry.try_get<Projectile>(e)) {
      proj->ClearHits();
      if (b.returning_damage_mult != 1.0f) {
        if (proj->payload_context.has_value()) {
          proj->payload_context->more_damage *= b.returning_damage_mult;
        }
        for (auto &m : proj->snapshot.damage_multipliers) {
          m *= b.returning_damage_mult;
        }
      }
    }
  };

  auto view = registry.view<BoomerangComponent, Position, Velocity>();
  for (auto entity : view) {
    auto &bc = view.get<BoomerangComponent>(entity);
    auto &pos = view.get<Position>(entity);
    auto &vel = view.get<Velocity>(entity);
    const bool isBladeBoomerang = (bc.skill_id == 8u);

    // 870/872：元素路径在飞行各阶段持续沿轨迹生成
    if (isBladeBoomerang) {
      EmitElementPath(registry, entity, bc, pos, vel, dt);
    }

    switch (bc.phase) {
    case BoomerangPhase::Outward: {
      bool reached = false;
      const bool ownerHasPos =
          registry.valid(bc.owner) && registry.all_of<Position>(bc.owner);
      if (isBladeBoomerang && ownerHasPos) {
        // 设计 §3.8：飞出至最大距离立即折返
        const auto &ownerPos = registry.get<Position>(bc.owner);
        const float maxDist = bc.max_distance > 0.0f ? bc.max_distance : 300.0f;
        const float dx = pos.x - ownerPos.x;
        const float dy = pos.y - ownerPos.y;
        reached = (dx * dx + dy * dy) >= maxDist * maxDist;
      } else {
        // 技能2等保持回旋计时折返语义（回归不变）
        bc.returnTimer -= dt;
        reached = bc.returnTimer <= 0.0f;
      }

      if (!reached) {
        break;
      }

      bc.apex_position = {pos.x, pos.y};

      // 顶点冲击波粒子：由原 ProjectileSystem 回旋分支迁移而来，技能2/8 共用
      if (const auto *proj = registry.try_get<Projectile>(entity)) {
        components::GPUParticle shockwave;
        shockwave.position = {pos.x, pos.y};
        shockwave.velocity = {0.0f, 0.0f};
        shockwave.color = {180, 240, 255, 200};
        shockwave.lifetime = 0.4f;
        shockwave.maxLifetime = 0.4f;
        shockwave.scale = proj->radius;
        shockwave.flags = 2;           // Glow
        shockwave.growthRate = 120.0f; // 快速膨胀
        systems::GPUParticleSystem::Get().Emit(shockwave);
      }

      // 仅配置了 hover_duration 的版本（810=0.8s）在顶点悬停
      if (bc.hover_duration > 0.0f) {
        bc.phase = BoomerangPhase::HoverApex;
        bc.hover_timer = bc.hover_duration;
        bc.returnTimer = 0.0f; // 悬停切割节拍累计器
        vel.vx = 0.0f;
        vel.vy = 0.0f;
      } else {
        enterReturning(entity, bc, vel);
      }
      break;
    }

    case BoomerangPhase::HoverApex: {
      vel.vx = 0.0f;
      vel.vy = 0.0f;
      bc.hover_timer -= dt;

      if (isBladeBoomerang) {
        // 810 滞空切割：每 0.2s 派发一次 AoE 命中
        bc.returnTimer += dt;
        while (bc.returnTimer >= kHoverCutInterval) {
          bc.returnTimer -= kHoverCutInterval;
          ApplyHoverCut(registry, grid, entity, bc, {pos.x, pos.y});
        }
      } else if (bc.pull_radius > 0.0f && bc.pull_strength > 0.0f) {
        // 技能2顶点的引力牵引保持不变
        grid.query({pos.x, pos.y}, bc.pull_radius,
                   [&](entt::entity target, const Position &tpos) {
                     if (!registry.any_of<EnemyTag>(target) ||
                         registry.any_of<KilledTag>(target)) {
                       return;
                     }
                     if (auto *tvel = registry.try_get<Velocity>(target)) {
                       Vector2 toCenter =
                           Vector2Subtract({pos.x, pos.y}, {tpos.x, tpos.y});
                       const float dist = Vector2Length(toCenter);
                       if (dist > 1.0f) {
                         const Vector2 pullDir =
                             Vector2Scale(toCenter, 1.0f / dist);
                         tvel->vx += pullDir.x * bc.pull_strength * dt;
                         tvel->vy += pullDir.y * bc.pull_strength * dt;
                       }
                     }
                   });
      }

      if (bc.hover_timer > 0.0f) {
        break;
      }

      // 技能2：悬停结束时按需眩晕（回归不变）
      if (!isBladeBoomerang && bc.stun_on_apex_end && bc.pull_radius > 0.0f) {
        grid.query({pos.x, pos.y}, bc.pull_radius,
                   [&](entt::entity target, const Position &) {
                     if (!registry.any_of<EnemyTag>(target) ||
                         registry.any_of<KilledTag>(target)) {
                       return;
                     }
                     (void)ApplyStun(registry, target, bc.owner, 1.0f,
                                     "BladeBoomerangApexStun", bc.skill_id);
                   });
      }
      enterReturning(entity, bc, vel);
      break;
    }

    case BoomerangPhase::Returning: {
      // 非法 returnTarget 统一销毁，避免无限飞行
      if (bc.returnTarget != entt::null && !registry.valid(bc.returnTarget)) {
        s_to_destroy.push_back(entity);
        break;
      }
      const entt::entity targetEnt =
          registry.valid(bc.returnTarget) ? bc.returnTarget : bc.owner;
      if (!registry.valid(targetEnt) ||
          !registry.all_of<Position>(targetEnt)) {
        s_to_destroy.push_back(entity);
        break;
      }
      const auto &targetPos = registry.get<Position>(targetEnt);

      if (isBladeBoomerang) {
        MarkReturningKills(registry, entity, bc);
        // 850/851：牵引作用于折返路径（不再只在顶点生效）
        if (bc.pull_radius > 0.0f && bc.pull_strength > 0.0f) {
          ApplyReturnPull(registry, grid, bc, {pos.x, pos.y},
                          {targetPos.x, targetPos.y}, dt);
        }
      }

      Vector2 toTarget =
          Vector2Subtract({targetPos.x, targetPos.y}, {pos.x, pos.y});
      const float dist = Vector2Length(toTarget);
      if (dist < kReturnCatchThreshold) {
        // 接刃统一在交付系统结算
        if (bc.catch_by_owner && registry.valid(bc.owner)) {
          HandleCatch(registry, entity, bc);
        }
        s_to_destroy.push_back(entity);
      } else {
        const float speed = (bc.returnSpeed > 0.1f) ? bc.returnSpeed : 600.0f;
        const Vector2 dir = Vector2Scale(toTarget, 1.0f / dist);
        vel.vx = dir.x * speed;
        vel.vy = dir.y * speed;
      }
      break;
    }
    }
  }

  for (auto e : s_to_destroy) {
    if (registry.valid(e)) {
      registry.destroy(e);
    }
  }
}

} // namespace NoMoreDay
