#pragma once
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "game/data/TagRegistry.hpp"

namespace NoMoreDay
{

    // --- 游戏平衡性常量 (Attribute Caps & Defaults) ---
    namespace GameConstants
    {
        static constexpr int MAX_LEVEL = 100;

        // 生存上限
        static constexpr float RESISTANCE_CAP = 0.75f;          // 基础抗性上限 75%
        static constexpr float MAX_RESISTANCE_HARD_CAP = 0.90f; // 即使有增加上限的装备，绝对上限为 90%
        static constexpr float DODGE_CAP = 0.75f;               // 闪避上限 75%
        static constexpr float BLOCK_CAP = 0.75f;               // 格挡上限 75%
        static constexpr float GLOBAL_DR_CAP = 0.90f;           // 全局减伤上限 90%

        // 进攻与效能上限
        static constexpr float CRIT_CHANCE_CAP = 1.00f;  // 暴击率上限 100%
        static constexpr float CDR_CAP = 0.75f;          // 冷却缩减上限 75%
        static constexpr float ATTACK_SPEED_CAP = 10.0f; // 攻击速度上限 (每秒10次)

        // 默认值 (Defaults)
        static_assert(MAX_LEVEL == 100);
        static constexpr float DEFAULT_MOVE_SPEED = 100.0f;
        static constexpr float DEFAULT_MAX_HEALTH = 1000000.0f;
        static constexpr float DEFAULT_MAX_MANA = 1000000.0f;
        static constexpr float DEFAULT_CRIT_CHANCE = 0.05f;
        static constexpr float DEFAULT_CRIT_DAMAGE = 1.50f;
        static constexpr float DEFAULT_ATTACK_SPEED = 1.0f;
        static constexpr float DEFAULT_ACCURACY = 0.97f;
        static constexpr float DEFAULT_PICKUP_RANGE = 50.0f;

        // 移动
        static constexpr float MOVE_SPEED_CAP = 500.0f; // 移动速度上限

        // 属性比例 (Attribute Ratios)
        static constexpr float STR_TO_ARMOR = 2.0f;
        static constexpr float VIT_TO_HEALTH = 15.0f;
        static constexpr float INT_TO_MANA = 5.0f;
        static constexpr float VIT_TO_HEALTH_REGEN = 0.2f;
        static constexpr float INT_TO_MANA_REGEN = 0.2f;
        static constexpr float STR_TO_PHYS_DAMAGE_INC = 1.0f; // 1% per point
        static constexpr float DEX_TO_CRIT_CHANCE = 0.2f;    // 0.2% per point
        static constexpr float DEX_TO_ACCURACY = 0.1f;       // 0.1% per point
        static constexpr float STR_TO_KNOCKBACK = 0.5f;

        // 空手/默认攻击 (Unarmed/Default Attack)
        static constexpr float UNARMED_DAMAGE_MIN = 2.0f;
        static constexpr float UNARMED_DAMAGE_MAX = 3.0f;
        static constexpr float UNARMED_KNOCKBACK = 10.0f;
        static constexpr float DEFAULT_WEAPON_KNOCKBACK = 20.0f;
    }

    // 1. 伤害类型定义

    // 使用枚举作为数组索引，方便 SIMD 优化和统一计算
    enum class DamageType : uint8_t
    {
        Physical = 0,
        Fire,
        Cold,
        Lightning,
        Poison,
        Shadow,
        Count // 6种类型
    };

    // 辅助：获取类型名称（用于 UI 显示或调试）
    constexpr const char *GetDamageTypeName(DamageType type)
    {
        switch (type)
        {
        case DamageType::Physical:
            return "Physical";
        case DamageType::Fire:
            return "Fire";
        case DamageType::Cold:
            return "Cold";
        case DamageType::Lightning:
            return "Lightning";
        case DamageType::Poison:
            return "Poison";
        case DamageType::Shadow:
            return "Shadow";
        default:
            return "Unknown";
        }
    }

