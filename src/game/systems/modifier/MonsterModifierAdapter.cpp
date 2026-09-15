#include "game/systems/modifier/MonsterModifierAdapter.hpp"

#include "core/logging/Logger.hpp"
#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierRuntimeSupport.hpp"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint32_t kMonsterNodeIdBase = 800000u;
// 词缀 record id = 基数 + MonsterAffixType 枚举值，与生成器 MONSTER_ID_BASE 对齐。
constexpr uint32_t kMonsterRecordIdBase = 5001000u;

// 一次性致命告警：注册表不可用属致命级，每帧都会失败，只报告一次避免刷屏。
std::atomic<bool> gWarnedRegistryUnavailable{false};
// 缺失记录告警器：按 recordId 去重且总数有上限，多个不同 id 都会被报告。
ModifierRuntimeMissingRecordWarnLimiter gMissingAffixRecordWarner;

uint32_t EncodeMonsterAffixNodeId(const MonsterAffixType affixType) {
  return kMonsterNodeIdBase + static_cast<uint32_t>(affixType);
}

// 从运行时 registry 读取词缀记录并按类别掩码求值。
// 记录形状（属性 op / 事件 op / 行为 op）全部由 registry 提供；
// 三档入口显式传入所需类别，窄入口不会为其不消费的算子组做容器插入。
ModifierDelta
EvaluateMonsterAffixDelta(const MonsterAffixComponent &affixComponent,
                          const ModifierOpCategory categories) {
  auto &registry = ModifierRuntimeRegistry::Get();
  if (!registry.EnsureLoaded()) {
    if (!gWarnedRegistryUnavailable.exchange(true)) {
      LOG_ERROR("MonsterModifierAdapter: modifier runtime registry unavailable; "
                "all monster affixes are inactive (check "
                "assets/generated/modifier_runtime_v2.bin)");
    }
    return ModifierDelta{};
  }

  ModifierEvalContext ctx;
  std::vector<ModifierRecordRequest> requests;
  requests.reserve(affixComponent.affixes.size());

  for (const auto affixType : affixComponent.affixes) {
    // None 与越界枚举没有对应记录，直接跳过（比旧的越界索引更安全）。
    if (affixType == MonsterAffixType::None ||
        static_cast<uint8_t>(affixType) >=
            static_cast<uint8_t>(MonsterAffixType::Count)) {
      continue;
    }

    const uint32_t recordId =
        kMonsterRecordIdBase + static_cast<uint32_t>(affixType);
    if (registry.FindRecordById(recordId) == nullptr) {
      if (gMissingAffixRecordWarner.ShouldReport(recordId)) {
        LOG_WARN("MonsterModifierAdapter: record {} for affix type {} missing "
                 "from registry; affix is inactive",
                 recordId, static_cast<uint32_t>(affixType));
      }
      continue;
    }

    ctx.active_node_ids.push_back(EncodeMonsterAffixNodeId(affixType));
    requests.push_back({recordId, false, 0.0f, kAllStatTargets});
  }

  if (requests.empty()) {
    return ModifierDelta{};
  }

  return ModifierEvaluator::Evaluate(
      registry,
      std::span<const ModifierRecordRequest>(requests.data(), requests.size()),
      ctx, categories);
}

} // namespace

ModifierDelta
MonsterModifierAdapter::EvaluateAffixDelta(
    const MonsterAffixComponent &affixComponent) {
  // 三档语义：本入口 = 属性 + 行为，不暴露事件集合。
  // 类别掩码已保证事件集合为空，无需再全量求值后清空字段。
  return EvaluateMonsterAffixDelta(
      affixComponent, ModifierOpCategory::Stats | ModifierOpCategory::Behavior);
}

MonsterModifierAdapter::MonsterAffixEventSet
MonsterModifierAdapter::EvaluateAffixEvents(
    const MonsterAffixComponent &affixComponent) {
  const ModifierDelta delta =
      EvaluateMonsterAffixDelta(affixComponent, ModifierOpCategory::Events);

  MonsterAffixEventSet events;
  events.onUpdateAffixIds = delta.monster_event_on_update_affix_ids;
  events.onHitAffixIds = delta.monster_event_on_hit_affix_ids;
  events.onDeathAffixIds = delta.monster_event_on_death_affix_ids;
  return events;
}

MonsterModifierAdapter::MonsterAffixBehaviorOpSet
MonsterModifierAdapter::EvaluateBehaviorOps(
    const MonsterAffixComponent &affixComponent) {
  const ModifierDelta delta =
      EvaluateMonsterAffixDelta(affixComponent, ModifierOpCategory::Behavior);

  MonsterAffixBehaviorOpSet behaviorOps;
  behaviorOps.onUpdateOpcodes = delta.monster_behavior_on_update_opcodes;
  behaviorOps.onHitOpcodes = delta.monster_behavior_on_hit_opcodes;
  behaviorOps.onDeathOpcodes = delta.monster_behavior_on_death_opcodes;
  return behaviorOps;
}

float MonsterModifierAdapter::GetBerserkWeaponDamageMultiplier(
    const MonsterAffixComponent &affixComponent) {
  if (!affixComponent.isBerserk) {
    return 1.0f;
  }
  return std::pow(MonsterAffixRegistry::Params::BERSERKER_DAMAGE_MULT,
                  static_cast<float>(affixComponent.affixes.size()));
}

} // namespace NoMoreDay
