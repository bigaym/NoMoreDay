#pragma once
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/foundation/components/SkillPointAccess.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay {
namespace damage {

// 无敌预检结果：blocked=true 时本次结算整体归零。
struct InvulnerabilityIntercept {
  bool blocked = false;
};

// Blade Ward 投射物拦截结果：intercepted=true 时调用方需清零最终伤害与分池。
struct BladeWardIntercept {
  bool intercepted = false;
};

// Layer 1 (Pre-Mitigation) 业务拦截：防御方无敌（护盾、分身无敌等）时伤害归零。
// 模拟路径 (is_simulation=true) 不参与拦截，保证模拟与实战数值分离。
[[nodiscard]] InvulnerabilityIntercept
EvaluateInvulnerability(entt::registry &registry, entt::entity defender,
                        bool is_simulation);

// Layer 3 (Post-Mitigation) 业务拦截：Blade Ward 投射物偏转判定。
// 条件：命中带 Projectile 标签、且非物理投射物（ProjectileSystem 已自行判定），
// 防御方持有飞剑。偏转成功后消耗一柄飞剑（is_solidified 不消耗）。
// combined_tags 为本次命中的完整标签；is_simulation=true 时跳过。
[[nodiscard]] BladeWardIntercept EvaluateBladeWardInterception(
    entt::registry &registry, entt::entity defender, entt::entity source_entity,
    Tag combined_tags, bool is_simulation);

// 技能4 剑气反震反击请求的唯一构造实现：三处伤害站点（投射物偏转拦截、
// 单目标 Calculate、批量 CalculateBatch）共用同一份元素分流与基伤读取。
// 元素沿用守方的转元素配置；基伤读机制表节点 470（缺失时回退 35.0f），
// 使旧硬编码路径与机制数据单源对齐。仅构造请求，派发时机由调用方决定。
[[nodiscard]] inline DamageRequest
ResolveSkill4Counter(entt::entity counter_attacker,
                     entt::entity counter_defender,
                     const BladeWardComponent &ward) noexcept {
  Tag element = Tag::Physical;
  if (ward.is_lightning_ward) {
    element = Tag::Lightning;
  } else if (ward.is_cold_ward) {
    element = Tag::Cold;
  }

  DamageRequest request;
  request.origin = DamageOrigin::ThornsReflect;
  request.attacker = counter_attacker;
  request.defender = counter_defender;
  request.skill_id = 4;
  request.base_pool.Add(
      element, skills::GetMech(4, 470, "base_damage", 35.0f) *
                   (1.0f + ward.counter_damage_more));
  request.additional_tags = Tag::Hit | Tag::Melee | Tag::SecondaryHit;
  request.source_entity = counter_attacker;
  return request;
}

} // namespace damage
} // namespace NoMoreDay
