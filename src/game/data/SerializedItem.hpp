#pragma once
#include "game/components/ItemComponent.hpp"
#include "game/components/ItemStats.hpp"
#include <cstdint>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Data Transfer Object for Item Snapshot.
 * Decouples the ECS entity from the persistent storage.
 */
struct SerializedItem {
  // Identity
  uint32_t itemId = 0;     // Config ID from ItemFactory
  uint64_t instanceId = 0; // Unique ID for tracking
  std::string name;        // procedurally generated or fixed name
  ItemType type = ItemType::Material;
  uint32_t textureId = 0; // Asset ID
  int quantity = 1;

  // Stat Snapshot (The "Real" values)
  struct StatsSnapshot {
    int level = 1;
    Rarity rarity = Rarity::Common;
    float attack = 0.0f;  // Final Base Attack
    float defense = 0.0f; // Final Base Defense
    EquipmentSlot slot = EquipmentSlot::None;
    int forgingPotential = 0;
    int legendaryPotential = 0;
    float value = 0.0f; // Gold Value

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StatsSnapshot, level, rarity, attack,
                                   defense, slot, forgingPotential,
                                   legendaryPotential, value)
  };
  StatsSnapshot stats;

  // Affix Snapshot (Explicit values)
  struct SavedAffix {
    AffixType type;
    int tier;
    float value; // The exact rolled value
    bool isPrefix;
    bool isLegendary; // For merged items
    std::string name; // Cache for UI
    Tag required_tags = Tag::None;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SavedAffix, type, tier, value, isPrefix,
                                   isLegendary, name, required_tags)
  };

  std::vector<SavedAffix> affixes;
  std::vector<SavedAffix> implicits;

  // Recursive Sockets
  std::vector<SerializedItem> socketedItems;
};

// nlohmann::json support for SerializedItem
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SerializedItem, itemId, instanceId, name,
                                   type, textureId, quantity, stats, affixes,
                                   implicits, socketedItems)

} // namespace NoMoreDay
