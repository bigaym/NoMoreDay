#pragma once

#include "raylib.h"
#include <array>
#include <cstdint>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

namespace NoMoreDay::Constants
{
  // 游戏世界相关常量
  namespace World
  {
    constexpr int WORLD_WIDTH = 5000;       // 世界总宽度（像素）
    constexpr int WORLD_HEIGHT = 5000;      // 世界总高度（像素）
    constexpr float GRID_TILE_SIZE = 10.0f; // 瓷砖地图块的基本大小
    constexpr float GRID_CELL_SIZE = 32.0f; // 用于寻找路径/网格划分的单元格大小
    constexpr int GRID_COLS =
        WORLD_WIDTH / static_cast<int>(GRID_CELL_SIZE) + 1; // 总列数
    constexpr int GRID_ROWS =
        WORLD_HEIGHT / static_cast<int>(GRID_CELL_SIZE) + 1; // 总行数
    constexpr float MAP_BOUNDARY = 5000.0f;                  // 地图物理边界范围

    namespace Map
    {
      constexpr uint8_t COST_WALL = 255;        // 墙壁的寻路代价（不可通行）
      constexpr uint8_t COST_FLOOR = 1;         // 地板的寻路代价（默认）
      constexpr int TOWN_EXIT_X_OFFSET = 12;      // 城镇出口 X 偏移 (向右移)
constexpr int TOWN_EXIT_Y_OFFSET = -8;      // 城镇出口 Y 偏移 (稍微向上)
      constexpr int FLOW_FIELD_MAX_DEPTH = 100; // 流场寻路的最大搜索深度
      constexpr float RENDER_PADDING = 4.0f;    // 地图渲染时的额外缓冲距离
    } // namespace Map

    namespace Fog
    {
      constexpr float VIEW_RADIUS_BUFFER = 2.0f;    // 战争迷雾视野半径的缓冲
      constexpr int COMPUTE_GROUP_SIZE = 16;        // GPU计算迷雾时的线程组大小
      constexpr float BACKGROUND_PADDING = 5000.0f; // 迷雾底图的边距
    } // namespace Fog
  } // namespace World

  // 地图生成相关常量
  namespace Generator
  {
    namespace Cave
    {
      constexpr float INITIAL_WALL_PROB = 0.05f; // 初始生存细胞生成墙壁的概率
      constexpr int START_SEARCH_RADIUS = 20;    // 开始搜索可行走区域的半径
      constexpr int EXIT_ATTEMPTS = 1000;        // 寻找出口位置的最大尝试次数
      constexpr int SMOOTH_THRESHOLD = 4;        // 细胞自动机平滑操作的阈值
      constexpr int ROCK_SIZE_MIN = 100;         // 岩石簇的最小尺寸
      constexpr int ROCK_SIZE_MAX = 400;         // 岩石簇的最大尺寸
      constexpr int ROCK_DENSITY_DIVISOR = 1200; // 岩石密度系数
      constexpr int ROCK_MIN_COUNT = 12;         // 场景中岩石的最少数量
      constexpr int ROCK_EXPANSION_CHANCE = 85;  // 岩石向外扩展生成的概率
      constexpr int ROCK_SMOOTH_ITERATIONS = 4;  // 岩石边缘平滑的迭代次数
      constexpr int REGION_THRESHOLD_WALL = 80;  // 移除小于此面积的孤立墙壁区域
      constexpr int REGION_THRESHOLD_FLOOR = 40; // 移除小于此面积的孤立地板区域
    } // namespace Cave
  } // namespace Generator

  // AI 行为相关常量
  namespace AI
  {
    constexpr float NORMALIZE_THRESHOLD = 0.01f;    // 向量归一化的极小阈值
    constexpr float FRICTION = 0.90f;               // AI移动时的摩擦力（速度衰减）
    constexpr float LEASH_RESET_MULTIPLIER = 4.0f;  // 怪物脱战后重置距离的倍数
    constexpr float HEALTH_REGEN_THRESHOLD = 0.95f; // 开始脱战回血的生命值阈值
    constexpr float HEALTH_REGEN_PER_SEC = 0.10f;   // 脱战后每秒回血比例 (10%)
    constexpr float FLOW_CELL_SIZE = 10.0f;         // 流场单元格大小

