#include "game/systems/combat/damage/DamageConditions.hpp"

#include "game/foundation/components/Common.hpp" // HealthComponent
#include "game/foundation/components/Stats.hpp"  // CombatStats

#include <string>

namespace NoMoreDay {
namespace damage {
namespace {

// 判断单个 BuffEffect 是否构成“受控”：与历史 DamagePipeline 口径一致，
// 覆盖眩晕/冻结/定身/减速，以及 id 文本含 "Slow" 的 legacy 减速效果。
bool IsControllingEffect(const BuffEffect &effect) {
  if (effect.type == BuffType::Stun || effect.type == BuffType::Freeze ||
      effect.type == BuffType::Root || effect.type == BuffType::SpeedDown) {
    return true;
  }
  return effect.id.find("Slow") != std::string::npos;
}

} // namespace

TargetConditionState
EvaluateTargetConditionState(const entt::registry &registry,
                             entt::entity target) {
  TargetConditionState state;
  if (target == entt::null || !registry.valid(target)) {
    return state;
  }

  // HP 阈值：优先 HealthComponent；缺失时回退 CombatStats，口径与旧实现一致
  // (HealthComponent 存在但 max<=0 时不回退)。
  bool hp_above_80 = false;
  if (const auto *hp = registry.try_get<HealthComponent>(target)) {
    hp_above_80 = (hp->max > 0.0f) && (hp->current / hp->max > 0.80f);
  } else if (const auto *stats = registry.try_get<CombatStats>(target)) {
    hp_above_80 = (stats->max_health > 0.0f) &&
                  (stats->health / stats->max_health > 0.80f);
  }
  if (hp_above_80) {
    state.mask |= ToMask(TargetCondition::HpAbove80);
  }

  const auto *effects = registry.try_get<ActiveEffectsComponent>(target);
  if (effects == nullptr) {
    return state;
  }

  for (const auto &effect : effects->effects) {
    if (effect.type == BuffType::Freeze) {
      state.mask |= ToMask(TargetCondition::Frozen);
    }
    if (effect.type == BuffType::Bleed) {
      state.mask |= ToMask(TargetCondition::Bleeding);
    }
    if (IsControllingEffect(effect)) {
      state.mask |= ToMask(TargetCondition::Controlled);
    }
    const uint32_t kind_idx = static_cast<uint32_t>(effect.kind);
    if (kind_idx < kBuffKindCount && state.stacks[kind_idx] == 0) {
      state.stacks[kind_idx] = effect.stacks;
    }
  }

  if (state.stacks[static_cast<uint32_t>(BuffKind::FateMark)] > 0) {
    state.mask |= ToMask(TargetCondition::HasFateMark);
  }
  if (state.stacks[static_cast<uint32_t>(BuffKind::QiBrand)] > 0) {
    state.mask |= ToMask(TargetCondition::HasQiBrand);
  }
  return state;
}

uint32_t EvaluateTargetConditions(const entt::registry &registry,
                                  entt::entity target) {
  return EvaluateTargetConditionState(registry, target).mask;
}

namespace {

// 条件匹配：required_condition 允许组合位，任一位置命中即视为满足。
inline bool ConditionMatches(uint32_t target_mask,
                             TargetCondition required) {
  const uint32_t required_mask = ToMask(required);
  return required_mask == 0 || (target_mask & required_mask) != 0;
}

} // namespace

float ApplyConditionalMore(std::span<const ConditionalDamageOp> ops,
                           const TargetConditionState &state) {
  float factor = 1.0f;
  for (const auto &op : ops) {
    if (op.stage != OpStage::More ||
        !ConditionMatches(state.mask, op.required_condition)) {
      continue;
    }
    const float stacks = static_cast<float>(StackCountFor(state, op.stack_source));
    factor *= (1.0f + op.base_value + op.per_stack_value * stacks);
  }
  return factor;
}

float ApplyConditionalCritDamage(std::span<const ConditionalDamageOp> ops,
                                 const TargetConditionState &state) {
  float extra = 0.0f;
  for (const auto &op : ops) {
    if (op.stage != OpStage::CritDamage ||
        !ConditionMatches(state.mask, op.required_condition)) {
      continue;
    }
    const float stacks = static_cast<float>(StackCountFor(state, op.stack_source));
    extra += op.base_value + op.per_stack_value * stacks;
  }
  return extra;
}

} // namespace damage
} // namespace NoMoreDay
