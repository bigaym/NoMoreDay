#pragma once

#include "game/foundation/components/ItemStats.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/modifier/ModifierContext.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <vector>

namespace NoMoreDay {

// 未誓约职业哨兵值（与 AstrolabeComponent::mainProfession 的默认值一致）。
inline constexpr int32_t kUnswornProfessionId = -1;

// 已装备修饰记录的来源引用：记录 ID 与其所在装备槽位掩码配对。
// 求值层按槽位分组时用它为同一槽位施加一致的 equip_slot_mask。
struct EquippedRecordRef {
  uint32_t record_id = 0;
  uint32_t slot_bit = 0;
};

class EquipmentModifierAdapter {
public:
  [[nodiscard]] static ModifierEvalContext
  BuildContextFromCharacter(const entt::registry &registry, entt::entity entity,
                            uint32_t skillId, Tag skillTags);

  // 已装备记录 ID 的排序去重视图（仅 ID，供过滤/诊断使用）。
  [[nodiscard]] static std::vector<uint32_t>
  CollectEquippedRecordIds(const entt::registry &registry, entt::entity entity);

  // 已装备记录的来源引用，按 (slot_bit, record_id) 去重并排序；
  // 槽位掩码为 1u << 槽位下标（下标即 EquipmentSlot 值），戒指三槽归一为
  // Ring1|Ring2|Ring 的组合掩码（互为别名）。
  [[nodiscard]] static std::vector<EquippedRecordRef>
  CollectEquippedRecordRefs(const entt::registry &registry, entt::entity entity);

  // 结算已装备词缀对指定技能的法力消耗乘算系数；无装备或词缀不匹配时返回 1.0f。
  [[nodiscard]] static float
  GetEquippedManaCostMultiplier(const entt::registry &registry,
                                entt::entity entity, uint32_t skillId,
                                Tag skillTags);

  static void ApplyEquippedSkillLevelBonuses(entt::registry &registry,
                                             entt::entity entity);
};

} // namespace NoMoreDay
