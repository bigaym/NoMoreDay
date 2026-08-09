#pragma once

#include <array>
#include <cstdint>

namespace NoMoreDay::Constants
{
  // 战斗系统核心常量
  namespace Combat
  {
    constexpr float DEFAULT_MAX_HEALTH = 100000.0f; // 默认最大生命值--测试用值
    constexpr float DEFAULT_MAX_MANA = 100000.0f;   // 默认最大法力值--测试用值
    constexpr float DEFAULT_MOVE_SPEED = 100.0f;    // 默认移动速度 (pixels/sec)
    constexpr float DEFAULT_CRIT_CHANCE = 0.05f;    // 默认暴击率 (5%)
    constexpr float DEFAULT_CRIT_DAMAGE = 1.50f;    // 默认暴击伤害倍率 (150%)
    constexpr float DEFAULT_ATTACK_SPEED = 1.0f;    // 默认每秒攻击次数
    constexpr float DEFAULT_ACCURACY = 0.97f;       // 默认命中率
    constexpr float BASE_PICKUP_RANGE = 50.0f;      // 基础自动拾取半径
    constexpr float MOVE_SPEED_CAP = 500.0f;        // 移动速度硬上限
    constexpr float MAGIC_FIND_BASE = 4.0f;         // 基础掉落幸运值
    constexpr float REGEN_BASE = 1.0f;              // 基础每秒法力回复

    namespace Elite
    {
      constexpr int UPDATE_INTERVAL_FRAMES = 5; // 精英怪技能逻辑更新帧间隔
      constexpr float Vfx_Scale_Base = 3.0f;    // 精英怪视觉特效基础缩放
      constexpr float Vfx_Scale_Step = 0.3f;    // 精英怪视觉特效级别增长步长
    } // namespace Elite

    namespace Pipeline
    {
      constexpr int MAX_INSTANCES = 64;         // 伤害管道每帧能处理的最大实例数
      constexpr int DAMAGE_POOL_SIZE = 16;      // 预分配的伤害池大小
      constexpr int ELEMENTAL_TYPE_COUNT = 6;   // 伤害类型总数
      constexpr float DEFAULT_CRIT_MULT = 1.5f; // 默认暴击结算倍率
      constexpr float RESISTANCE_MIN = -1.0f;   // 抗性下限（破甲最大上限）
      constexpr float RESISTANCE_MAX = 0.75f;   // 处理抗性上限
      constexpr float DR_MAX = 0.9f;            // 全局减伤 (Damage Reduction) 最大上限
      constexpr float SHADOW_MULTIPLIER = 0.5f; // 影子/分身造成的原始伤害比例
      constexpr float ARMOR_BASE = 100.0f;      // 护甲公式的基础常数系数
      constexpr int BATCH_GRAIN_SIZE = 32;      // 并行处理伤害时的每批任务大小
    } // namespace Pipeline

    namespace Conversion
    {
      // 转换优先级顺序（单向：idx 小 -> idx 大）
      // Physical(0) -> Lightning(3) -> Cold(2) -> Fire(1) -> Poison(4) -> Shadow(5)
      constexpr std::array<int, 6> CONVERSION_ORDER = {0, 3, 2, 1, 4, 5};

      // 最大递归深度（防止配置错误导致无限循环）
      constexpr int MAX_CONVERSION_DEPTH = 3;

      // 合法转换方向检查 (from_idx, to_idx) -> from 在 ORDER 中的位置必须 < to
      inline constexpr bool IsValidConversion(int from_idx, int to_idx)
      {
        int from_pos = -1, to_pos = -1;
        for (int i = 0; i < 6; ++i)
        {
          if (CONVERSION_ORDER[i] == from_idx)
            from_pos = i;
          if (CONVERSION_ORDER[i] == to_idx)
            to_pos = i;
        }
        return from_pos < to_pos;
      }
    } // namespace Conversion

    namespace System
    {
      constexpr float DEFAULT_ATTACK_COOLDOWN = 1.0f;       // 默认攻击冷却时间
      constexpr float DEFAULT_ATTACK_RANGE = 60.0f;         // 默认攻击检测半径
      constexpr float DEFAULT_ATTACK_ARC = 120.0f;          // 默认近战攻击扇形角度
      constexpr float ATTACK_EFFECT_LIFETIME = 0.2f;        // 攻击视觉特效持续时间
      constexpr float SCREEN_SHAKE_THRESHOLD = 10000000.0f; // 触发屏幕震动的单次伤害阈值
      constexpr float CRIT_DAMAGE_FALLBACK = 1.5f;          // 暴击伤害缺失时的回退值
      constexpr float DUAL_WIELD_AS_BONUS =
          15.0f;                                        // 双持武器时的额外攻击速度百分比加成
      constexpr float TWO_HANDED_DMG_BONUS = 1.25f;     // 双手武器的基础伤害乘区加成
      constexpr float SWORD_HEART_MORE_DMG = 1.15f;     // “剑心”状态下的独立伤增乘数
      constexpr float SWORD_HEART_BLOCK_CHANCE = 0.20f; // “剑心”提供的格挡率加成
      constexpr float SWORD_HEART_BLOCK_AMT = 50.0f;    // “剑心”格挡减免的基础伤害值
      constexpr float SWORD_HEART_SPELL_BONUS_RATIO =
          0.5f;                                  // 物攻加成转化给法攻的比例
      constexpr float SHIELD_BASE_BLOCK = 0.20f; // 盾牌的基础格挡几率
    } // namespace System

