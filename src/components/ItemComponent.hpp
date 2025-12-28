#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "ItemStats.hpp"

namespace NoMoreDay {

enum class ItemType {
    Weapon,
    Armor,
    Consumable,
    Material,
    Quest
};

enum class EquipmentSlot {
    None = 0,
    MainHand,
    OffHand,
    Head,
    Shoulder,
    Chest,
    Hands,
    Legs,
    Feet,
    Neck,
    Ring1,
    Ring2,
    Count // For array sizing
};

enum class Rarity {
    Common,
    Magic,
    Rare,
    Uncommon,
    Set,
    Epic,
    Legendary,
    Mythic
};

// Component that marks an entity as an Item
struct ItemComponent {
    uint32_t id;          // Database/Config ID
    std::string name;
    ItemType type;
    EquipmentSlot slot;
    Rarity rarity;
    
    int quantity = 1;     // Current stack size
    int maxStack = 1;     // Max stack size
    float value = 0.0f;   // Gold value
    
    // Base Stats
    float attack = 0.0f;  // Base Weapon Damage
    float defense = 0.0f; // Base Armor Defense

    // --- Crafting & Stats ---
    int forgingPotential = 0; // Consumed when crafting
    int legendaryPotential = 0; // For Uniques
    
    // Implicit Stats (Base stats intrinsic to the item base type)
    // e.g. A "Plate Armor" always has Armor, a "Wand" always has Spell Damage.
    std::vector<NoMoreDay::Affix> implicits;

    // Explicit Affixes (Rolled or Crafted)
    std::vector<NoMoreDay::Affix> affixes;
    
    // Sockets (future)
    // std::vector<entt::entity> sockets; 
    
    // Description
    std::string description;
};

// --- Loot & Drop System Components ---

enum class LootEntryType {
    Item,
    Gold,
    SubTable
};

struct LootEntry {
    LootEntryType type;
    uint32_t id;      // Item ID (Base Item Type)
    uint32_t minAmount = 1;
    uint32_t maxAmount = 1;
    float weight = 1.0f;
};

/**
 * @brief A collection of possible drops with associated weights.
 * Not usually a component, but a resource managed by AssetRegistry/ItemFactory.
 */
struct LootPool {
    std::string name;
    std::vector<LootEntry> entries;
    float totalWeight = 0.0f;
};

/**
 * @brief Component attached to enemies to define what they drop.
 */
struct DropTableComponent {
    uint32_t poolId = 0;     // ID for the specific LootPool (0 = Global)
    float dropChance = 1.0f; // Chance to drop any loot (0.0 to 1.0)
    int minRolls = 1;        // Minimum number of times to roll on the pool
    int maxRolls = 1;        // Maximum number of times to roll on the pool
};

} // namespace NoMoreDay
