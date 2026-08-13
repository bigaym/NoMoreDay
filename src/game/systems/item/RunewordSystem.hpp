#pragma once
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include <entt/entt.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map> // Keep this include for s_runes and s_runewords
#include <vector>

namespace NoMoreDay {

// Simple map for stats: "stat_name" -> value
using AffixStats = std::map<std::string, float>;

struct RuneDefinition {
  uint32_t id;
  std::string name;
  int tier;
  AffixStats weaponMod;
  AffixStats armorMod;
  AffixStats shieldMod;
  int levelReq;
};

// JSON serialization for RuneDefinition
inline void from_json(const nlohmann::json &j, RuneDefinition &r) {
  j.at("id").get_to(r.id);
  j.at("name").get_to(r.name);
  j.at("tier").get_to(r.tier);
  if (j.contains("weapon_mod"))
    r.weaponMod = j.at("weapon_mod").get<AffixStats>();
  if (j.contains("armor_mod"))
    r.armorMod = j.at("armor_mod").get<AffixStats>();
  if (j.contains("shield_mod"))
    r.shieldMod = j.at("shield_mod").get<AffixStats>();
  j.at("level_req").get_to(r.levelReq);
}

struct RunewordDefinition {
  uint32_t id;
  std::string name;
  std::vector<uint32_t> runeIds; // 符文ID序列 (按 s_runes 的 id, 取代名字字符串)
  std::vector<ItemType> allowedTypes;
  std::vector<WeaponSubtype> allowedSubtypes; // e.g. [Sword, Axe]
  AffixStats stats;
};

class RunewordSystem {
public:
  static void initialize();

  // Returns the RuneDefinition if found, otherwise nullptr
  static const RuneDefinition *getRune(uint32_t id);

  // Checks if the item forms a runeword. Returns the Runeword ID or 0.
  static uint32_t
  checkForRuneword(const ItemComponent &item,
                   const std::vector<entt::entity> &socketedRunes,
                   entt::registry &registry);

  // Applies runeword properties to an item
  static void applyRuneword(ItemComponent &item, uint32_t runewordId);

  // Removes runeword properties (keeping rune stats if desired, but usually
  // this completely resets specialized stats)
  static void removeRuneword(ItemComponent &item);

  // Helper to determine if an ID is a Rune
  static bool isRune(uint32_t itemId);

private:
  static std::unordered_map<uint32_t, RuneDefinition> s_runes;
  static std::unordered_map<uint32_t, RunewordDefinition> s_runewords;

  static void loadRunes(const std::string &path);
  static void loadRunewords(const std::string &path);
};

} // namespace NoMoreDay
