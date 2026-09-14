/**
 * @file FlowingThrust.cpp
 * @brief 流云刺 (ID 1) - 模块化突进穿透技能实现
 */
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "SevenStarSlashShared.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/FlowingThrustComponents.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillProfileResolve.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/FlowingThrust.hpp"
#include "game/systems/skill/behaviors/generated/FlowingThrustSpecState.gen.hpp"
#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace NoMoreDay::skills {

// 节点常量随 A-01 Phase 4c 信封化迁移至生成头，此处以别名保持既有机制读取引用不变。
namespace FlowingThrustNodes = FlowingThrustNodesGen;

namespace {

// 172 凛风 / 175 余韵余波的减速共用构造：B2-18 后走 AilmentAdapter 的单源
// 减速构建口（与 HazardSystem 冰冻球减速同型），id="FrostSlow"/type/kind
// 逐字保持不变；减速幅度与时长仍来自技能机制数据（等价映射）。
// Slow 异常契约（ailment_contracts.json）只承载身份，不产生 tick 伤害。
void ApplyFrostSlowDebuff(entt::registry &registry, entt::entity target,
                          float slowMagnitude, float duration) {
  auto slow = systems::AilmentAdapter::BuildMoveSpeedDebuff(
      AilmentType::Slow, "FrostSlow", "Frost Slow", "", BuffKind::Slow,
      slowMagnitude, duration);
  registry.get_or_emplace<ActiveEffectsComponent>(target).AddOrRefresh(slow);
  (void)registry.get_or_emplace<StatsDirty>(target);
}

} // namespace

struct FlowingThrust : SkillBehaviorBase<FlowingThrust> {
  static constexpr uint32_t kSkillId = 1;
  // 技能8 御剑·回旋 协同节点: 拔血流云 (814)。流云刺命中悬停切割区内的
  // 敌人时引爆其全部流血层数，故效果由技能1 命中触发、门控在技能8 上。
  static constexpr uint32_t kBloodRipSynergySkillId = 8;
  static constexpr uint32_t kBloodRipSynergyNode = 814;

  // 效果层 SpecState 解析入口：唯一调用模板 ResolveSpecState 的位置（A-01 D-A1）。
  [[nodiscard]] static FlowingThrustSpecStateGen ResolveState(entt::registry &registry,
                                                              entt::entity owner) {
    return ResolveSpecState(registry, owner, kSkillId, kFlowingThrustTableGen);
  }

  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    auto *pos = registry.try_get<Position>(owner);
    auto *stats = registry.try_get<CombatStats>(owner);
    if (!pos) return;

    const auto link = seven_star_shared::ConsumeLinkBuffs(registry, owner, kSkillId, true, exec.cast_id);
    if (link.consume_returning_step) {
      seven_star_shared::ApplyReturningStepOverride(registry, owner, kSkillId);
    }

    BakedSkillProfile localProfile;
    const auto *profile =
        ResolveBakedProfile(registry, owner, kSkillId, localProfile);
    const FlowingThrustSpecStateGen specState = ResolveState(registry, owner);

    Vector2 startPos = {pos->x, pos->y};
    Vector2 dir = Vector2Normalize(Vector2Subtract(exec.target_pos, startPos));
    if (dir.x == 0.0f && dir.y == 0.0f) {
      dir = {1.0f, 0.0f};
    }

    float speed = profile ? profile->delivery.speed : 400.0f;
    if (const auto *res = registry.try_get<BladeResourceComponent>(owner); res && res->kind == BladeResourceKind::SwordFlow) {
      speed *= (1.0f + std::clamp(res->current, 0, 10) * 0.04f);
    }

    const bool hasWindwalker =
        specState.windwalker || exec.active_nodes.test(FlowingThrustNodes::Windwalker % 100);
    const bool spawnShadow =
        specState.afterimage || exec.active_nodes.test(FlowingThrustNodes::Afterimage % 100);
    const bool shadowStrike =
        specState.shadowStrike || exec.active_nodes.test(FlowingThrustNodes::ShadowStrike % 100);
    const bool isSwap =
        specState.swap || exec.active_nodes.test(FlowingThrustNodes::Swap % 100);

    // 113 风行者: 疾风状态 (2s) 移速 +40%，无视体积碰撞，激活御剑步
    if (hasWindwalker) {
      seven_star_shared::GrantSwordStep(registry, owner, 2.0f, 40.0f);
    }

    // 114 御风而行: 仅当处于御剑步状态时,近战交付获得 8%...24% 额外暴击率
    const float ridingWindCrit =
        (registry.any_of<PhaseTag>(owner) && profile) ? profile->riding_wind_bonus_crit : 0.0f;

    // 130 留影: 施放时起点留残影 4s，模仿伤害 40%
    if (!registry.any_of<ShadowComponent>(owner) && !registry.any_of<ShadowLifetime>(owner)) {
      if (spawnShadow) {
        SkillSystem::SpawnShadowEcho(registry, owner, kSkillId, startPos, exec.target_pos, stats, 0.4f, 0.15f, 4.0f, {50, 0, 80, 180}, exec.active_nodes);
        if (exec.active_nodes.test(FlowingThrustNodes::PhantomShield % 100)) {
          float dex = stats ? stats->effective_dexterity : 20.0f;
          BuffEffect ward{.id = "PhantomShield", .name = "Phantom Shield", .type = BuffType::Shield, .duration = 3.0f, .remaining = 3.0f};
          ward.modifiers.push_back({.value = dex * 2.0f, .type = StatType::MaxBarrier, .mode = ModifierMode::Flat});
          registry.get_or_emplace<ActiveEffectsComponent>(owner).AddOrRefresh(ward);
        }
      } else if (const auto *res = registry.try_get<BladeResourceComponent>(owner); res && res->kind == BladeResourceKind::SwordFlow && res->current >= 5) {
        SkillSystem::SpawnShadowEcho(registry, owner, kSkillId, startPos, exec.target_pos, stats, 0.22f, 0.08f, 1.0f, {120, 240, 255, 180}, exec.active_nodes);
        if (res->current >= 8) {
          SkillSystem::SpawnShadowEcho(registry, owner, kSkillId, startPos, exec.target_pos, stats, 0.18f, 0.16f, 0.9f, {180, 255, 255, 170}, exec.active_nodes);
        }
        if (res->current >= 10) {
          SkillSystem::SpawnShadowEcho(registry, owner, kSkillId, startPos, exec.target_pos, stats, 0.14f, 0.24f, 0.85f, {255, 240, 180, 180}, exec.active_nodes);
        }
      }
    }

