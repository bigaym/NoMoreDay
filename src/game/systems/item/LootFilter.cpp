#include "game/systems/item/LootFilter.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace NoMoreDay {

LootFilterProfile LootFilter::s_currentProfile;

// --- Helper Functions for Enum <-> String ---

// Rarity 字符串解析统一走 ItemComponent.hpp 的 RarityFromString (大小写不敏感单源)

static ItemType stringToItemType(const std::string& str) {
    static const std::unordered_map<std::string, ItemType> kStringToItemType = {
        {"WEAPON", ItemType::Weapon},
        {"ARMOR", ItemType::Armor},
        {"SHIELD", ItemType::Shield},
        {"CONSUMABLE", ItemType::Consumable},
        {"MATERIAL", ItemType::Material},
        {"QUEST", ItemType::Quest},
        {"BAG", ItemType::Bag}
    };

    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    auto it = kStringToItemType.find(s);
    if (it != kStringToItemType.end()) return it->second;
    return ItemType::Material; 
}

// ItemType -> 大写字符串 (JSON 写出用, 与 stringToItemType 读端对称)
static std::string itemTypeToString(ItemType type) {
    switch (type) {
    case ItemType::Weapon: return "WEAPON";
    case ItemType::Armor: return "ARMOR";
    case ItemType::Shield: return "SHIELD";
    case ItemType::Consumable: return "CONSUMABLE";
    case ItemType::Material: return "MATERIAL";
    case ItemType::Quest: return "QUEST";
    case ItemType::Bag: return "BAG";
    }
    return "MATERIAL";
}

static FilterActionType stringToActionType(const std::string& str) {
    static const std::unordered_map<std::string, FilterActionType> kStringToFilterAction = {
        {"SHOW", FilterActionType::SHOW},
        {"HIDE", FilterActionType::HIDE},
        {"EMPHASIZE", FilterActionType::EMPHASIZE}
    };

    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    auto it = kStringToFilterAction.find(s);
    if (it != kStringToFilterAction.end()) return it->second;
    return FilterActionType::SHOW;
}

// --- JSON Serialization ---

// 读端兼容两种格式: 字符串 ("Common"/"COMMON") 或整数 (旧写端曾写 int)
static void parseCondition(const nlohmann::json& j, FilterCondition& c) {
    if (j.contains("min_rarity")) {
        const auto& v = j.at("min_rarity");
        if (v.is_string()) c.minRarity = RarityFromString(v.get<std::string>());
        else if (v.is_number_integer()) c.minRarity = static_cast<Rarity>(v.get<int>());
    }
    if (j.contains("max_rarity")) {
        const auto& v = j.at("max_rarity");
        if (v.is_string()) c.maxRarity = RarityFromString(v.get<std::string>());
        else if (v.is_number_integer()) c.maxRarity = static_cast<Rarity>(v.get<int>());
    }
    if (j.contains("min_level")) c.minLevel = j.at("min_level").get<int>();
    if (j.contains("max_level")) c.maxLevel = j.at("max_level").get<int>();
    if (j.contains("item_type")) {
        const auto& v = j.at("item_type");
        if (v.is_string()) c.itemType = stringToItemType(v.get<std::string>());
        else if (v.is_number_integer()) c.itemType = static_cast<ItemType>(v.get<int>());
    }
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
    
    // Parse Action Type (读端兼容 string 与旧 int 两种格式)
    if (j.contains("action")) {
        const auto& v = j.at("action");
        if (v.is_string()) r.action.type = stringToActionType(v.get<std::string>());
        else if (v.is_number_integer()) r.action.type = static_cast<FilterActionType>(v.get<int>());
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
                if (GetAffixDescription(aff, false).find(requiredAffix) != std::string::npos) {
                    found = true; 
                    break;
                }
            }
            if (found) continue;

            // Check explicit affixes
            for (const auto& aff : item.affixes) {
                 if (GetAffixDescription(aff, false).find(requiredAffix) != std::string::npos) {
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
        // 写端统一为字符串 (Rarity 用 RarityToString, ItemType 用大写), 读端大小写不敏感
        nlohmann::json cj;
        if (rule.condition.minRarity.has_value()) {
            cj["min_rarity"] = RarityToString(rule.condition.minRarity.value());
        }
        if (rule.condition.maxRarity.has_value()) {
            cj["max_rarity"] = RarityToString(rule.condition.maxRarity.value());
        }
        if (rule.condition.minLevel.has_value()) {
            cj["min_level"] = rule.condition.minLevel.value();
        }
        if (rule.condition.maxLevel.has_value()) {
            cj["max_level"] = rule.condition.maxLevel.value();
        }
        if (rule.condition.itemType.has_value()) {
            cj["item_type"] = itemTypeToString(rule.condition.itemType.value());
        }
        if (!rule.condition.hasAffixes.empty()) {
            cj["has_affixes"] = rule.condition.hasAffixes;
        }
        if (rule.condition.baseName.has_value()) {
            cj["base_name"] = rule.condition.baseName.value();
        }
        rj["conditions"] = cj;
        
        // Action (写端统一为字符串, 与读端 stringToActionType 对称)
        switch (rule.action.type) {
        case FilterActionType::SHOW: rj["action"] = "SHOW"; break;
        case FilterActionType::HIDE: rj["action"] = "HIDE"; break;
        case FilterActionType::EMPHASIZE: rj["action"] = "EMPHASIZE"; break;
        }
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
