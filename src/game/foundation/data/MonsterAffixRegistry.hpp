#pragma once

#include "game/foundation/components/Stats.hpp"
#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NoMoreDay {

/**
 * @brief 怪物词缀类型枚举
 *
 * 词缀分为三类：
 * - 数值型 (Stat-based): 直接修改CombatStats
 * - 机制型 (Mechanic-based): 需要MonsterAffixSystem处理Update/OnHit/OnDeath
 * - 混合型 (Hybrid): 同时修改数值和添加机制
 */
enum class MonsterAffixType : uint8_t {
  None = 0,

  // === 数值型词缀 (Stat Modifiers) ===
  Fast,     // 疾风: +50% 移速, +30% 攻速
  Tanky,    // 坚韧: +100% 护甲, +50% 生命
  Powerful, // 强力: +50% 伤害
  Accurate, // 精准: +50% 命中, 投射物追踪

  // === 元素词缀 (Elemental) ===
  Molten,       // 熔火: 留下火焰路径
  Frozen,       // 极寒: 生成追踪冰球，延迟爆炸
  Storm,        // 雷暴: 周期性落雷
  Toxic,        // 剧毒: 死亡时生成追踪毒球
  Void,         // 虚空: 攻击造成真实伤害
  VoidZone,     // 虚空区域: 在玩家脚下生成高伤害区域
  StormStrider, // 雷行: 受击时生成雷电残影

  // === 机制型词缀 (Mechanics) ===
  Teleporter, // 闪烁: 瞬移到玩家附近
  Nullifier,  // 消魔: 攻击驱散玩家Buff
  Shielding,  // 护盾: 给友军施加无敌护盾
  Waller,     // 筑墙: 生成阻挡物
  Vampiric,   // 吸血: 造成伤害回复生命
  Berserker,  // 狂暴: 低血量时伤害翻倍
  Vortex,     // 漩涡: 周期性吸引力场
  Entangler,  // 纠缠: 攻击定身玩家

  // === 特殊词缀 (Unique to Rarity) ===
  Avenger,  // 复仇者: 友军死亡时获得增益 (已实现)
  SoulLink, // 灵魂链接: 伤害共享 (已实现)

  // === 高级战斗机制词缀 (Advanced Combat Mechanics) ===
  MirrorImage, // 镜像: 生成克隆体
  SoulEater,   // 噬魂: 吸收死亡灵魂获得增益
  Suppressor,  // 压制: 距离减伤
  ManaSiphon,  // 虹吸: 剥夺玩家法力

  Count // 词缀总数 (用于数组大小)
};

/**
 * @brief 词缀回调类型标志
 * 用于快速判断词缀需要哪些处理路径
 */
struct AffixFlags {
  bool hasUpdate = false;  // 需要Update循环处理
  bool hasOnHit = false;   // 需要OnHit回调处理
  bool hasOnDeath = false; // 需要OnDeath回调处理
  bool hasTint = false;    // 需要颜色叠加
};

/**
 * @brief 单个词缀的数据定义
 */
struct MonsterAffixDef {
  MonsterAffixType id;
  std::string_view name;    // 显示名称 (中文)
  std::string_view name_en; // 显示名称 (英文)
  uint8_t tier;             // 词缀等级 (1=弱, 2=中, 3=强)

  // 数值修改器列表
  struct StatMod {
    StatType type;
    ModifierMode mode;
    float value;
  };
  std::array<StatMod, 4> statMods = {}; // 最多4个数值修改
  int statModCount = 0;

  AffixFlags flags;

  // 视觉颜色 (RGB)
  uint8_t tintR = 255, tintG = 255, tintB = 255;
};

/**
 * @brief 词缀数据注册表 (静态常量)
 */
class MonsterAffixRegistry {
public:
  // Constants moved from MonsterAffixSystem
  struct Params {
    static constexpr float MOLTEN_TICK_INTERVAL = 0.5f;
    static constexpr float MOLTEN_TRAIL_DURATION = 3.0f;
    static constexpr float MOLTEN_TRAIL_DAMAGE = 10.0f;
    static constexpr float MOLTEN_TRAIL_RADIUS = 20.0f;

