#pragma once
// 效果层 SpecState 表驱动解析基元（A-01 D3=b）。
//
// 把技能 10~12 手写的「POD SpecState + 节点成员指针绑定表 + ResolveSpecState」
// 三连泛化为单一模板：节点 id -> State 成员指针，首个匹配槽位一次性填表。
// 约束：
//   - State 必须为 POD（缺省构造即全 0/false）；
//   - 本基元只读专精点数（ReadPoints/HasNode），不读 skill_mechanics
//     （机制数值由效果逻辑经 GetMech 单独取，见设计 §8.1）；
//   - 转质/非点读节点不得作为 **point** 绑定入表：技能 10 的 1021/1022
//     （PoleStarOrbit/Starfall）现为 descriptor flag 绑定，表内 HasNode 填充仅为占位；
//     技能 10 的 2 参包装函数会以 `SkillSystem::GetActiveTransmuterNode` 判等覆盖这两个字段，
//     任何绕过包装函数直接使用该表的消费者不得读取 poleStarOrbit/starfall；
//     技能 12 的转质节点按 **flag** 绑定入表（0/1 选择态，生成器授权）；
//     节点 973 属技能 9 `OverloadShield`（设计不变量 §5.4-2）。
#include "game/foundation/components/SkillPointAccess.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <type_traits>

namespace NoMoreDay::skills {

template <typename State>
struct SpecPointBinding {
  uint32_t node = 0;
  int State::*points = nullptr;
};

template <typename State>
struct SpecFlagBinding {
  uint32_t node = 0;
  bool State::*flag = nullptr;
};

// 单技能绑定表。节点集合须与 talent_tree 一致（缺省 Passive 仍须进表）。
template <typename State>
struct SpecStateTable {
  using PointBinding = SpecPointBinding<State>;
  using FlagBinding = SpecFlagBinding<State>;

  std::span<const PointBinding> points;
  std::span<const FlagBinding> flags;
};

// 解析：首个 skill_id 匹配的专精槽位一次性填表；无槽位时返回 POD 缺省值。
// 与手写实现语义一致：先填 points 再填 flags，命中即停止遍历。
template <typename State>
[[nodiscard]] State ResolveSpecState(const entt::registry &registry,
                                     entt::entity owner, uint32_t skillId,
                                     const SpecStateTable<State> &table) {
  static_assert(std::is_trivially_copyable_v<State> &&
                    std::is_standard_layout_v<State>,
                "SpecState 必须为 POD（A-01 D-A1）：可平凡复制且标准布局");
  State state{};
  const auto *active = registry.try_get<ActiveSkillsComponent>(owner);
  if (active == nullptr) {
    return state;
  }
  for (const auto &spec : active->specialized_slots) {
    if (spec.skill_id != skillId) {
      continue;
    }
    for (const auto &binding : table.points) {
      state.*(binding.points) = std::max(0, ReadPoints(spec, binding.node));
    }
    for (const auto &binding : table.flags) {
      state.*(binding.flag) = HasNode(spec, binding.node);
    }
    break;
  }
  return state;
}

} // namespace NoMoreDay::skills