    // 性能优化：基于距离的分级更新
    constexpr float DORMANCY_THRESHOLD = 1600.0f;    // 进入休眠状态的距离阈值
    constexpr float UPDATE_DIST_MEDIUM = 600.0f;     // 中距离更新范围
    constexpr float UPDATE_DIST_FAR = 1000.0f;       // 远距离更新范围
    constexpr float UPDATE_INTERVAL_MEDIUM = 0.066f; // 中距离更新频率 (15Hz)
    constexpr float UPDATE_INTERVAL_FAR = 0.166f;    // 远距离更新频率 (6Hz)

    // 具体行为策略常量
    namespace Support
    {
      constexpr float MIN_SAFE_DIST = 150.0f;     // 辅助怪物的最小安全距离
      constexpr float MAX_SAFE_DIST = 400.0f;     // 辅助怪物的最大安全距离
      constexpr float FLEE_SPEED_MULT = 1.2f;     // 逃跑时的速度加成
      constexpr float APPROACH_SPEED_MULT = 0.5f; // 靠近友军的速度
      constexpr float RETREAT_SPEED_MULT = 0.6f;  // 撤退时的速度
      constexpr float BUFF_DURATION = 5.0f;       // 给友军施加Buff的持续时间
      constexpr float ARMOR_MOD_VALUE = 0.30f;    // 护甲强化Buff的数值
    } // namespace Support

    namespace Assassin
    {
      constexpr float BACKSTAB_DIST = 250.0f;         // 背刺触发距离
      constexpr float LURKER_DIST = 350.0f;           // 潜行怪物的警戒距离
      constexpr float STEALTH_TIMER_THRESHOLD = 2.0f; // 再次进入潜行所需的冷却
      constexpr float STALK_SPEED_MULT = 0.6f;        // 跟踪时的速度倍率
      constexpr float FLEE_DIST = 100.0f;             // 触发逃跑的距离
      constexpr float TELEPORT_OFFSET = 30.0f;        // 瞬移到背后的偏移
      constexpr float BUFF_DURATION = 1.0f;           // 爆发Buff的持续时间
      constexpr float BACKSTAB_DOT_THRESHOLD = 0.5f;  // 背刺角度阈值 (dot > 0.5)
    } // namespace Assassin

    namespace Tank
    {
      constexpr float SEARCH_RADIUS = 300.0f; // 寻找嘲讽目标的半径
      constexpr float MIDPOINT_OFFSET = 0.3f; // 拦截路径的偏移比例
      constexpr float ARRIVAL_DIST = 20.0f;   // 到达目标的判定距离
      constexpr float SPEED_MULT = 0.8f;      // 坦克怪物的速度倍率
    } // namespace Tank

    namespace Patrol
    {
      constexpr float ARRIVAL_DIST = 10.0f; // 巡逻点到达判定距离
      constexpr float MIN_STEP_DIST = 0.1f; // 有效移动的最小步长
      constexpr float SPEED = 15.0f;        // 默认巡逻移动速度
      constexpr float WAIT_TIME = 2.0f;     // 巡逻点停留时间
    } // namespace Patrol

    namespace Chase
    {
      constexpr float SPEED_FALLBACK = 50.0f;   // 追逐时的保底速度
      constexpr float ATTACK_EXIT_MULT = 1.2f;  // 退出攻击状态所需的距离倍数
      constexpr float HUNTER_SPEED_MULT = 1.2f; // 猎杀者加速倍率
    } // namespace Chase
  } // namespace AI

  // 战斗系统核心常量
  // 掉落与装备系统常量
  namespace Items
  {
    constexpr float LEVEL_SCALING_FACTOR = 1.5f; // 100级时的额外属性增幅 (1.0 + 1.5 = 2.5x)
    constexpr int MAX_ITEM_LEVEL = 100;
    inline float GetLevelMultiplier(int level)
    {
      return 1.0f + (float)std::max(0, level - 1) * (LEVEL_SCALING_FACTOR / 99.0f);
    }
  } // namespace Items

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

