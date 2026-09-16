#pragma once

#include "game/foundation/components/ItemStats.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/modifier/ModifierContext.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <vector>

namespace NoMoreDay {

class EquipmentModifierAdapter {
public:
  [[nodiscard]] static ModifierEvalContext
  BuildContextFromCharacter(const entt::registry &registry, entt::entity entity,
                            uint32_t skillId, Tag skillTags);

  [[nodiscard]] static std::vector<uint32_t>
  CollectEquippedRecordIds(const entt::registry &registry, entt::entity entity);

  // 结算已装备词缀对指定技能的法力消耗乘算系数；无装备或词缀不匹配时返回 1.0f。
  [[nodiscard]] static float
  GetEquippedManaCostMultiplier(const entt::registry &registry,
                                entt::entity entity, uint32_t skillId,
                                Tag skillTags);

  static void ApplyEquippedSkillLevelBonuses(entt::registry &registry,
                                             entt::entity entity);
};

} // namespace NoMoreDay
