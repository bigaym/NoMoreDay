#pragma once

#include "game/components/Stats.hpp"
#include "raylib.h"
#include <cstdint>

namespace NoMoreDay {

/**
 * @brief 通用危害区域组件 - 地面 DoT 或延迟爆炸
 * 
 * 用于实现各种环境危害效果：
 * - 持续伤害区域 (DoT Zones)
 * - 延迟爆炸物 (Delayed Explosions)
 * - 预警区域 (Warning Zones)
 */
struct HazardComponent {
    float damagePerTick = 0.0f;      // 每次判定造成的伤害
    float tickInterval = 0.5f;        // 伤害判定间隔 (秒)
    float currentTickTimer = 0.0f;    // 运行时计时器
    
    float duration = 0.0f;            // 总持续时间 (秒)
    float radius = 50.0f;             // 伤害半径
    
    DamageType damageType = DamageType::Fire;  // 伤害类型
    bool isDelayedExplosion = false;  // true: 倒计时结束造成一次性伤害; false: 持续 DoT
    
    // 预警阶段 (用于 Void Zone 等需要预警的效果)
    bool hasWarningPhase = false;     // 是否有预警阶段
    float warningDuration = 0.0f;     // 预警持续时间
    bool isWarningActive = true;      // 当前是否处于预警阶段
    
    // Target filtering
    bool hitsPlayers = true;
    bool hitsEnemies = false;
    
    // 归属者 (用于伤害结算)
    entt::entity owner = entt::null;
    
    // 爆炸伤害 (仅用于 isDelayedExplosion = true)
    float explosionDamage = 0.0f;
};

/**
 * @brief 危害视觉组件 - 关联 GPU 粒子效果
 */
struct HazardVisualComponent {
    uint32_t particleSystemID = 0;    // 关联的粒子效果 ID (预留)
    Color tintColor = WHITE;          // 颜色
    float visualScale = 1.0f;         // 视觉缩放
    
    // 粒子发射控制
    float particleEmitTimer = 0.0f;   // 粒子发射计时器
    float particleEmitInterval = 0.1f; // 粒子发射间隔
    int particlesPerEmit = 3;         // 每次发射的粒子数量
};

/**
 * @brief 冰球组件 - Frozen 词缀专用
 * 
 * 冰球会向玩家飞行，停止后延迟爆炸
 */
struct FrozenOrbComponent {
    float travelDuration = 2.0f;      // 飞行持续时间
    float stopDuration = 1.0f;        // 停止后等待爆炸的时间
    float currentTimer = 0.0f;        // 当前计时器
    
    bool isTraveling = true;          // 是否正在飞行
    bool hasStopped = false;          // 是否已停止
};

/**
 * @brief 挥发性球体组件 - Toxic 词缀专用
 * 
 * 追踪玩家的毒球，撞击或超时后生成毒池
 */
struct VolatileOrbComponent {
    float maxLifetime = 3.0f;         // 最大飞行时间
    float currentLifetime = 0.0f;     // 当前存活时间
    
    float homingStrength = 200.0f;    // 追踪强度
    float speed = 150.0f;             // 飞行速度
    entt::entity owner = entt::null;
};

/**
 * @brief 虚空区域组件 - Void Zone 词缀专用
 */
struct VoidZoneComponent {
    // 虚空区域使用 HazardComponent 的 hasWarningPhase 和 warningDuration
    // 此标签用于快速识别和特殊渲染
};

/**
 * @brief 雷电残影组件 - Storm Strider 词缀专用
 * 
 * 怪物受击时生成的静态残影，延迟后爆炸
 */
struct LightningGhostComponent {
    float explosionDelay = 1.5f;      // 爆炸延迟
    float currentTimer = 0.0f;        // 当前计时器
};

// 快速查询标签
struct FrozenOrbTag {};
struct VolatileOrbTag {};
struct VoidZoneTag {};
struct LightningGhostTag {};
struct ToxicPoolTag {};

} // namespace NoMoreDay
