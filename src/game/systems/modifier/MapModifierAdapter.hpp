#pragma once

#include "game/foundation/components/WorldState.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"

#include <cstdint>

namespace NoMoreDay {

/**
 * @brief 地图词缀记录/节点 ID 编码常量。
 *
 * 必须与 scripts/gen_map_monster_modifier_v2.py 的 MAP_ID_BASE /
 * MAP_RESONANCE_ID / MAP_NODE_BASE / MAP_RESONANCE_NODE 保持一致。
 * 暴露给测试以便守护常量漂移：map 词缀 record_id 与 MapAffixDefinition::combatStat
 * 的映射一致性由 MapModifierAdapterTests 校验。
 */
namespace MapModifierIds {
inline constexpr uint32_t kMapNodeIdBase = 700000u;
inline constexpr uint32_t kMapResonanceNode = 799999u;
// 词缀 record id = 基数 + MapAffixType 枚举值，与生成器 MAP_ID_BASE 对齐。
inline constexpr uint32_t kMapRecordIdBase = 4001000u;
// 共鸣词缀无对应枚举，使用生成器 MAP_RESONANCE_ID 约定的独立记录。
inline constexpr uint32_t kMapResonanceRecordId = 4099999u;
} // namespace MapModifierIds

class MapModifierAdapter {
public:
  [[nodiscard]] static ModifierDelta
  EvaluateEnemyAffixDelta(const ActiveDimensionalState &state);
};

} // namespace NoMoreDay
