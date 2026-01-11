#include "game/systems/item/LootFilter.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace NoMoreDay {

LootFilterProfile LootFilter::s_currentProfile;

// --- Helper Functions for Enum <-> String ---

static Rarity stringToRarity(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    if (s == "COMMON") return Rarity::Common;
    if (s == "MAGIC") return Rarity::Magic;
    if (s == "RARE") return Rarity::Rare;
    if (s == "UNCOMMON") return Rarity::Uncommon;
    if (s == "SET") return Rarity::Set;
    if (s == "EPIC") return Rarity::Epic;
    if (s == "LEGENDARY") return Rarity::Legendary;
    if (s == "MYTHIC") return Rarity::Mythic;
    return Rarity::Common;
}

static ItemType stringToItemType(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    if (s == "WEAPON") return ItemType::Weapon;
    if (s == "ARMOR") return ItemType::Armor;
    if (s == "SHIELD") return ItemType::Shield;
    if (s == "CONSUMABLE") return ItemType::Consumable;
    if (s == "MATERIAL") return ItemType::Material;
    if (s == "QUEST") return ItemType::Quest;
    if (s == "BAG") return ItemType::Bag;
    return ItemType::Material; 
}

static FilterActionType stringToActionType(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    if (s == "SHOW") return FilterActionType::SHOW;
    if (s == "HIDE") return FilterActionType::HIDE;
    if (s == "EMPHASIZE") return FilterActionType::EMPHASIZE;
    return FilterActionType::SHOW;
}

// --- JSON Serialization ---

static void parseCondition(const nlohmann::json& j, FilterCondition& c) {
    if (j.contains("min_rarity")) c.minRarity = stringToRarity(j.at("min_rarity").get<std::string>());
    if (j.contains("max_rarity")) c.maxRarity = stringToRarity(j.at("max_rarity").get<std::string>());
    if (j.contains("min_level")) c.minLevel = j.at("min_level").get<int>();
    if (j.contains("max_level")) c.maxLevel = j.at("max_level").get<int>();
    if (j.contains("item_type")) c.itemType = stringToItemType(j.at("item_type").get<std::string>());
    if (j.contains("has_affixes")) c.hasAffixes = j.at("has_affixes").get<std::vector<std::string>>();
    if (j.contains("base_name")) c.baseName = j.at("base_name").get<std::string>();
}

static void parseAction(const nlohmann::json& j, FilterAction& a) {
    if (j.contains("type")) a.type = stringToActionType(j.at("type").get<std::string>());
    if (j.contains("color")) {
        auto c = j.at("color");
        if (c.is_array() && c.size() >= 3) {
            Color color;
            color.r = c[0];
            color.g = c[1];
            color.b = c[2];
            color.a = (c.size() > 3) ? c[3].get<unsigned char>() : 255;
            a.colorOverride = color;
        }
    }
    if (j.contains("scale")) a.scale = j.at("scale").get<float>();
    if (j.contains("minimap_icon")) a.minimapIcon = j.at("minimap_icon").get<bool>();
}

static void parseRule(const nlohmann::json& j, FilterRule& r) {
    if (j.contains("name")) r.name = j.at("name").get<std::string>();
    if (j.contains("enabled")) r.enabled = j.at("enabled").get<bool>();
    if (j.contains("conditions")) parseCondition(j.at("conditions"), r.condition);
    
    // Parse Action Type
    if (j.contains("action")) {
        std::string actStr = j.at("action").get<std::string>();
        r.action.type = stringToActionType(actStr);
    }
    
    // Parse Action Details from rule root (merging)
    if (j.contains("color")) {
        auto c = j.at("color");
        if (c.is_array() && c.size() >= 3) {
            Color color;
            color.r = c[0];
            color.g = c[1];
            color.b = c[2];
            color.a = (c.size() > 3) ? c[3].get<unsigned char>() : 255;
            r.action.colorOverride = color;
        }
    }
    if (j.contains("scale")) r.action.scale = j.at("scale").get<float>();
    if (j.contains("minimap_icon")) r.action.minimapIcon = j.at("minimap_icon").get<bool>();
}

