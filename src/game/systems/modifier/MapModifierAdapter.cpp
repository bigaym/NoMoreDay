#include "game/systems/modifier/MapModifierAdapter.hpp"

#include "game/data/MapAffix.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/modifier/ModifierContext.hpp"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint32_t kMapNodeIdBase = 700000u;
constexpr uint32_t kMapResonanceNode = 799999u;

uint32_t EncodeMapAffixNodeId(const MapAffixType type) {
  return kMapNodeIdBase + static_cast<uint32_t>(type);
}

void AddPercentMultRecord(std::vector<ModifierRecord> &records,
                          const uint32_t nodeId, const StatType statType,
                          const float value) {
  ModifierRecord record;
  record.filter.node_id_whitelist = {nodeId};

  ModifierOp op;
  op.opcode = ModifierOpCode::ADD_STAT_PERCENT_MULT;
  op.param_u32 = static_cast<uint32_t>(statType);
  op.param_f32 = value;
  record.ops.push_back(op);

  records.push_back(std::move(record));
}

} // namespace

ModifierDelta
MapModifierAdapter::EvaluateEnemyAffixDelta(const ActiveDimensionalState &state) {
  ModifierEvalContext ctx;
  std::vector<ModifierRecord> records;

  if (!state.isActive) {
    return ModifierDelta{};
  }

  if (state.resonance.totalEnemyDensity > 0.0f) {
    ctx.active_node_ids.push_back(kMapResonanceNode);
    AddPercentMultRecord(records, kMapResonanceNode, StatType::MaxHealth,
                         state.resonance.totalEnemyDensity * 0.05f);
  }

  for (const auto &affix : state.explicitAffixes) {
    const uint32_t nodeId = EncodeMapAffixNodeId(affix.type);
    switch (affix.type) {
    case MapAffixType::Enemy_ExtraHealth:
      ctx.active_node_ids.push_back(nodeId);
      AddPercentMultRecord(records, nodeId, StatType::MaxHealth, affix.value);
      break;
    case MapAffixType::Enemy_ExtraDamage:
      ctx.active_node_ids.push_back(nodeId);
      AddPercentMultRecord(records, nodeId, StatType::PhysicalDamage,
                           affix.value);
      break;
    case MapAffixType::Enemy_Fast:
      ctx.active_node_ids.push_back(nodeId);
      AddPercentMultRecord(records, nodeId, StatType::MoveSpeed, affix.value);
      break;
    default:
      break;
    }
  }

  return ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records.data(), records.size()), ctx);
}

} // namespace NoMoreDay