    // 132 影之突袭: 拥有残影时施放此技能，残影也向目标位置发起流云刺 (防影子互召)
    if (shadowStrike && !registry.any_of<ShadowComponent>(owner) && !registry.any_of<ShadowLifetime>(owner)) {
      auto shadowView = registry.view<ShadowComponent, SummonComponent, Position>();
      for (auto sEnt : shadowView) {
        const auto &summon = shadowView.get<SummonComponent>(sEnt);
        if (summon.owner == owner) {
          const auto &sPos = shadowView.get<Position>(sEnt);
          SkillSystem::ShadowCast(registry, sEnt, kSkillId, {sPos.x, sPos.y}, exec.target_pos);
          break;
        }
      }
    }

    // 133 移形换位: 流云刺变传送，起终点物理爆炸
    if (isSwap) {
      // 节点数值统一读取 skill_mechanics.json
      const auto &mech = data::SkillMechanicsRegistry::Get();
      // 112 势如破竹: 位移距离增加 10%...40%（每点 +10%），
      // 读取 skill_mechanics.json 112 节点配置；未分配 112 时倍率为 1。
      const bool hasMomentum =
          specState.momentumPoints > 0 || exec.active_nodes.test(FlowingThrustNodes::Momentum % 100);
      float dashRangeMult = 1.0f;
      if (hasMomentum) {
        const int momentumPoints = specState.momentumPoints;
        const float rangePct = mech.GetFloat(kSkillId, FlowingThrustNodes::Momentum, "dash_range_pct_per_point", 10.0f) / 100.0f;
        dashRangeMult = 1.0f + rangePct * static_cast<float>(momentumPoints);
      }
      // 传送落点沿原目标方向按倍率延伸；实际位移距离用于 112 伤害 More 结算
      pos->x = startPos.x + (exec.target_pos.x - startPos.x) * dashRangeMult;
      pos->y = startPos.y + (exec.target_pos.y - startPos.y) * dashRangeMult;
      float explosionRadius = 35.0f * (profile ? profile->area_radius : 1.0f);
      float moreDamageMult = (profile ? profile->more_damage_mult : 1.0f) * (exec.is_empowered ? 1.5f : 1.0f);

      // 112 势如破竹: 每多移动 10 码，该次伤害总增 (More) 2%（floor(距离/10)）。
      // 仅分配 112 时结算；未分配时该次伤害不获得位移 More。
      if (hasMomentum) {
        const float dashDist = std::sqrt((pos->x - startPos.x) * (pos->x - startPos.x) +
                                         (pos->y - startPos.y) * (pos->y - startPos.y));
        const float morePer10yd =
            mech.GetFloat(kSkillId, FlowingThrustNodes::Momentum, "damage_more_per_10yd", 2.0f) / 100.0f;
        moreDamageMult *= (1.0f + morePer10yd * std::floor(dashDist / 10.0f));
      }

      auto spawnExplosion = [&](Vector2 ePos) {
        auto burstEnt = registry.create();
        registry.emplace<LocalLevelTag>(burstEnt);
        registry.emplace<Position>(burstEnt, ePos.x, ePos.y);
        auto &ds = registry.emplace<DirectStrikeComponent>(burstEnt);
        ds.owner = owner;
        ds.cast_id = exec.cast_id;
        ds.skill_id = kSkillId;
        ds.radius = explosionRadius;
        ds.lifetime = 0.05f;
        registry.emplace<SkillComponent>(burstEnt, kSkillId, owner);
        if (stats) {
          // 爆炸伤害有效载荷：more 倍率 / 元素 tags / 暴击修正写入组件字段，
          // 由 ProjectileSystem 的 DirectStrike 处理直接消费（与投射物路径一致），
          // 不再向爆炸实体写入未被读取的 CombatStats 快照。
          ds.has_payload = true;
          ds.payload_context = {
              .base_damage_min = stats->min_weapon_damage,
              .base_damage_max = stats->max_weapon_damage,
              .crit_chance = stats->crit_chance +
                              (profile ? profile->delivery.bonus_crit : 0.0f) +
                              ridingWindCrit,
              .crit_multiplier = stats->crit_damage,
              .more_damage = moreDamageMult,
              .effective_tags = profile ? profile->effective_tags : Tag::Physical,
              .source_skill_id = kSkillId
          };
        }
      };

      spawnExplosion(startPos);
      spawnExplosion(exec.target_pos);

      // 170 劫火: 沿传送起点→终点铺设燃烧余烬带
      // (余烬持续/宽度读 skill_mechanics.json，171 每点 +0.5s / 宽度 +25%)
      if (profile && HasTag(profile->effective_tags, Tag::Fire)) {
        const auto &mech = data::SkillMechanicsRegistry::Get();
        const int infernalPoints = specState.infernalPathPoints;
        const float baseDuration = mech.GetFloat(kSkillId, FlowingThrustNodes::Hellfire, "ember_duration", 2.0f);
        const float baseWidth = mech.GetFloat(kSkillId, FlowingThrustNodes::Hellfire, "ember_width", 60.0f);
        const float widthPerPointPct =
            mech.GetFloat(kSkillId, FlowingThrustNodes::InfernalPath, "ember_width_per_point_pct", 25.0f) / 100.0f;
        const float durationPerPoint =
            mech.GetFloat(kSkillId, FlowingThrustNodes::InfernalPath, "ember_duration_per_point", 0.5f);

        auto emberEnt = registry.create();
        registry.emplace<LocalLevelTag>(emberEnt);
        registry.emplace<Position>(emberEnt, (startPos.x + exec.target_pos.x) * 0.5f,
                                   (startPos.y + exec.target_pos.y) * 0.5f);
        auto &zone = registry.emplace<FlowingEmberZoneComponent>(emberEnt);
        zone.owner = owner;
        zone.start_x = startPos.x;
        zone.start_y = startPos.y;
        zone.end_x = exec.target_pos.x;
        zone.end_y = exec.target_pos.y;
        zone.width = baseWidth * (1.0f + widthPerPointPct * static_cast<float>(infernalPoints));
        zone.duration = baseDuration + durationPerPoint * static_cast<float>(infernalPoints);
        zone.remaining = zone.duration;
        zone.infernal_points = infernalPoints;
      }
      return;
    }

