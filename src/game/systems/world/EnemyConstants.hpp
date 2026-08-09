#pragma once

#include "game/systems/ai/AIConstants.hpp"

#include <cstdint>

namespace NoMoreDay::Constants
{
  // 敌人驱动设置
  namespace Enemy
  {
    constexpr float DEFAULT_ACTIVATION_DISTANCE = 1200.0f;   // 敌人进入活跃状态的视距 (Spawning/Active)
    constexpr float DEFAULT_AGGRO_DISTANCE = 500.0f;         // 敌人触发仇恨的距离 (AI Aggro)
    constexpr float DEFAULT_DEACTIVATION_DISTANCE = 2000.0f; // 敌人进入休眠状态的脱战距离
    constexpr int CLUSTER_DENSITY_DIVISOR = 200;             // 敌人集群生成的密度分母
    constexpr int MIN_CLUSTER_ENEMY_COUNT = 5;               // 最小敌人群规模
    constexpr int MAX_CLUSTER_ENEMY_COUNT = 12;              // 最大敌人群规模
    constexpr float SPAWN_RADIUS_MIN = 2.0f;                 // 怪物生成点的最小偏差半径
    constexpr float SPAWN_RADIUS_MAX = 4.0f;                 // 怪物生成点的最大偏差半径

    constexpr float LEVEL_HP_MULTIPLIER = 0.1f;     // 随等级增加的血量加成比例 (10%/级)
    constexpr float DAMAGE_VARIANCE_MIN = 0.8f;     // 敌人输出伤害的波动下限系数
    constexpr float DAMAGE_VARIANCE_MAX = 1.2f;     // 敌人输出伤害的波动上限系数
    constexpr float DEFAULT_ATTACK_INTERVAL = 1.5f; // 默认普攻间隔
    constexpr float ELITE_HP_MULTIPLIER = 5.0f;     // 精英怪额外的生命值乘数
    constexpr float CHAMPION_HP_MULTIPLIER = 2.5f;  // 冠军怪生命值乘数
    constexpr float BOSS_HP_MULTIPLIER = 15.0f;     // Boss生命值乘数
    constexpr float NEMESIS_HP_MULTIPLIER = 25.0f;  // 宿敌生命值乘数
    constexpr float BOSS_CHANCE = 0.002f;           // Boss 生成概率 (0.2%)
    constexpr float ELITE_CHANCE = 0.02f;           // 精英怪生成概率 (2%)
    constexpr float CHAMPION_CHANCE = 0.01f;        // 冠军怪生成概率 (1%)

    constexpr int DORMANT_CHECK_INTERVAL_FRAMES = 60; // 更新休眠实体的帧间隔
    constexpr int MAX_AWAKEN_PER_CYCLE = 50;          // 每个更新周期允许唤醒的最大实体数
    constexpr float AWAKEN_DISTANCE_MAX =
        AI::DORMANCY_THRESHOLD - 25.0f;               // 唤醒后仍保持活跃的最远距离
    constexpr float AWAKEN_DISTANCE_MIN =
        AWAKEN_DISTANCE_MAX - 150.0f;                // 唤醒环带的最近距离

    constexpr float DEFAULT_SPRITE_SCALE = 0.3f;     // 默认怪物贴图缩放
    constexpr float DEFAULT_COLLISION_RADIUS = 5.0f; // 怪物默认物理碰撞半径
    constexpr int NEXT_LEVEL_PORTAL_KILL_REQUIREMENT = 100;
    constexpr int BIOME_MAX_ENEMIES_MIN = 150;
    constexpr int BIOME_MAX_ENEMIES_MAX = 5000;
  } // namespace Enemy
} // namespace NoMoreDay::Constants
