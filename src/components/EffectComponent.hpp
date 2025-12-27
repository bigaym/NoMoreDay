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