    static constexpr float TELEPORT_COOLDOWN = 5.0f;
    static constexpr float TELEPORT_TRIGGER_DISTANCE = 300.0f;
    static constexpr float TELEPORT_TARGET_DISTANCE = 50.0f;

    static constexpr float BERSERKER_HP_THRESHOLD = 0.5f;
    static constexpr float BERSERKER_DAMAGE_MULT = 2.0f;
    static constexpr float BERSERKER_SCALE_MULT = 1.5f;

    static constexpr float FROZEN_ORB_INTERVAL = 4.0f;
    static constexpr float FROZEN_ORB_SPEED = 120.0f;
    static constexpr float FROZEN_ORB_DAMAGE = 60.0f;

    static constexpr float VOIDZONE_SPAWN_INTERVAL_MIN = 5.0f;
    static constexpr float VOIDZONE_SPAWN_INTERVAL_MAX = 8.0f;
    static constexpr float VOIDZONE_WARNING_DURATION = 1.0f;
    static constexpr float VOIDZONE_ACTIVE_DURATION = 4.0f;
    static constexpr float VOIDZONE_DAMAGE_PER_TICK = 50.0f;
    static constexpr float VOIDZONE_TICK_INTERVAL = 0.2f;

    static constexpr float STORMSTRIDER_TRIGGER_CHANCE = 0.25f;
    static constexpr float STORMSTRIDER_GHOST_DELAY = 1.5f;
    static constexpr float STORMSTRIDER_DAMAGE = 80.0f;

    static constexpr float STORM_UPDATE_INTERVAL = 6.0f;
    static constexpr float STORM_UPDATE_GHOST_DELAY = 1.0f;

    static constexpr float VOID_ON_HIT_BONUS_RATIO = 0.12f;
    static constexpr float VOID_ON_HIT_MIN_BONUS_DAMAGE = 3.0f;

    static constexpr float VORTEX_INTERVAL = 8.0f;
    static constexpr float VORTEX_DURATION = 3.0f;
    static constexpr float VORTEX_RADIUS = 300.0f;
    static constexpr float VORTEX_STRENGTH = -500.0f;

    static constexpr float WALLER_COOLDOWN = 12.0f;
    static constexpr float WALLER_DURATION = 5.0f;
    static constexpr float WALLER_DISTANCE = 150.0f;

    static constexpr float ENTANGLER_ROOT_DURATION = 2.0f;
    static constexpr float ENTANGLER_CHANCE = 0.3f;
  };

  static const std::unordered_map<std::string_view, MonsterAffixType>
      kNameToType;
  static MonsterAffixType GetTypeFromName(std::string_view name);

  static constexpr auto &GetAffixDef(MonsterAffixType type) {
    return kAffixData[static_cast<size_t>(type)];
  }

  static constexpr std::string_view GetAffixName(MonsterAffixType type) {
    return kAffixData[static_cast<size_t>(type)].name;
  }

  static constexpr std::string_view GetAffixNameEn(MonsterAffixType type) {
    return kAffixData[static_cast<size_t>(type)].name_en;
  }

