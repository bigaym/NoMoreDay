/**
 * @file SwordArray.cpp
 * @brief 剑阵·诛仙 (ID 6) - 模块化地表领域行为实现
 */
#include "SwordArray.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/SkillPointAccess.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillProfileResolve.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/generated/SwordArraySpecState.gen.hpp"
#include "raymath.h"
#include <algorithm>
#include <vector>

namespace NoMoreDay::skills {

// 节点常量随 A-01 Phase 4a 信封化迁移至生成头，此处以别名保持既有机制读取引用不变。
namespace SwordArrayNodes = SwordArrayNodesGen;

namespace {
// 技能 6 的节点点亮/点数统一经生成 SpecState 读取（A-01 D-A1）。
[[nodiscard]] SwordArraySpecStateGen ResolveState(const entt::registry &registry, entt::entity owner) {
  return ResolveSpecState(registry, owner, SwordArray::kSkillId, kSwordArrayTableGen);
}
} // namespace

void SwordArray::DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
  const auto &mechanics = data::SkillMechanicsRegistry::Get();

  BakedSkillProfile localProfile;
  const auto *profile =
      ResolveBakedProfile(registry, owner, kSkillId, localProfile);
  const SwordArraySpecStateGen specState = ResolveState(registry, owner);
  // 生成 SpecState 仅承载专精点数/点亮；保留 active_nodes 回退以维持迁移前
  // getPoints 的语义（无专精分配时由 exec.active_nodes 提供点亮语义）。
  const auto nodePoints = [&](uint32_t node_id, int stored_points) -> int {
    if (stored_points > 0) {
      return stored_points;
    }
    return exec.active_nodes.test(node_id % 100) ? 1 : 0;
  };
  const auto nodeActive = [&](uint32_t node_id, bool stored_flag) -> bool {
    return stored_flag || exec.active_nodes.test(node_id % 100);
  };

  // 1. 阵法数量与上限管理
  int max_arrays = 1;
  if (nodeActive(SwordArrayNodes::TwinArrays, specState.twinArrays)) {
    max_arrays = 2;
  }
  if (nodeActive(SwordArrayNodes::TriFormation, specState.triFormation)) {
    max_arrays = 3;
  }

  std::vector<entt::entity> existing_arrays;
  auto arrayView = registry.view<SwordArrayComponent>();
  for (auto e : arrayView) {
    if (arrayView.get<SwordArrayComponent>(e).owner == owner) {
      existing_arrays.push_back(e);
    }
  }

  // 675 移形换阵: 重按挪阵
  const bool allow_relocate = nodeActive(SwordArrayNodes::Relocate, specState.relocate);
  bool chargesExhausted = false;
  if (registry.all_of<ActiveSkillsComponent>(owner)) {
    const auto &active = registry.get<ActiveSkillsComponent>(owner);
    if (exec.slot_index >= 0 && exec.slot_index < SkillConstants::MAX_SKILL_SLOTS) {
      chargesExhausted = (active.slots[exec.slot_index].current_charges <= 0);
    }
  }
  if (allow_relocate && !existing_arrays.empty() && (chargesExhausted || existing_arrays.size() >= static_cast<size_t>(max_arrays))) {
    entt::entity closest_ent = existing_arrays.front();
    float min_d2 = 1e9f;
    for (auto e : existing_arrays) {
      if (const auto *p = registry.try_get<Position>(e)) {
        const float d2 = Vector2DistanceSqr({p->x, p->y}, {exec.target_pos.x, exec.target_pos.y});
        if (d2 < min_d2) {
          min_d2 = d2;
          closest_ent = e;
        }
      }
    }
    if (auto *p = registry.try_get<Position>(closest_ent)) {
      p->x = exec.target_pos.x;
      p->y = exec.target_pos.y;
    }
    auto &arr = registry.get<SwordArrayComponent>(closest_ent);
    const float reset_dur = arr.total_duration * 0.5f;
    arr.duration = reset_dur;
    if (auto *field = registry.try_get<AreaFieldComponent>(closest_ent)) {
      field->remaining_duration = reset_dur;
    }
    return;
  }

