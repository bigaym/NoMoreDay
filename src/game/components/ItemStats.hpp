#pragma once
#include <cstdint>
#include <string>
#include "raylib.h"
#include <nlohmann/json.hpp>
#include "game/components/Stats.hpp"
#include "game/data/TagRegistry.hpp"

namespace NoMoreDay {

enum class AffixType : uint8_t {
    // Primary Stats
    Strength,
    Dexterity, // 敏捷
    Intelligence,
    Vitality, // 体质
    AllAttributes, // 所有属性

    // Offensive
    FlatPhysicalDamage, // 基础物理伤害
    FlatFireDamage,     // 基础火焰伤害
    FlatColdDamage,     // 基础冰霜伤害
    FlatLightningDamage,// 基础闪电伤害
    FlatPoisonDamage,   // 基础毒素伤害
    FlatShadowDamage,   // 基础暗影伤害
    
    PercentPhysicalDamage,  // 百分比物理伤害
    PercentFireDamage,      // 百分比火焰伤害
    PercentColdDamage,      // 百分比冰霜伤害
    PercentLightningDamage, // 百分比闪电伤害
    PercentPoisonDamage,    // 百分比毒素伤害
    PercentShadowDamage,    // 百分比暗影伤害

    CritChance, // 暴击几率
    CritDamage, // 暴击伤害
    AttackSpeed,// 攻击速度
    CastSpeed,  // 施法速度
    Accuracy,   // 命中率

    // Defensive
    FlatArmor,      // 基础护甲
    PercentArmor,   // 百分比护甲
    FlatHealth,     // 基础生命值
    PercentHealth,  // 百分比生命值
    FlatMana,
    ResistAll,
    ResistFire,
    ResistCold,
    ResistLightning,
    ResistPoison,
    ResistShadow,
    Thorns,         // 荆棘
    DamageReduction,// 伤害减免

    // Recovery
    HealthRegen,        // 生命回复 (Flat)
    ManaRegen,          // 法力回复 (Flat)
    PercentHealthRegen, // 生命回复加成 %
    PercentManaRegen,   // 法力回复加成 %

    // Utility
    MoveSpeed,          // 移动速度
    CooldownReduction,  // 冷却缩减
    LifeSteal,          // 生命偷取
    LifeOnHit,          // 42
    ManaOnHit,          // 43
    
    PlusAllSkills,      // 44
    PlusFlowingThrust,  // 45
    PlusRendingWave,    // 46

    Count
};

// 为枚举提供简单的序列化支持 (转为底层整数)
inline void to_json(nlohmann::json& j, const AffixType& e) { j = static_cast<uint8_t>(e); }
inline void from_json(const nlohmann::json& j, AffixType& e) { e = static_cast<AffixType>(j.get<uint8_t>()); }

struct Affix {
    AffixType type = AffixType::Count;
    float value = 0.0f;       // 词缀值
    int tier = 0;          // 词缀等级 (1到7，通常T1最低，T7最高/神级)
    bool isPrefix = true;     // true = 前缀, false = 后缀
    std::string name;  // 用于UI显示的缓存名称，例如 "of the Bear"
    Tag required_tags = Tag::None;  // 条件标签，只有技能携带这些标签时该词缀才生效
    bool isLegendary = false; // 是否为传奇融合词缀
};

// Custom JSON serialization for Affix (backward compatible)
inline void to_json(nlohmann::json& j, const Affix& a) {
    j = nlohmann::json{
        {"type", a.type}, {"value", a.value}, {"tier", a.tier},
        {"isPrefix", a.isPrefix}, {"name", a.name},
        {"isLegendary", a.isLegendary}
    };
    if (a.required_tags != Tag::None) {
        j["required_tags"] = static_cast<uint64_t>(a.required_tags);
    }
}

inline void from_json(const nlohmann::json& j, Affix& a) {
    j.at("type").get_to(a.type);
    j.at("value").get_to(a.value);
    j.at("tier").get_to(a.tier);
    j.at("isPrefix").get_to(a.isPrefix);
    j.at("name").get_to(a.name);
    if (j.contains("isLegendary")) {
        j.at("isLegendary").get_to(a.isLegendary);
    } else {
        a.isLegendary = false;
    }
    if (j.contains("required_tags")) {
        a.required_tags = static_cast<Tag>(j.at("required_tags").get<uint64_t>());
    } else {
        a.required_tags = Tag::None;
    }
}

// --- NEW DEFINITIONS FOR DATA LOADING ---

struct AffixTier {
    int tier; // 1 to 7
    int minLevel; // Required item level
    float minValue;
    float maxValue;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AffixTier, tier, minLevel, minValue, maxValue)

struct AffixDefinition {
    std::string id;
    AffixType type;
    std::string nameTemplate;              // e.g. "of the Bear" or "Strong"
    bool isPrefix;
    std::vector<AffixTier> tiers;
    std::vector<std::string> allowedTags;  // Slot filtering: "weapon", "armor", etc.
    std::vector<std::string> requiredSkillTags;  // Skill tag conditions (parsed to Tag bitmask)
    
