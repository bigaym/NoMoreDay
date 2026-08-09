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

  static void ApplyEquippedSkillLevelBonuses(entt::registry &registry,
                                             entt::entity entity);
};

} // namespace NoMoreDay
