#pragma once

#include "game/components/HazardComponents.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Buff.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "core/logging/Logger.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay {

namespace systems {
class SpatialHashGrid;
}

/**
 * @brief 危害区域系统 - 统一处理所有 Hazard 实体
 * 
 * 职责：
 * 1. 生命周期管理：更新 duration，超时销毁
 * 2. Tick 逻辑：处理周期性伤害判定
 * 3. 空间查询：使用简单的距离检测查找范围内目标
 * 4. 伤害应用：直接修改 HP 或通过 DamagePipeline
 * 5. 视觉效果：触发粒子发射
 */
class HazardSystem {
public:
    /**
     * @brief 主更新循环
     */
    static void Update(entt::registry& registry, float dt, const systems::SpatialHashGrid& grid);
    
private:
    /**
     * @brief 处理通用 Hazard 实体的生命周期和伤害
     */
    static void ProcessHazards(entt::registry& registry, float dt, const systems::SpatialHashGrid& grid);
    
    /**
     * @brief 处理冰球 (Frozen Orb) 的移动和爆炸逻辑
     */
    static void ProcessFrozenOrbs(entt::registry& registry, float dt, const systems::SpatialHashGrid& grid);
    
    /**
     * @brief 处理挥发性球体 (Volatile Orb) 的追踪和碰撞
     */
    static void ProcessVolatileOrbs(entt::registry& registry, float dt);
    
    /**
     * @brief 处理雷电残影 (Lightning Ghost) 的爆炸
     */
    static void ProcessLightningGhosts(entt::registry& registry, float dt, const systems::SpatialHashGrid& grid);
    
    /**
     * @brief 处理虚空区域 (Void Zone) 的预警和激活
     */
    static void ProcessVoidZones(entt::registry& registry, float dt);
    
    /**
     * @brief 发射危害区域的视觉粒子效果
     */
    static void EmitHazardParticles(entt::registry& registry, float dt);
    
    /**
     * @brief 应用 Chill/Freeze Debuff
     */
    static void ApplyChillDebuff(entt::registry& registry, entt::entity target, float duration, float slowAmount);
    
    /**
     * @brief 应用 Poison Debuff
     */
    
    /**
     * @brief 在指定位置造成范围伤害
     */
    static void DealAreaDamage(entt::registry& registry, Position center, float radius, 
                               float damage, DamageType type, bool hitsPlayers, bool hitsEnemies,
                               const systems::SpatialHashGrid& grid, entt::entity owner = entt::null);
};

} // namespace NoMoreDay
