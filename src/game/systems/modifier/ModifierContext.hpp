#pragma once

#include "game/foundation/data/TagRegistry.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace NoMoreDay {

enum class ModifierOpCode : uint16_t {
  ADD_STAT_FLAT = 0,
  ADD_STAT_PERCENT_ADD = 1,
  ADD_STAT_PERCENT_MULT = 2,
  ADD_SKILL_LEVEL = 3,
  MANA_COST_MULT = 4,
  MONSTER_EVENT_ON_UPDATE = 5,
  MONSTER_EVENT_ON_HIT = 6,
  MONSTER_EVENT_ON_DEATH = 7,
  MONSTER_BEHAVIOR_MOLTEN_UPDATE = 8,
  MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT = 9,
  MONSTER_BEHAVIOR_TELEPORTER_UPDATE = 10,
  MONSTER_BEHAVIOR_FROZEN_UPDATE = 11,
  MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE = 12,
  MONSTER_BEHAVIOR_SHIELDING_UPDATE = 13,
  MONSTER_BEHAVIOR_VORTEX_UPDATE = 14,
  MONSTER_BEHAVIOR_WALLER_UPDATE = 15,
  MONSTER_BEHAVIOR_NULLIFIER_ON_HIT = 16,
  MONSTER_BEHAVIOR_ENTANGLER_ON_HIT = 17,
  MONSTER_BEHAVIOR_TOXIC_ON_DEATH = 18,
  MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE = 19,
  MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE = 20,
  MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH = 21,
  MONSTER_BEHAVIOR_BERSERKER_UPDATE = 22,
  MONSTER_BEHAVIOR_VOIDZONE_UPDATE = 23,
  MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE = 24,
  MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH = 25,
  MONSTER_BEHAVIOR_SOUL_LINK_UPDATE = 26,
  MONSTER_BEHAVIOR_STORM_UPDATE = 27,
  MONSTER_BEHAVIOR_VOID_ON_HIT = 28,

  // 技能交付参数算子 (30..36)：param_u32 恒为技能 ID（delta 容器键），
  // param_f32 为每点幅度；应用时按节点分配点数 N 线性缩放（见设计 §3.2.1）。
  SKILL_MORE_DAMAGE_MULT = 30, // 每点 More 加成（0.10 = 每点 +10%，负值合法）
  SKILL_COOLDOWN_FLAT = 31,    // 每点绝对秒数（负值 = 减冷却）
  SKILL_COOLDOWN_MULT = 32,    // 每点乘算偏移（0.15 = 每点 +15% CD）
  SKILL_CHARGES_ADD = 33,      // 每点平加充能数（1.0 = 每点 +1 充能）
  SKILL_BONUS_CRIT = 34,       // 每点平加暴击率（0.02 = 每点 +2%）
  SKILL_AREA_MULT = 35,        // 每点范围乘算偏移（0.20 = 每点 +20%）
  SKILL_MANA_COST_MULT = 36,   // 每点法耗折扣（0.15 = 每点 −15%，下限 0）
};

/**
 * @brief 求值算子类别位掩码。
 *
 * 供窄入口按需跳过无关算子组：例如怪物行为系统每帧只消费行为 opcode，
 * 传入 Behavior 后求值循环不会为属性/事件算子做无序容器插入，避免白算。
 */
enum class ModifierOpCategory : uint32_t {
  None = 0u,
  Stats = 1u << 0,
  Events = 1u << 1,
  Behavior = 1u << 2,
  SkillDelivery = 1u << 3,
  All = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3),
};

[[nodiscard]] constexpr ModifierOpCategory
operator|(const ModifierOpCategory lhs, const ModifierOpCategory rhs) {
  return static_cast<ModifierOpCategory>(static_cast<uint32_t>(lhs) |
                                         static_cast<uint32_t>(rhs));
}

