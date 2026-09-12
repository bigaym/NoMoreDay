#pragma once
#include "game/contracts/DamagePipelineTypes.hpp"
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

} // namespace damage
} // namespace NoMoreDay