    // 2. 基础属性 (Primary Stats)
    // 来源：升级加点、装备白字
    // 作用：经过公式计算转化为 CombatStats
    struct alignas(32) PrimaryStats
    {
        float strength = 0.0f;     // -> 增加护甲, 物理伤害等
        float dexterity = 0.0f;    // -> 增加闪避, 暴击率等
        float intelligence = 0.0f; // -> 增加全抗性, 元素伤害，最大蓝量等
        float vitality = 0.0f;     // -> 增加最大生命值等
    };
    static_assert(alignof(PrimaryStats) == 32, "PrimaryStats must be 32-byte aligned");
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PrimaryStats, strength, dexterity, intelligence, vitality)

    // 3. 战斗属性 (Combat Stats) - "Baked" Data
    // 这是战斗系统直接读取的最终面板。
    // 所有的 Buff、装备词缀、天赋加成都在 StatsSystem 中计算并“烘焙”进这里。
    // NOTE: tag_stat_cache moved to StatsSystem to keep CombatStats as POD for SIMD
    struct alignas(32) CombatStats
    {
        // --- 生存资源 ---
        float health = GameConstants::DEFAULT_MAX_HEALTH;
        float max_health = GameConstants::DEFAULT_MAX_HEALTH;
        float mana = GameConstants::DEFAULT_MAX_MANA;
        float max_mana = GameConstants::DEFAULT_MAX_MANA;

        // --- 有效属性 (用于 UI 显示) ---
        float effective_strength = 0.0f;
        float effective_dexterity = 0.0f;
        float effective_intelligence = 0.0f;
        float effective_vitality = 0.0f;

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

        // Added for proper conditional stacking (Fix 2026-01-09)
        // Stores the sum of all "Increased" modifiers (e.g. 0.5 for +50%)
        std::array<float, 6> damage_percent_add = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        // Stores the product of all "More" modifiers (e.g. 1.5 for x1.5)
        std::array<float, 6> damage_percent_mult_component = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

        // D. 暴击体系 (Crit)
        float crit_chance = GameConstants::DEFAULT_CRIT_CHANCE;
        float crit_damage = GameConstants::DEFAULT_CRIT_DAMAGE;

        // E. 速度与穿透
        float attack_speed = GameConstants::DEFAULT_ATTACK_SPEED;
        float cast_speed = 1.0f; // 施法速度
        float accuracy = GameConstants::DEFAULT_ACCURACY;
        float armor_pen = 0.0f; // 护甲穿透 (固定值或百分比，视设计而定)
        float knockback = 0.0f; // 击退力度

        // --- 防御面板 ---

        float armor = 0.0f;        // 物理减伤核心
        float dodge_chance = 0.0f; // 闪避几率

        // 抗性 (Resistances) - 直接百分比减伤 (e.g., 0.75 = 75% Res)
        std::array<float, 6> resistances = {0.0f};

        // 格挡 (Block)
        float block_chance = 0.0f;
        float block_amount = 0.0f; // 格挡减免的伤害值

        // --- 特殊机制 ---
        float move_speed = GameConstants::DEFAULT_MOVE_SPEED; // 移动速度 (pixels/sec)
        float life_steal = 0.0f;                              // 吸血 %
        float life_on_hit = 0.0f;                             // 击回
        float mana_on_hit = 0.0f;                             // 蓝回
        float thorns = 0.0f;                                  // 荆棘伤害 (反伤)
        float magic_find = 4.0f;                              // 魔法寻宝率

        // --- 技能效率 (Skill Efficiency) ---
        // 最终冷却 = (基础冷却 / cooldown_recovery_speed) * (1.0 - cooldown_reduction)
        float cooldown_recovery_speed = 1.0f; // 冷却回复速度 (1.0 = 100%, +100% = 2.0)
        float cooldown_reduction = 0.0f;      // 冷却时间缩减 % (Hard Cap通常设为 75% 或 90%)
        float resource_cost_reduction = 0.0f; // 资源消耗降低 %
        float cast_range = 0.0f;              // 施法距离加成

        // --- 持续回复 (Regeneration) ---
        float health_regen = 1.0f;     // 每秒生命回复 (Flat)
        float mana_regen = 1.0f;       // 每秒法力回复 (Flat)
        float health_regen_pct = 0.0f; // 生命回复加成 %
        float mana_regen_pct = 0.0f;   // 法力回复加成 %

        // --- 技能形态 (Skill Mechanics) ---
        float area_scale = 1.0f;       // 范围大小 (AoE) 乘区
        float projectile_speed = 1.0f; // 投射物速度
        float duration_scale = 1.0f;   // 持续时间乘区

        // --- 冒险与其它 (Adventure & Misc) ---
        float pickup_range = 50.0f;        // 自动拾取范围
        float gold_bonus = 0.0f;           // 金币获取加成
        float experience_gain_mult = 0.0f; // 经验获取加成
        float damage_reduction = 0.0f;     // 全局伤害减免 % (稀有属性)

        // --- Stat Cap Tracking (Raw values before clamping) ---
        std::array<float, 6> raw_resistances = {0.0f};
        float raw_move_speed = 0.0f;
        float raw_cooldown_reduction = 0.0f;
        float raw_attack_speed = 0.0f;
        float raw_dodge_chance = 0.0f;
        float raw_block_chance = 0.0f;

        // Padding to maintain struct size consistency
        float _padding[2] = {0.0f, 0.0f};
    };
    static_assert(alignof(CombatStats) == 32, "CombatStats must be 32-byte aligned");

    // 定义 CombatStats 的 JSON 序列化
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CombatStats,
                                       health, max_health, mana, max_mana,
                                       effective_strength, effective_dexterity, effective_intelligence, effective_vitality,
                                       min_weapon_damage, max_weapon_damage,
                                       flat_damage, damage_multipliers, damage_percent_add, damage_percent_mult_component,
                                       crit_chance, crit_damage,
                                       attack_speed, cast_speed, accuracy, armor_pen, knockback,
                                       armor, dodge_chance,
                                       resistances,
                                       block_chance, block_amount,
                                       move_speed, life_steal, life_on_hit, mana_on_hit, thorns, magic_find,
                                       cooldown_recovery_speed, cooldown_reduction, resource_cost_reduction, cast_range,
                                       health_regen, mana_regen, health_regen_pct, mana_regen_pct,
                                       area_scale, projectile_speed, duration_scale,
                                       pickup_range, gold_bonus, experience_gain_mult, damage_reduction)

    // 请求重新计算属性的标签组件
    struct StatsDirty
    {
    };

    // --- 修饰符的新定义 ---

    enum class StatType : uint8_t
    {
        Strength,
        Dexterity,
        Intelligence,
        Vitality,
        MaxHealth, // 最大生命值
        MaxMana,   // 最大法力值
        MoveSpeed, // 移动速度
        Armor,     // 护甲

        // Offensive
        PhysicalDamage,
        FireDamage,
        ColdDamage,
        LightningDamage,
        PoisonDamage,
        ShadowDamage,

        CritChance,
        CritDamage,
        AttackSpeed,
        CastSpeed,
        Accuracy,
        ManaOnHit,
        ArmorPenetration,

        // Defensive
        ResistPhysical,
        ResistFire,
        ResistCold,
        ResistLightning,
        ResistPoison,
        ResistShadow,
        ResistAll,

        CooldownReduction,     // 冷却缩减 %
        ResourceCostReduction, // 资源消耗降低 %

        ProjectileCount, // 投射物数量
        AreaScale,       // 范围大小
        ProjectileSpeed, // 投射物速度
        DurationScale,   // 持续时间乘区

        DodgeChance, // 闪避率
        BlockChance, // 格挡几率
        LifeSteal,   // 吸血 %
        LifeOnHit,   // 击回
        HealthRegen, // 生命回复
        ManaRegen,   // 法力回复
        Thorns,      // 荆棘
        MagicFind,   // 掉率

        Count
    };

    enum class ModifierMode : uint8_t
    {
        Flat,       // 添加到基础值 (例如, +10 生命值)
        PercentAdd, // 添加到乘数 (例如, +10% 生命值)
        PercentMult // 乘以最终结果 (例如, x1.2 伤害)
    };

    enum class ModifierSource : uint8_t
    {
        Base,       // 实体固有或基础属性
        Item,       // 来自装备
        Skill,      // 来自被动或主动技能
        Buff,       // 临时效果
        Environment // 区域修饰符等
    };

    // 为枚举提供简单的序列化支持 (转为底层整数)
    inline void to_json(nlohmann::json &j, const StatType &e) { j = static_cast<uint8_t>(e); }
    inline void from_json(const nlohmann::json &j, StatType &e) { e = static_cast<StatType>(j.get<uint8_t>()); }

    inline void to_json(nlohmann::json &j, const ModifierMode &e) { j = static_cast<uint8_t>(e); }
    inline void from_json(const nlohmann::json &j, ModifierMode &e) { e = static_cast<ModifierMode>(j.get<uint8_t>()); }

    inline void to_json(nlohmann::json &j, const ModifierSource &e) { j = static_cast<uint8_t>(e); }
    inline void from_json(const nlohmann::json &j, ModifierSource &e) { e = static_cast<ModifierSource>(j.get<uint8_t>()); }

    struct StatModifier
    {
        StatType type = StatType::Count;
        ModifierMode mode = ModifierMode::Flat;
        float value = 0.0f;
        Tag required_tags = Tag::None;                // 只有当查询携带这些标签时，该修饰符才生效
        ModifierSource source = ModifierSource::Base; // 修饰符来源
        uint32_t source_id = 0;                       // 可选: 用于追踪特定的物品/技能ID
    };

    inline void to_json(nlohmann::json &j, const StatModifier &m)
    {
        j = nlohmann::json{{"type", m.type}, {"mode", m.mode}, {"value", m.value}, {"required_tags", m.required_tags}, {"source", m.source}, {"source_id", m.source_id}};
    }
    inline void from_json(const nlohmann::json &j, StatModifier &m)
    {
        j.at("type").get_to(m.type);
        j.at("mode").get_to(m.mode);
        j.at("value").get_to(m.value);
        if (j.contains("required_tags"))
            j.at("required_tags").get_to(m.required_tags);
        else
            m.required_tags = Tag::None;
        if (j.contains("source"))
            j.at("source").get_to(m.source);
        else
            m.source = ModifierSource::Base;
        if (j.contains("source_id"))
            j.at("source_id").get_to(m.source_id);
        else
            m.source_id = 0;
    }

    struct ModifierList
    {
        std::vector<StatModifier> modifiers;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ModifierList, modifiers)

} // namespace NoMoreDay