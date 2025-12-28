#pragma once
#include <array>
#include <cstdint>

namespace NoMoreDay {

// 1. 伤害类型定义
// 使用枚举作为数组索引，方便 SIMD 优化和统一计算
enum class DamageType : uint8_t {
    Physical = 0,
    Fire,
    Cold,
    Lightning,
    Poison,
    Shadow,
    Count // 6种类型
};

// 辅助：获取类型名称（用于 UI 显示或调试）
constexpr const char* GetDamageTypeName(DamageType type) {
    switch (type) {
        case DamageType::Physical: return "Physical";
        case DamageType::Fire:     return "Fire";
        case DamageType::Cold:     return "Cold";
        case DamageType::Lightning:return "Lightning";
        case DamageType::Poison:   return "Poison";
        case DamageType::Shadow:   return "Shadow";
        default:                   return "Unknown";
    }
}

// 2. 基础属性 (Primary Stats)
// 来源：升级加点、装备白字
// 作用：经过公式计算转化为 CombatStats
struct alignas(32) PrimaryStats {
    float strength = 0.0f;      // -> 增加护甲, 物理伤害等
    float dexterity = 0.0f;     // -> 增加闪避, 暴击率等
    float intelligence = 0.0f;  // -> 增加全抗性, 元素伤害，最大蓝量等
    float vitality = 0.0f;      // -> 增加最大生命值等
};

// 3. 战斗属性 (Combat Stats) - "Baked" Data
// 这是战斗系统直接读取的最终面板。
// 所有的 Buff、装备词缀、天赋加成都在 StatsSystem 中计算并“烘焙”进这里。
struct alignas(32) CombatStats {
    // --- 生存资源 ---
    float health = 100.0f;
    float max_health = 100.0f;
    float mana = 100.0f;
    float max_mana = 100.0f;

    // --- 进攻面板 ---
    
    // A. 武器基础伤害 (Base Weapon Damage)
    // 通常是物理，但传奇武器可能带有元素基底
    float min_weapon_damage = 0.0f;
    float max_weapon_damage = 0.0f;

    // B. 附加点伤 (Flat Added Damage)
    // 来源：戒指上的 "+5-10 火焰伤害"，技能的 "附加 50 点毒素伤害"
    // 索引对应 DamageType
    std::array<float, 6> flat_damage = {0.0f}; 

    // C. 伤害倍率 (Damage Multipliers)
    // 来源：天赋 "+10% 物理伤害"，智力加成
    // 基准值为 1.0 (100%)
    std::array<float, 6> damage_multipliers = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    // D. 暴击体系 (Crit)
    float crit_chance = 0.05f;      // 基础 5%
    float crit_damage = 1.50f;      // 基础 150%
    
    // E. 速度与穿透
    float attack_speed = 1.0f;      // 攻击频率倍率
    float cast_speed = 1.0f;        // 施法速度
    float armor_pen = 0.0f;         // 护甲穿透 (固定值或百分比，视设计而定)

    // --- 防御面板 ---
    
    float armor = 0.0f;             // 物理减伤核心
    float dodge_chance = 0.0f;      // 闪避几率
    
    // 抗性 (Resistances) - 直接百分比减伤 (e.g., 0.75 = 75% Res)
    std::array<float, 6> resistances = {0.0f}; 

    // 格挡 (Block)
    float block_chance = 0.0f;
    float block_amount = 0.0f;      // 格挡减免的伤害值

    // --- 特殊机制 ---
    float move_speed = 300.0f;      // 移动速度 (pixels/sec)
    float life_steal = 0.0f;        // 吸血 %
    float life_on_hit = 0.0f;       // 击回
    float thorns = 0.0f;            // 荆棘伤害 (反伤)
    float magic_find = 0.0f;        // 魔法寻宝率

    // --- 技能效率 (Skill Efficiency) ---
    // 最终冷却 = (基础冷却 / cooldown_recovery_speed) * (1.0 - cooldown_reduction)
    float cooldown_recovery_speed = 1.0f; // 冷却回复速度 (1.0 = 100%, +100% = 2.0)
    float cooldown_reduction = 0.0f;      // 冷却时间缩减 % (Hard Cap通常设为 75% 或 90%)
    float resource_cost_reduction = 0.0f; // 资源消耗降低 %
    float cast_range = 0.0f;              // 施法距离加成

    // --- 持续回复 (Regeneration) ---
    float health_regen = 0.0f;            // 每秒生命回复 (Flat)
    float mana_regen = 0.0f;              // 每秒法力回复 (Flat)
    float health_regen_pct = 0.0f;        // 生命回复加成 %

    // --- 技能形态 (Skill Mechanics) ---
    float area_scale = 1.0f;              // 范围大小 (AoE) 乘区
    float projectile_speed = 1.0f;        // 投射物速度
    float duration_scale = 1.0f;          // 持续时间乘区

    // --- 冒险与其它 (Adventure & Misc) ---
    float pickup_range = 50.0f;           // 自动拾取范围
    float gold_bonus = 0.0f;              // 金币获取加成
    float exp_bonus = 0.0f;               // 经验获取加成
    float damage_reduction = 0.0f;        // 全局伤害减免 % (稀有属性)
};

// Tag component to request a stats recalculation
struct StatsDirty {};

// --- New Definitions for Modifiers ---

enum class StatType : uint8_t {
    Strength,
    Dexterity,
    Intelligence,
    Vitality,
    MaxHealth,
    MaxMana,
    MoveSpeed,
    Armor,
    Count
};

enum class ModifierMode : uint8_t {
    Flat,           // Adds to base (e.g., +10 HP)
    PercentAdd,     // Adds to multiplier (e.g., +10% HP)
    PercentMult     // Multiplies the final result (e.g., x1.2 damage)
};

enum class ModifierSource : uint8_t {
    Base,       // Intrinsic to the entity or base attributes
    Item,       // From equipment
    Skill,      // From passive or active skills
    Buff,       // Temporary effects
    Environment // Zone modifiers etc.
};

struct StatModifier {
    StatType type;
    ModifierMode mode;
    float value;
    ModifierSource source = ModifierSource::Base;
    uint32_t source_id = 0; // Optional: to track specific item/skill ID
};

struct ModifierList {
    std::vector<StatModifier> modifiers;
};

} // namespace NoMoreDay