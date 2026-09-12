#pragma once

#include "game/foundation/components/Buff.hpp"

#include <cstdint>
#include <entt/entt.hpp>
#include <span>

namespace NoMoreDay {
namespace damage {

// 守方实时条件位掩码 (P3-1 攻守状态解耦)。
// 攻方快照只携带静态乘区与条件规则，命中时按本掩码极速求值，
// 避免把守方动态状态烘焙进攻方模板。
enum class TargetCondition : uint32_t {
  None = 0,
  Frozen = 1u << 0,       // 处于冻结 (BuffType::Freeze)
  Bleeding = 1u << 1,     // 处于流血 (BuffType::Bleed)
  HpAbove80 = 1u << 2,    // 生命值 > 80%
  Controlled = 1u << 3,   // 受控 (眩晕/冻结/定身/减速)
  HasFateMark = 1u << 4,  // 拥有命印 (BuffKind::FateMark)
  HasQiBrand = 1u << 5,   // 拥有气印 (BuffKind::QiBrand)
};

// 条件位可组合 (如 HpAbove80 | Controlled 表示“任一满足”)，故提供按位或。
constexpr TargetCondition operator|(TargetCondition a, TargetCondition b) {
  return static_cast<TargetCondition>(static_cast<uint32_t>(a) |
                                      static_cast<uint32_t>(b));
}

constexpr uint32_t ToMask(TargetCondition condition) {
  return static_cast<uint32_t>(condition);
}

// 条件修饰阶段：More 进 More 乘区；CritDamage 为暴击倍率增量。
enum class OpStage : uint8_t { More = 0, CritDamage = 1 };

// 动态条件修饰项 (轻量 POD)。base_value/per_stack_value 均为小数增量：
//   More:       最终系数 *= (1 + base_value + per_stack_value * stacks)
//   CritDamage: extra_crit_mult += base_value + per_stack_value * stacks
// required_condition 允许组合位，按“任一命中”求值。
struct ConditionalDamageOp {
  TargetCondition required_condition = TargetCondition::None;
  OpStage stage = OpStage::More;
  float base_value = 0.0f;
  float per_stack_value = 0.0f;
  BuffKind stack_source = BuffKind::None;
};

// BuffKind 数量上界：None/QiBrand/FateMark/FreeCast。
constexpr uint32_t kBuffKindCount = 4;

// 守方条件求值结果：位掩码 + 按 BuffKind 索引的叠层数 (热路径无分配)。
struct TargetConditionState {
  uint32_t mask = 0;
  int stacks[kBuffKindCount] = {0, 0, 0, 0};
};

inline int StackCountFor(const TargetConditionState &state, BuffKind kind) {
  const uint32_t idx = static_cast<uint32_t>(kind);
  return idx < kBuffKindCount ? state.stacks[idx] : 0;
}

// 只读、无分配：读取守方 Health/Buff/异常组件，产出条件掩码与叠层数。
[[nodiscard]] TargetConditionState
EvaluateTargetConditionState(const entt::registry &registry,
                             entt::entity target);

// 应用 More 阶段条件规则：返回乘算系数 (无匹配为 1.0)。
[[nodiscard]] float
ApplyConditionalMore(std::span<const ConditionalDamageOp> ops,
                     const TargetConditionState &state);

// 应用暴击倍率阶段条件规则：返回增量之和 (无匹配为 0.0)。
[[nodiscard]] float
ApplyConditionalCritDamage(std::span<const ConditionalDamageOp> ops,
                           const TargetConditionState &state);

} // namespace damage
} // namespace NoMoreDay
