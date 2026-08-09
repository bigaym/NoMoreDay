#include "game/systems/modifier/SkillSpecModifierAdapter.hpp"

#include "game/foundation/components/Stats.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint32_t kPhysicalDamageStat =
    static_cast<uint32_t>(StatType::PhysicalDamage);
constexpr uint16_t kPercentMultOpcode =
    static_cast<uint16_t>(ModifierOpCode::ADD_STAT_PERCENT_MULT);

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
    if (op.opcode == kPercentMultOpcode && op.param_u32 == kPhysicalDamageStat) {
      return true;
    }
  }
  return false;
}

std::vector<uint32_t>
ResolveDamageRecordIds(const ModifierRuntimeRegistry &registry,
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

std::vector<uint32_t> SkillSpecModifierAdapter::CollectAllocatedNodeIds(
    const SpecializedSkill &activeSkillSlot) {
  std::vector<uint32_t> ids;
  ids.reserve(activeSkillSlot.allocated_points.size());

  for (const auto &[nodeId, points] : activeSkillSlot.allocated_points) {
    if (points > 0) {
      ids.push_back(nodeId);
    }
  }

  std::sort(ids.begin(), ids.end());
  return ids;
}

float SkillSpecModifierAdapter::EvaluateDamageMultiplier(
    const uint32_t skillId, const Tag skillTags,
    const std::span<const uint32_t> nodeIds) {
  auto &runtimeRegistry = ModifierRuntimeRegistry::Get();
  if (!runtimeRegistry.EnsureLoaded()) {
    return 1.0f;
  }

  const auto recordIds = ResolveDamageRecordIds(runtimeRegistry, nodeIds);
  if (recordIds.empty()) {
    return 1.0f;
  }

  ModifierEvalContext ctx;
  ctx.skill_id = skillId;
  ctx.skill_tags = skillTags;
  ctx.active_node_ids.assign(nodeIds.begin(), nodeIds.end());

  const auto delta = ModifierEvaluator::Evaluate(
      runtimeRegistry,
      std::span<const uint32_t>(recordIds.data(), recordIds.size()), ctx);
  return ModifierEvaluator::ApplyStat(1.0f, kPhysicalDamageStat, delta);
}

float SkillSpecModifierAdapter::ApplyHeavyMomentum(
    const float baseline, const uint32_t skillId, const Tag skillTags,
    const std::span<const uint32_t> nodeIds) {
  return baseline * EvaluateDamageMultiplier(skillId, skillTags, nodeIds);
}

void SkillSpecModifierAdapter::ApplyHeavyMomentumToDamageMultipliers(
    std::array<float, 6> &damageMultipliers,
    const uint32_t skillId,
    const Tag skillTags,
    const std::span<const uint32_t> nodeIds) {
  const float heavyMomentumMultiplier =
      EvaluateDamageMultiplier(skillId, skillTags, nodeIds);
  if (heavyMomentumMultiplier == 1.0f) {
    return;
  }

  damageMultipliers[static_cast<uint8_t>(DamageType::Physical)] *=
      heavyMomentumMultiplier;
}

} // namespace NoMoreDay
