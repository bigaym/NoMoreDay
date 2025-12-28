#pragma once
#include <cstdint>
#include <string>
#include "Stats.hpp"

namespace NoMoreDay {

enum class AffixType : uint8_t {
    // Primary Stats
    Strength,
    Dexterity,
    Intelligence,
    Vitality,

    // Offensive
    FlatPhysicalDamage,
    FlatFireDamage,
    FlatColdDamage,
    FlatLightningDamage,
    FlatPoisonDamage,
    FlatShadowDamage,
    
    PercentPhysicalDamage,
    PercentFireDamage,
    PercentColdDamage,
    PercentLightningDamage,
    PercentPoisonDamage,
    PercentShadowDamage,

    CritChance,
    CritDamage,
    AttackSpeed,
    CastSpeed,

    // Defensive
    FlatArmor,
    PercentArmor,
    FlatHealth,
    PercentHealth,
    FlatMana,
    ResistAll,
    ResistFire,
    ResistCold,
    ResistLightning,
    ResistPoison,
    ResistShadow,

    // Utility
    MoveSpeed,
    CooldownReduction,
    
    Count
};

struct Affix {
    AffixType type;
    float value;
    int tier; // 1 to 7 (T1 is lowest usually, or T7 is lowest? Design says T6 is Exalted, usually higher tier # is better in ARPGs like LE)
              // Let's assume T1 = Low, T7 = High/Godly.
    bool isPrefix; // true = Prefix, false = Suffix
    std::string name; // Cached name for UI, e.g. "of the Bear" or "Burning"
};

// Returns a human readable string for the affix, e.g. "+10 Strength"
inline std::string GetAffixDescription(const Affix& affix) {
    std::string text = "+";
    text += std::to_string((int)affix.value); // Simplify for now
    
    // Percent signs are handled in the switch cases for better control
    
    switch (affix.type) {
        case AffixType::Strength: text += " Strength"; break;
        case AffixType::Dexterity: text += " Dexterity"; break;
        case AffixType::Intelligence: text += " Intelligence"; break;
        case AffixType::Vitality: text += " Vitality"; break;
        
        case AffixType::FlatPhysicalDamage: text += " Physical Dmg"; break;
        case AffixType::FlatFireDamage: text += " Fire Dmg"; break;
        case AffixType::FlatColdDamage: text += " Cold Dmg"; break;
        case AffixType::FlatLightningDamage: text += " Lightning Dmg"; break;
        case AffixType::FlatPoisonDamage: text += " Poison Dmg"; break;
        case AffixType::FlatShadowDamage: text += " Shadow Dmg"; break;

        case AffixType::PercentPhysicalDamage: text += "% Increased Physical Dmg"; break;
        case AffixType::PercentFireDamage: text += "% Increased Fire Dmg"; break;
        case AffixType::PercentColdDamage: text += "% Increased Cold Dmg"; break;
        case AffixType::PercentLightningDamage: text += "% Increased Lightning Dmg"; break;
        case AffixType::PercentPoisonDamage: text += "% Increased Poison Dmg"; break;
        case AffixType::PercentShadowDamage: text += "% Increased Shadow Dmg"; break;

        case AffixType::CritChance: text += "% Crit Chance"; break;
        case AffixType::CritDamage: text += "% Crit Damage"; break;
        case AffixType::AttackSpeed: text += "% Attack Speed"; break;
        case AffixType::CastSpeed: text += "% Cast Speed"; break;

        case AffixType::FlatArmor: text += " Armor"; break;
        case AffixType::FlatHealth: text += " Health"; break;
        case AffixType::FlatMana: text += " Mana"; break;
        
        case AffixType::ResistAll: text += "% All Resistances"; break;
        case AffixType::ResistFire: text += "% Fire Resistance"; break;
        case AffixType::ResistCold: text += "% Cold Resistance"; break;
        case AffixType::ResistLightning: text += "% Lightning Resistance"; break;
        case AffixType::ResistPoison: text += "% Poison Resistance"; break;
        case AffixType::ResistShadow: text += "% Shadow Resistance"; break;

        case AffixType::MoveSpeed: text += "% Move Speed"; break;
        case AffixType::CooldownReduction: text += "% Cooldown Reduction"; break;
        
        default: text += " Stat"; break;
    }
    return text;
}

} // namespace NoMoreDay
