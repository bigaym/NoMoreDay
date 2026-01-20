#pragma once
#include "game/components/Common.hpp"
#include "game/components/Progression.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/PlayerCombatHistory.hpp"
#include "game/data/SerializedItem.hpp"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Metadata for the save file, used for the Load Game menu.
 */
struct SaveHeader {
  std::string name;
  std::string characterClass;
  int level = 1;
  double playtime = 0.0;
  int64_t timestamp = 0;
  uint32_t version = 1;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(SaveHeader, name, characterClass, level,
                                 playtime, timestamp, version)
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

  // Progression Systems
  ActiveSkillsComponent skills;
  AstrolabeComponent astrolabe;
  PlayerCombatHistory combatHistory;

  // Note: QuestState and other world variables should be added here in the
  // future.
};

// JSON serialization for the root DTO
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CharacterSaveData, header, primaryStats,
                                   position, mapId, gold, inventory, equipment,
                                   skills, astrolabe, combatHistory)

} // namespace NoMoreDay