void LootFilter::from_json(const nlohmann::json& j, LootFilterProfile& p) {
    if (j.contains("name")) p.name = j.at("name").get<std::string>();
    if (j.contains("description")) p.description = j.at("description").get<std::string>();
    if (j.contains("rules")) {
        for (const auto& r : j.at("rules")) {
            FilterRule rule;
            parseRule(r, rule);
            p.rules.push_back(rule);
        }
    }
}

// --- Implementation ---

void LootFilter::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to load loot filter: {}", path);
        return;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        s_currentProfile = LootFilterProfile(); // Reset
        from_json(j, s_currentProfile);
        LOG_INFO("Loaded loot filter: {} ({} rules)", s_currentProfile.name, s_currentProfile.rules.size());
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing loot filter JSON: {}", e.what());
    }
}

bool FilterRule::matches(const ItemComponent& item, int itemLevel) const {
    if (!enabled) return false;

    // Rarity
    if (condition.minRarity.has_value() && item.rarity < condition.minRarity.value()) return false;
    if (condition.maxRarity.has_value() && item.rarity > condition.maxRarity.value()) return false;

    // Level
    if (condition.minLevel.has_value() && itemLevel < condition.minLevel.value()) return false;
    if (condition.maxLevel.has_value() && itemLevel > condition.maxLevel.value()) return false;

    // Type
    if (condition.itemType.has_value() && item.type != condition.itemType.value()) return false;

    // Base Name (Substr match)
    if (condition.baseName.has_value()) {
        if (item.name.find(condition.baseName.value()) == std::string::npos) return false;
    }

    // Affixes
    if (!condition.hasAffixes.empty()) {
        for (const auto& requiredAffix : condition.hasAffixes) {
            bool found = false;
            // Check implicits
            for (const auto& aff : item.implicits) {
                if (aff.name.find(requiredAffix) != std::string::npos) {
                    found = true; 
                    break;
                }
            }
            if (found) continue;

            // Check explicit affixes
            for (const auto& aff : item.affixes) {
                 if (aff.name.find(requiredAffix) != std::string::npos) {
                    found = true; 
                    break;
                }
            }
            if (!found) return false; // Missing one required affix
        }
    }

    return true;
}

FilterAction LootFilter::evaluate(const ItemComponent& item, int itemLevel) {
    // Default action if no rule matches: SHOW
    FilterAction result; 
    result.type = FilterActionType::SHOW;

    for (const auto& rule : s_currentProfile.rules) {
        if (rule.matches(item, itemLevel)) {
            return rule.action;
        }
    }

    return result;
}

// 实现 to_json 以支持保存过滤器配置
void LootFilter::to_json(nlohmann::json& j, const LootFilterProfile& p) {
    j["name"] = p.name;
    j["description"] = p.description;
    j["rules"] = nlohmann::json::array();
    
    for (const auto& rule : p.rules) {
        nlohmann::json rj;
        rj["name"] = rule.name;
        rj["enabled"] = rule.enabled;
        
        // Conditions
        nlohmann::json cj;
        if (rule.condition.minRarity.has_value()) {
            cj["min_rarity"] = static_cast<int>(rule.condition.minRarity.value());
        }
        if (rule.condition.maxRarity.has_value()) {
            cj["max_rarity"] = static_cast<int>(rule.condition.maxRarity.value());
        }
        if (rule.condition.minLevel.has_value()) {
            cj["min_level"] = rule.condition.minLevel.value();
        }
        if (rule.condition.maxLevel.has_value()) {
            cj["max_level"] = rule.condition.maxLevel.value();
        }
        if (rule.condition.itemType.has_value()) {
            cj["item_type"] = static_cast<int>(rule.condition.itemType.value());
        }
        if (!rule.condition.hasAffixes.empty()) {
            cj["has_affixes"] = rule.condition.hasAffixes;
        }
        if (rule.condition.baseName.has_value()) {
            cj["base_name"] = rule.condition.baseName.value();
        }
        rj["conditions"] = cj;
        
        // Action
        rj["action"] = static_cast<int>(rule.action.type);
        if (rule.action.colorOverride.has_value()) {
            const auto& c = rule.action.colorOverride.value();
            rj["color"] = { c.r, c.g, c.b, c.a };
        }
        rj["scale"] = rule.action.scale;
        rj["minimap_icon"] = rule.action.minimapIcon;
        
        j["rules"].push_back(rj);
    }
}

} // namespace NoMoreDay
