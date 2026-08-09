#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace NoMoreDay {

enum class MapAffixCategory : uint8_t {
    Buff,       // Positive (Reward) - e.g., Drop Rate
    Debuff,     // Negative (Challenge) - e.g., Monster Strength, Player Weakness
    Environment // Environmental - Reserved for future use
};

enum class MapAffixType : uint8_t {
    // --- BUFFS (Rewards) ---
    DropRarity,          // +% Magic Find
    DropQuantity,        // +% Item Quantity

    // --- DEBUFFS (Challenges) ---
    // 1. Structural
    MonsterDensity,      // +% Pack Size
    MonsterLevel,        // + Level Offset

    // 2. Enemy Defense
    Enemy_ExtraHealth,   // +% HP
    Enemy_ExtraBarrier,  // Gain Barrier (% of Max HP)
    Enemy_BarrierRegen,  // Barrier Regen per Sec (% Max)
    Enemy_Armor,         // + Flat Armor
    Enemy_Dodge,         // + Flat Dodge Rating
    Enemy_ResistAll,     // +% All Resistances
    Enemy_ResistPhys,    // +% Physical Resist
    Enemy_ResistFire,    // +% Fire Resist
    Enemy_ResistCold,    // +% Cold Resist
    Enemy_ResistLight,   // +% Lightning Resist
    Enemy_ResistPois,    // +% Poison Resist
    Enemy_ResistVoid,    // +% Void/Shadow Resist
    Enemy_CritResist,    // Reduced Bonus Damage from Crits

    // 3. Enemy Offense
    Enemy_ExtraDamage,   // +% Global Damage
    Enemy_Fast,          // +% Speed
    Enemy_CritChance,    // +% Critical Strike Chance
    Enemy_ArmorShred,    // Chance to shred armor

    // 4. Player Penalties
    Player_ResistRedAll,      // -% Player All Resistances
    Player_ResistRedSpecific, // -% Specific Resistance
    Player_RedRecovery,       // -% Health/Mana Regen & Leech
    Player_Fragile,           // +% Damage Taken
    Player_DodgePenalty,      // -% Player Dodge Rating

    // --- ENVIRONMENT ---
    Env_Firestorm,
    Env_Darkness,
    Env_GroundIce,
    Env_LightningStorm,

    Count // Enum size marker
};

struct MapAffix {
    MapAffixType type;
    MapAffixCategory category;
    float value;              // Computed value based on Tier
    int tier;                 // 1-10
    std::string source;       // e.g. "Glacial Fragment"

    // Persistence
    int remainingLayers = -1; // -1 = Permanent
};

// JSON Serialization
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MapAffix, type, category, value, tier, source, remainingLayers)

/**
 * @brief Aggregated view of multiple affixes of the same type
 */
struct AggregatedAffix {
    MapAffixType type;
    MapAffixCategory category;
    float totalValue;
    int maxTier;
    std::vector<std::string> sources;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AggregatedAffix, type, category, totalValue, maxTier, sources)

} // namespace NoMoreDay