    // Helper to get parsed required tags
    Tag GetRequiredTags() const {
        return ParseTagList(requiredSkillTags);
    }
};

// Custom JSON for AffixDefinition (backward compatible)
inline void to_json(nlohmann::json& j, const AffixDefinition& d) {
    j = nlohmann::json{
        {"id", d.id}, {"type", d.type}, {"nameTemplate", d.nameTemplate},
        {"isPrefix", d.isPrefix}, {"tiers", d.tiers}, {"allowedTags", d.allowedTags}
    };
    if (!d.requiredSkillTags.empty()) {
        j["requiredSkillTags"] = d.requiredSkillTags;
    }
}

inline void from_json(const nlohmann::json& j, AffixDefinition& d) {
    j.at("id").get_to(d.id);
    j.at("type").get_to(d.type);
    j.at("nameTemplate").get_to(d.nameTemplate);
    j.at("isPrefix").get_to(d.isPrefix);
    j.at("tiers").get_to(d.tiers);
    j.at("allowedTags").get_to(d.allowedTags);
    if (j.contains("requiredSkillTags")) {
        j.at("requiredSkillTags").get_to(d.requiredSkillTags);
    }
}

// Helper to determine if an affix is a primary stat
inline bool IsPrimaryStat(AffixType type) {
    return type == AffixType::Strength || type == AffixType::Dexterity || 
           type == AffixType::Intelligence || type == AffixType::Vitality || 
           type == AffixType::AllAttributes;
}

// Get color for affix tier
inline Color GetAffixTierColor(int tier) {
    switch (tier) {
        case 1: return GRAY;
        case 2: return WHITE;
        case 3: return GREEN;
        case 4: return BLUE;
        case 5: return YELLOW;
        case 6: return PURPLE;
        case 7: return RED;
        default: return WHITE;
    }
}

// Returns a human readable string for the affix, e.g. "[T1] +10 Strength"
inline std::string GetAffixDescription(const Affix& affix, bool showTier = true) {
    std::string text = "";
    if (showTier) {
        text += "[T" + std::to_string(affix.tier) + "] ";
    }
    text += "+";
    text += std::to_string((int)affix.value); // Simplify for now
    
    // 百分号在switch语句中处理，以便更好地控制
    
    switch (affix.type) {
        case AffixType::Strength: text += " 力量"; break;
        case AffixType::Dexterity: text += " 敏捷"; break;
        case AffixType::Intelligence: text += " 智力"; break;
        case AffixType::Vitality: text += " 体质"; break;
        case AffixType::AllAttributes: text += " 所有属性"; break;
        
        case AffixType::FlatPhysicalDamage: text += " 物理伤害"; break;
        case AffixType::FlatFireDamage: text += " 火焰伤害"; break;
        case AffixType::FlatColdDamage: text += " 冰霜伤害"; break;
        case AffixType::FlatLightningDamage: text += " 闪电伤害"; break;
        case AffixType::FlatPoisonDamage: text += " 毒素伤害"; break;
        case AffixType::FlatShadowDamage: text += " 暗影伤害"; break;

        case AffixType::PercentPhysicalDamage: text += "% 物理伤害"; break;
        case AffixType::PercentFireDamage: text += "% 火焰伤害"; break;
        case AffixType::PercentColdDamage: text += "% 冰霜伤害"; break;
        case AffixType::PercentLightningDamage: text += "% 闪电伤害"; break;
        case AffixType::PercentPoisonDamage: text += "% 毒素伤害"; break;
        case AffixType::PercentShadowDamage: text += "% 暗影伤害"; break;

        case AffixType::CritChance: text += "% 暴击率"; break;
        case AffixType::CritDamage: text += "% 暴击伤害"; break;
        case AffixType::AttackSpeed: text += "% 攻击速度"; break;
        case AffixType::CastSpeed: text += "% 施法速度"; break;
        case AffixType::Accuracy: text += "% 命中率"; break;

        case AffixType::FlatArmor: text += " 护甲"; break;
        case AffixType::FlatHealth: text += " 生命"; break;
        case AffixType::FlatMana: text += " 法力"; break;
        
        case AffixType::ResistAll: text += "% 全抗性"; break;
        case AffixType::ResistFire: text += "% 火焰抗性"; break;
        case AffixType::ResistCold: text += "% 冰霜抗性"; break;
        case AffixType::ResistLightning: text += "% 闪电抗性"; break;
        case AffixType::ResistPoison: text += "% 毒素抗性"; break;
        case AffixType::ResistShadow: text += "% 暗影抗性"; break;

        case AffixType::Thorns: text += " 荆棘伤害"; break;
        case AffixType::DamageReduction: text += "% 伤害减免"; break;

        case AffixType::HealthRegen: text += " 生命回复"; break;
        case AffixType::ManaRegen: text += " 法力回复"; break;
        case AffixType::PercentHealthRegen: text += "% 生命回复"; break;
        case AffixType::PercentManaRegen: text += "% 法力回复"; break;

        case AffixType::MoveSpeed: text += "% 移动速度"; break;
        case AffixType::CooldownReduction: text += "% 冷却缩减"; break;

        case AffixType::LifeSteal: text += "% 生命偷取"; break;
        case AffixType::LifeOnHit: text += " 击中回复"; break;
        case AffixType::ManaOnHit: text += " 击中回蓝"; break;
        
        case AffixType::PlusAllSkills: text += " 所有技能等级"; break;
        case AffixType::PlusFlowingThrust: text += " 流云刺等级"; break;
        case AffixType::PlusRendingWave: text += " 裂空斩等级"; break;

        default: text += " 属性"; break;
    }
    return text;
}

// Zero-allocation version using Raylib's TextFormat (Internal pool of buffers)
inline const char* GetAffixDescriptionRef(const Affix& affix, bool showTier = true) {
    float val = affix.value;
    int tier = affix.tier;
    
    const char* prefix = showTier ? TextFormat("[T%d] ", tier) : "";
    
    switch (affix.type) {
        case AffixType::Strength: return TextFormat("%s+%.0f 力量", prefix, val);
        case AffixType::Dexterity: return TextFormat("%s+%.0f 敏捷", prefix, val);
        case AffixType::Intelligence: return TextFormat("%s+%.0f 智力", prefix, val);
        case AffixType::Vitality: return TextFormat("%s+%.0f 体质", prefix, val);
        case AffixType::AllAttributes: return TextFormat("%s+%.0f 所有属性", prefix, val);
        
        case AffixType::FlatHealth: return TextFormat("%s+%.0f 生命", prefix, tier, val);
        case AffixType::FlatMana: return TextFormat("%s+%.0f 法力", prefix, tier, val);
        
        case AffixType::FlatPhysicalDamage: return TextFormat("%s+%.0f 物理伤害", prefix, val);
        case AffixType::FlatFireDamage: return TextFormat("%s+%.0f 火焰伤害", prefix, val);
        case AffixType::FlatColdDamage: return TextFormat("%s+%.0f 冰霜伤害", prefix, val);
        case AffixType::FlatLightningDamage: return TextFormat("%s+%.0f 闪电伤害", prefix, val);
        case AffixType::FlatPoisonDamage: return TextFormat("%s+%.0f 毒素伤害", prefix, val);
        case AffixType::FlatShadowDamage: return TextFormat("%s+%.0f 暗影伤害", prefix, val);

        case AffixType::PercentPhysicalDamage: return TextFormat("%s+%.0f%% 物理伤害", prefix, val);
        case AffixType::PercentFireDamage: return TextFormat("%s+%.0f%% 火焰伤害", prefix, val);
        case AffixType::PercentColdDamage: return TextFormat("%s+%.0f%% 冰霜伤害", prefix, val);
        case AffixType::PercentLightningDamage: return TextFormat("%s+%.0f%% 闪电伤害", prefix, val);
        case AffixType::PercentPoisonDamage: return TextFormat("%s+%.0f%% 毒素伤害", prefix, val);
        case AffixType::PercentShadowDamage: return TextFormat("%s+%.0f%% 暗影伤害", prefix, val);

        case AffixType::CritChance: return TextFormat("%s+%.1f%% 暴击率", prefix, val);
        case AffixType::CritDamage: return TextFormat("%s+%.1f%% 暴击伤害", prefix, val);
        case AffixType::AttackSpeed: return TextFormat("%s+%.0f%% 攻击速度", prefix, val);
        case AffixType::CastSpeed: return TextFormat("%s+%.0f%% 施法速度", prefix, val);
        case AffixType::Accuracy: return TextFormat("%s+%.0f%% 命中率", prefix, val);

        case AffixType::FlatArmor: return TextFormat("%s+%.0f 护甲", prefix, val);
        case AffixType::PercentArmor: return TextFormat("%s+%.0f%% 护甲", prefix, val);
        case AffixType::ResistAll: return TextFormat("%s+%.0f%% 全抗性", prefix, val);
        case AffixType::ResistFire: return TextFormat("%s+%.0f%% 火焰抗性", prefix, val);
        case AffixType::ResistCold: return TextFormat("%s+%.0f%% 冰霜抗性", prefix, val);
        case AffixType::ResistLightning: return TextFormat("%s+%.0f%% 闪电抗性", prefix, val);
        case AffixType::ResistPoison: return TextFormat("%s+%.0f%% 毒素抗性", prefix, val);
        case AffixType::ResistShadow: return TextFormat("%s+%.0f%% 暗影抗性", prefix, val);

        case AffixType::Thorns: return TextFormat("%s+%.0f 荆棘伤害", prefix, val);
        case AffixType::DamageReduction: return TextFormat("%s+%.0f%% 伤害减免", prefix, val);

        case AffixType::HealthRegen: return TextFormat("%s+%.1f 生命回复", prefix, val);
        case AffixType::ManaRegen: return TextFormat("%s+%.1f 法力回复", prefix, val);
        case AffixType::PercentHealthRegen: return TextFormat("%s+%.0f%% 生命回复", prefix, val);
        case AffixType::PercentManaRegen: return TextFormat("%s+%.0f%% 法力回复", prefix, val);

        case AffixType::MoveSpeed: return TextFormat("%s+%.0f%% 移动速度", prefix, val);
        case AffixType::CooldownReduction: return TextFormat("%s+%.0f%% 冷却缩减", prefix, val);

        case AffixType::LifeSteal: return TextFormat("%s+%.1f%% 生命偷取", prefix, val);
        case AffixType::LifeOnHit: return TextFormat("%s+%.1f 击中回复", prefix, val);
        case AffixType::ManaOnHit: return TextFormat("%s+%.1f 击中回蓝", prefix, val);
        
        case AffixType::PlusAllSkills: return TextFormat("%s+%.0f 所有技能等级", prefix, val);
        case AffixType::PlusFlowingThrust: return TextFormat("%s+%.0f 流云刺等级", prefix, val);
        case AffixType::PlusRendingWave: return TextFormat("%s+%.0f 裂空斩等级", prefix, val);

        default: return TextFormat("%s+%.1f 属性", prefix, val);
    }
}

// 符文组件: 标记物品为符文，并定义其在不同装备上的效果
struct RuneComponent {
    std::vector<Affix> weaponEffects;
    std::vector<Affix> armorEffects;
    std::vector<Affix> jewelryEffects;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RuneComponent, weaponEffects, armorEffects, jewelryEffects)

} // namespace NoMoreDay