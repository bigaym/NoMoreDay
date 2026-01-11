#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <set>
#include "game/components/Stats.hpp"
#include "game/components/SkillDefs.hpp"
#include <nlohmann/json.hpp>

namespace NoMoreDay {

    enum class AstrolabeNodeType : uint8_t {
        Minor = 0,
        Major,
        Keystone
    };

    enum class AstrolabeEffectType : uint8_t {
        GrantComponent
    };

    struct AstrolabeNodeEffect {
        AstrolabeEffectType type;
        std::string value; // e.g., "SwordHeart"
    };

    // For JSON parsing
    inline void to_json(nlohmann::json& j, const AstrolabeEffectType& t) {
        j = static_cast<uint8_t>(t);
    }
    inline void from_json(const nlohmann::json& j, AstrolabeEffectType& t) {
        t = static_cast<AstrolabeEffectType>(j.get<uint8_t>());
    }

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AstrolabeNodeEffect, type, value)

    struct AstrolabeNode {
        uint32_t id = 0;
        std::string name_key;
        std::string desc_key;
        AstrolabeNodeType type = AstrolabeNodeType::Minor;
        
        std::vector<uint32_t> prerequisites;
        std::vector<StatModifier> modifiers;
        std::vector<DamageModifier> damage_modifiers; // 用于转换和独立增伤
        std::vector<AstrolabeNodeEffect> effects;
        
        float x = 0.0f;
        float y = 0.0f;
        std::string icon_id;
    };

    // For JSON parsing
    inline void to_json(nlohmann::json& j, const AstrolabeNodeType& t) {
        j = static_cast<uint8_t>(t);
    }
    inline void from_json(const nlohmann::json& j, AstrolabeNodeType& t) {
        t = static_cast<AstrolabeNodeType>(j.get<uint8_t>());
    }

    inline void to_json(nlohmann::json& j, const AstrolabeNode& n) {
        j = nlohmann::json{
            {"id", n.id}, {"name_key", n.name_key}, {"desc_key", n.desc_key}, {"type", n.type},
            {"prerequisites", n.prerequisites}, {"modifiers", n.modifiers}, 
            {"damage_modifiers", n.damage_modifiers}, {"effects", n.effects},
            {"x", n.x}, {"y", n.y}, {"icon_id", n.icon_id}
        };
    }
    inline void from_json(const nlohmann::json& j, AstrolabeNode& n) {
        j.at("id").get_to(n.id);
        j.at("name_key").get_to(n.name_key);
        j.at("desc_key").get_to(n.desc_key);
        j.at("type").get_to(n.type);
        j.at("prerequisites").get_to(n.prerequisites);
        j.at("modifiers").get_to(n.modifiers);
        if (j.contains("damage_modifiers")) j.at("damage_modifiers").get_to(n.damage_modifiers);
        j.at("effects").get_to(n.effects);
        j.at("x").get_to(n.x);
        j.at("y").get_to(n.y);
        j.at("icon_id").get_to(n.icon_id);
    }

    struct AstrolabeComponent {
        std::set<uint32_t> activated_nodes;
        int available_points = 0;
    };

    // Serialize AstrolabeComponent (useful for saving)
    inline void to_json(nlohmann::json& j, const AstrolabeComponent& c) {
        j = nlohmann::json{
            {"activated_nodes", c.activated_nodes},
            {"available_points", c.available_points}
        };
    }
    inline void from_json(const nlohmann::json& j, AstrolabeComponent& c) {
        j.at("activated_nodes").get_to(c.activated_nodes);
        j.at("available_points").get_to(c.available_points);
    }

    // --- Class Traits ---
    
    // Sword Cultivator Trait
    struct SwordHeartComponent {};
    
    inline void to_json(nlohmann::json& j, const SwordHeartComponent& c) { j = nlohmann::json::object(); }
    inline void from_json(const nlohmann::json& j, SwordHeartComponent& c) { }

} // namespace NoMoreDay
