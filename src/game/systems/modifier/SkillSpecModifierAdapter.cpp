#include "game/systems/modifier/SkillSpecModifierAdapter.hpp"

#include "game/foundation/components/Stats.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <algorithm>
#include <cassert>
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

bool HasSkillDeliveryOp(const ModifierRuntimeRegistry &registry,
                        const ModifierRuntimeRecord &record) {
  for (const auto &op : registry.GetOps(record)) {
    if (IsSkillDeliveryOpCode(static_cast<ModifierOpCode>(op.opcode))) {
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

// 交付记录采集：节点白名单为空视为通配，否则需与已加点节点相交；且算子组含技能交付算子。
// 与 ResolveDamageRecordIds 的目标（ADD_STAT_PERCENT_MULT + 物理伤害）无关，两者独立。
std::vector<uint32_t>
ResolveSkillDeliveryRecordIds(const ModifierRuntimeRegistry &registry,
                              const std::span<const uint32_t> activeNodeIds) {
  std::vector<uint32_t> recordIds;
  if (activeNodeIds.empty()) {
    return recordIds;
  }

  for (const auto &record : registry.GetRecords()) {
    const ModifierRuntimeFilter *filter = registry.GetFilter(record);
    if (filter == nullptr) {
      continue;
    }

    // 空白名单与求值层一致，视为通配（ResolveEffectivePoints 返回 1 点）：
    // 仅当白名单非空且与已加点节点无交集时才跳过。
    const auto nodeWhitelist = registry.GetNodeWhitelist(*filter);
    if (!nodeWhitelist.empty() &&
        !HasIntersection(nodeWhitelist, activeNodeIds)) {
      continue;
    }
    if (!HasSkillDeliveryOp(registry, record)) {
      continue;
    }

    recordIds.push_back(record.id);
  }

  std::sort(recordIds.begin(), recordIds.end());
  recordIds.erase(std::unique(recordIds.begin(), recordIds.end()),
                  recordIds.end());
  return recordIds;
}

// 从同一份加点数据同时产出升序 node_points 与 active_node_ids：
// 求值层 GetPointsForNode 依赖升序前提（否则退化线性扫描），此处显式排序保证。
struct NodePointSnapshot {
  std::vector<NodePointEntry> nodePoints;
  std::vector<uint32_t> activeNodeIds;
};

NodePointSnapshot BuildNodePointSnapshot(const SpecializedSkill &slot) {
  NodePointSnapshot snapshot;
  snapshot.nodePoints.reserve(slot.allocated_points.size());

  for (const auto &[nodeId, points] : slot.allocated_points) {
    if (points > 0) {
      snapshot.nodePoints.push_back(
          NodePointEntry{nodeId, static_cast<uint16_t>(points)});
    }
  }

  std::sort(snapshot.nodePoints.begin(), snapshot.nodePoints.end(),
            [](const NodePointEntry &lhs, const NodePointEntry &rhs) {
              return lhs.node_id < rhs.node_id;
            });

  snapshot.activeNodeIds.reserve(snapshot.nodePoints.size());
  for (const NodePointEntry &entry : snapshot.nodePoints) {
    snapshot.activeNodeIds.push_back(entry.node_id);
  }

#ifndef NDEBUG
  // active_node_ids 与 node_points 必须始终同源（同一份加点），否则求值的过滤层
  // 与点数层会分叉；仅调试期校验，发布期零开销。
  assert(snapshot.activeNodeIds.size() == snapshot.nodePoints.size());
  for (size_t i = 0; i < snapshot.nodePoints.size(); ++i) {
    assert(snapshot.activeNodeIds[i] == snapshot.nodePoints[i].node_id);
  }
#endif
  return snapshot;
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

ModifierDelta SkillSpecModifierAdapter::EvaluateSkillDeliveryDeltas(
    const uint32_t skillId, const Tag skillTags,
    const SpecializedSkill &activeSkillSlot) {
  auto &runtimeRegistry = ModifierRuntimeRegistry::Get();
  if (!runtimeRegistry.EnsureLoaded()) {
    return ModifierDelta{};
  }

  const NodePointSnapshot snapshot = BuildNodePointSnapshot(activeSkillSlot);
  const auto recordIds =
      ResolveSkillDeliveryRecordIds(runtimeRegistry, snapshot.activeNodeIds);
  if (recordIds.empty()) {
    return ModifierDelta{};
  }

  ModifierEvalContext ctx;
  ctx.skill_id = skillId;
  ctx.skill_tags = skillTags;
  ctx.active_node_ids = snapshot.activeNodeIds;
  ctx.node_points = snapshot.nodePoints;

  // 仅应用 SkillDelivery 类别（opcode 30..36）：采集记录若同时携带 Stats 类别算子
  // （如 MANA_COST_MULT），不得被折叠进交付 delta 而误改法耗等参数。
  return ModifierEvaluator::Evaluate(
      runtimeRegistry,
      std::span<const uint32_t>(recordIds.data(), recordIds.size()), ctx,
      ModifierOpCategory::SkillDelivery);
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
