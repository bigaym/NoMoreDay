#include "game/systems/combat/DamageMitigationService.hpp"
#include "game/foundation/components/Buff.hpp" // ActiveEffectsComponent (减抗来源过滤)
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp" // 253 physical_ignore_res_pct
#include "game/systems/combat/CombatConstants.hpp"
#include "game/contracts/CombatFormula.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>
#include <bit>

namespace NoMoreDay {
namespace {

float ClampMoreToMultiplier(float more) {
  return std::max(0.0f, 1.0f + more);
}

} // namespace

float DamageMitigationService::ApplySkillScopedResistEffects(
    entt::registry &registry, entt::entity defender, uint32_t skill_id,
    DamageType type) {
  float aggregate = 0.0f;
  if (const auto *eff = registry.try_get<ActiveEffectsComponent>(defender)) {
    for (const auto &b : eff->effects) {
      // 来源技能归属过滤 (SkillOnly scope)：
      //   - source_skill_id == 0 的减抗对全体伤害生效（含 skill_id == 0 的无归属伤害）；
      //   - source_skill_id != 0 的减抗仅当 == 当前 skill_id 时生效，否则跳过。
      if (b.source_skill_id != 0 &&
          b.source_skill_id != static_cast<int>(skill_id))
        continue;
      for (const auto &m : b.modifiers) {
        if (m.mode != ModifierMode::Flat)
          continue;
        const bool isFire =
            (m.type == StatType::ResistFire && type == DamageType::Fire);
        const bool isCold =
            (m.type == StatType::ResistCold && type == DamageType::Cold);
        const bool isLightning =
            (m.type == StatType::ResistLightning && type == DamageType::Lightning);
        // 聚合语义：debuff 修饰符真正参与结算。resistances[] 以小数存储，
        // 而 StatType 修饰符以百分比点计 (AttributePipeline 除以 100)，故 /100。
        // m.value 为负(减抗)，累加后使抗性下降。
        if (isFire || isCold || isLightning)
          aggregate += m.value / 100.0f;
      }
    }
  }
  return aggregate;
}

float DamageMitigationService::Apply(
    entt::registry &registry, entt::entity attacker, entt::entity defender,
    uint32_t skill_id,
    Tag instance_tags, Tag final_type, float damage,
    const CombatStats *defender_stats,
    const systems::EndgameModifierAggregate &endgame, bool skip_mitigation,
    bool was_blocked, float block_multiplier, entt::entity source_entity) {
  using namespace NoMoreDay::Constants::Combat::Pipeline;

  const int type_idx = std::countr_zero(static_cast<uint64_t>(final_type));
  float damage_after_res = damage;
  if (skip_mitigation) {
    return damage_after_res;
  }

  if (was_blocked) {
    damage_after_res *= block_multiplier;
  }

  float res = 0.0f;
  if (type_idx < ELEMENTAL_TYPE_COUNT) {
    res = defender_stats ? defender_stats->resistances[type_idx] : 0.0f;
    // 减抗来源过滤 (SkillOnly scope)：聚合当前伤害生效的 debuff 减抗并参与结算
    res += ApplySkillScopedResistEffects(
        registry, defender, skill_id, static_cast<DamageType>(type_idx));
    // 技能2 灵根亲和 (Node 274)：裂空斩及其触发效果的对应元素抗性穿透增加 5%...20%
    if (skill_id == 2 && registry.valid(attacker)) {
      const auto *profile = SkillSystem::GetBakedSkillProfile(registry, attacker, 2);
      BakedSkillProfile localProfile;
      if (!profile && registry.all_of<ActiveSkillsComponent>(attacker)) {
        for (const auto &spec : registry.get<ActiveSkillsComponent>(attacker).specialized_slots) {
          if (spec.skill_id == 2) {
            SkillSpecializationBaker::Bake(registry, attacker, 2, &spec, localProfile, nullptr);
            profile = &localProfile;
            break;
          }
        }
      }
      if (profile && (profile->delivery.feature_flags & (1 << 22)) != 0 && profile->delivery.armor_pen > 0.0f) {
        if ((type_idx == static_cast<int>(DamageType::Cold) && HasTag(profile->effective_tags, Tag::Cold)) ||
            (type_idx == static_cast<int>(DamageType::Lightning) && HasTag(profile->effective_tags, Tag::Lightning))) {
          res -= (profile->delivery.armor_pen / 100.0f);
        }
      }
    }
    res += endgame.incoming_resistance_bonus;
    res -= endgame.outgoing_resistance_reduction;
    res = std::clamp(res, RESISTANCE_MIN, RESISTANCE_MAX);
  }

  damage_after_res *= (1.0f - res);

  if (final_type == Tag::Physical && defender_stats) {
    float armor = defender_stats->armor + endgame.incoming_armor_bonus;
    const float pen = StatsSystem::GetStatWithTags(
        registry, attacker, StatType::ArmorPenetration, instance_tags, skill_id,
        source_entity);
    float effective_armor =
        armor - pen - endgame.outgoing_armor_reduction;
    // 技能2 湮灭波 (Node 253)：满层剑意巨波无视物理护甲/抗性。
    // 消费交付层显式布尔标志（253 分支设置、经 Projectile 传递），取代旧哨兵值
    // snapshot.armor_pen>=500（穿透数值语义不得承载布尔标记，且被其它系统误读为穿透）
    // 与死条件 arcWidth>=100（几何字段仅渲染消费）。无视比例数据驱动：
    // mech 253.physical_ignore_res_pct（默认 50）→ 保留一半有效护甲。
    if (skill_id == 2 && source_entity != entt::null && registry.valid(source_entity)) {
      if (const auto *proj = registry.try_get<Projectile>(source_entity)) {
        if (proj->ignore_resist) {
          const float ignore_pct = data::SkillMechanicsRegistry::Get().GetFloat(
              2, 253, "physical_ignore_res_pct", 50.0f);
          effective_armor = std::max(0.0f, effective_armor * (1.0f - ignore_pct / 100.0f));
        }
      }
    }
    const int area_level = defender_stats->cached_area_level;
    const float armor_multiplier =
        NoMoreDay::CombatFormula::CalculateArmorMultiplier(effective_armor,
                                                           area_level);

    damage_after_res *= armor_multiplier;
  }

  if (defender_stats && defender_stats->damage_reduction > 0.0f) {
    const float effective_dr = std::clamp(
        defender_stats->damage_reduction +
            endgame.incoming_global_damage_reduction_bonus -
            endgame.outgoing_global_damage_reduction_reduction,
        0.0f, DR_MAX);
    damage_after_res *= (1.0f - effective_dr);
  } else {
    const float effective_dr = std::clamp(
        endgame.incoming_global_damage_reduction_bonus -
            endgame.outgoing_global_damage_reduction_reduction,
        0.0f, DR_MAX);
    damage_after_res *= (1.0f - effective_dr);
  }

  damage_after_res *= ClampMoreToMultiplier(endgame.incoming_damage_taken_more);

  if (defender_stats != nullptr) {
    damage_after_res *=
        systems::BladeResourceService::GetBloodthirstDamageTakenMultiplier(
            registry, defender);
  }

  return damage_after_res;
}

} // namespace NoMoreDay
