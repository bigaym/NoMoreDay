#include "game/systems/combat/DamageMitigationService.hpp"
#include "game/systems/combat/damage/DamageTypes.hpp"
#include "game/foundation/components/Buff.hpp" // ActiveEffectsComponent (减抗来源过滤)
#include "game/foundation/components/Common.hpp" // Position (元素路径归属判定)
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp" // 253 physical_ignore_res_pct
#include "game/systems/combat/CombatConstants.hpp"
#include "game/contracts/CombatFormula.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/ElementPathSystem.hpp" // 871/875/876 元素路径
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>
#include <bit>

namespace NoMoreDay {
namespace {

// 御剑·回旋技能 ID：元素路径系列节点 (871/875/876) 的抗性修正如挂于本技能。
constexpr uint32_t kSkill8Id = 8u;

float ClampMoreToMultiplier(float more) {
  return std::max(0.0f, 1.0f + more);
}

} // namespace

// Type E 抗性上限压制 (技能7 心念灭抗 775) 聚合量：
// 与 ApplySkillScopedResistEffects 同构的 SkillOnly 过滤，但读取专用的
// resist_cap_suppression 字段而非 StatModifier：
//   - 仅统计 is_debuff 效果 (Type E 语义上必为减益)；
//   - source_skill_id == 0 对全体伤害生效；!= 0 仅当 == 当前 skill_id 时生效；
//   - resist_cap_element == Tag::None (全元素) 或 == 当前伤害元素时才计入；
//   - 多个来源取 max 而非求和：上限压制是对抗性天花板的"收紧"，取最强压制
//     最符合"上限"语义，可避免同一节点重复施加导致压制叠加、甚至令有效上限
//     跌破 RESISTANCE_MIN；后续再以 max(RESISTANCE_MIN, ...) 兜底。
// 单实体 (Apply) 与批处理 (CalculateBatch) 共用，定义于此、由 DamagePipeline 跨 TU 调用。
float AggregateSkillScopedResistCapSuppression(entt::registry &registry,
                                               entt::entity defender,
                                               uint32_t skill_id,
                                               DamageType type) {
  if (!registry.valid(defender))
    return 0.0f;
  const auto *eff = registry.try_get<ActiveEffectsComponent>(defender);
  if (eff == nullptr)
    return 0.0f;
  const Tag element_tag = damage::ElementTagOf(type);
  float suppression = 0.0f;
  for (const auto &b : eff->effects) {
    if (!b.is_debuff || b.resist_cap_suppression <= 0.0f)
      continue;
    if (b.source_skill_id != 0 &&
        b.source_skill_id != static_cast<int>(skill_id))
      continue;
    if (b.resist_cap_element != Tag::None &&
        b.resist_cap_element != element_tag)
      continue;
    suppression = std::max(suppression, b.resist_cap_suppression);
  }
  return suppression;
}

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
    bool was_blocked, float block_multiplier, entt::entity source_entity,
    float armor_pen_override) {
  using namespace NoMoreDay::Constants::Combat::Pipeline;

  const int type_idx = std::countr_zero(static_cast<uint64_t>(final_type));
  // final_type 是 Tag 位（池序），与 DamageType / resistances[] 索引仅 4/5
  // 位 (Shadow/Poison) 互换，必须经 damage::PoolIndexTo* 映射后再消费。
  const DamageType damage_type =
      damage::PoolIndexToDamageType(static_cast<size_t>(type_idx));
  const size_t resist_index =
      damage::PoolIndexToResistIndex(static_cast<size_t>(type_idx));
  float damage_after_res = damage;
  if (skip_mitigation) {
    return damage_after_res;
  }

  if (was_blocked) {
    damage_after_res *= block_multiplier;
  }

  float res = 0.0f;
  if (type_idx < ELEMENTAL_TYPE_COUNT) {
    res = defender_stats ? defender_stats->resistances[resist_index] : 0.0f;
    // 减抗来源过滤 (SkillOnly scope)：聚合当前伤害生效的 debuff 减抗并参与结算
    res += ApplySkillScopedResistEffects(
        registry, defender, skill_id, damage_type);
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
        if ((damage_type == DamageType::Cold && HasTag(profile->effective_tags, Tag::Cold)) ||
            (damage_type == DamageType::Lightning && HasTag(profile->effective_tags, Tag::Lightning))) {
          res -= (profile->delivery.armor_pen / 100.0f);
        }
      }
    }
    // 技能 5 灵根感应 (Node 574): 万剑归宗对应元素抗性穿透增加 (Int -> 穿透, 上限 30%)
    // 热路径保护：绝大多数敌人/未专精玩家没有技能 5 槽位，直接跳过，
    // 仅在确有技能 5 槽位且缺 Bake 结果时才执行整树 Bake 兜底，避免反复高开销。
    if (skill_id == 5 && registry.valid(attacker)) {
      const auto *active = registry.try_get<ActiveSkillsComponent>(attacker);
      if (active != nullptr) {
        const SpecializedSkill *skill5Slot = nullptr;
        for (const auto &spec : active->specialized_slots) {
          if (spec.skill_id == 5u) {
            skill5Slot = &spec;
            break;
          }
        }
        if (skill5Slot != nullptr) {
          const auto *profile = SkillSystem::GetBakedSkillProfile(registry, attacker, 5u);
          BakedSkillProfile localProfile;
          if (profile == nullptr) {
            SkillSpecializationBaker::Bake(registry, attacker, 5u, skill5Slot,
                                           localProfile, nullptr);
            profile = &localProfile;
          }
          if (profile && (profile->delivery.feature_flags & 33554432) != 0 &&
              profile->delivery.armor_pen > 0.0f) {
            if ((damage_type == DamageType::Fire &&
                 HasTag(profile->effective_tags, Tag::Fire)) ||
                (damage_type == DamageType::Cold &&
                 HasTag(profile->effective_tags, Tag::Cold))) {
              res -= (profile->delivery.armor_pen / 100.0f);
            }
          }
        }
      }
    }
    // 技能8 灵根破壁 (Node 875)：飞剑命中处于施法者自身元素路径内的敌人时，
    // 本次攻击的对应元素穿透提升 (SkillOnly 语义由契约 scope_policies 保证)。
    // 仅技能8、路径穿透非零且目标确在路径内时生效，技能1..7/9 行为不变。
    if (skill_id == kSkill8Id && registry.valid(attacker) &&
        registry.valid(defender)) {
      const Tag path_element = damage::ElementTagOf(damage_type);
      if (path_element != Tag::None) {
        const float path_pen =
            element_path::PenetrationFor(registry, attacker, path_element);
        if (path_pen > 0.0f) {
          const auto *defender_pos = registry.try_get<Position>(defender);
          if (defender_pos != nullptr &&
              element_path::IsInside(registry, attacker, path_element,
                                     {defender_pos->x, defender_pos->y})) {
            res -= path_pen;
          }
        }
      }
    }
    res += endgame.incoming_resistance_bonus;
    res -= endgame.outgoing_resistance_reduction;
    // Type E (技能7 心念灭抗 775)：抗性"上限"被动态压制。
    // 有效上限 = max(下限, 上限 - 压制量)，确保扣除后不低于 RESISTANCE_MIN。
    const float cap_suppression = AggregateSkillScopedResistCapSuppression(
        registry, defender, skill_id, damage_type);
    const float effective_max =
        std::max(RESISTANCE_MIN, RESISTANCE_MAX - cap_suppression);
    res = std::clamp(res, RESISTANCE_MIN, effective_max);
  }

  damage_after_res *= (1.0f - res);

  // 技能8 燎原之势 (Node 871)：目标处于施法者的元素路径内时，受到的对应元素
  // 伤害总增 (More) 乘算 (1 + amp)。仅技能8、路径增幅非零且目标在路径内时生效。
  // 技能8 元素护体 (Node 876)：施法者站在自身对应元素路径上时，受到的该元素
  // 伤害绝对减伤乘算 (1 - pct)。仅防守方拥有路径与已投入节点时生效。
  {
    const Tag path_element = damage::ElementTagOf(damage_type);
    if (path_element != Tag::None) {
      if (skill_id == kSkill8Id && registry.valid(attacker) &&
          registry.valid(defender)) {
        const float path_amp =
            element_path::AmpAgainst(registry, attacker, path_element);
        if (path_amp > 0.0f) {
          const auto *defender_pos = registry.try_get<Position>(defender);
          if (defender_pos != nullptr &&
              element_path::IsInside(registry, attacker, path_element,
                                     {defender_pos->x, defender_pos->y})) {
            damage_after_res *= (1.0f + path_amp);
          }
        }
      }
      if (registry.valid(defender)) {
        const float shield_pct =
            element_path::ShieldReductionFor(registry, defender, path_element);
        if (shield_pct > 0.0f) {
          damage_after_res *= (1.0f - shield_pct);
        }
      }
    }
  }

  if (final_type == Tag::Physical && defender_stats) {
    float armor = defender_stats->armor + endgame.incoming_armor_bonus;
    // 单目标快照路径由 DamagePipeline 传入冻结的 snapshot.armor_pen；
    // armor_pen_override < 0 表示无覆盖，回退实时查询（批量/历史调用方语义不变）。
    const float pen =
        (armor_pen_override >= 0.0f)
            ? armor_pen_override
            : StatsSystem::GetStatWithTags(
                  registry, attacker, StatType::ArmorPenetration, instance_tags,
                  skill_id, source_entity);
    float effective_armor =
        armor - pen - endgame.outgoing_armor_reduction;
    // 技能2 湮灭波 (Node 253)：满层剑意巨波无视物理护甲/抗性。
    // 消费交付层显式布尔标志（253 分支设置、经 Projectile 传递），取代旧哨兵值
    // snapshot.armor_pen>=500（穿透数值语义不得承载布尔标记）。snapshot.armor_pen
    // 已由单目标快照路径经 armor_pen_override 正常消费，不再承载布尔语义。
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

  // 技能 3 灵剑护体 (Node 350)：每柄活跃灵剑提供全局伤害减免 (1%...5%/柄)
  if (registry.valid(defender)) {
    if (const auto *formation = registry.try_get<BladeFormationComponent>(defender)) {
      if (formation->current_swords > 0 && formation->ward_dr_per_sword > 0.0f) {
        const float ward_dr = std::clamp(
            static_cast<float>(formation->current_swords) * formation->ward_dr_per_sword,
            0.0f, 0.75f);
        damage_after_res *= (1.0f - ward_dr);
      }
    }

    // 技能 5 气定神闲 (Node 530): 仅当引导万剑归宗（技能 5）时受到的所有伤害降低 6%...24%
    // 校验组件携带的 skill_id，避免玩家引导任意技能时误享技能 5 减伤。
    const auto *beamChannel = registry.try_get<BeamChannelComponent>(defender);
    const auto *channeling = registry.try_get<ChannelingComponent>(defender);
    const bool channelingSkill5 = (beamChannel != nullptr && beamChannel->skill_id == 5u) ||
                                  (channeling != nullptr && channeling->skill_id == 5u);
    if (channelingSkill5) {
      const auto *profile = SkillSystem::GetBakedSkillProfile(registry, defender, 5u);
      if (profile && (profile->delivery.feature_flags & 512) != 0) {
        int pts_530 = 0;
        if (const auto *active = registry.try_get<ActiveSkillsComponent>(defender)) {
          for (const auto &spec : active->specialized_slots) {
            if (spec.skill_id == 5u) {
              auto it = spec.allocated_points.find(530);
              if (it != spec.allocated_points.end()) pts_530 = it->second;
              break;
            }
          }
        }
        if (pts_530 > 0) {
          float dr = std::clamp(static_cast<float>(pts_530) * 0.06f, 0.0f, 0.50f);
          damage_after_res *= (1.0f - dr);
        }
      }
    }
  }

  return damage_after_res;
}

} // namespace NoMoreDay