    // 正常突进机动与投射物
    if (auto *vel = registry.try_get<Velocity>(owner)) {
      vel->vx = dir.x * speed;
      vel->vy = dir.y * speed;
    }
    auto &mob = registry.emplace_or_replace<MobilityComponent>(owner);
    mob.owner = owner;
    mob.target_entity = owner;
    mob.direction = dir;
    mob.speed = speed;
    mob.duration = 0.375f;
    mob.leaves_motion_trail = true;

    if (auto *dash = registry.try_get<DashComponent>(owner)) {
      dash->isDashing = true;
      dash->dashTimer = 0.375f;
      dash->dirX = dir.x;
      dash->dirY = dir.y;
      dash->dashSpeed = speed;
    }

    auto proj_ent = registry.create();
    registry.emplace<LocalLevelTag>(proj_ent);
    registry.emplace<Position>(proj_ent, pos->x + dir.x * 24.0f, pos->y + dir.y * 24.0f);
    registry.emplace<Velocity>(proj_ent, dir.x * speed, dir.y * speed);
    auto &proj = registry.emplace<Projectile>(proj_ent);
    proj.owner = owner;
    proj.cast_id = exec.cast_id;
    proj.speed = speed;
    proj.lifeTime = 0.375f;
    proj.radius = exec.is_empowered ? 35.0f : 20.0f;
    proj.pierce = true;
    proj.pierceCount = 99;
    proj.max_pierce = 99;

