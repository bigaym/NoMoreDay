#pragma once
// 技能节点读点 helper：全工程（行为/UI/战斗层）唯一的节点点数与机制数值读取入口。
// 置于 foundation 层，避免 UI/战斗层反向依赖 behaviors 层（SkillBehaviorBase.hpp）。
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"

#include <cstdint>
#include <string_view>

namespace NoMoreDay::skills {

// 节点点数读点：SpecializedSkill 是全工程唯一的节点点数据来源。
// 所有行为/UI 必须经本 helper 读点，禁止直接 allocated_points.find/contains/count。
[[nodiscard]] inline int ReadPoints(const SpecializedSkill &spec,
                                    uint32_t node) noexcept {
  const auto it = spec.allocated_points.find(node);
  return it != spec.allocated_points.end() ? it->second : 0;
}

[[nodiscard]] inline int ReadPoints(const SpecializedSkill *spec,
                                    uint32_t node) noexcept {
  return spec ? ReadPoints(*spec, node) : 0;
}

// 节点点亮判定：ReadPoints > 0，全工程唯一的点亮语义词。
[[nodiscard]] inline bool HasNode(const SpecializedSkill &spec,
                                  uint32_t node) noexcept {
  return ReadPoints(spec, node) > 0;
}

[[nodiscard]] inline bool HasNode(const SpecializedSkill *spec,
                                  uint32_t node) noexcept {
  return spec && ReadPoints(*spec, node) > 0;
}

// 机制数值读点：唯一来源为 skill_mechanics.json（SkillMechanicsRegistry），
// 禁止在行为中硬编码机制数值；缺失键返回 default_value（结构错误由加载期 schema 校验兜底）。
[[nodiscard]] inline float GetMech(uint32_t skill_id, uint32_t node_id,
                                   std::string_view key,
                                   float default_value) noexcept {
  return data::SkillMechanicsRegistry::Get().GetFloat(skill_id, node_id, key,
                                                       default_value);
}

} // namespace NoMoreDay::skills
