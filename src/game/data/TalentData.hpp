#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include "game/components/Stats.hpp"
#include <nlohmann/json.hpp>

namespace NoMoreDay {

enum class StarNodeType : uint8_t {
    Minor = 0,
    Major,
    Keystone
};

enum class AstrolabeEffectType : uint8_t {
    GrantComponent = 0,
    ModifyIntent = 1,
    ModifyStat = 2,
    SpecialBehavior = 3
};

enum class TraitID : uint16_t {
    None = 0,
    SwordHeart = 100,
    SwordIntentUnlock = 101,
    MaxSwordIntent = 200,
    SwordIntentGain = 201,
    SwordIntentGrace = 202
};

struct AstrolabeNodeEffect {
    AstrolabeEffectType type;
    TraitID trait_id = TraitID::None;
    std::string value; // e.g., "SwordHeart"
};

struct StarNode {
    uint32_t id = 0;
    std::string name_key;
    std::string desc_key;
    uint32_t constellation_id = 0;
    StarNodeType type = StarNodeType::Minor;
    
    float x = 0.0f;
    float y = 0.0f;
    
    std::vector<uint32_t> prerequisites;
    std::vector<uint32_t> connections;
    std::vector<StatModifier> modifiers;
    std::vector<DamageModifier> damage_modifiers;
    std::vector<StatConversion> conversions;
    std::vector<AstrolabeNodeEffect> effects;
    
    std::string icon_id;
};

struct Constellation {
    uint32_t id = 0;
    std::string name_key;
    std::string bg_icon;
    std::vector<uint32_t> star_ids;
};

struct AstrolabeMap {
    std::vector<Constellation> constellations;
    std::unordered_map<uint32_t, StarNode> stars;
};

// JSON Serialization helpers

inline void to_json(nlohmann::json& j, const StarNodeType& t) {
    switch (t) {
        case StarNodeType::Minor: j = "Minor"; break;
        case StarNodeType::Major: j = "Major"; break;
        case StarNodeType::Keystone: j = "Keystone"; break;
    }
}

inline void from_json(const nlohmann::json& j, StarNodeType& t) {
    if (j.is_number()) {
        t = static_cast<StarNodeType>(j.get<uint8_t>());
        return;
    }
    std::string s = j.get<std::string>();
    if (s == "Minor") t = StarNodeType::Minor;
    else if (s == "Major") t = StarNodeType::Major;
    else if (s == "Keystone") t = StarNodeType::Keystone;
}

inline void to_json(nlohmann::json& j, const AstrolabeEffectType& t) { j = static_cast<uint8_t>(t); }
inline void from_json(const nlohmann::json& j, AstrolabeEffectType& t) { t = static_cast<AstrolabeEffectType>(j.get<uint8_t>()); }

inline void to_json(nlohmann::json& j, const TraitID& t) { j = static_cast<uint16_t>(t); }
inline void from_json(const nlohmann::json& j, TraitID& t) { t = static_cast<TraitID>(j.get<uint16_t>()); }

inline void to_json(nlohmann::json& j, const AstrolabeNodeEffect& e) {
    j = nlohmann::json{{"type", e.type}, {"trait_id", e.trait_id}, {"value", e.value}};
}
inline void from_json(const nlohmann::json& j, AstrolabeNodeEffect& e) {
    j.at("type").get_to(e.type);
    if (j.contains("trait_id")) j.at("trait_id").get_to(e.trait_id);
    j.at("value").get_to(e.value);
}

inline void to_json(nlohmann::json& j, const StarNode& n) {
    j = nlohmann::json{
        {"id", n.id}, {"name_key", n.name_key}, {"desc_key", n.desc_key},
        {"constellation_id", n.constellation_id}, {"type", n.type},
        {"x", n.x}, {"y", n.y}, {"prerequisites", n.prerequisites},
        {"connections", n.connections}, {"modifiers", n.modifiers},
        {"damage_modifiers", n.damage_modifiers}, {"conversions", n.conversions},
        {"effects", n.effects}, {"icon_id", n.icon_id}
    };
}

inline void from_json(const nlohmann::json& j, StarNode& n) {
    j.at("id").get_to(n.id);
    if (j.contains("name_key")) j.at("name_key").get_to(n.name_key);
    if (j.contains("desc_key")) j.at("desc_key").get_to(n.desc_key);
    if (j.contains("constellation_id")) j.at("constellation_id").get_to(n.constellation_id);
    if (j.contains("type")) j.at("type").get_to(n.type);
    if (j.contains("x")) j.at("x").get_to(n.x);
    if (j.contains("y")) j.at("y").get_to(n.y);
    if (j.contains("prerequisites")) j.at("prerequisites").get_to(n.prerequisites);
    if (j.contains("connections")) j.at("connections").get_to(n.connections);
    if (j.contains("modifiers")) j.at("modifiers").get_to(n.modifiers);
    if (j.contains("damage_modifiers")) j.at("damage_modifiers").get_to(n.damage_modifiers);
    if (j.contains("conversions")) j.at("conversions").get_to(n.conversions);
    if (j.contains("effects")) j.at("effects").get_to(n.effects);
    if (j.contains("icon_id")) j.at("icon_id").get_to(n.icon_id);
}

inline void to_json(nlohmann::json& j, const Constellation& c) {
    j = nlohmann::json{
        {"id", c.id}, {"name_key", c.name_key}, {"bg_icon", c.bg_icon}, {"star_ids", c.star_ids}
    };
}

inline void from_json(const nlohmann::json& j, Constellation& c) {
    j.at("id").get_to(c.id);
    j.at("name_key").get_to(c.name_key);
    j.at("bg_icon").get_to(c.bg_icon);
    j.at("star_ids").get_to(c.star_ids);
}

} // namespace NoMoreDay