  while (existing_arrays.size() >= static_cast<size_t>(max_arrays)) {
    registry.destroy(existing_arrays.front());
    existing_arrays.erase(existing_arrays.begin());
  }

  // 2. 随身剑垒 (Node 653) 与坐标初始化
  const bool is_mobile_aura = nodeActive(SwordArrayNodes::MobileAura, specState.mobileAura);
  Vector2 spawn_pos = exec.target_pos;
  if (is_mobile_aura) {
    if (const auto *ownerPos = registry.try_get<Position>(owner)) {
      spawn_pos = {ownerPos->x, ownerPos->y};
    }
  }

  auto array_ent = registry.create();
  registry.emplace<Position>(array_ent, spawn_pos.x, spawn_pos.y);
  registry.emplace<LocalLevelTag>(array_ent);

  float dur = (profile && profile->delivery.duration > 0.0f) ? profile->delivery.duration : 5.0f;
  float rad = (profile && profile->area_radius > 0.0f) ? profile->area_radius : 150.0f;
  float interval = (profile && profile->delivery.sub_interval > 0.0f) ? profile->delivery.sub_interval : 0.5f;

  auto &array = registry.emplace<SwordArrayComponent>(array_ent);
  array.owner = owner;
  array.duration = dur;
  array.total_duration = dur;
  array.radius = rad;
  array.damage_interval = interval;
  array.is_empowered = exec.is_empowered;
  array.cast_id = exec.cast_id;
  array.max_arrays = max_arrays;
  array.is_mobile_aura = is_mobile_aura;

  // 3. 专精机制映射
  const int pts_630 = nodePoints(SwordArrayNodes::SlowPressure, specState.slowPressurePoints);
  array.has_slow = pts_630 > 0;
  array.slow_magnitude = mechanics.GetFloat(kSkillId, SwordArrayNodes::SlowPressure, "slow_pct_per_point", 0.10f) * static_cast<float>(std::max(1, pts_630));
  array.slow_duration = mechanics.GetFloat(kSkillId, SwordArrayNodes::SlowPressure, "slow_duration", 2.0f);

  const int pts_631 = nodePoints(SwordArrayNodes::ArmorIntent, specState.armorIntentPoints);
  array.has_armor_shred = pts_631 > 0;
  array.armor_shred_chance = mechanics.GetFloat(kSkillId, SwordArrayNodes::ArmorIntent, "shred_chance_per_point", 0.50f) * static_cast<float>(std::max(1, pts_631));
  array.shred_duration = mechanics.GetFloat(kSkillId, SwordArrayNodes::ArmorIntent, "shred_duration", 4.0f);
  array.shred_armor_per_stack = mechanics.GetFloat(kSkillId, SwordArrayNodes::ArmorIntent, "shred_armor_per_stack", 10.0f);
  array.shred_max_stacks = static_cast<int>(mechanics.GetFloat(kSkillId, SwordArrayNodes::ArmorIntent, "shred_max_stacks", 10.0f));

  const int pts_632 = nodePoints(SwordArrayNodes::Weaken, specState.weakenPoints);
  array.weaken_less_damage = mechanics.GetFloat(kSkillId, SwordArrayNodes::Weaken, "weaken_less_per_point", 0.06f) * static_cast<float>(pts_632);

  array.has_execute = nodeActive(SwordArrayNodes::ExecuteField, specState.executeField);
  if (array.has_execute) {
    array.execute_health_threshold_ratio = mechanics.GetFloat(kSkillId, SwordArrayNodes::ExecuteField, "execute_threshold_ratio", 0.12f);
    array.boss_more_damage = 1.0f + mechanics.GetFloat(kSkillId, SwordArrayNodes::ExecuteField, "boss_more_damage_pct", 0.20f);
  } else {
    array.boss_more_damage = 1.0f;
  }

