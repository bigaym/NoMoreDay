#include "game/systems/modifier/TalentModifierAdapter.hpp"

#include "game/components/Stats.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint32_t kMaxHealthStat = static_cast<uint32_t>(StatType::MaxHealth);
constexpr uint16_t kFlatStatOpcode =
    static_cast<uint16_t>(ModifierOpCode::ADD_STAT_FLAT);

bool HasIntersection(const std::span<const uint32_t> lhs,
                     const std::span<const uint32_t> rhs) {
  for (const uint32_t value : lhs) {
    if (std::find(rhs.begin(), rhs.end(), value) != rhs.end()) {
      return true;
    }
  }
  return false;
}

bool HasTargetedOp(const ModifierRuntimeRegistry &registry,
                   const ModifierRuntimeRecord &record) {
  for (const auto &op : registry.GetOps(record)) {
    if (op.opcode == kFlatStatOpcode && op.param_u32 == kMaxHealthStat) {
      return true;
    }
  }
  return false;
}

std::vector<uint32_t>
ResolveFlatHealthRecordIds(const ModifierRuntimeRegistry &registry,
                           const std::span<const uint32_t> nodeIds) {
  std::vector<uint32_t> recordIds;
  if (nodeIds.empty()) {
    return recordIds;
  }

  for (const auto &record : registry.GetRecords()) {
    const ModifierRuntimeFilter *filter = registry.GetFilter(record);
    if (filter == nullptr) {
      continue;
    }

    const auto nodeWhitelist = registry.GetNodeWhitelist(*filter);
    if (nodeWhitelist.empty() || !HasIntersection(nodeWhitelist, nodeIds)) {
      continue;
    }
    if (!HasTargetedOp(registry, record)) {
      continue;
    }

    recordIds.push_back(record.id);
  }

  std::sort(recordIds.begin(), recordIds.end());
  recordIds.erase(std::unique(recordIds.begin(), recordIds.end()),
                  recordIds.end());
  return recordIds;
}

} // namespace

std::vector<uint32_t> TalentModifierAdapter::CollectActiveNodeIds(
    const AstrolabeComponent &astrolabeComp) {
  std::vector<uint32_t> ids;
  ids.reserve(astrolabeComp.nodePoints.size());

  for (const auto &[nodeId, points] : astrolabeComp.nodePoints) {
    if (points > 0) {
      ids.push_back(nodeId);
    }
  }

  std::sort(ids.begin(), ids.end());
  return ids;
}

float TalentModifierAdapter::EvaluateFlatHealthBonus(
    const std::span<const uint32_t> nodeIds) {
  auto &runtimeRegistry = ModifierRuntimeRegistry::Get();
  if (!runtimeRegistry.EnsureLoaded()) {
    return 0.0f;
  }

  const auto recordIds = ResolveFlatHealthRecordIds(runtimeRegistry, nodeIds);
  if (recordIds.empty()) {
    return 0.0f;
  }

  ModifierEvalContext ctx;
  ctx.active_node_ids.assign(nodeIds.begin(), nodeIds.end());

  const auto delta = ModifierEvaluator::Evaluate(
      runtimeRegistry,
      std::span<const uint32_t>(recordIds.data(), recordIds.size()), ctx);

  const auto it = delta.flat.find(kMaxHealthStat);
  if (it == delta.flat.end()) {
    return 0.0f;
  }
  return it->second;
}

float TalentModifierAdapter::ApplyFlatHealthBonus(
    const float baseHealth, const std::span<const uint32_t> nodeIds) {
  return baseHealth + EvaluateFlatHealthBonus(nodeIds);
}

} // namespace NoMoreDay