    namespace Attribute
    {
      constexpr float STR_TO_ARMOR = 2.0f;             // 每点力量提供的护甲
      constexpr float VIT_TO_HEALTH = 15.0f;           // 每点体质提供的最大生命值
      constexpr float INT_TO_MANA = 5.0f;              // 每点智力提供的最大法力值
      constexpr float VIT_TO_HEALTH_REGEN = 0.2f;      // 每点体质提供的生命回复
      constexpr float INT_TO_MANA_REGEN = 0.2f;        // 每点智力提供的法力回复
      constexpr float STR_TO_PHYS_DAMAGE_INC = 1.0f;   // 每点力量提供的物理伤害百分比增加
      constexpr float DEX_TO_CRIT_CHANCE = 0.2f;       // 每点敏捷提供的暴击率增加
      constexpr float DEX_TO_ACCURACY = 0.1f;          // 每点敏捷提供的命中率增加
      constexpr float STR_TO_KNOCKBACK = 0.5f;         // 每点力量提供的击退力度增加
      constexpr float INT_TO_BARRIER_RETENTION = 1.0f; // 每点智力提供的护盾维持(降低衰减)百分比
    } // namespace Attribute

    // 等级缩放因子 (Last Epoch 风格)
    namespace Scaling
    {
      constexpr float LEVEL_BASE = 10.0f;
      constexpr float LEVEL_LINEAR = 0.5f;
      constexpr float LEVEL_QUADRATIC = 0.05f;

      // 闪避评级系数
      constexpr float DODGE_RATING_LINEAR = 0.1f;
      constexpr float DODGE_RATING_QUADRATIC = 0.001f;
      constexpr float DODGE_MAX_CHANCE = 0.90f; // 90% 上限

      // 格挡上限
      constexpr float BLOCK_MAX_CHANCE = 0.75f;
      // 敌人等级成长配置 (D2/POE Style)
      namespace Monster
      {
        // --- HP Growth ---
        constexpr float HP_GROWTH_RATE = 0.10f; // 激进的指数成长 (10%/级)

        // --- Damage Growth ---
        constexpr float DMG_GROWTH_RATE = 0.08f; // 8%/级
        constexpr float DMG_VARIANCE_MIN = 0.90f;
        constexpr float DMG_VARIANCE_MAX = 1.10f;

        // --- Armor Growth ---
        constexpr float TARGET_DR_AT_100 = 0.20f; // 100级目标减伤 20%
        constexpr float DR_PER_LEVEL = 0.002f;    // 线性每级增加减伤

        // --- Resistance Growth (Lv 100+) ---
        constexpr float RES_GROWTH_NORMAL = 0.002f;
        constexpr float RES_GROWTH_CHAMPION = 0.004f;
        constexpr float RES_GROWTH_ELITE = 0.006f;
        constexpr float RES_GROWTH_BOSS = 0.008f;
        constexpr float RES_GROWTH_NEMESIS = 0.010f;
        constexpr float RES_HARD_CAP = 0.75f;

        // --- XP Growth ---
        constexpr float XP_GROWTH_RATE = 0.05f;
        constexpr float XP_DIFF_THRESHOLD = 5.0f;
        constexpr float XP_PENALTY_PER_LEVEL = 0.10f;
        constexpr float XP_MIN_MULT = 0.10f;

        // --- Level Sync ---
        constexpr int LEVEL_SYNC_OFFSET = 5;

        // --- Rarity HP Multipliers ---
        constexpr float RARITY_HP_NORMAL = 1.0f;
        constexpr float RARITY_HP_CHAMPION = 2.5f;
        constexpr float RARITY_HP_ELITE = 5.0f;
        constexpr float RARITY_HP_BOSS = 15.0f;
        constexpr float RARITY_HP_NEMESIS = 25.0f;

        // --- Rarity Damage Multipliers ---
        constexpr float RARITY_DMG_NORMAL = 1.0f;
        constexpr float RARITY_DMG_CHAMPION = 1.25f;
        constexpr float RARITY_DMG_ELITE = 1.6f;
        constexpr float RARITY_DMG_BOSS = 2.5f;
        constexpr float RARITY_DMG_NEMESIS = 3.0f;
      } // namespace Monster
    } // namespace Scaling

    namespace Cap
    {
      constexpr int MAX_LEVEL = 100;           // 角色/敌人最大等级
      constexpr float RESISTANCE = 0.75f;      // 基础抗性上限 (75%)
      constexpr float RESISTANCE_HARD = 0.90f; // 抗性极限硬上限 (90%)
      constexpr float DODGE =
          Scaling::DODGE_MAX_CHANCE; // 闪避率上限 using new scaling constant
      constexpr float BLOCK =
          Scaling::BLOCK_MAX_CHANCE;        // 格挡率上限 using new scaling constant
      constexpr float DR = 0.90f;           // 伤害减免上限
      constexpr float CRIT_CHANCE = 1.00f;  // 暴击率上限
      constexpr float CDR = 0.75f;          // 冷却缩减 (CDR) 上限
      constexpr float ATTACK_SPEED = 10.0f; // 每秒攻击次数上限
    } // namespace Cap
  } // namespace Combat
} // namespace NoMoreDay::Constants