  array.has_cage = nodeActive(SwordArrayNodes::Cage, specState.cage);

  const int pts_650 = nodePoints(SwordArrayNodes::Core, specState.corePoints);
  array.core_buff_more_damage = mechanics.GetFloat(kSkillId, SwordArrayNodes::Core, "global_damage_more_per_point", 0.15f) * static_cast<float>(pts_650);

  const int pts_651 = nodePoints(SwordArrayNodes::ManaSpring, specState.manaSpringPoints);
  array.mana_regen_per_sec = mechanics.GetFloat(kSkillId, SwordArrayNodes::ManaSpring, "mana_regen_per_point", 2.0f) * static_cast<float>(pts_651);

  const int pts_652 = nodePoints(SwordArrayNodes::MindUnity, specState.mindUnityPoints);
  array.gain_intent_on_tick = pts_652 > 0;
  array.intent_gen_chance = mechanics.GetFloat(kSkillId, SwordArrayNodes::MindUnity, "intent_chance_per_point", 0.333333f) * static_cast<float>(std::max(1, pts_652));

  const int pts_654 = nodePoints(SwordArrayNodes::CooldownRecovery, specState.cooldownRecoveryPoints);
  array.cdr_buff = mechanics.GetFloat(kSkillId, SwordArrayNodes::CooldownRecovery, "cdr_pct_per_point", 0.10f) * static_cast<float>(pts_654);

  const int pts_655 = nodePoints(SwordArrayNodes::ArrayWard, specState.arrayWardPoints);
  array.ward_int_mult_per_sec = mechanics.GetFloat(kSkillId, SwordArrayNodes::ArrayWard, "ward_int_pct_per_point", 0.50f) * static_cast<float>(pts_655);

  array.is_fire_field = nodeActive(SwordArrayNodes::FireField, specState.fireField);
  array.burn_stacks = static_cast<int>(mechanics.GetFloat(kSkillId, SwordArrayNodes::FireField, "burn_stacks", 2.0f));
  array.burn_duration = mechanics.GetFloat(kSkillId, SwordArrayNodes::FireField, "burn_duration", 3.0f);
  array.base_ignite_magnitude = mechanics.GetFloat(kSkillId, SwordArrayNodes::FireField, "base_ignite_magnitude", 15.0f);

  const int pts_671 = nodePoints(SwordArrayNodes::InfernalGround, specState.infernalGroundPoints);
  array.ignite_more_damage = mechanics.GetFloat(kSkillId, SwordArrayNodes::InfernalGround, "ignite_more_per_point", 0.20f) * static_cast<float>(pts_671);

  array.is_lightning_pool = nodeActive(SwordArrayNodes::LightningField, specState.lightningField);
  const int pts_673 = nodePoints(SwordArrayNodes::ChainThunder, specState.chainThunderPoints);
  // 落雷基线从机制表读取 (base_targets: 2)，再随机 2-3 名并叠加 673 每点 +1
  const int baseTargets = static_cast<int>(mechanics.GetFloat(kSkillId, SwordArrayNodes::LightningField, "base_targets", 2.0f));
  array.lightning_targets = baseTargets + utils::ThreadSafeRandom::GetInt(0, 1) + pts_673;
  array.has_chain_lightning = pts_673 > 0;
  array.chain_lightning_damage_pct = mechanics.GetFloat(kSkillId, SwordArrayNodes::ChainThunder, "chain_damage_pct", 0.50f);