    float moreDamageMult = (profile ? profile->more_damage_mult : 1.0f) * (exec.is_empowered ? 1.5f : 1.0f);
    if (stats) {
      proj.snapshot = *stats;
      for (auto &mult : proj.snapshot.damage_multipliers) mult *= moreDamageMult;
      proj.payload_context = {
        .base_damage_min = stats->min_weapon_damage,
        .base_damage_max = stats->max_weapon_damage,
        // payload crit_chance 与 CombatStats 统一为分数制 [0,1]
        .crit_chance =
            stats->crit_chance + (profile ? profile->delivery.bonus_crit : 0.0f) + ridingWindCrit,
        .crit_multiplier = stats->crit_damage,
        .more_damage = moreDamageMult,
        .effective_tags = profile ? profile->effective_tags : Tag::Physical,
        .source_skill_id = kSkillId
      };
      registry.emplace<CombatStats>(proj_ent, proj.snapshot);
    }
    registry.emplace<SkillComponent>(proj_ent, kSkillId, owner);
  }

  static void DoHit(entt::registry &reg, entt::entity attacker, entt::entity victim, Tag element_tag, bool) {
    entt::entity actualAttacker = attacker;
    if (const auto *summon = reg.try_get<SummonComponent>(attacker); summon && reg.valid(summon->owner)) {
      actualAttacker = summon->owner;
    }

    // 节点机制数值统一从 skill_mechanics.json 读取
    const auto &mech = data::SkillMechanicsRegistry::Get();

    const auto *profile = SkillSystem::GetBakedSkillProfile(reg, actualAttacker, kSkillId);
    const FlowingThrustSpecStateGen specState = ResolveState(reg, actualAttacker);
    const int ridingPoints = specState.ridingTheWindPoints;
    const int deepWoundsPoints = specState.deepWoundsPoints;
    const int arteryPoints = specState.arterySeverPoints;
    const int severFatePoints = specState.severFatePoints;
    const int relentlessPoints = specState.relentlessPoints;
    const int boneFrostPoints = specState.boneDeepFrostPoints;
    const int elementalErosionPoints = specState.elementalErosionPoints;
    const int residualElementsPoints = specState.residualElementsPoints;
    const bool hasAllIn = specState.allInPoints > 0;

    // 1. 基础命中回蓝 (基底描述：若命中目标，回复少量法力)
    if (auto *st = reg.try_get<CombatStats>(actualAttacker)) {
      float manaGain = 3.0f;
      // 114 御风而行: 御剑步期间近战击中回蓝 3...9
      if (ridingPoints > 0 && reg.any_of<PhaseTag>(actualAttacker)) {
        manaGain += 3.0f * static_cast<float>(ridingPoints);
      }
      st->mana = std::min(st->max_mana, st->mana + manaGain);
    }

    // 2. 剑意与流剑资源联动 (剑圣流派特性: 命中获得剑意并消费御剑重铸窗口)
    if (const auto *res = reg.try_get<BladeResourceComponent>(actualAttacker); res && res->kind == BladeResourceKind::SwordFlow) {
      (void)SkillSystem::GainSwordIntent(reg, actualAttacker, 1, kSkillId);
      (void)systems::BladeResourceService::TryConsumeSwordFlowRestartWindow(reg, actualAttacker, kSkillId);
    }

    // 3. 破阵流流血与护甲击碎
    // 按 BuffKind 整数比较判定元素异常状态：创建点均写入 kind，id 字符串仅
    // 保留给序列化与日志边界，热路径不再做子串匹配。
    bool victimHasBleed = false;
    bool victimHasFire = false;
    bool victimHasCold = false;
    if (auto *effects = reg.try_get<ActiveEffectsComponent>(victim)) {
      victimHasBleed = effects->GetByKind(BuffKind::Bleed) != nullptr;
      victimHasFire = effects->GetByKind(BuffKind::Ignite) != nullptr;
      victimHasCold = effects->GetByKind(BuffKind::Chill) != nullptr ||
                      effects->GetByKind(BuffKind::Freeze) != nullptr ||
                      effects->GetByKind(BuffKind::Slow) != nullptr;
    }

    // 814 拔血流云 (技能8 协同): 流云刺命中处于"御剑·回旋悬停切割区"内的
    // 敌人时，拔出其身上全部流血层数，并立即结算一次等量的物理真实伤害。
    // 前置: 施法者已在技能8 点出 814；目标在任一技能8 悬停飞剑的有效半径内。
    if (SkillSystem::HasAllocatedNode(reg, actualAttacker,
                                      kBloodRipSynergySkillId,
                                      kBloodRipSynergyNode)) {
      if (auto *vicEffects = reg.try_get<ActiveEffectsComponent>(victim)) {
        const auto *vicPos = reg.try_get<Position>(victim);
        if (vicPos != nullptr) {
          // 有效半径取悬停飞剑自身的碰撞半径 (Projectile.radius)，即其滞空
          // 切割的作用范围；无 Projectile 时回退到默认半径。
          constexpr float kHoverZoneRadiusDefault = 40.0f;
          bool insideHoverZone = false;
          auto boomerangView = reg.view<BoomerangComponent>();
          for (const auto boomerang : boomerangView) {
            const auto &bc = boomerangView.get<BoomerangComponent>(boomerang);
            if (bc.skill_id != kBloodRipSynergySkillId ||
                bc.phase != BoomerangPhase::HoverApex) {
              continue;
            }
            const auto *proj = reg.try_get<Projectile>(boomerang);
            const float radius =
                proj ? std::max(1.0f, proj->radius) : kHoverZoneRadiusDefault;
            const float dx = vicPos->x - bc.apex_position.x;
            const float dy = vicPos->y - bc.apex_position.y;
            if (dx * dx + dy * dy <= radius * radius) {
              insideHoverZone = true;
              break;
            }
          }

          if (insideHoverZone) {
            // 剩余流血总额 = Σ(单层单跳伤害 × 层数 × 剩余跳数)，与
            // AilmentTickDriver 的 PerStack 结算口径保持一致。
            constexpr float kBleedTickIntervalDefault = 0.5f;
            float bleedBurst = 0.0f;
            bool consumedBleed = false;
            for (const auto &b : vicEffects->effects) {
              // AilmentEngine 的 Bleed 契约统一写入 BuffType::Bleed，走整数比较
              const bool isBleed = b.type == BuffType::Bleed;
              if (!isBleed || b.remaining <= 0.0f || b.tick_damage <= 0.0f) {
                continue;
              }
              const float interval =
                  (b.tick_interval > 0.0f && b.tick_interval < 1.0e9f)
                      ? b.tick_interval
                      : kBleedTickIntervalDefault;
              const int remainingTicks =
                  std::max(1, static_cast<int>(std::floor(b.remaining / interval)));
              bleedBurst += b.tick_damage *
                            static_cast<float>(std::max(1, b.stacks)) *
                            static_cast<float>(remainingTicks);
              consumedBleed = true;
            }

            if (consumedBleed && bleedBurst > 0.0f) {
              // 拔除全部流血层数，后续 152 等"对流血目标"判定同步失效。
              for (auto it = vicEffects->effects.begin();
                   it != vicEffects->effects.end();) {
                if (it->type == BuffType::Bleed) {
                  it = vicEffects->effects.erase(it);
                } else {
                  ++it;
                }
              }
              victimHasBleed = false;
              reg.get_or_emplace<StatsDirty>(victim);

              // 物理真实伤害: skip_mitigation 绕过护甲/减伤；附加
              // DamageOverTime/SecondaryHit 防止本次爆发再次派发 OnSkillHit
              // 造成触发链递归 (与 173 碎裂的标签口径一致)。
              DamageRequest burstReq;
              burstReq.origin = DamageOrigin::SecondaryProc;
              burstReq.attacker = actualAttacker;
              burstReq.defender = victim;
              burstReq.skill_id = kSkillId;
              burstReq.base_pool.Add(Tag::Physical, bleedBurst);
              burstReq.additional_tags =
                  Tag::Physical | Tag::DamageOverTime | Tag::SecondaryHit;
              burstReq.skip_mitigation = true;
              (void)DamagePipeline::Execute(reg, burstReq, actualAttacker, true);
            }
          }
        }
      }
    }

    // 151 重创: 命中 25%...100% 几率造成流血
    if (deepWoundsPoints > 0 && GetRandomValue(1, 100) <= 25 * deepWoundsPoints) {
      systems::AilmentApplyRequest req{
        .ailment = AilmentType::Bleed,
        .source = actualAttacker,
        .magnitude = 10.0f,
        .duration = 4.0f,
        .stacks = 1,
        .source_skill_id = 1
      };
      (void)systems::AilmentApplier::Apply(reg, victim, req);
      victimHasBleed = true;
    }

    // 152 动脉割裂: 对流血目标 75%...225% 几率施加护甲击碎
    if (victimHasBleed && arteryPoints > 0) {
      int rollChance = 75 * arteryPoints;
      int stacks = rollChance / 100;
      if (GetRandomValue(1, 100) <= (rollChance % 100)) {
        stacks += 1;
      }
      if (stacks > 0) {
        // 护甲击碎为跨技能共享的减益（BuffId::ArmorShred）。数值口径：
        // AttributePipeline 直接累加 modifiers 且不乘 .stacks，故 modifier
        // 承载完整降甲量 (-10×层数)，.stacks 仅作展示并同步为层数。
        BuffEffect shred{
          .id = std::string(BuffIdToString(BuffId::ArmorShred)),
          .name = "Armor Shred",
          .type = BuffType::DefenseDown,
          .duration = 4.0f,
          .remaining = 4.0f,
          .stacks = stacks,
          .max_stacks = stacks,
          .is_debuff = true
        };
        shred.modifiers.push_back({
          .value = -10.0f * static_cast<float>(stacks),
          .type = StatType::Armor,
          .mode = ModifierMode::Flat
        });
        reg.get_or_emplace<ActiveEffectsComponent>(victim).AddOrRefresh(shred);
        reg.get_or_emplace<StatsDirty>(victim);
      }
    }

    // 153 饮血刃: 击中流血敌人治疗自身。
    // 按设计"治疗量 = 造成的流血伤害的 100%"，治疗必须依据 DoT tick 的
    // 实际结算扣血值，故此处不再硬编码回血；改由 AilmentTickDriver::Tick
    // 在流血伤害结算处对 DoT 施加者（其流云刺分配 153 时）按该次实际
    // 流血伤害 × 配置比例治疗（见 AilmentEngine.cpp）。

    // 4. 元素异常
    if (HasTag(element_tag, Tag::Fire)) {
      // 170 劫火: 点燃（复用现有点燃机制，数值统一走 skill_mechanics.json 170 节点）
      systems::AilmentApplyRequest req{
        .ailment = AilmentType::Ignite,
        .source = actualAttacker,
        .magnitude = mech.GetFloat(kSkillId, FlowingThrustNodes::Hellfire, "ignite_magnitude", 15.0f),
        .duration = mech.GetFloat(kSkillId, FlowingThrustNodes::Hellfire, "ignite_duration", 3.0f),
        .stacks = 1,
        .source_skill_id = 1
      };
      (void)systems::AilmentApplier::Apply(reg, victim, req);
      victimHasFire = true;
    }
    if (HasTag(element_tag, Tag::Cold)) {
      // 172 凛风: 减速 30% (legacy SpeedDown buff；数值类别归入 Slow，
      // 与 AilmentEngine 的 Slow 口径一致，见 ApplyFrostSlowDebuff)
      const float slowMagnitude = mech.GetFloat(kSkillId, FlowingThrustNodes::FreezingWind, "slow_magnitude", 0.30f);
      const float slowDuration = mech.GetFloat(kSkillId, FlowingThrustNodes::FreezingWind, "slow_duration", 2.5f);
      ApplyFrostSlowDebuff(reg, victim, slowMagnitude, slowDuration);

      // 173 霜凝寒骨: 命中减速敌人额外施加 1 层寒冷，寒冷叠满冻结敌人
      if (victimHasCold && boneFrostPoints > 0) {
        const float chillDuration = mech.GetFloat(kSkillId, FlowingThrustNodes::BoneDeepFrost, "chill_duration", 3.0f);
        const float chillSlowPct = mech.GetFloat(kSkillId, FlowingThrustNodes::BoneDeepFrost, "chill_slow_pct", 20.0f);
        const int freezeFallback = static_cast<int>(mech.GetFloat(kSkillId, FlowingThrustNodes::BoneDeepFrost, "chill_stacks_to_freeze", 3.0f));
        const float freezeDuration = mech.GetFloat(kSkillId, FlowingThrustNodes::BoneDeepFrost, "freeze_duration", 2.5f);
        // 寒冷层数上限以 AilmentEngine 的 Chill 契约为单一来源（等价映射）；
        // 契约缺失时回退到 skill_mechanics 的 chill_stacks_to_freeze，保持旧行为。
        const systems::AilmentContract *chillContract =
            systems::AilmentRegistry::Get().Find(AilmentType::Chill);
        const int chillMaxStacks =
            std::max(1, chillContract
                            ? static_cast<int>(chillContract->max_stacks)
                            : freezeFallback);
        auto &vicEffects = reg.get_or_emplace<ActiveEffectsComponent>(victim);
        BuffEffect chill{
          .id = "FrostChill",
          .name = "Frost Chill",
          .type = BuffType::SpeedDown,
          .kind = BuffKind::Chill,
          .duration = chillDuration,
          .remaining = chillDuration,
          .stacks = 1,
          .max_stacks = chillMaxStacks,
          .is_debuff = true
        };
        chill.modifiers.push_back({
          .value = -chillSlowPct,
          .type = StatType::MoveSpeed,
          .mode = ModifierMode::PercentAdd
        });
        // 来源技能归属使本 buff 可按 (kind, 来源) 整数比较定位
        chill.source_skill_id = static_cast<int>(kSkillId);
        vicEffects.AddOrRefresh(chill);
        // 叠满判定：按数值类别 + 来源技能定位本次 FrostChill，替换原来的
        // Get("FrostChill") 字符串键查找；AilmentEngine 托管的寒冷来源技能为 0，
        // 不会被误命中。
        BuffEffect *frostChill = nullptr;
        for (auto &effect : vicEffects.effects) {
          if (effect.kind == BuffKind::Chill &&
              effect.source_skill_id == static_cast<int>(kSkillId)) {
            frostChill = &effect;
            break;
          }
        }
        if (frostChill != nullptr && frostChill->stacks >= chillMaxStacks) {
          // 叠满 → 转为冻结；先拷贝 id 再移除，避免移除时引用元素被搬移。
          const auto frostChillId = frostChill->id;
          vicEffects.Remove(frostChillId);
          BuffEffect frozen{
            .id = "Frozen",
            .name = "Frozen",
            .type = BuffType::Freeze,
            .kind = BuffKind::Freeze,
            .duration = freezeDuration,
            .remaining = freezeDuration,
            .is_debuff = true
          };
          vicEffects.AddOrRefresh(frozen);
        }
        reg.get_or_emplace<StatsDirty>(victim);
      }
      victimHasCold = true;
    }

    // 173 霜凝寒骨: 命中冻结目标时按几率触发"碎裂"，
    // 对周围敌人造成该目标上次暴击伤害 × 30% 的冰霜溅射
    if (boneFrostPoints > 0) {
      bool victimFrozen = false;
      if (auto *fx = reg.try_get<ActiveEffectsComponent>(victim)) {
        for (const auto &b : fx->effects) {
          if (b.type == BuffType::Freeze) {
            victimFrozen = true;
            break;
          }
        }
      }
      if (victimFrozen) {
        const float shatterChancePct =
            mech.GetFloat(kSkillId, FlowingThrustNodes::BoneDeepFrost, "shatter_chance_pct_per_point", 15.0f) *
            static_cast<float>(boneFrostPoints);
        if (GetRandomValue(1, 100) <= static_cast<int>(shatterChancePct)) {
          const float shatterRadius = mech.GetFloat(kSkillId, FlowingThrustNodes::BoneDeepFrost, "shatter_radius", 200.0f);
          const float shatterMult = mech.GetFloat(kSkillId, FlowingThrustNodes::BoneDeepFrost, "shatter_crit_mult", 0.30f);
          const auto *lastCrit = reg.try_get<LastCritDamageComponent>(victim);
          if (lastCrit && lastCrit->amount > 0.0f) {
            // 来源合法性: 暴击须来自本攻击者(或其召唤物)
            bool sourceValid = lastCrit->source == actualAttacker;
            if (!sourceValid) {
              if (const auto *sm = reg.try_get<SummonComponent>(lastCrit->source);
                  sm && reg.valid(sm->owner) && sm->owner == actualAttacker) {
                sourceValid = true;
              }
            }
            if (sourceValid) {
              if (auto *vicPos = reg.try_get<Position>(victim)) {
                const float splash = lastCrit->amount * shatterMult;
                // 碎裂为范围 AOE：对半径内所有其他敌人各造成一次冰霜溅射
                auto enemyView = reg.view<EnemyTag, Position>();
                for (auto other : enemyView) {
                  if (other == victim || reg.any_of<KilledTag>(other)) continue;
                  const auto &oPos = enemyView.get<Position>(other);
                  const float dx = oPos.x - vicPos->x;
                  const float dy = oPos.y - vicPos->y;
                  if (dx * dx + dy * dy <= shatterRadius * shatterRadius) {
                    // 溅射走 DamagePipeline 正常请求；带 DamageOverTime 标签：
                    // 不再派发 OnSkillHit，防止碎裂递归触发自身
                    DamageRequest req;
                    req.origin = DamageOrigin::SecondaryProc;
                    req.attacker = actualAttacker;
                    req.defender = other;
                    req.skill_id = kSkillId;
                    req.base_pool.Add(Tag::Cold, splash);
                    req.additional_tags = Tag::Cold | Tag::DamageOverTime | Tag::Area;
                    (void)DamagePipeline::Execute(reg, req, actualAttacker, true);
                  }
                }
              }
            }
          }
        }
      }
    }

    // 174 灵根侵蚀: 命中带有对应元素异常状态敌人，施加 1 层元素侵蚀 (最多 5 层，持续 4 秒，每层降低抗性 3...12 点)
    if (elementalErosionPoints > 0) {
      bool isEligible = (HasTag(element_tag, Tag::Fire) && victimHasFire) ||
                        (HasTag(element_tag, Tag::Cold) && victimHasCold);
      if (isEligible) {
        auto &effects = reg.get_or_emplace<ActiveEffectsComponent>(victim);
        const bool isFire = HasTag(element_tag, Tag::Fire);
        const std::string debuffId = isFire ? "ElementalErosionFire" : "ElementalErosionCold";
        const StatType resType = isFire ? StatType::ResistFire : StatType::ResistCold;

        int currentStacks = 0;
        for (const auto &eff : effects.effects) {
          if (eff.id == debuffId && !eff.modifiers.empty()) {
            float basePerStack = 3.0f * static_cast<float>(elementalErosionPoints);
            currentStacks = static_cast<int>(-eff.modifiers[0].value / (basePerStack > 0.0f ? basePerStack : 1.0f));
            break;
          }
        }
        int newStacks = std::min(5, currentStacks + 1);
        BuffEffect erosion{
          .id = debuffId,
          .name = "Elemental Erosion",
          .type = BuffType::DefenseDown,
          .duration = 4.0f,
          .remaining = 4.0f,
          .source_skill_id = static_cast<int>(kSkillId) // 归属流云刺，仅其伤害受益 (SkillOnly)
        };
        erosion.modifiers.push_back({
          .value = -3.0f * static_cast<float>(elementalErosionPoints) * static_cast<float>(newStacks),
          .type = resType,
          .mode = ModifierMode::Flat
        });
        effects.AddOrRefresh(erosion);
        reg.get_or_emplace<StatsDirty>(victim);
      }
    }

    // 175 元素余韵: 20%...60% 几率将异常传染给 200 码内 1 名附近敌人
    // (每次成功传染有 1.0s 内部冷却，状态存 FlowingThrustStateComponent)
    if (residualElementsPoints > 0 && (victimHasFire || victimHasCold)) {
      const float spreadIcd = mech.GetFloat(kSkillId, FlowingThrustNodes::ResidualElements, "spread_icd", 1.0f);
      const float spreadRadius = mech.GetFloat(kSkillId, FlowingThrustNodes::ResidualElements, "spread_radius", 200.0f);
      const float spreadChancePct =
          mech.GetFloat(kSkillId, FlowingThrustNodes::ResidualElements, "spread_chance_pct_per_point", 20.0f) *
          static_cast<float>(residualElementsPoints);
      const float now = static_cast<float>(GetTime());
      auto &ftState = reg.get_or_emplace<FlowingThrustStateComponent>(actualAttacker);
      if ((now - ftState.last_infect_time) < spreadIcd) {
        // 冷却中：本命中不传染
      } else if (GetRandomValue(1, 100) <= static_cast<int>(spreadChancePct)) {
        if (auto *vicPos = reg.try_get<Position>(victim)) {
          // 选择半径内距离最近的敌人作为传染目标（设计：\"200 码内 1 名附近敌人\"）
          entt::entity spreadTarget = entt::null;
          float bestDistSq = spreadRadius * spreadRadius;
          auto enemyView = reg.view<EnemyTag, Position>();
          for (auto other : enemyView) {
            if (other == victim || reg.any_of<KilledTag>(other)) continue;
            const auto &oPos = enemyView.get<Position>(other);
            const float dx = oPos.x - vicPos->x;
            const float dy = oPos.y - vicPos->y;
            const float distSq = dx * dx + dy * dy;
            if (distSq <= bestDistSq) {
              bestDistSq = distSq;
              spreadTarget = other;
            }
          }
          if (spreadTarget != entt::null) {
              if (victimHasFire) {
                systems::AilmentApplyRequest spreadReq{
                  .ailment = AilmentType::Ignite,
                  .source = actualAttacker,
                  .magnitude = mech.GetFloat(kSkillId, FlowingThrustNodes::ResidualElements, "spread_ignite_magnitude", 15.0f),
                  .duration = mech.GetFloat(kSkillId, FlowingThrustNodes::ResidualElements, "spread_ignite_duration", 3.0f),
                  .stacks = 1,
                  .source_skill_id = 1
                };
                (void)systems::AilmentApplier::Apply(reg, spreadTarget, spreadReq);
              }
              if (victimHasCold) {
                // 传染的减速与 172 凛风同源：复用同一辅助函数，数值取余韵配置
                const float spreadSlowMag =
                    mech.GetFloat(kSkillId, FlowingThrustNodes::ResidualElements, "spread_slow_magnitude", 0.30f);
                const float spreadSlowDur =
                    mech.GetFloat(kSkillId, FlowingThrustNodes::ResidualElements, "spread_slow_duration", 2.5f);
                ApplyFrostSlowDebuff(reg, spreadTarget, spreadSlowMag, spreadSlowDur);
              }

              // 传染成功后才写入冷却时间戳
              ftState.last_infect_time = now;
          }
        }
      }
    }

    // 5. 击杀判定 (双重保险：KilledTag 或当前生命值归零)
    const auto *vhp = reg.try_get<HealthComponent>(victim);
    const bool isKilled = reg.any_of<KilledTag>(victim) || (vhp && vhp->current <= 0.0f);
    if (isKilled) {
      // 115 无止境: 击杀 20%...60% 几率立即回复 1 次充能
      if (relentlessPoints > 0 && GetRandomValue(1, 100) <= 20 * relentlessPoints) {
        // 充能上限回退: 无专精覆盖时取 skills.json 的 charge_count (max_charges),不再硬编码
        uint8_t maxCharges = 2;
        if (profile && profile->effective_charges > 0) {
          maxCharges = static_cast<uint8_t>(profile->effective_charges);
        } else if (const auto *skill = SkillRegistry::Get().GetSkill(kSkillId)) {
          maxCharges = static_cast<uint8_t>(std::max(1, skill->max_charges));
        }
        if (auto *act = reg.try_get<ActiveSkillsComponent>(actualAttacker)) {
          for (auto &s : act->slots) {
            if (s.id == kSkillId) {
              s.current_charges = std::min<uint8_t>(maxCharges, s.current_charges + 1);
              if (s.current_charges >= maxCharges) {
                s.cooldown = 0.0f;
              }
              break;
            }
          }
        }
      }

      // 155 斩断因果: 强化版流云刺击杀 25%...75% 重置 CD，回 1 层剑意
      if (hasAllIn && severFatePoints > 0 && GetRandomValue(1, 100) <= 25 * severFatePoints) {
        if (auto *act = reg.try_get<ActiveSkillsComponent>(actualAttacker)) {
          for (auto &s : act->slots) {
            if (s.id == kSkillId) {
              s.current_charges = 1;
              s.cooldown = 0.0f;
              break;
            }
          }
        }
        SkillSystem::GainSwordIntent(reg, actualAttacker, 1, kSkillId);
      }
    }
  }
};
REGISTER_SKILL_BEHAVIOR(FlowingThrust)
void RegisterFlowingThrust() {}

