#pragma once
#include <cstdint>
#include <string>
#include "raylib.h"
#include <nlohmann/json.hpp>
#include "Stats.hpp"

namespace NoMoreDay {

enum class AffixType : uint8_t {
    // Primary Stats
    Strength,
    Dexterity, // 敏捷
    Intelligence,
    Vitality, // 体质

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

    // Utility
    MoveSpeed,          // 移动速度
    CooldownReduction,  // 冷却缩减
    LifeSteal,          // 生命偷取
    LifeOnHit,          // 击中回复
    
    Count
};

// 为枚举提供简单的序列化支持 (转为底层整数)
inline void to_json(nlohmann::json& j, const AffixType& e) { j = static_cast<uint8_t>(e); }
inline void from_json(const nlohmann::json& j, AffixType& e) { e = static_cast<AffixType>(j.get<uint8_t>()); }

struct Affix {
    AffixType type;
    float value; // 词缀值
    int tier;    // 词缀等级 (1到7，通常T1最低，T7最高/神级)
                 // 假设 T1 = 低，T7 = 高/神级。
    bool isPrefix; // true = 前缀, false = 后缀
    std::string name; // 用于UI显示的缓存名称，例如 "of the Bear" 或 "Burning"
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Affix, type, value, tier, isPrefix, name)

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
    std::string nameTemplate; // e.g. "of the Bear" or "Strong"
    bool isPrefix;
    std::vector<AffixTier> tiers;
    std::vector<std::string> allowedTags; // e.g. "weapon", "armor"
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AffixDefinition, id, type, nameTemplate, isPrefix, tiers, allowedTags)

// Returns a human readable string for the affix, e.g. "+10 Strength"
inline std::string GetAffixDescription(const Affix& affix) {
    std::string text = "+";
    text += std::to_string((int)affix.value); // Simplify for now
    
    // 百分号在switch语句中处理，以便更好地控制
    
    switch (affix.type) {
        case AffixType::Strength: text += " 力量"; break;
        case AffixType::Dexterity: text += " 敏捷"; break;
        case AffixType::Intelligence: text += " 智力"; break;
        case AffixType::Vitality: text += " 体质"; break;
        
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

        case AffixType::MoveSpeed: text += "% 移动速度"; break;
        case AffixType::CooldownReduction: text += "% 冷却缩减"; break;

        case AffixType::LifeSteal: text += "% 生命偷取"; break;
        case AffixType::LifeOnHit: text += " 击中回复"; break;
        
        default: text += " 属性"; break;
    }
    return text;
}

// Zero-allocation version using Raylib's TextFormat (Internal pool of buffers)
inline const char* GetAffixDescriptionRef(const Affix& affix) {
    float val = affix.value;
    switch (affix.type) {
        case AffixType::Strength: return TextFormat("+%.0f 力量", val);
        case AffixType::Dexterity: return TextFormat("+%.0f 敏捷", val);
        case AffixType::Intelligence: return TextFormat("+%.0f 智力", val);
        case AffixType::Vitality: return TextFormat("+%.0f 体质", val);
        case AffixType::FlatHealth: return TextFormat("+%.0f 生命", val);
        case AffixType::FlatMana: return TextFormat("+%.0f 法力", val);
        
        case AffixType::FlatPhysicalDamage: return TextFormat("+%.0f 物理伤害", val);
        case AffixType::FlatFireDamage: return TextFormat("+%.0f 火焰伤害", val);
        case AffixType::FlatColdDamage: return TextFormat("+%.0f 冰霜伤害", val);
        case AffixType::FlatLightningDamage: return TextFormat("+%.0f 闪电伤害", val);
        case AffixType::FlatPoisonDamage: return TextFormat("+%.0f 毒素伤害", val);
        case AffixType::FlatShadowDamage: return TextFormat("+%.0f 暗影伤害", val);

        case AffixType::PercentPhysicalDamage: return TextFormat("+%.0f%% 物理伤害", val);
        case AffixType::PercentFireDamage: return TextFormat("+%.0f%% 火焰伤害", val);
        case AffixType::PercentColdDamage: return TextFormat("+%.0f%% 冰霜伤害", val);
        case AffixType::PercentLightningDamage: return TextFormat("+%.0f%% 闪电伤害", val);
        case AffixType::PercentPoisonDamage: return TextFormat("+%.0f%% 毒素伤害", val);
        case AffixType::PercentShadowDamage: return TextFormat("+%.0f%% 暗影伤害", val);

        case AffixType::CritChance: return TextFormat("+%.1f%% 暴击率", val);
        case AffixType::CritDamage: return TextFormat("+%.1f%% 暴击伤害", val);
        case AffixType::AttackSpeed: return TextFormat("+%.0f%% 攻击速度", val);
        case AffixType::CastSpeed: return TextFormat("+%.0f%% 施法速度", val);
        case AffixType::Accuracy: return TextFormat("+%.0f%% 命中率", val);

        case AffixType::FlatArmor: return TextFormat("+%.0f 护甲", val);
        case AffixType::PercentArmor: return TextFormat("+%.0f%% 护甲", val);
        case AffixType::ResistAll: return TextFormat("+%.0f%% 全抗性", val);
        case AffixType::ResistFire: return TextFormat("+%.0f%% 火焰抗性", val);
        case AffixType::ResistCold: return TextFormat("+%.0f%% 冰霜抗性", val);
        case AffixType::ResistLightning: return TextFormat("+%.0f%% 闪电抗性", val);
        case AffixType::ResistPoison: return TextFormat("+%.0f%% 毒素抗性", val);
        case AffixType::ResistShadow: return TextFormat("+%.0f%% 暗影抗性", val);

        case AffixType::Thorns: return TextFormat("+%.0f 荆棘伤害", val);
        case AffixType::DamageReduction: return TextFormat("+%.0f%% 伤害减免", val);

        case AffixType::MoveSpeed: return TextFormat("+%.0f%% 移动速度", val);
        case AffixType::CooldownReduction: return TextFormat("+%.0f%% 冷却缩减", val);

        case AffixType::LifeSteal: return TextFormat("+%.1f%% 生命偷取", val);
        case AffixType::LifeOnHit: return TextFormat("+%.1f 击中回复", val);
        
        default: return TextFormat("+%.1f 属性", val);
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