  // 物理与碰撞处理常量
  namespace Physics
  {
    constexpr float DEFAULT_ENTITY_RADIUS = 5.0f;    // 实体默认碰撞半径
    constexpr float SEPARATION_DIST_MULT = 2.0f;     // 实体间分离距离倍数
    constexpr float REPULSION_STRENGTH = 200.0f;     // 实体间互相排斥的力度
    constexpr float MAX_VELOCITY = 2000.0f;          // 最大速度限制
    constexpr float EPSILON_VELOCITY = 0.001f;       // 速度归一化/停止判定阈值
    constexpr float MIN_DIST_SQ_THRESHOLD = 0.0001f; // 距离计算的极小过滤阈值

    // [NEW] Added from audit (2026-01-24)
    constexpr float WALL_REPULSION_FACTOR = 20.0f;      // 墙壁排斥力系数
    constexpr float ENTITY_DAMPING_FACTOR = 0.92f;      // 瀹炰綋闃诲凹 (绌烘皵闃诲姏)
    constexpr float CCD_STEP_SIZE = 10.0f;              // 杩炵画纰版挒妫€娴嬫闀
    constexpr float MAP_COLLISION_RADIUS_FACTOR = 0.8f; // 鍦板浘纰版挒鍗婂緞缂╁皬绯鏁
  } // namespace Physics

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
    constexpr float AWAKEN_DISTANCE_MIN = 1650.0f;    // 唤醒行为的最近触发距离
    constexpr float AWAKEN_DISTANCE_MAX = 1800.0f;    // 唤醒行为的最远触发距离

