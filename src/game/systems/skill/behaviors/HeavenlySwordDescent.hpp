#pragma once

#include "SkillBehaviorBase.hpp"

#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/components/PersistentFieldComponents.hpp"
#include "game/systems/skill/behaviors/generated/HeavenlySwordDescentSpecState.gen.hpp"

#include <cstdint>

namespace NoMoreDay {
struct CombatEvent;
}

namespace NoMoreDay::skills {

// 技能 11（天剑降临）节点 id：由生成器单源产出，禁止在其他 TU 重复声明。
namespace HeavenlySwordNodes = HeavenlySwordDescentNodesGen;

inline constexpr uint32_t kHeavenlySwordSkillId = 11;

// 天剑降临施法信封（POD）：仅保留节点点数与点亮标志；机制系数与技能级参数回退
// 已从 POD 剥离，改由 DoCast 经 GetMech / skills.json 本地读取（设计 §2.3）。
using HeavenlySwordCastSpec = HeavenlySwordCastSpecGen;

// 解析技能 11 的 SpecState：首个匹配槽位一次性填表（点数 + 点亮标志）。
// 机制数值不再入信封，由消费点 DoCast 按需读取，保证行为等价。
[[nodiscard]] inline HeavenlySwordCastSpec
ResolveHeavenlySwordCastSpec(const entt::registry &registry,
                             const entt::entity owner) {
  return ResolveSpecState(registry, owner, kHeavenlySwordSkillId,
                          kHeavenlySwordDescentTableGen);
}

struct HeavenlySwordDescent : SkillBehaviorBase<HeavenlySwordDescent> {
  static constexpr uint32_t kSkillId = kHeavenlySwordSkillId;

  // 技能级参数回退（skills.json params 缺失时的兜底值）：字面量与迁移前
  // HeavenlySwordCastSpec 字段默认值逐项一致（设计 §2.3）。
  static constexpr float kImpactRadiusFallback = 90.0f;
  static constexpr float kFieldRadiusFallback = 140.0f;
  static constexpr float kFieldDurationFallback = 5.0f;
  static constexpr float kTierDamageBonusFallback = 0.18f;
  static constexpr float kTierRadiusBonusFallback = 14.0f;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec);
  static void UpdateField(entt::registry &registry, entt::entity entity,
                          HeavenlySwordFieldComponent &field, float dt,
                          const systems::SpatialHashGrid &grid);
  static void HandleLinkedHit(entt::registry &registry, const CombatEvent &evt);
  // 切换到非天剑专精时的场状态还原：回退攻速/引导频率基准并清理天剑场。
  static void OnMasterySwitchCleanup(entt::registry &registry,
                                     entt::entity owner);
};

// HUD 只读快照：表现层只取所需时序值，不接触 40+ 字段的底层组件定义。
struct HeavenlyFieldHudSnapshot {
  float remaining_duration = 0.0f;
};

// 查询施法者当前天剑场的剩余时长（含同实体 AreaField 窗口，0 表示无活动场）。
[[nodiscard]] HeavenlyFieldHudSnapshot
QueryHeavenlyFieldHud(const entt::registry &registry, entt::entity player);

void RegisterHeavenlySwordDescent();

} // namespace NoMoreDay::skills