namespace {

// 点到线段的最短距离平方的平方根；用于余烬带"进入/处于"判定
float DistanceToSegment(float px, float py, float ax, float ay, float bx, float by) {
  const float abx = bx - ax;
  const float aby = by - ay;
  const float lenSq = abx * abx + aby * aby;
  if (lenSq <= 1e-6f) {
    const float dx = px - ax;
    const float dy = py - ay;
    return std::sqrt(dx * dx + dy * dy);
  }
  float t = ((px - ax) * abx + (py - ay) * aby) / lenSq;
  t = std::clamp(t, 0.0f, 1.0f);
  const float cx = ax + t * abx;
  const float cy = ay + t * aby;
  const float dx = px - cx;
  const float dy = py - cy;
  return std::sqrt(dx * dx + dy * dy);
}

} // namespace

// 更新流云刺余烬带 (170 劫火 / 171 业火焚途)：
//  - 敌人进入/处于余烬内 → 施加点燃（同一余烬对同一敌人只触发一次）；
//  - 171 分配时主人站在自己余烬内 → 刷新火焰伤害加成 buff（短时长，离开自然过期）。
// 由 SkillSystem::Update 每帧调用。
void UpdateFlowingThrustEmbers(entt::registry &registry, float dt) {
  const auto &mech = data::SkillMechanicsRegistry::Get();
  constexpr uint32_t kSkillId = 1;
  constexpr uint32_t kHellfireNode = 170;
  constexpr uint32_t kInfernalPathNode = 171;

  auto zoneView = registry.view<FlowingEmberZoneComponent>();
  if (zoneView.empty()) {
    return;
  }
  const float tickInterval = mech.GetFloat(kSkillId, kHellfireNode, "ember_tick_interval", 0.25f);
  const float igniteMagnitude = mech.GetFloat(kSkillId, kHellfireNode, "ignite_magnitude", 15.0f);
  const float igniteDuration = mech.GetFloat(kSkillId, kHellfireNode, "ignite_duration", 3.0f);

  std::vector<entt::entity> toDestroy;
  for (auto zoneEnt : zoneView) {
    auto &zone = zoneView.get<FlowingEmberZoneComponent>(zoneEnt);
    zone.remaining -= dt;
    if (zone.remaining <= 0.0f) {
      toDestroy.push_back(zoneEnt);
      continue;
    }
    zone.tick_accum += dt;
    if (zone.tick_accum < tickInterval) {
      continue;
    }
    zone.tick_accum = 0.0f;

    // 敌人进入余烬 → 点燃（同 zone 去重）
    auto enemyView = registry.view<EnemyTag, Position>();
    for (auto enemy : enemyView) {
      if (registry.any_of<KilledTag>(enemy)) continue;
      bool alreadyIgnited = false;
      for (const auto e : zone.ignited) {
        if (e == enemy) {
          alreadyIgnited = true;
          break;
        }
      }
      if (alreadyIgnited) continue;
      const auto &ePos = enemyView.get<Position>(enemy);
      if (DistanceToSegment(ePos.x, ePos.y, zone.start_x, zone.start_y, zone.end_x, zone.end_y) <= zone.width) {
        systems::AilmentApplyRequest req{
          .ailment = AilmentType::Ignite,
          .source = zone.owner,
          .magnitude = igniteMagnitude,
          .duration = igniteDuration,
          .stacks = 1,
          .source_skill_id = 1
        };
        (void)systems::AilmentApplier::Apply(registry, enemy, req);
        zone.ignited.push_back(enemy);
      }
    }

    // 171 业火焚途: 主人站在自己余烬上 → 火焰伤害加成 buff（短时长逐帧刷新，离开过期）
    if (zone.infernal_points > 0 && registry.valid(zone.owner)) {
      const auto *oPos = registry.try_get<Position>(zone.owner);
      if (oPos &&
          DistanceToSegment(oPos->x, oPos->y, zone.start_x, zone.start_y, zone.end_x, zone.end_y) <= zone.width) {
        const float firePct =
            mech.GetFloat(kSkillId, kInfernalPathNode, "fire_damage_pct_per_point", 6.0f) *
            static_cast<float>(zone.infernal_points);
        const float buffDuration = mech.GetFloat(kSkillId, kInfernalPathNode, "standing_buff_duration", 0.25f);
        BuffEffect pathBuff{
          .id = "InfernalPath",
          .name = "Infernal Path",
          .type = BuffType::AttackUp,
          .duration = buffDuration,
          .remaining = buffDuration
        };
        pathBuff.modifiers.push_back({
          .value = firePct,
          .type = StatType::FireDamage,
          .mode = ModifierMode::PercentAdd
        });
        registry.get_or_emplace<ActiveEffectsComponent>(zone.owner).AddOrRefresh(pathBuff);
        (void)registry.get_or_emplace<StatsDirty>(zone.owner);
      }
    }
  }
  for (auto e : toDestroy) {
    if (registry.valid(e)) {
      registry.destroy(e);
    }
  }
}