    constexpr float DEFAULT_SPRITE_SCALE = 0.3f;     // 默认怪物贴图缩放
    constexpr float DEFAULT_COLLISION_RADIUS = 5.0f; // 怪物默认物理碰撞半径
    constexpr int NEXT_LEVEL_PORTAL_KILL_REQUIREMENT = 100;
    constexpr int BIOME_MAX_ENEMIES_MIN = 150;
    constexpr int BIOME_MAX_ENEMIES_MAX = 5000;
  } // namespace Enemy

  // 物品、背包与拾取相关
  namespace Item
  {
    constexpr float DROP_OFFSET_X = 20.0f; // 物品掉落时的横向飞散偏移
    constexpr float PICKUP_VISUAL_EFFECT_DURATION =
        0.3f;                                       // 拾取物品时的视觉动画时长
    constexpr float POTION_DEFAULT_COOLDOWN = 1.0f; // 药水的基础使用冷却
    constexpr float POTION_HEAL_AMOUNT = 50.0f;     // 测试用药水的基础回复量
    constexpr float POTION_MANA_AMOUNT = 50.0f;     // 测试用药水的基础回蓝量
    constexpr float SORT_COOLDOWN = 1.0f;           // 整理背包的冷却时间

    constexpr float DEFAULT_PICKUP_RANGE = 75.0f;       // 地面上物品的默认交互/拾取半径
    constexpr float MIN_EFFECTIVE_PICKUP_RANGE = 50.0f; // 有效拾取的最小距离限制
  } // namespace Item

  namespace Astrolabe
  {
    constexpr float INITIAL_ZOOM = 2.0f;    // Default zoom (smaller = zoomed out)
    constexpr float MIN_ZOOM = 0.4f;        // Minimum zoom level
    constexpr float MAX_ZOOM = 5.0f;
    constexpr float PAN_SPEED = 1.0f;
    constexpr float ZOOM_SPEED = 0.12f;

    // Galaxy Rendering Constants (for background shader)
    // Galaxy center in world coordinates - aligned with origin node (0, 0)
    constexpr float GALAXY_CENTER_X = 0.0f;
    constexpr float GALAXY_CENTER_Y = 0.0f;
    // Scale factor: worldPos * SCALE -> uv. 
    // At Zoom=1, screen edge ~1280 world units, galaxy falloff at r~5
    // Scale = 5.0 / 1280 ≈ 0.004, using 0.003 for wider coverage
    constexpr float GALAXY_SCALE = 0.003f;
    
    // ============================================================
    // 六扇区布局参数 (V1.1)
    // ============================================================
    
    // 职业数量
    constexpr int PROFESSION_COUNT = 6;
    constexpr float SECTOR_ANGLE = 360.0f / PROFESSION_COUNT;  // 60°
    
    // 轨道半径 (世界单位)
    constexpr float ORBIT_R1 = 150.0f;   // 本命星轨道
    constexpr float ORBIT_R2 = 300.0f;   // Tier 1 节点轨道
    constexpr float ORBIT_R3 = 500.0f;   // Tier 2 节点轨道
    constexpr float ORBIT_R4 = 750.0f;   // Tier 3 / Core 节点轨道
    
    // 节点大小
    constexpr float NODE_RADIUS_MINOR = 12.0f;
    constexpr float NODE_RADIUS_MAJOR = 12.0f;
    constexpr float NODE_RADIUS_CORE  = 16.0f;
    constexpr float PROFESSION_STAR_RADIUS = 35.0f;
    
    // 扇区 Angular Padding (避免边缘拥挤)
    constexpr float SECTOR_PADDING_DEG = 5.0f;

    // File Paths
    constexpr const char* TALENT_DATA_PATH = "assets/data/profession_talents.json";
    
  } // namespace Astrolabe

  namespace StashConfig
  {
    constexpr int MAX_TABS = 10;
    // 解锁每一页的费用 (第1页免费，数组从索引1开始对应第2页解锁费)
    constexpr int UNLOCK_COSTS[] = {
        0,        // Tab 1 (Free)
        5000,     // Tab 2
        15000,    // Tab 3
        50000,    // Tab 4
        150000,   // Tab 5
        500000,   // Tab 6
        1500000,  // Tab 7
        5000000,  // Tab 8
        10000000, // Tab 9
        20000000  // Tab 10
    };

    constexpr int getUnlockCost(int tabIndex)
    {
      if (tabIndex < 0 || tabIndex >= MAX_TABS)
        return -1;
      return UNLOCK_COSTS[tabIndex];
    }
  } // namespace StashConfig

  // 角色移动与姿态设置
  namespace Movement
  {
    constexpr float STANCE_THRESHOLD_SPEED = 50.0f; // 切换“静止/移动”姿态的速度阈值
    constexpr float STANCE_THRESHOLD_SPEED_SQ =
        STANCE_THRESHOLD_SPEED * STANCE_THRESHOLD_SPEED; // 阈值的平方（用于计算）
    constexpr float DEFAULT_REQUIRED_MOVE_TIME =
        2.0f; // 保持某个移动姿态所需的默认持续时间
  } // namespace Movement

  // 技能、投射物与特殊机制设置
  namespace Skill
  {
    constexpr float PROJECTILE_RETURN_THRESHOLD =
        20.0f; // 回旋投射物返回持有者的判定半径
    constexpr float PROJECTILE_DEFAULT_RETURN_SPEED =
        800.0f; // 回旋投射物的返回初速度
    constexpr float PROJECTILE_PULL_RADIUS_MULTIPLIER =
        3.0f;                                           // 投射物吸引周围目标的半径倍数
    constexpr float PROJECTILE_TRAIL_VEL_SCALE = -0.1f; // 拖尾特效的初速度缩放
    constexpr float PROJECTILE_ROTATING_TRAIL_RADIUS =
        10.0f; // 旋转投射物特效的轨道半径
    constexpr float PROJECTILE_COLLISION_RADIUS_OFFSET =
        10.0f;                                    // 投射物碰撞体相对于贴图的偏移量
    constexpr float PROJECTILE_MIN_DAMAGE = 5.0f; // 投射单次最低伤害

    namespace BladeWard
    {
      constexpr float BASE_INTERCEPTION_CHANCE =
          0.15f; // “剑气护体”拦截投射物的初始几率
    }
  } // namespace Skill

  // 视觉渲染与粒子系统参数
  namespace Render
  {
    constexpr int MAX_PARTICLES_DEFAULT =
        100000;                                     // 同时显示的最大粒子总数 (GPU Buffer)
    constexpr int PARTICLE_STAGING_RESERVE = 10000; // 每帧允许申请的粒子缓冲保留量
    constexpr int WORKGROUP_SIZE_PARTICLES = 256;   // GPU计算粒子时的线程工作组大小
    constexpr float MAX_DELTA_TIME_PARTICLES =
        0.1f; // 粒子模拟的最大允许时间步长（防止卡顿后飞天）
    constexpr float DEFAULT_DELTA_TIME_PARTICLES =
        0.016f; // 默认每帧粒子物理模拟步长
                // (60fps)---此值不可信，实际帧率由全局设置决定，目前是180fps
    constexpr int MAX_SKILL_EFFECTS =
        10000; // 同时存在的最大技能特效数量 (GPU Buffer)
  } // namespace Render

  namespace Visuals
  {
    inline constexpr Color COLOR_BLADE_ASCENDANT = {195, 248, 245,
                                                    255}; // #C3F8F5 Pale Cyan
  }
} // namespace NoMoreDay::Constants

