#include "game/systems/modifier/MapModifierAdapter.hpp"

#include "core/logging/Logger.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/MapAffix.hpp"
#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierRuntimeSupport.hpp"
#include "game/systems/world/MapAffixRegistry.hpp"

#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

namespace NoMoreDay {
namespace {

using MapModifierIds::kMapNodeIdBase;
using MapModifierIds::kMapRecordIdBase;
using MapModifierIds::kMapResonanceNode;
using MapModifierIds::kMapResonanceRecordId;

// 一次性致命告警：注册表不可用属致命级，每帧都会失败，只报告一次避免刷屏。
std::atomic<bool> gWarnedRegistryUnavailable{false};
// 缺失记录告警器：按 recordId 去重且总数有上限，多个不同 id 都会被报告。
ModifierRuntimeMissingRecordWarnLimiter gMissingAffixRecordWarner;
ModifierRuntimeMissingRecordWarnLimiter gMissingResonanceRecordWarner;

uint32_t EncodeMapAffixNodeId(const MapAffixType type) {
  return kMapNodeIdBase + static_cast<uint32_t>(type);
}

} // namespace

ModifierDelta
MapModifierAdapter::EvaluateEnemyAffixDelta(const ActiveDimensionalState &state) {
  if (!state.isActive) {
    return ModifierDelta{};
  }

  auto &registry = ModifierRuntimeRegistry::Get();
  if (!registry.EnsureLoaded()) {
    if (!gWarnedRegistryUnavailable.exchange(true)) {
      LOG_ERROR("MapModifierAdapter: modifier runtime registry unavailable; all "
                "map affixes are inactive (check "
                "assets/generated/modifier_runtime_v2.bin)");
    }
    return ModifierDelta{};
  }

  ModifierEvalContext ctx;
  std::vector<ModifierRecordRequest> requests;
  requests.reserve(state.explicitAffixes.size() + 1);

  // 共鸣：倍率按运行时敌方密度滚值，JSON 模板 param_f32 仅作占位。
  if (state.resonance.totalEnemyDensity > 0.0f) {
    if (registry.FindRecordById(kMapResonanceRecordId) != nullptr) {
      ctx.active_node_ids.push_back(kMapResonanceNode);
      requests.push_back({kMapResonanceRecordId, true,
                          state.resonance.totalEnemyDensity * 0.05f,
                          kAllStatTargets});
    } else if (gMissingResonanceRecordWarner.ShouldReport(
                   kMapResonanceRecordId)) {
      LOG_WARN("MapModifierAdapter: resonance record {} missing from registry; "
               "enemy density bonus is inactive",
               kMapResonanceRecordId);
    }
  }

  for (const auto &affix : state.explicitAffixes) {
    const auto typeIndex = static_cast<uint32_t>(affix.type);
    if (typeIndex >= static_cast<uint32_t>(MapAffixType::Count)) {
      continue;
    }

    // 记录形状（filter/ops/目标属性）完全来自 registry；
    // 无战斗映射的词缀（combatStat == Count 哨兵）跳过。
    const StatType combatStat = MapAffixRegistry::GetDef(affix.type).combatStat;
    if (combatStat == StatType::Count) {
      continue;
    }

    const uint32_t recordId = kMapRecordIdBase + typeIndex;
    if (registry.FindRecordById(recordId) == nullptr) {
      if (gMissingAffixRecordWarner.ShouldReport(recordId)) {
        LOG_WARN("MapModifierAdapter: record {} for affix type {} missing from "
                 "registry; affix is inactive",
                 recordId, typeIndex);
      }
      continue;
    }

    ctx.active_node_ids.push_back(EncodeMapAffixNodeId(affix.type));
    // 数值必须使用运行时按 Tier 滚动得到的 affix.value 覆盖模板值，
    // 否则高阶地图词缀会塌陷为 JSON 中的 valT1 强度。
    // 同一词缀类型出现多次时逐条入队（碎片各产一条），各自携带滚值，
    // 叠加语义为连乘；override 精确限定到 combatStat 对应的目标属性。
    requests.push_back({recordId, true, affix.value,
                        static_cast<uint32_t>(combatStat)});
  }

  if (requests.empty()) {
    return ModifierDelta{};
  }

  return ModifierEvaluator::Evaluate(
      registry,
      std::span<const ModifierRecordRequest>(requests.data(), requests.size()),
      ctx);
}

} // namespace NoMoreDay