  /**
   * @brief Calculate scaled value based on evolution tier.
   * Default scaling: +10% per tier above 1.
   */
  static float GetScaledValue(float baseValue, int tier) {
    if (tier <= 1)
      return baseValue;
    return baseValue * (1.0f + (tier - 1) * 0.1f);
  }

private:
  static constexpr std::array<MonsterAffixDef,
                              static_cast<size_t>(MonsterAffixType::Count)>
      kAffixData = {{
          // None (占位)
          {MonsterAffixType::None, "无", "None", 0, {}, 0, {}, 255, 255, 255},

          // === 数值型词缀 ===
          // Fast: +50% MoveSpeed, +30% AttackSpeed
          {MonsterAffixType::Fast,
           "疾风",
           "Fast",
           1,
           {{{StatType::MoveSpeed, ModifierMode::PercentAdd, 50.0f},
             {StatType::AttackSpeed, ModifierMode::PercentAdd, 30.0f}}},
           2,
           {false, false, false, true},
           200,
           200,
           255}, // 淡蓝色

          // Tanky: +100% Armor, +50% MaxHealth
          {MonsterAffixType::Tanky,
           "坚韧",
           "Tanky",
           1,
           {{{StatType::Armor, ModifierMode::PercentAdd, 100.0f},
             {StatType::MaxHealth, ModifierMode::PercentAdd, 50.0f}}},
           2,
           {false, false, false, true},
           150,
           150,
           150}, // 灰色

          // Powerful: +50% Damage (all types)
          {MonsterAffixType::Powerful,
           "强力",
           "Powerful",
           1,
           {{{StatType::PhysicalDamage, ModifierMode::PercentAdd, 50.0f}}},
           1,
           {false, false, false, true},
           255,
           100,
           100}, // 浅红色

          // Accurate: +50% Accuracy (TODO: Homing logic in projectile)
          {MonsterAffixType::Accurate,
           "精准",
           "Accurate",
           2,
           {{{StatType::Accuracy, ModifierMode::PercentAdd, 50.0f}}},
           1,
           {false, false, false, true},
           255,
           255,
           100}, // 黄色

          // === 元素词缀 ===
          // Molten: Fire trail (requires Update)
          {MonsterAffixType::Molten,
           "熔火",
           "Molten",
           2,
           {{{StatType::ResistFire, ModifierMode::Flat, 50.0f}}},
           1,
           {true, false, false, true},
           255,
           80,
           0}, // 橙红色

          // Frozen: Spawn Frozen Orbs (requires Update)
          {MonsterAffixType::Frozen,
           "极寒",
           "Frozen",
           2,
           {{{StatType::ResistCold, ModifierMode::Flat, 50.0f}}},
           1,
           {true, false, false, true},
           100,
           200,
           255}, // 冰蓝色

          // Storm: Lightning strikes (requires Update)
          {MonsterAffixType::Storm,
           "雷暴",
           "Storm",
           3,
           {{{StatType::ResistLightning, ModifierMode::Flat, 50.0f}}},
           1,
           {true, false, false, true},
           255,
           255,
           50}, // 亮黄色

          // Toxic: Volatile Death (requires OnDeath)
          {MonsterAffixType::Toxic,
           "剧毒",
           "Toxic",
           2,
           {{{StatType::ResistPoison, ModifierMode::Flat, 50.0f}}},
           1,
           {false, false, true, true},
           50,
           200,
           50}, // 绿色

          // Void: True damage (requires OnHit logic)
          {MonsterAffixType::Void,
           "虚空",
           "Void",
           3,
           {{{StatType::ResistShadow, ModifierMode::Flat, 75.0f}}},
           1,
           {false, true, false, true},
           100,
           0,
           150}, // 紫色

          // VoidZone: Spawn void zones under player (requires Update)
          {MonsterAffixType::VoidZone,
           "虚空区域",
           "Void Zone",
           3,
           {{{StatType::ResistShadow, ModifierMode::Flat, 50.0f}}},
           1,
           {true, false, false, true},
           80,
           0,
           120}, // 暗紫色

          // StormStrider: Lightning ghost on hit (requires OnHit)
          {MonsterAffixType::StormStrider,
           "雷行",
           "Storm Strider",
           2,
           {{{StatType::ResistLightning, ModifierMode::Flat, 50.0f}}},
           1,
           {false, true, false, true},
           255,
           255,
           100}, // 黄色

          // === 机制型词缀 ===
          // Teleporter: Blink (requires Update)
          {MonsterAffixType::Teleporter,
           "闪烁",
           "Teleporter",
           2,
           {},
           0,
           {true, false, false, true},
           200,
           150,
           255}, // 淡紫色

          // Nullifier: Dispel buffs (requires OnHit)
          {MonsterAffixType::Nullifier,
           "消魔",
           "Nullifier",
           3,
           {},
           0,
           {false, true, false, true},
           255,
           255,
           255}, // 白色

          // Shielding: Give shields to allies (requires Update)
          {MonsterAffixType::Shielding,
           "护盾",
           "Shielding",
           3,
           {},
           0,
           {true, false, false, true},
           255,
           200,
           50}, // 金色

          // Waller: Create walls (requires Update)
          {MonsterAffixType::Waller,
           "筑墙",
           "Waller",
           3,
           {},
           0,
           {true, false, false, true},
           120,
           80,
           50}, // 棕色

          // Vampiric: Lifesteal (requires OnHit)
          {MonsterAffixType::Vampiric,
           "吸血",
           "Vampiric",
           2,
           {{{StatType::LifeSteal, ModifierMode::Flat, 50.0f}}},
           1,
           {false, true, false, true},
           180,
           0,
           0}, // 深红色

          // Berserker: Enrage at low HP (requires Update)
          {MonsterAffixType::Berserker,
           "狂暴",
           "Berserker",
           2,
           {},
           0,
           {true, false, false, true},
           255,
           50,
           50}, // 红色

          // Avenger (existing) - handled by AvengerComponent
          {MonsterAffixType::Avenger,
           "复仇者",
           "Avenger",
           3,
           {},
           0,
           {false, false, false, true},
           200,
           50,
           255}, // 品红色

          // SoulLink (existing) - handled by SoulLinkComponent
          {MonsterAffixType::SoulLink,
           "灵魂链接",
           "Soul Link",
           3,
           {},
           0,
           {true, false, false, true},
           100,
           255,
           100}, // 亮绿色

          // === Advanced Combat Mechanics ===
          // MirrorImage: Spawn clones (requires OnHit for trigger)
          {MonsterAffixType::MirrorImage,
           "镜像",
           "Mirror Image",
           3,
           {},
           0,
           {true, true, false, true},
           180,
           180,
           255}, // 淡紫色

          // SoulEater: Absorb souls on nearby death (requires Update for stack
          // management)
          {MonsterAffixType::SoulEater,
           "噬魂",
           "Soul Eater",
           3,
           {},
           0,
           {true, false, true, true},
           50,
           0,
           100}, // 暗紫色

          // Suppressor: Distance-based damage reduction (requires
          // DamagePipeline hook)
          {MonsterAffixType::Suppressor,
           "压制",
           "Suppressor",
           3,
           {},
           0,
           {false, false, false, true},
           255,
           50,
           50}, // 红色

          // ManaSiphon: Drain player mana (requires Update for aura)
          {MonsterAffixType::ManaSiphon,
           "虹吸",
           "Mana Siphon",
           3,
           {},
           0,
           {true, false, false, true},
           150,
           50,
           200}, // 紫色
      }};
};