// 更新流云刺 135 虚实相生：
//  - 每帧汇总玩家与自身残影（130 留影 / 剑流余影）的距离；
//  - 玩家从"残影离开判定区"（距任一残影 ≤ leave_radius）离开到区外时，
//    获得相当于敏捷 ×(2.0×点数) 的临时护盾 (Ward)，持续 3s；
//  - 每次离开触发一次，状态存 FlowingThrustStateComponent::last_in_shadow_zone_time。
// 由 SkillSystem::Update 每帧调用。
void UpdateFlowingThrustPhantomShield(entt::registry &registry, float dt) {
  (void)dt;
  const auto &mech = data::SkillMechanicsRegistry::Get();
  constexpr uint32_t kSkillId = 1;
  constexpr uint32_t kPhantomShieldNode = 135;

  auto shadowView = registry.view<ShadowComponent, SummonComponent, Position>();

  const float leaveRadius = mech.GetFloat(kSkillId, kPhantomShieldNode, "leave_radius", 80.0f);
  const float now = static_cast<float>(GetTime());

  // 汇总每个残影所有者的"是否处于任一残影判定区内"（残影可多个，去重所有者）
  // 无残影时 owners 为空，下方循环自然跳过
  std::vector<entt::entity> owners;
  std::vector<bool> insideAny;
  for (auto shadowEnt : shadowView) {
    const auto &summon = shadowView.get<SummonComponent>(shadowEnt);
    if (!registry.valid(summon.owner)) continue;
    const auto *ownerPos = registry.try_get<Position>(summon.owner);
    if (!ownerPos) continue;
    const auto &shadowPos = shadowView.get<Position>(shadowEnt);
    const float dx = ownerPos->x - shadowPos.x;
    const float dy = ownerPos->y - shadowPos.y;
    const bool inside = (dx * dx + dy * dy) <= leaveRadius * leaveRadius;

    auto it = std::find(owners.begin(), owners.end(), summon.owner);
    if (it == owners.end()) {
      owners.push_back(summon.owner);
      insideAny.push_back(inside);
    } else {
      const size_t idx = static_cast<size_t>(it - owners.begin());
      insideAny[idx] = insideAny[idx] || inside;
    }
  }

  for (size_t i = 0; i < owners.size(); ++i) {
    entt::entity owner = owners[i];

    // 仅流云刺专精分配了 135 时生效
    const int phantomPoints = FlowingThrust::ResolveState(registry, owner).phantomShieldPoints;
    if (phantomPoints <= 0) {
      continue;
    }

    auto &ftState = registry.get_or_emplace<FlowingThrustStateComponent>(owner);
    if (insideAny[i]) {
      // 仍处于残影判定区：逐帧刷新"最近一次在区内"时间，离开后才算离开
      ftState.last_in_shadow_zone_time = std::max(ftState.last_in_shadow_zone_time, now);
    } else if (ftState.last_in_shadow_zone_time > 0.0f) {
      // 从残影区离开 → 触发临时护盾（每次离开触发一次）。
      // 数值直接写入 CombatStats.barrier 超额段：AttributePipeline 不聚合
      // ActiveEffects 的 modifiers，且重算会清零 max_barrier，走 buff 修饰符
      // 是零生效路径。引擎既有的 Ward 机制（RegenerationSystem::Ward Mode）
      // 将超过 max_barrier 的部分按 barrier_decay 指数衰减，CombatSystem 的
      // 承伤结算直接消耗 stats.barrier，因此超额护盾即临时护盾。
      ftState.last_in_shadow_zone_time = -1.0e9f;
      const float wardDuration = mech.GetFloat(kSkillId, kPhantomShieldNode, "ward_duration", 3.0f);
      const float dexMult = mech.GetFloat(kSkillId, kPhantomShieldNode, "ward_dex_mult_per_point", 2.0f);
      auto *stats = registry.try_get<CombatStats>(owner);
      const float dex = stats ? stats->effective_dexterity : 20.0f;
      const float wardValue = dex * dexMult * static_cast<float>(phantomPoints);
      if (stats) {
        stats->barrier += wardValue;
        // 超额护盾依赖 BarrierComponent 的 last_damage_time 参与 decay 判定
        (void)registry.get_or_emplace<BarrierComponent>(owner);
        (void)registry.get_or_emplace<StatsDirty>(owner);
      }
      // buff 仅作状态展示（UI 图标/剩余时间），数值由上面的 barrier 超额段承载
      BuffEffect ward{
        .id = "PhantomShield",
        .name = "Phantom Shield",
        .type = BuffType::Shield,
        .duration = wardDuration,
        .remaining = wardDuration
      };
      registry.get_or_emplace<ActiveEffectsComponent>(owner).AddOrRefresh(ward);
    }
  }
}

} // namespace NoMoreDay::skills
