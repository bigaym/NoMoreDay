#include "game/systems/item/RunewordSystem.hpp"
#include "core/logging/Logger.hpp"
#include <algorithm>
#include <fstream>

namespace NoMoreDay {

std::unordered_map<uint32_t, RuneDefinition> RunewordSystem::s_runes;
std::unordered_map<uint32_t, RunewordDefinition> RunewordSystem::s_runewords;

// Helper to parse strings to ItemType
static ItemType parseItemType(const std::string &typeStr) {
  static const std::unordered_map<std::string, ItemType> kStringToType = {
      {"Armor", ItemType::Armor},
      {"Weapon", ItemType::Weapon},
      {"Shield", ItemType::Shield},
      {"Axe", ItemType::Weapon},
      {"Sword", ItemType::Weapon},
      {"Mace", ItemType::Weapon},
      {"Staff", ItemType::Weapon}
  };

  auto it = kStringToType.find(typeStr);
  if (it != kStringToType.end()) return it->second;
  return ItemType::Material;
}

void RunewordSystem::initialize() {
  loadRunes("assets/data/runes.json");
  loadRunewords("assets/data/runewords.json");
}

void RunewordSystem::loadRunes(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("RunewordSystem: Failed to open runes file: {}", path);
    return;
  }

  try {
    nlohmann::json j;
    file >> j;
    auto runeList = j.at("runes");
    for (const auto &rJson : runeList) {
      RuneDefinition rune;
      rune.id = rJson.at("id").get<uint32_t>();
      rune.name = rJson.at("name").get<std::string>();
      rune.tier = rJson.at("tier").get<int>();
      rune.levelReq = rJson.at("level_req").get<int>();
      // Manual parsing or implicit if AffixStats defined from_json
      if (rJson.contains("weapon_mod"))
        rJson.at("weapon_mod").get_to(rune.weaponMod);
      if (rJson.contains("armor_mod"))
        rJson.at("armor_mod").get_to(rune.armorMod);
      if (rJson.contains("shield_mod"))
        rJson.at("shield_mod").get_to(rune.shieldMod);

      s_runes[rune.id] = rune;
    }
    LOG_INFO("RunewordSystem: Loaded {} runes.", s_runes.size());
  } catch (const std::exception &e) {
    LOG_ERROR("RunewordSystem: Error parsing runes file: {}", e.what());
  }
}

void RunewordSystem::loadRunewords(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("RunewordSystem: Failed to open runewords file: {}", path);
    return;
  }

  try {
    nlohmann::json j;
    file >> j;
    auto list = j.at("runewords");
    for (const auto &rwJson : list) {
      RunewordDefinition rw;
      rw.id = rwJson.at("id").get<uint32_t>();
      rw.name = rwJson.at("name").get<std::string>();
      rw.runes = rwJson.at("runes").get<std::vector<std::string>>();

      std::vector<std::string> types =
          rwJson.at("allowed_types").get<std::vector<std::string>>();
      for (const auto &t : types) {
        rw.allowedTypes.push_back(parseItemType(t));
      }

      if (rwJson.contains("stats"))
        rwJson.at("stats").get_to(rw.stats);

      s_runewords[rw.id] = rw;
    }
    LOG_INFO("RunewordSystem: Loaded {} runewords.", s_runewords.size());
  } catch (const std::exception &e) {
    LOG_ERROR("RunewordSystem: Error parsing runewords file: {}", e.what());
  }
}

const RuneDefinition *RunewordSystem::getRune(uint32_t id) {
  auto it = s_runes.find(id);
  if (it != s_runes.end())
    return &it->second;
  return nullptr;
}

const RuneDefinition *RunewordSystem::getRuneByName(const std::string &name) {
  for (const auto &[id, rune] : s_runes) {
    if (rune.name == name)
      return &rune;
  }
  return nullptr;
}

bool RunewordSystem::isRune(uint32_t itemId) {
  return s_runes.find(itemId) != s_runes.end();
}

