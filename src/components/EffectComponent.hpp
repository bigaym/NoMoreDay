#pragma once
#include "Common.hpp"
#include "raylib.h"

// 伤害数字飘字
struct DamagePopup {
    float damage;
    float timer;
    float lifeTime;
    // 自定义速度，不使用通用 Velocity 组件以避免物理碰撞
    float velX;
    float velY;
    Color color;
    bool isDodge = false; // 是否为闪避
    bool isBlock = false; // 是否为格挡
    bool isMiss = false;  // 是否未命中
    bool isCrit = false;  // 是否为暴击
    
    // Animation state
    float currentScale = 1.0f;
};

// 攻击特效 (如挥剑轨迹)
struct AttackEffect {
    float timer;
    float lifeTime;
    float rotation; // 角度
    float range;    // 范围/大小
    float arcAngle; // 扇形角度
    Color color;
};

enum class VisualEffectType {
    None,
    Pickup,       // 拾取时的扩散圆圈
    DropPillar,   // 掉落时的光柱
    GoldSparkle,  // 金币拾取的闪光
    LevelUp       // 升级时的特效
};

struct VisualEffect {
    VisualEffectType type = VisualEffectType::None;
    float timer = 0.0f;
    float lifeTime = 1.0f;
    float startScale = 0.5f;
    float endScale = 1.5f;
    Color color = WHITE;
    
    // 额外的属性，根据类型不同有不同含义
    // 例如 Pickup 的扩散半径，DropPillar 的高度等
    float param1 = 0.0f; 
};