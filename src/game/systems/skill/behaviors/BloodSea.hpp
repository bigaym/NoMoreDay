#pragma once

#include "SkillBehaviorBase.hpp"

#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/components/PersistentFieldComponents.hpp"
#include "game/systems/skill/behaviors/generated/BloodSeaSpecState.gen.hpp"

#include <cstdint>

namespace NoMoreDay {
struct CombatEvent;
}

namespace NoMoreDay::skills {

// 技能 12（血海）节点常量：由生成头 `BloodSeaSpecState.gen.hpp` 的 `BloodSeaNodesGen`
// 承接，行为实现与单测共享；禁止在其他 TU 重复声明。
namespace BloodSeaNodes = BloodSeaNodesGen;

inline constexpr uint32_t kBloodSeaSkillId = 12;

// 技能 12 施法信封（POD）：由生成头 `BloodSeaCastSpecGen` 承接，仅含点数字段与点亮标志。
// 机制系数已从 POD 剥离，改为 DoCast 内经 GetMech 局部读取（设计 §2.4）。
using BloodSeaCastSpec = BloodSeaCastSpecGen;

// 解析技能 12 的 SpecState：节点 id -> 成员指针由统一模板表驱动，
// 首个匹配槽位一次性填表（点数 + 点亮标志），无匹配槽位返回 POD 缺省值。
[[nodiscard]] inline BloodSeaCastSpec
ResolveBloodSeaCastSpec(const entt::registry &registry, const entt::entity owner) {
  return ResolveSpecState(registry, owner, kBloodSeaSkillId, kBloodSeaTableGen);
}

struct BloodSea : SkillBehaviorBase<BloodSea> {
  static constexpr uint32_t kSkillId = kBloodSeaSkillId;

  // 技能级参数回退：非节点绑定，等价于迁移前 POD 内联默认值（设计 §2.4）。
  static constexpr float kFieldDurationDefault = 4.8f;
  static constexpr float kFieldRadiusDefault = 120.0f;
  static constexpr float kFieldTickDefault = 0.25f;
  static constexpr float kLeechRatioDefault = 0.12f;
  static constexpr float kBloodthirstDamageBonusDefault = 0.12f;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec);
  static void UpdateField(entt::registry &registry, entt::entity entity,
                          BloodSeaFieldComponent &field, float dt,
                          const systems::SpatialHashGrid &grid);
  static void HandleLinkedHit(entt::registry &registry, const CombatEvent &evt);
};

// HUD 只读快照：表现层只取所需时序值，不接触 40+ 字段的底层组件定义。
struct BloodSeaHudSnapshot {
  float remaining_duration = 0.0f;
  bool has_void_keystone = false;
  float miasma_duration_bonus = 0.0f;
};

// 查询施法者当前血海场的剩余时长与构筑标志（0 表示无活动场）。
[[nodiscard]] BloodSeaHudSnapshot QueryBloodSeaHud(const entt::registry &registry,
                                                   entt::entity player);

void RegisterBloodSea();

} // namespace NoMoreDay::skills
