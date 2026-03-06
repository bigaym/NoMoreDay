#pragma once
#include "game/components/Common.hpp"
#include "game/components/Progression.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillContract.hpp"
#include "game/data/PlayerCombatHistory.hpp"
#include "game/data/SerializedItem.hpp"
#include "game/data/StashData.hpp"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>

namespace NoMoreDay {

inline constexpr uint32_t CURRENT_CHARACTER_SAVE_VERSION = 2;

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
  std::vector<SerializedItem> inventory;
  std::vector<SerializedItem> equipment; // Flat list of equipped items
  std::optional<SerializedStash> personalStash;

  // Progression Systems
  ActiveSkillsComponent skills;
  SkillContractRuntimeSaveData skill_contract_runtime;
  AstrolabeComponent astrolabe;
  PlayerCombatHistory combatHistory;

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
        {"inventory", p.inventory},
        {"equipment", p.equipment},
        {"skills", p.skills},
        {"skill_contract_runtime", p.skill_contract_runtime},
        {"astrolabe", p.astrolabe},
        {"combatHistory", p.combatHistory}
    };
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
    j.at("inventory").get_to(p.inventory);
    j.at("equipment").get_to(p.equipment);
    j.at("skills").get_to(p.skills);
    if (j.contains("skill_contract_runtime")) {
        j.at("skill_contract_runtime").get_to(p.skill_contract_runtime);
    }
    j.at("astrolabe").get_to(p.astrolabe);
    j.at("combatHistory").get_to(p.combatHistory);
    
    if (j.contains("personalStash")) {
        p.personalStash = j.at("personalStash").get<SerializedStash>();
    }
}

} // namespace NoMoreDay
