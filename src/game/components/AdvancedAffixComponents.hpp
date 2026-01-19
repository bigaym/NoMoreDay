#pragma once

#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay {

/**
 * @brief 连接类型枚举
 */
enum class LinkType : uint8_t {
    None = 0,
    Shielding,      // 护盾连接
    SoulLink,       // 灵魂链接 (已有独立系统)
    ManaSiphon,     // 法力虹吸
    Count
};

/**
 * @brief 实体连接组件 - 用于建立两个实体之间的逻辑连接
 * 
 * 用于 Shielding 词缀: 护盾源 -> 护盾目标
 * 渲染系统会绘制连线
 */
struct LinkComponent {
    entt::entity target = entt::null;  // 连接目标
    LinkType type = LinkType::None;    // 连接类型
    float visualWidth = 2.0f;          // 连线宽度
    Color color = GOLD;                // 连线颜色
    
    // 运行时状态
    float lifetime = 0.0f;             // 连接持续时间 (0 = 永久)
    bool isActive = true;              // 是否激活
};

/**
 * @brief 克隆体组件 - 标记该实体为克隆体
 * 
 * 用于 Mirror Image 词缀
 */
struct CloneComponent {
    entt::entity parent = entt::null;  // 本体引用 (可选)
    float damageMultiplier = 0.5f;     // 伤害倍率
    float healthMultiplier = 0.1f;     // 生命倍率
    float lifetime = 10.0f;            // 克隆体存活时间
    float elapsed = 0.0f;              // 已存活时间
    
    // 克隆体特性
    bool hasInvulnerableFrame = true;  // 生成时是否有无敌帧
    float invulnerableDuration = 1.0f; // 无敌帧持续时间
};

/**
 * @brief 资源类型枚举
 */
enum class ResourceType : uint8_t {
    Mana = 0,
    Stamina,
    Health,
    Count
};

/**
 * @brief 资源剥夺组件 - 定义资源剥夺光环
 * 
 * 用于 Mana Siphon 词缀
 */
struct ResourceDrainComponent {
    float radius = 200.0f;              // 光环半径
    float drainRate = 10.0f;            // 每秒剥夺量
    ResourceType resource = ResourceType::Mana;  // 剥夺的资源类型
    bool safeZoneInside = true;         // true = 甜甜圈模式 (内圈安全)
    float innerRadius = 50.0f;          // 内圈半径 (仅在甜甜圈模式下有效)
    
    // 视觉效果
    Color effectColor = PURPLE;         // 效果颜色
    float pulseSpeed = 2.0f;            // 脉冲速度
};

/**
 * @brief 无敌状态组件 - 标记实体处于无敌状态
 * 
 * 用于 Shielding 词缀和克隆体无敌帧
 */
struct InvulnerableComponent {
    float duration = 0.0f;              // 持续时间 (0 = 永久)
    float elapsed = 0.0f;               // 已持续时间
    entt::entity source = entt::null;   // 无敌来源 (例如护盾提供者)
    
    // 视觉效果
    Color shieldColor = {255, 200, 50, 150};  // 护盾颜色
    float shieldRadius = 0.0f;          // 护盾半径 (0 = 使用实体半径)
};

/**
 * @brief 灵魂吞噬者组件 - 用于 Soul Eater 词缀
 * 
 * 监听全局死亡事件,范围内敌人死亡时获得层数
 */
struct SoulEaterComponent {
    int stacks = 0;                     // 当前层数
    int maxStacks = 50;                 // 最大层数
    float stackRadius = 300.0f;         // 吸收范围
    
    // 每层增益 (百分比)
    float sizePerStack = 5.0f;          // 体型增长
    float damagePerStack = 5.0f;        // 伤害增长
    float attackSpeedPerStack = 5.0f;   // 攻速增长
    
    // 视觉效果
    float baseScale = 1.0f;             // 基础缩放
};

/**
 * @brief 压制者组件 - 用于 Suppressor 词缀
 * 
 * 距离超过阈值的伤害来源造成的伤害大幅减少
 */
struct SuppressorComponent {
    float threshold = 300.0f;           // 距离阈值 (像素)
    float damageReduction = 0.9f;       // 伤害减免 (90%)
    
    // 视觉效果
    Color bubbleColor = {255, 50, 50, 100};  // 护盾气泡颜色
    float pulseSpeed = 1.5f;            // 脉冲速度
};

} // namespace NoMoreDay
