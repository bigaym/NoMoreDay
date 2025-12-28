#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "ItemStats.hpp"

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