uint32_t
RunewordSystem::checkForRuneword(const ItemComponent &item,
                                 const std::vector<entt::entity> &socketedRunes,
                                 entt::registry &registry) {
  // Basic validation: socket count, type
  if (socketedRunes.empty())
    return 0;

  // Convert entity list to Rune Names
  std::vector<std::string> socketedNames;
  for (auto runeEntity : socketedRunes) {
    if (!registry.valid(runeEntity))
      return 0;
    const auto &runeItem = registry.get<ItemComponent>(runeEntity);
    // Assuming runeItem.id corresponds to Rune ID
    const auto *runeDef = getRune(runeItem.id);
    if (!runeDef)
      return 0; // Not a rune
    socketedNames.push_back(runeDef->name);
  }

  // Check against all runewords
  for (const auto &[id, word] : s_runewords) {
    // Check rune sequence
    if (word.runes.size() != socketedNames.size())
      continue;
    if (word.runes != socketedNames)
      continue;

    // Check Allowed Type
    bool typeMatch = false;
    for (auto allowed : word.allowedTypes) {
      if (allowed == item.type) {
        typeMatch = true;
        break;
      }
      // TODO: Granular checks (e.g. "Sword" vs "Mace") would go here using item
      // name or tags
    }
    if (typeMatch)
      return id;
  }

  return 0;
}

// Helper to map string keys to AffixType
static AffixType stringToAffixType(const std::string &key) {
  static const std::unordered_map<std::string, AffixType> kStringToAffixType = {
      // Attributes
      {"strength", AffixType::Strength},
      {"dexterity", AffixType::Dexterity},
      {"intelligence", AffixType::Intelligence},
      {"vitality", AffixType::Vitality},
      {"all_attributes", AffixType::AllAttributes},

      // Stats
      {"cast_speed", AffixType::CastSpeed},
      {"fcr", AffixType::CastSpeed},
      {"attack_speed", AffixType::AttackSpeed},
      {"ias", AffixType::AttackSpeed},
      {"move_speed", AffixType::MoveSpeed},
      {"run_speed", AffixType::MoveSpeed},
      {"frw", AffixType::MoveSpeed},
      {"crit_chance", AffixType::CritChance},
      {"crit_damage", AffixType::CritDamage},

      // Defense
      {"defense", AffixType::FlatArmor},
      {"armor", AffixType::FlatArmor},
      {"res_all", AffixType::ResistAll},
      {"all_res", AffixType::ResistAll},
      {"res_fire", AffixType::ResistFire},
      {"fire_res", AffixType::ResistFire},
      {"res_cold", AffixType::ResistCold},
      {"cold_res", AffixType::ResistCold},
      {"res_light", AffixType::ResistLightning},
      {"res_lightning", AffixType::ResistLightning},
      {"lightning_res", AffixType::ResistLightning},
      {"res_poison", AffixType::ResistPoison},
      {"poison_res", AffixType::ResistPoison},

      // Recovery
      {"mana_regen", AffixType::PercentManaRegen},
      {"life_regen", AffixType::HealthRegen},

      // Others
      {"fhr", AffixType::CooldownReduction},
      {"all_skills", AffixType::PlusAllSkills},
      {"magic_damage_reduced", AffixType::DamageReduction},
      {"mdr", AffixType::DamageReduction}
  };

  auto it = kStringToAffixType.find(key);
  if (it != kStringToAffixType.end()) return it->second;

  return AffixType::Count;
}

void RunewordSystem::applyRuneword(ItemComponent &item, uint32_t runewordId) {
  auto it = s_runewords.find(runewordId);
  if (it == s_runewords.end())
    return;

  const auto &word = it->second;

  // Change Name
  item.name = word.name;

  // Change Rarity
  item.rarity = Rarity::Legendary;
  item.activeRunewordId = runewordId;

  // Add Stats
  for (const auto &[key, val] : word.stats) {
    AffixType type = stringToAffixType(key);
    if (type != AffixType::Count) {
      Affix affix;
      affix.type = type;
      affix.value = val;
      affix.tier = 10; // Special tier for runewords
      affix.isPrefix = true;
      // affix.name = "Runeword Bonus"; // REMOVED
      item.affixes.push_back(affix);
    } else {
      LOG_WARN("RunewordSystem: Unknown stat key '{}' in runeword '{}'", key,
               word.name);
    }
  }

  LOG_INFO("Applied Runeword: {} to item.", word.name);
}

void RunewordSystem::removeRuneword(ItemComponent &item) {
  if (item.activeRunewordId == 0)
    return;

  // Remove all affixes tagged as Runeword Bonus (Tier 10)
  // std::remove_if doesn't resizing vector automatically? actually erase-remove
  // idiom.
  item.affixes.erase(std::remove_if(item.affixes.begin(), item.affixes.end(),
                                    [](const Affix &a) {
                                      return a.tier == 10;
                                    }),
                     item.affixes.end());

  item.activeRunewordId = 0;
  item.rarity =
      Rarity::Common; // Revert to base rarity (assuming base was white/grey)
  item.name =
      "Socketed Item"; // Placeholder, strictly we lose the original name
}

} // namespace NoMoreDay