  const int pts_674 = nodePoints(SwordArrayNodes::ArrayCorrosion, specState.arrayCorrosionPoints);
  array.corrosion_stacks_per_sec = mechanics.GetFloat(kSkillId, SwordArrayNodes::ArrayCorrosion, "stacks_per_second_per_point", 1.0f) * static_cast<float>(pts_674);
  array.corrosion_resist_per_stack = mechanics.GetFloat(kSkillId, SwordArrayNodes::ArrayCorrosion, "shred_per_stack", 2.0f);
  array.corrosion_max_stacks = static_cast<int>(mechanics.GetFloat(kSkillId, SwordArrayNodes::ArrayCorrosion, "max_stacks", 10.0f));
  array.corrosion_linger_duration = mechanics.GetFloat(kSkillId, SwordArrayNodes::ArrayCorrosion, "debuff_linger", 3.0f);

  array.allow_relocate = allow_relocate;
  // 从烘焙 profile 继承的总增伤乘数 (供 613/614 等硬编码效果力消费，与 650 主伤害路径一致)
  array.damage_more_mult = (profile && profile->more_damage_mult > 0.0f) ? profile->more_damage_mult : 1.0f;

  const int pts_612 = nodePoints(SwordArrayNodes::Resonance, specState.resonancePoints);
  array.resonance_more_mult = 1.0f + mechanics.GetFloat(kSkillId, SwordArrayNodes::Resonance, "resonance_more_per_point", 0.20f) * static_cast<float>(pts_612);

  array.has_chain_connection = nodeActive(SwordArrayNodes::Connection, specState.connection);
  array.has_dash_detonation = nodeActive(SwordArrayNodes::DashTrigger, specState.dashTrigger);
  const int pts_615 = nodePoints(SwordArrayNodes::SwordStepArray, specState.swordStepArrayPoints);
  array.sword_step_frequency_bonus = mechanics.GetFloat(kSkillId, SwordArrayNodes::SwordStepArray, "freq_bonus_per_point", 0.20f) * static_cast<float>(pts_615);

  // 互斥安全防线：火/雷互斥优先保留火
  if (array.is_fire_field && array.is_lightning_pool) {
    array.is_lightning_pool = false;
  }
  if (array.is_lightning_pool && (!profile || profile->delivery.sub_interval <= 0.0f)) {
    array.damage_interval = mechanics.GetFloat(kSkillId, SwordArrayNodes::LightningField, "lightning_interval", 1.0f);
  }

  // 4. 元素转换与视觉色彩联动 (L8)
  Tag effective_tag = Tag::Physical;
  const Tag attunement = systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);

  if (array.is_fire_field || (profile && HasTag(profile->effective_tags, Tag::Fire))) {
    effective_tag = Tag::Fire;
    array.core_color = {255, 80, 20, 255};
    array.glow_color = {255, 160, 40, 255};
    auto &mods = registry.emplace_or_replace<SkillModifierComponent>(array_ent);
    mods.damage_modifiers.push_back({Tag::Physical, Tag::Fire, 1.0f, ModifierType::Convert});
  } else if (array.is_lightning_pool || (profile && HasTag(profile->effective_tags, Tag::Lightning))) {
    effective_tag = Tag::Lightning;
    array.core_color = {80, 180, 255, 255};
    array.glow_color = {160, 220, 255, 255};
    auto &mods = registry.emplace_or_replace<SkillModifierComponent>(array_ent);
    mods.damage_modifiers.push_back({Tag::Physical, Tag::Lightning, 1.0f, ModifierType::Convert});
  } else if (attunement != Tag::None) {
    effective_tag = attunement;
    if (attunement == Tag::Fire) {
      array.core_color = {255, 80, 20, 255};
      array.glow_color = {255, 160, 40, 255};
    } else if (attunement == Tag::Lightning) {
      array.core_color = {80, 180, 255, 255};
      array.glow_color = {160, 220, 255, 255};
    }
    auto &mods = registry.emplace_or_replace<SkillModifierComponent>(array_ent);
    mods.damage_modifiers.push_back({Tag::Physical, attunement, 0.5f, ModifierType::Convert});
  } else if (auto *ownerMods = registry.try_get<SkillModifierComponent>(owner)) {
    registry.emplace_or_replace<SkillModifierComponent>(array_ent, *ownerMods);
  }

  array.effective_tag = effective_tag;

  // 5. 地表领域交付组件 (AreaFieldComponent)
  auto &field = registry.emplace<AreaFieldComponent>(array_ent);
  field.owner = owner;
  field.cast_id = exec.cast_id;
  field.source_skill_id = kSkillId;
  field.remaining_duration = dur;
  field.radius = rad;
  field.pulse_interval = array.damage_interval;
  field.payload_count = 0;

  field.payloads[field.payload_count++] = PayloadDefinition{
      .type = PayloadType::Damage,
      .value_mult = 0.4f * (profile ? profile->more_damage_mult : 1.0f),
      .damage_tags = effective_tag
  };
}

