#include "game/systems/item/RunewordSystem.hpp"
#include "core/logging/Logger.hpp"
#include <algorithm>
#include <fstream>

namespace NoMoreDay {

std::unordered_map<uint32_t, RuneDefinition> RunewordSystem::s_runes;
std::unordered_map<uint32_t, RunewordDefinition> RunewordSystem::s_runewords;

// Helper to parse strings to ItemType
static ItemType parseItemType(const std::string &typeStr) {
  if (typeStr == "Armor")
    return ItemType::Armor;
  if (typeStr == "Weapon")
    return ItemType::Weapon;
  if (typeStr == "Shield")
    return ItemType::Shield;
  // Add more granular checks (Axe, Sword etc) if we distinguish them in
  // ItemType enum or just treat all weapons as Weapon For now, map specific
  // weapon types to ItemType::Weapon, but we might need SubType check logic
  // later. The simplified version treats "Axe", "Sword" etc as Weapon. The
  // Runeword check logic will need to check ItemComponent properties (e.g.
  // isTwoHanded, or name/tag) for specific base types.
  if (typeStr == "Axe" || typeStr == "Sword" || typeStr == "Mace" ||
      typeStr == "Staff")
    return ItemType::Weapon;
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
  // Attributes
  if (key == "strength")
    return AffixType::Strength;
  if (key == "dexterity")
    return AffixType::Dexterity;
  if (key == "intelligence")
    return AffixType::Intelligence;
  if (key == "vitality")
    return AffixType::Vitality;
  if (key == "all_attributes")
    return AffixType::AllAttributes;

  // Stats
  if (key == "cast_speed" || key == "fcr")
    return AffixType::CastSpeed;
  if (key == "attack_speed" || key == "ias")
    return AffixType::AttackSpeed;
  if (key == "move_speed" || key == "run_speed" || key == "frw")
    return AffixType::MoveSpeed;
  if (key == "crit_chance")
    return AffixType::CritChance;
  if (key == "crit_damage")
    return AffixType::CritDamage;

  // Defense
  if (key == "defense" || key == "armor")
    return AffixType::FlatArmor;
  if (key == "res_all" || key == "all_res")
    return AffixType::ResistAll;
  if (key == "res_fire" || key == "fire_res")
    return AffixType::ResistFire;
  if (key == "res_cold" || key == "cold_res")
    return AffixType::ResistCold;
  if (key == "res_light" || key == "res_lightning" || key == "lightning_res")
    return AffixType::ResistLightning;
  if (key == "res_poison" || key == "poison_res")
    return AffixType::ResistPoison;

  // Recovery
  if (key == "mana_regen")
    return AffixType::PercentManaRegen; // Usually D2 is percent
  if (key == "life_regen")
    return AffixType::HealthRegen;

  // Others
  if (key == "fhr")
    return AffixType::CooldownReduction; // Approx mapping for now
  if (key == "all_skills")
    return AffixType::PlusAllSkills;
  if (key == "magic_damage_reduced" || key == "mdr")
    return AffixType::DamageReduction; // Treated as % DR for now

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
