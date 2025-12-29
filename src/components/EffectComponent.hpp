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