void SwordArray::Update(entt::registry &registry, entt::entity entity, SwordArrayComponent &array, float dt, const systems::SpatialHashGrid &grid) {
  // 随身剑垒跟随所有者移动
  if (array.is_mobile_aura && registry.valid(array.owner)) {
    if (const auto *ownerPos = registry.try_get<Position>(array.owner)) {
      if (auto *pos = registry.try_get<Position>(entity)) {
        pos->x = ownerPos->x;
        pos->y = ownerPos->y;
      }
    }
  }

  // 寿命同步与单一权威管理
  if (auto *field = registry.try_get<AreaFieldComponent>(entity)) {
    array.duration = field->remaining_duration;
    if (field->remaining_duration <= 0.0f) {
      return;
    }
  } else {
    array.duration -= dt;
    if (array.duration <= 0.0f) {
      registry.destroy(entity);
      return;
    }
  }

  const auto *pos = registry.try_get<Position>(entity);
  if (!pos) return;

  // 634 剑阵牢笼: 实体剑墙，阻挡敌人进出边缘
  if (array.has_cage) {
    grid.query(Position{pos->x, pos->y}, array.radius + 40.0f, [&](entt::entity target, const Position &) {
      if (!registry.valid(target) || !registry.any_of<::EnemyTag>(target)) return;
      auto *ePos = registry.try_get<Position>(target);
      if (!ePos) return;
      float dx = ePos->x - pos->x;
      float dy = ePos->y - pos->y;
      float dist = std::sqrt(dx * dx + dy * dy);
      if (dist < 1e-4f) return;
      float nx = dx / dist;
      float ny = dy / dist;
      if (dist < array.radius) {
        if (dist > array.radius - 15.0f) {
          ePos->x = pos->x + nx * (array.radius - 15.0f);
          ePos->y = pos->y + ny * (array.radius - 15.0f);
        }
      } else {
        if (dist < array.radius + 15.0f) {
          ePos->x = pos->x + nx * (array.radius + 15.0f);
          ePos->y = pos->y + ny * (array.radius + 15.0f);
        }
      }
    });
  }

  // 614 流云穿阵: 当位移技能穿过自己的剑阵时，引爆剑阵造成 150% 物理爆发伤害并销毁
  if (array.has_dash_detonation && registry.valid(array.owner)) {
    const auto *oPos = registry.try_get<Position>(array.owner);
    bool isDashing = false;
    if (const auto *dash = registry.try_get<::DashComponent>(array.owner)) {
      if (dash->isDashing) isDashing = true;
    }
    if (!isDashing && registry.any_of<MobilityComponent>(array.owner)) {
      isDashing = true;
    }
    if (!isDashing) {
      if (const auto *effects = registry.try_get<ActiveEffectsComponent>(array.owner)) {
        if (effects->Get(BuffId::SwordStep) != nullptr || effects->Get("Dash") != nullptr) {
          isDashing = true;
        }
      }
    }
    if (isDashing && oPos) {
      const float d2 = Vector2DistanceSqr({oPos->x, oPos->y}, {pos->x, pos->y});
      if (d2 <= array.radius * array.radius) {
        grid.query(Position{pos->x, pos->y}, array.radius, [&](entt::entity target, const Position &tPos) {
          if (!registry.valid(target) || target == array.owner || registry.any_of<KilledTag>(target)) return;
          if (!registry.any_of<::EnemyTag>(target)) return;
          if (Vector2DistanceSqr({tPos.x, tPos.y}, {pos->x, pos->y}) <= array.radius * array.radius) {
            DamageRequest req;
            req.attacker = array.owner;
            req.defender = target;
            req.skill_id = kSkillId;
            req.source_entity = entity;
            req.added_effectiveness = 1.50f * array.damage_more_mult;
            req.trigger_effectiveness = 1.50f * array.damage_more_mult;
            req.additional_tags = array.effective_tag;
            (void)ResolveDamage(registry, req, target);
          }
        });
        if (auto *field = registry.try_get<AreaFieldComponent>(entity)) {
          field->remaining_duration = 0.0f;
        }
        array.duration = 0.0f;
        registry.destroy(entity);
        return;
      }
    }
  }

  // 613 千丝万缕: 每两个存在的剑阵中心会自动生成能量连线，触碰敌人受到伤害并减速 50%
  if (array.has_chain_connection && registry.valid(array.owner)) {
    for (auto otherEnt : registry.view<SwordArrayComponent, Position>()) {
      if (otherEnt <= entity) continue;
      const auto &otherArr = registry.get<SwordArrayComponent>(otherEnt);
      if (otherArr.owner != array.owner) continue;
      const auto &otherPos = registry.get<Position>(otherEnt);
      Vector2 A = {pos->x, pos->y};
      Vector2 B = {otherPos.x, otherPos.y};
      Vector2 AB = Vector2Subtract(B, A);
      float lenSqr = Vector2LengthSqr(AB);
      if (lenSqr <= 1.0f) continue;
      Vector2 mid = Vector2Scale(Vector2Add(A, B), 0.5f);
      float checkRadius = std::sqrt(lenSqr) * 0.5f + 40.0f;

      grid.query(Position{mid.x, mid.y}, checkRadius, [&](entt::entity target, const Position &tPos) {
        if (!registry.valid(target) || !registry.any_of<::EnemyTag>(target) || registry.any_of<KilledTag>(target)) return;
        Vector2 P = {tPos.x, tPos.y};
        float t = std::clamp(Vector2DotProduct(Vector2Subtract(P, A), AB) / lenSqr, 0.0f, 1.0f);
        Vector2 proj = Vector2Add(A, Vector2Scale(AB, t));
        if (Vector2DistanceSqr(P, proj) <= 30.0f * 30.0f) {
          DamageRequest req;
          req.attacker = array.owner;
          req.defender = target;
          req.skill_id = kSkillId;
          req.source_entity = entity;
          req.added_effectiveness = 0.80f * dt * array.damage_more_mult;
          req.trigger_effectiveness = 0.80f * dt * array.damage_more_mult;
          req.additional_tags = array.effective_tag;
          (void)ResolveDamage(registry, req, target);

          auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);
          BuffEffect slow;
          slow.id = std::string(BuffIdToString(BuffId::SwordArrayConnectionSlow));
          slow.name = "Connection Slow";
          slow.type = BuffType::SpeedDown;
          slow.kind = BuffKind::Slow;
          slow.duration = 1.0f;
          slow.remaining = 1.0f;
          slow.is_debuff = true;
          StatModifier mod;
          mod.type = StatType::MoveSpeed;
          mod.mode = ModifierMode::PercentAdd;
          mod.value = -50.0f;
          mod.source = ModifierSource::Skill;
          slow.modifiers.push_back(mod);
          effects.AddOrRefresh(slow);
          (void)registry.get_or_emplace<StatsDirty>(target);
        }
      });
    }
  }

  // 阵内持续增益与每秒结算 (带多阵去重保护: 冷却由系统级统一递减)

  array.buff_timer += dt;
  if (array.buff_timer >= 1.0f) {
    array.buff_timer = 0.0f;
    if (registry.valid(array.owner)) {
      const auto *oPos = registry.try_get<Position>(array.owner);
      if (oPos) {
        const float d2 = Vector2DistanceSqr({oPos->x, oPos->y}, {pos->x, pos->y});
        if (d2 <= array.radius * array.radius) {
          auto &buffState = registry.get_or_emplace<SwordArrayOwnerBuffState>(array.owner);
          if (buffState.tick_cooldown <= 0.0f) {
            buffState.tick_cooldown = 1.0f;

            // 650 阵眼: 阵内全局伤害 More 15%..60% (全伤害类型通用)
            if (array.core_buff_more_damage > 0.0f) {
              auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(array.owner);
              BuffEffect core;
              core.id = std::string(BuffIdToString(BuffId::SwordArrayCore));
              core.name = "Sword Array Core";
              core.type = BuffType::AttackUp;
              core.duration = 1.2f;
              core.remaining = 1.2f;
              for (auto st : {StatType::PhysicalDamage, StatType::FireDamage, StatType::ColdDamage, StatType::LightningDamage, StatType::PoisonDamage, StatType::ShadowDamage}) {
                StatModifier mod;
                mod.type = st;
                mod.mode = ModifierMode::PercentMult;
                mod.value = array.core_buff_more_damage * 100.0f;
                mod.source = ModifierSource::Skill;
                core.modifiers.push_back(mod);
              }
              effects.AddOrRefresh(core);
            }

            // 651 灵力泉涌: 回蓝
            if (array.mana_regen_per_sec > 0.0f) {
              if (auto *stats = registry.try_get<CombatStats>(array.owner)) {
                stats->mana = std::min(stats->max_mana, stats->mana + array.mana_regen_per_sec);
              }
            }

            // 652 意念合一: 阵内概率生成剑意 (设计为阵内每秒概率生成)
            if (array.gain_intent_on_tick) {
              if (utils::ThreadSafeRandom::GetFloat01() <= array.intent_gen_chance) {
                SkillSystem::GainSwordIntent(registry, array.owner, 1, kSkillId);
              }
            }

            // 654 剑神领域: 冷却缩减 10%..30% (Flat 模式累加，避免 0 基底乘法失效)
            if (array.cdr_buff > 0.0f) {
              auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(array.owner);
              BuffEffect cdr;
              cdr.id = std::string(BuffIdToString(BuffId::SwordArrayCDR));
              cdr.name = "Sword Array CDR";
              cdr.type = BuffType::PowerBoost;
              cdr.duration = 1.2f;
              cdr.remaining = 1.2f;
              StatModifier mod;
              mod.type = StatType::CooldownReduction;
              mod.mode = ModifierMode::Flat;
              mod.value = array.cdr_buff * 100.0f;
              mod.source = ModifierSource::Skill;
              cdr.modifiers.push_back(mod);
              effects.AddOrRefresh(cdr);
            }

            // 655 法阵回护: 每秒智力比例 Ward
            if (array.ward_int_mult_per_sec > 0.0f) {
              if (auto *stats = registry.try_get<CombatStats>(array.owner)) {
                const float int_val = stats->effective_intelligence > 0.0f ? stats->effective_intelligence : 10.0f;
                stats->barrier = std::min(stats->max_barrier > 0.0f ? stats->max_barrier : 10000.0f,
                                          stats->barrier + int_val * array.ward_int_mult_per_sec);
              }
            }
          }
        }
      }
    }
  }
}

REGISTER_SKILL_BEHAVIOR(SwordArray)
void RegisterSwordArray() {}
} // namespace NoMoreDay::skills