namespace NoMoreDay
{
  // 生物群系ID枚举
  enum class BiomeStyle : uint8_t
  {
    Town = 0,
    Open = 1,
    Maze = 2,
    Special = 3
  };

  enum class BiomeFeature : uint32_t
  {
    None = 0,
    AirWall = 1u << 0,
    LowGravity = 1u << 1,
    Destructible = 1u << 2,
    DynamicSpawner = 1u << 3,
    LimitedVision = 1u << 4,
    SpeedZone = 1u << 5,
    FrictionMod = 1u << 6,
    VisualFilter = 1u << 7
  };

  [[nodiscard]] constexpr uint32_t ToBiomeFeatureMask(BiomeFeature feature) noexcept
  {
    return static_cast<uint32_t>(feature);
  }

  [[nodiscard]] constexpr uint32_t AddBiomeFeature(uint32_t mask, BiomeFeature feature) noexcept
  {
    return mask | ToBiomeFeatureMask(feature);
  }

  [[nodiscard]] constexpr uint32_t RemoveBiomeFeature(uint32_t mask, BiomeFeature feature) noexcept
  {
    return mask & ~ToBiomeFeatureMask(feature);
  }

  [[nodiscard]] constexpr bool HasBiomeFeature(uint32_t mask, BiomeFeature feature) noexcept
  {
    return (mask & ToBiomeFeatureMask(feature)) != 0u;
  }

  enum class BiomeID : uint8_t
  {
    None = 0,

    // Town variants (T01-T06)
    Town = 1,
    Town_SwordImmortal = 2,
    Town_Mage = 3,
    Town_Mech = 4,
    Town_Shadow = 5,
    Town_Beast = 6,
    Town_Radiant = 7,

    // Open biomes (A group, C01-C07)
    Cave = 10,
    SunPrairie = 11,
    IceTundra = 12,
    CrimsonWaste = 13,
    DustSea = 14,
    VoidFlats = 15,
    EmeraldWet = 16,
    AshPlain = 17,

    // Maze biomes (B group, C08-C14)
    GloomSpire = 20,
    MagmaVeins = 21,
    JadeMine = 22,
    DrownedLib = 23,
    ClockCore = 24,
    AncientCrypt = 25,
    CrystalLab = 26,

    // Special biomes (C group, C15-C21)
    FloatingIsle = 30,
    CoralRuin = 31,
    WhisperWood = 32,
    HolyArena = 33,
    HiveNest = 34,
    SkyPalace = 35,
    AbyssalGap = 36,

    COUNT
  };
} // namespace NoMoreDay

// 基础变换组件
struct Position
{
  float x = 0.0f;
  float y = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Position, x, y)

struct PrevPosition
{
  float x = 0.0f;
  float y = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PrevPosition, x, y) 


struct Rotation
{
  float angle = 0.0f; // In degrees
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Rotation, angle)

struct Velocity
{
  float vx = 0.0f;
  float vy = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Velocity, vx, vy)

