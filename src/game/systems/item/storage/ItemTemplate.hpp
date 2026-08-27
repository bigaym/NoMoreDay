#pragma once

#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include <cstdint>
#include <entt/entt.hpp>
#include <string>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Categorical item kind classification for storage and templates.
 */
enum class ItemKind : uint8_t {
  None = 0,
  Weapon,
  Armor,
  Shield,
  Jewelry,
  Consumable,
  Material,
  Quest,
  Bag
};

/**
 * @brief Immutable template defining static item characteristics mapped from baseId.
 */
struct ItemTemplate {
  uint32_t baseId = 0;                              // Global unique base ID
  ItemKind kind = ItemKind::None;                   // High-level categorical kind
  ItemType type = ItemType::Material;               // Item type enumeration
  EquipmentSlot slot = EquipmentSlot::None;         // Equipment slot (if equippable)
  WeaponSubtype weaponSubtype = WeaponSubtype::None; // Weapon subtype (if weapon)
  CatalystKind catalystKind = CatalystKind::None;   // Catalyst kind (if catalyst)
  bool isTwoHanded = false;                         // Whether the weapon requires two hands
  std::string name;                                 // Base / default display name
  std::string description;                          // Static flavor / usage description
  int minLevel = 1;                                 // Minimum required / drop level
  int maxStack = 1;                                 // Maximum stack count (1 for non-stackables)
  int bagCapacity = 0;                              // Inventory bag capacity (if bag)
  float baseStatMin = 0.0f;                         // Minimum base attack / defense roll
  float baseStatMax = 0.0f;                         // Maximum base attack / defense roll
  AffixType implicitType = AffixType::FlatHealth;   // Default base implicit affix type
  std::string setName;                              // Set name (if set item)
  uint32_t setNameHash = 0;                         // Set name hash for fast runtime lookup
  std::vector<SetBonus> setBonuses;                 // Set bonuses (if set item)
  entt::id_type textureId = 0;                      // Default texture asset ID
};

} // namespace NoMoreDay
