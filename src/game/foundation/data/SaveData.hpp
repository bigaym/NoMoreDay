#pragma once
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillContract.hpp"
#include "game/foundation/data/PlayerCombatHistory.hpp"
#include "game/foundation/data/SerializedItem.hpp"
#include "game/foundation/data/StashData.hpp"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>

namespace NoMoreDay {

inline constexpr uint32_t CURRENT_CHARACTER_SAVE_VERSION = 4;

struct SerializedInventoryEntry {
  int slotIndex = 0;
  SerializedItem item;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SerializedInventoryEntry, slotIndex, item)

struct SerializedBagSlot {
  uint8_t index = 0;
  SerializedItem bag;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SerializedBagSlot, index, bag)

struct SerializedMaterialEntry {
  uint32_t id = 0;
  int32_t count = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SerializedMaterialEntry, id, count)

/**
 * @brief Metadata for the save file, used for the Load Game menu.
 */
struct SaveHeader {
  std::string name;
  std::string characterClass;
  int level = 1;
  int64_t playtime = 0;
  int64_t timestamp = 0;
  uint32_t version = CURRENT_CHARACTER_SAVE_VERSION;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(SaveHeader, name, characterClass, level,
                                 playtime, timestamp, version)
};

struct TriggerCooldownSaveData {
  uint32_t node_id = 0;
  float remaining = 0.0f;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(TriggerCooldownSaveData, node_id, remaining)
};

struct SkillContractRuntimeSkillSaveData {
  uint32_t skill_id = 0;
  uint32_t active_transmuter_node = 0;
  std::vector<TriggerCooldownSaveData> trigger_cooldowns;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(SkillContractRuntimeSkillSaveData, skill_id,
                                 active_transmuter_node, trigger_cooldowns)
};

struct SkillContractRuntimeSaveData {
  uint32_t version = kSkillContractRuntimeVersion;
  std::vector<SkillContractRuntimeSkillSaveData> skills;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(SkillContractRuntimeSaveData, version, skills)
};

/**
 * @brief Root DTO for character persistence.
 */
struct CharacterSaveData {
  SaveHeader header;

  // Core Components
  PrimaryStats primaryStats;
  Position position;
  std::string mapId = "Town_01";

  // Economy
  int gold = 0;

  // Item Containers
  int32_t inventoryCapacity = 40;
  std::vector<SerializedInventoryEntry> inventory;
  std::vector<SerializedItem> equipment; // Flat list of equipped items
  std::vector<SerializedBagSlot> bagSlots;
  std::vector<SerializedMaterialEntry> materialBank;
  std::optional<SerializedStash> personalStash;

  // Progression Systems
  ActiveSkillsComponent skills;
  SkillContractRuntimeSaveData skill_contract_runtime;
  AstrolabeComponent astrolabe;
  PlayerCombatHistory combatHistory;
  std::optional<BladeMasteryComponent> blade_mastery;
  std::optional<BladeResourceComponent> blade_resource;
  std::optional<BladeSignatureSkillComponent> blade_signature_skill;

  // Note: QuestState and other world variables should be added here in the
  // future.
};

// JSON serialization for the root DTO
inline void to_json(nlohmann::json& j, const CharacterSaveData& p) {
    j = nlohmann::json{
        {"header", p.header},
        {"primaryStats", p.primaryStats},
        {"position", p.position},
        {"mapId", p.mapId},
        {"gold", p.gold},
        {"inventoryCapacity", p.inventoryCapacity},
        {"inventory", p.inventory},
        {"equipment", p.equipment},
        {"bagSlots", p.bagSlots},
        {"materialBank", p.materialBank},
        {"skills", p.skills},
        {"skill_contract_runtime", p.skill_contract_runtime},
        {"astrolabe", p.astrolabe},
        {"combatHistory", p.combatHistory}
    };
    if (p.blade_mastery.has_value()) {
        j["blade_mastery"] = p.blade_mastery.value();
    }
    if (p.blade_resource.has_value()) {
        j["blade_resource"] = p.blade_resource.value();
    }
    if (p.blade_signature_skill.has_value()) {
        j["blade_signature_skill"] = p.blade_signature_skill.value();
    }
    if (p.personalStash.has_value()) {
        j["personalStash"] = p.personalStash.value();
    }
}

inline void from_json(const nlohmann::json& j, CharacterSaveData& p) {
    j.at("header").get_to(p.header);
    j.at("primaryStats").get_to(p.primaryStats);
    j.at("position").get_to(p.position);
    j.at("mapId").get_to(p.mapId);
    j.at("gold").get_to(p.gold);

    if (j.contains("inventoryCapacity")) {
        j.at("inventoryCapacity").get_to(p.inventoryCapacity);
    } else {
        p.inventoryCapacity = 40;
    }

    p.inventory.clear();
    if (j.contains("inventory")) {
        const auto& invJson = j.at("inventory");
        if (invJson.is_array()) {
            int defaultSlotIndex = 0;
            for (const auto& elem : invJson) {
                if (elem.is_object() && elem.contains("slotIndex") && elem.contains("item")) {
                    SerializedInventoryEntry entry;
                    elem.at("slotIndex").get_to(entry.slotIndex);
                    elem.at("item").get_to(entry.item);
                    p.inventory.push_back(entry);
                } else {
                    // Legacy format: elem is SerializedItem
                    SerializedInventoryEntry entry;
                    entry.slotIndex = defaultSlotIndex;
                    entry.item = elem.get<SerializedItem>();
                    p.inventory.push_back(entry);
                }
                defaultSlotIndex++;
            }
        }
    }

    j.at("equipment").get_to(p.equipment);

    if (j.contains("bagSlots")) {
        j.at("bagSlots").get_to(p.bagSlots);
    } else {
        p.bagSlots.clear();
    }

    if (j.contains("materialBank")) {
        j.at("materialBank").get_to(p.materialBank);
    } else {
        p.materialBank.clear();
    }

    j.at("skills").get_to(p.skills);
    if (j.contains("skill_contract_runtime")) {
        j.at("skill_contract_runtime").get_to(p.skill_contract_runtime);
    }
    j.at("astrolabe").get_to(p.astrolabe);
    j.at("combatHistory").get_to(p.combatHistory);
    if (j.contains("blade_mastery")) {
        p.blade_mastery = j.at("blade_mastery").get<BladeMasteryComponent>();
    }
    if (j.contains("blade_resource")) {
        p.blade_resource = j.at("blade_resource").get<BladeResourceComponent>();
    }
    if (j.contains("blade_signature_skill")) {
        p.blade_signature_skill =
            j.at("blade_signature_skill").get<BladeSignatureSkillComponent>();
    }
    
    if (j.contains("personalStash")) {
        p.personalStash = j.at("personalStash").get<SerializedStash>();
    }
}

} // namespace NoMoreDay