[[nodiscard]] constexpr ModifierOpCategory
operator&(const ModifierOpCategory lhs, const ModifierOpCategory rhs) {
  return static_cast<ModifierOpCategory>(static_cast<uint32_t>(lhs) &
                                         static_cast<uint32_t>(rhs));
}

[[nodiscard]] constexpr bool HasOpCategory(const ModifierOpCategory set,
                                           const ModifierOpCategory flag) {
  return (static_cast<uint32_t>(set) & static_cast<uint32_t>(flag)) != 0u;
}

// target_stat 通配值：表示覆盖记录内全部乘算算子，而不限定目标属性。
inline constexpr uint32_t kAllStatTargets = 0xFFFFFFFFu;

struct ModifierFilter {
  uint64_t profession_mask = 0;
  std::vector<uint32_t> skill_id_whitelist;
  uint64_t required_skill_tags_all = 0;
  uint64_t forbidden_skill_tags_any = 0;
  uint32_t weapon_class_mask = 0xFFFFFFFFu;
  uint32_t equip_slot_mask = 0;
  std::vector<uint32_t> node_id_whitelist;
};

struct ModifierOp {
  ModifierOpCode opcode = ModifierOpCode::ADD_STAT_FLAT;
  uint32_t param_u32 = 0;
  float param_f32 = 0.0f;
};

struct ModifierRecord {
  ModifierFilter filter;
  std::vector<ModifierOp> ops;
};

// 单个专精节点的已分配点数，供求值层按点数线性缩放算子数值。
struct NodePointEntry {
  uint32_t node_id = 0;
  uint16_t points = 0;
};

struct ModifierEvalContext {
  uint32_t profession_id = 0;
  uint32_t skill_id = 0;
  Tag skill_tags = Tag::None;
  uint32_t weapon_class_mask = 0xFFFFFFFFu;
  uint32_t equip_slot_mask = 0xFFFFFFFFu;
  std::vector<uint32_t> active_node_ids;
  // 按 node_id 升序排列（调用方保证排序），与 active_node_ids 由同一份加点数据填充。
  std::vector<NodePointEntry> node_points;

  // 查找节点已分配点数；未分配返回 0。
  // 约定 node_points 按 node_id 升序（调用方排序），故先用 lower_bound 命中；
  // 未命中时再线性精确扫描兜底，保证表未排序时结果依然正确，
  // 且避免每次调用都做 O(n) 的 std::is_sorted 校验而抵消二分收益。
  [[nodiscard]] uint16_t GetPointsForNode(const uint32_t nodeId) const {
    const auto it = std::lower_bound(
        node_points.begin(), node_points.end(), nodeId,
        [](const NodePointEntry &entry, const uint32_t id) {
          return entry.node_id < id;
        });
    if (it != node_points.end() && it->node_id == nodeId) {
      return it->points;
    }
    for (const NodePointEntry &entry : node_points) {
      if (entry.node_id == nodeId) {
        return entry.points;
      }
    }
    return 0;
  }
};

/**
 * @brief 单条运行时求值请求。
 *
 * 与 registry 记录一一对应：同一 record_id 在请求序列中出现 N 次，即按顺序施加 N 次，
 * 每次携带各自的 param_f32，使叠加语义正确表达为 (1+v1)*(1+v2)*...。
 * 这解决了"按 record_id 只查首条 override"导致同类型词缀重复出现时
 * 后续滚值被静默丢弃的缺陷。
 *
 * override_percent_mult 为 true 时，param_f32 替代该记录乘算类算子
 * （ADD_STAT_PERCENT_MULT）的离线模板值；target_stat 进一步限定被覆盖的
 * 目标属性（对应 op.param_u32），仅该属性的乘算算子被替换，
 * kAllStatTargets 表示覆盖该记录全部乘算算子。
 * 记录形状（filter/ops）始终只读自 registry。
 */
struct ModifierRecordRequest {
  uint32_t record_id = 0;
  bool override_percent_mult = false;
  float param_f32 = 0.0f;
  uint32_t target_stat = kAllStatTargets;
};

} // namespace NoMoreDay
