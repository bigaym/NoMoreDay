#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <set>
#include "Stats.hpp"
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

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AstrolabeNode, id, name_key, desc_key, type, prerequisites, modifiers, effects, x, y, icon_id)

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
