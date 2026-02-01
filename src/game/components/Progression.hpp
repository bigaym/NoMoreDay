#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <set>
#include "game/components/Stats.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/data/TalentData.hpp"
#include <nlohmann/json.hpp>

namespace NoMoreDay {

    // Aliases for compatibility
    using AstrolabeNode = StarNode;
    using AstrolabeNodeType = StarNodeType;

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