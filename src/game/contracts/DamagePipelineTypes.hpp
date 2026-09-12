#pragma once
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include <entt/entt.hpp>
#include <optional>
#include <type_traits>
#include <vector>

namespace NoMoreDay {

// 伤害来源强类型分类：决定 Build 阶段是否允许注入武器与技能配置点伤。
// 仅 DirectSkillCast 允许注入；其余来源一律只消费调用方提供的 base_pool。
enum class DamageOrigin : uint8_t {
  DirectSkillCast = 0, // 技能直接施法：允许注入武器与技能配置点伤
  SecondaryProc = 1,   // 衍生次级打击：分裂、爆炸、弹射等
  AilmentTick = 2,     // 异常状态 DoT 跳伤
  HazardEnvironment = 3, // 地面环境 / 陷阱 / 持续伤害区域
  ThornsReflect = 4,   // 荆棘反伤
  ItemAffixProc = 5    // 装备 / 怪物词缀特效触发
};

struct DamageResult {
  float total_damage = 0.0f;
  bool is_crit = false;
  bool was_dodged = false;
  bool was_blocked = false;
  float block_multiplier = 1.0f;
  DamagePool final_pool; // Damage broken down by type
  // 结算时实际参与的元素类型标签（转换后的 final_type 按位或）。用于事件
  // 派发时把元素位重映为真实结算元素，非元素动作位（Melee/Projectile/Hit 等）
  // 由调用方从原始命中标签保留。全被拦截/无实例时为 None。
  Tag resolved_element_tags = Tag::None;
};

struct DamageRequest {
  DamageOrigin origin = DamageOrigin::DirectSkillCast;
  entt::entity attacker = entt::null;
  entt::entity defender = entt::null;
  uint32_t skill_id = 0;
  DamagePool base_pool;
  float added_effectiveness = 1.0f;
  float trigger_effectiveness = 1.0f;
  Tag additional_tags = Tag::None;
  entt::entity source_entity = entt::null;
  bool is_simulation = false;
  bool dispatch_damage_events = true;
  bool skip_mitigation = false;
  bool thorns_like_damage = false;
  std::optional<DamagePayloadContext> payload_context;
};

struct DamageExecutionResult {
  DamageResult damage;
  bool target_killed = false;
  float final_applied_damage = 0.0f;
  float barrier_absorbed = 0.0f;
  bool was_prevented = false;
};

} // namespace NoMoreDay
