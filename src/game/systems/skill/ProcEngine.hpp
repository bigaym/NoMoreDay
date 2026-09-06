#pragma once

#include <entt/entt.hpp>
#include "game/contracts/CombatEvents.hpp"

namespace NoMoreDay {

/**
 * @brief 现代化统一触发引擎 (Proc Engine)
 * 
 * 基于两阶段安全派发模式 (Collect-Then-Execute)，严守 EnTT 指针生命周期规范，
 * 支持自适应技能冷却加权与递归深度守卫 (trigger_depth <= 2)。
 */
class ProcEngine {
public:
    /**
     * @brief 两阶段安全派发触发事件 (Collect-Then-Execute)
     * @param reg ECS 注册表
     * @param listener 监听触发规则的实体
     * @param event 发生的战斗事件
     */
    static void DispatchEvent(entt::registry& reg, entt::entity listener, const CombatEvent& event);

    /**
     * @brief 触发规则冷却递减系统 (由 SkillSystem::UpdateCooldowns 调用)
     * @param reg ECS 注册表
     * @param dt 帧间隔时间 (秒)
     */
    static void UpdateCooldowns(entt::registry& reg, float dt);
};

} // namespace NoMoreDay