/**
 * @brief 怪物词缀运行时组件
 *
 * 存储实体拥有的词缀列表和运行时状态
 */
struct MonsterAffixComponent {
  std::vector<MonsterAffixType> affixes; // 词缀列表 (最多4个)
  uint64_t affixMask = 0; // Bitmask for O(1) lookup

  // 缓存的回调标志 (初始化时计算一次)
  bool hasUpdate = false;
  bool hasOnHit = false;
  bool hasOnDeath = false;

  // 运行时计时器 (每个词缀独立)
  std::array<float, 4> timers = {0.0f, 0.0f, 0.0f, 0.0f};

  // 运行时状态
  bool isBerserk = false;             // Berserker激活状态
  bool mirrorTriggered = false;       // MirrorImage HP阈值触发标志
  float mirrorCooldown = 0.0f;        // MirrorImage 独立冷却
  float voidZoneNextSpawnTime = 0.0f; // 修复：各怪物独立的虚空区域冷却

  void AddAffix(MonsterAffixType type) {
    if (affixes.size() >= 4)
      return; // 最多4个词缀
    affixes.push_back(type);
    affixMask |= (1ULL << static_cast<uint8_t>(type));

    // 更新缓存标志
    const auto &def = MonsterAffixRegistry::GetAffixDef(type);
    hasUpdate |= def.flags.hasUpdate;
    hasOnHit |= def.flags.hasOnHit;
    hasOnDeath |= def.flags.hasOnDeath;
  }

  [[nodiscard]] bool HasAffix(MonsterAffixType type) const {
    return (affixMask & (1ULL << static_cast<uint8_t>(type))) != 0;
  }
};

} // namespace NoMoreDay