// 视觉组件
struct ColorComponent
{
  Color color = WHITE;
};

struct SpriteComponent
{
  Texture2D texture = {0};
  float scale = 1.0f;
  int textureLayerIndex = -1; // Index in GL_TEXTURE_2D_ARRAY (-1 for none)
  // float rotation = 0.0f; // 未来扩展
  // Rectangle sourceRect = { 0 }; // 未来用于精灵图
};

// 用于标识玩家实体的标签
struct PlayerTag{};

// 存储实体的原始输入状态
struct InputComponent
{
  float moveX = 0.0f; // -1.0 到 1.0
  float moveY = 0.0f; // -1.0 到 1.0
  bool attack = false;
  bool dash = false;
};

// 战斗属性
struct HealthComponent
{
  float current = 0.0f;
  float max = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HealthComponent, current, max)

struct MovementAccumulator
{
  float distance = 0.0f;
  float threshold = 100.0f; // Default trigger every 100 distance (approx 1
                            // meter if scale 1:100)
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MovementAccumulator, distance, threshold)

// 视野组件
struct VisionComponent
{
  float radius = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VisionComponent, radius)

// 简单的近战武器定义
struct WeaponComponent
{
  float damage = 0.0f;
  float range = 0.0f;     // 攻击半径
  float cooldown = 0.0f;  // 两次攻击之间的秒数
  float knockback = 0.0f; // 施加到目标的击退力

  // 内部状态
  float cooldownTimer = 0.0f; // 0.0f 表示就绪
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WeaponComponent, damage, range, cooldown,
                                   knockback, cooldownTimer)

// 刚被击杀实体的标签组件

struct KilledTag
{
  entt::entity killer = entt::null;
};

struct XPProcessedTag{}; // Marks that XP has been awarded for this entity

// 掉落组件

struct GoldComponent
{
  uint32_t amount = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GoldComponent, amount)

// 持久化标签：跨关卡保留 (如：玩家、核心UI)
struct PersistentTag{};

// GPU 索引组件
struct GPUIndex
{
  int index = -1;
};

// 半径组件
struct Radius
{
  float value = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Radius, value)

enum class ColliderType : uint8_t
{
  Dynamic,
  Static,
  Trigger
};
struct ColliderComponent
{
  float width = 0.0f;
  float height = 0.0f;
  ColliderType type = ColliderType::Dynamic;
  uint8_t layer = 1;
  uint8_t mask = 1;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ColliderComponent, width, height, layer,
                                   mask)

// 本地关卡标签：切换关卡时销毁 (如：敌人、掉落物、投射物)
struct LocalLevelTag{};

// 资源 ID 组件 (用于持久化纹理引用)
struct TextureIDComponent
{
  uint32_t id = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TextureIDComponent, id)

// 定义 IDComponent (用于持久化唯一标识)
struct IDComponent
{
  uint64_t uuid = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IDComponent, uuid)

// 延迟销毁组件
struct DelayedDestroyComponent
{
  float timer = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DelayedDestroyComponent, timer)

// 休眠标签：标记实体处于休眠状态，跳过 AI 和 Physics 更新
struct DormantTag{};

// 脏标记组件：标记实体的变换（位置/旋转）是否发生改变，用于加速 GPU 同步
struct DirtyTransform
{
  bool isDirty = true;
};

// 护盾运行时状态组件 (Hybrid Barrier: ES + Ward)
// 护盾值存储在 CombatStats.barrier 中，此组件仅存储运行时状态
struct BarrierComponent
{
  float last_damage_time = 0.0f; // 上次受击时间戳（用于判断回复延迟）
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BarrierComponent, last_damage_time)

// LootTag: 用于标记掉落物（物品或金币），优化空间查询
struct LootTag {};

// 标签缓存组件：用于加速世界坐标中文字标签的渲染
struct LabelCacheComponent {
    char cachedText[64] = {0};
    Vector2 cachedSize = {0, 0};
    int lastFontSize = 0;
    uint32_t lastRarityHash = 0;
    bool isValid = false;

    // Helper to force re-validation
    void Invalidate() { isValid = false; }
};
