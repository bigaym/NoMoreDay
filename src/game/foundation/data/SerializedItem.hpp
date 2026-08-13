#pragma once
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
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

  // Enum identity (persisted since this format; legacy saves leave defaults)
  uint32_t baseId = 0; // BaseItemDef id from ItemFactory
  WeaponSubtype weaponSubtype = WeaponSubtype::None;
  CatalystKind catalystKind = CatalystKind::None;

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
    std::vector<uint32_t> modifier_record_ids;
  };

  friend void to_json(nlohmann::json &j, const SavedAffix &a) {
    j = nlohmann::json{{"type", a.type},
                       {"tier", a.tier},
                       {"value", a.value},
                       {"isPrefix", a.isPrefix},
                       {"isLegendary", a.isLegendary},
                       {"name", a.name},
                       {"required_tags", a.required_tags}};
    if (!a.modifier_record_ids.empty()) {
      j["modifier_record_ids"] = a.modifier_record_ids;
    }
  }

  friend void from_json(const nlohmann::json &j, SavedAffix &a) {
    j.at("type").get_to(a.type);
    j.at("tier").get_to(a.tier);
    j.at("value").get_to(a.value);
    j.at("isPrefix").get_to(a.isPrefix);
    j.at("isLegendary").get_to(a.isLegendary);
    if (j.contains("name")) {
      j.at("name").get_to(a.name);
    } else {
      a.name.clear();
    }
    if (j.contains("required_tags")) {
      j.at("required_tags").get_to(a.required_tags);
    } else {
      a.required_tags = Tag::None;
    }
    if (j.contains("modifier_record_ids")) {
      j.at("modifier_record_ids").get_to(a.modifier_record_ids);
    } else {
      a.modifier_record_ids.clear();
    }
  }

  std::vector<SavedAffix> affixes;
  std::vector<SavedAffix> implicits;

  // Recursive Sockets
  std::vector<SerializedItem> socketedItems;
};

// nlohmann::json support for SerializedItem. Explicit overloads (instead of
// the macro) so the newer enum-identity keys stay optional on read: legacy
// saves without them load with defaults instead of throwing type errors.
inline void to_json(nlohmann::json& j, const SerializedItem& item) {
  j = nlohmann::json{{"itemId", item.itemId},
                     {"instanceId", item.instanceId},
                     {"name", item.name},
                     {"type", item.type},
                     {"textureId", item.textureId},
                     {"quantity", item.quantity},
                     {"stats", item.stats},
                     {"affixes", item.affixes},
                     {"implicits", item.implicits},
                     {"socketedItems", item.socketedItems},
                     {"baseId", item.baseId},
                     {"weaponSubtype", item.weaponSubtype},
                     {"catalystKind", item.catalystKind}};
}

inline void from_json(const nlohmann::json& j, SerializedItem& item) {
  j.at("itemId").get_to(item.itemId);
  j.at("instanceId").get_to(item.instanceId);
  j.at("name").get_to(item.name);
  j.at("type").get_to(item.type);
  j.at("textureId").get_to(item.textureId);
  j.at("quantity").get_to(item.quantity);
  j.at("stats").get_to(item.stats);
  j.at("affixes").get_to(item.affixes);
  j.at("implicits").get_to(item.implicits);
  j.at("socketedItems").get_to(item.socketedItems);
  if (j.contains("baseId")) j.at("baseId").get_to(item.baseId);
  if (j.contains("weaponSubtype")) j.at("weaponSubtype").get_to(item.weaponSubtype);
  if (j.contains("catalystKind")) j.at("catalystKind").get_to(item.catalystKind);
}

} // namespace NoMoreDay
