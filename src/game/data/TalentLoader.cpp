#include "game/data/TalentLoader.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include <fstream>
#include "core/logging/Logger.hpp"

namespace NoMoreDay {

bool TalentLoader::LoadAstrolabe(const std::string& path, AstrolabeMap& outMap) {
    LOG_INFO("TalentLoader: Opening astrolabe data from {}", path);
    
    outMap.constellations.clear();
    outMap.stars.clear();

    // Primary source: Load constellations from file
    std::ifstream file(path);
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            if (j.contains("constellations")) {
                for (const auto& c_json : j["constellations"]) {
                    outMap.constellations.push_back(c_json.get<Constellation>());
                }
            }
        } catch (...) {}
    }

    // Secondary source: Import stars from AstrolabeRegistry (already contains loaded JSON nodes)
    const auto& registryNodes = AstrolabeRegistry::Get().GetAllNodes();
    if (!registryNodes.empty()) {
        LOG_INFO("TalentLoader: Importing {} stars from AstrolabeRegistry", registryNodes.size());
        for (const auto& [id, node] : registryNodes) {
            StarNode star = node;
            // Scale up coordinates for UI visibility (Original JSON values are small)
            star.x *= 8.0f;
            star.y *= 8.0f;
            outMap.stars[id] = std::move(star);
        }
    }

    if (outMap.stars.empty()) {
        LOG_WARN("TalentLoader: No stars found in Registry, using default map");
        CreateDefaultMap(outMap);
    }

    // Ensure at least one constellation/group exists for the UI renderer
    if (outMap.constellations.empty() && !outMap.stars.empty()) {
        Constellation dummy;
        dummy.id = 0;
        dummy.name_key = "星道 (Astrolabe)";
        for (const auto& [id, star] : outMap.stars) dummy.star_ids.push_back(id);
        outMap.constellations.push_back(dummy);
    }

    LOG_INFO("TalentLoader: Final map has {} constellations and {} stars", 
             outMap.constellations.size(), outMap.stars.size());
    return true;
}

void TalentLoader::CreateDefaultMap(AstrolabeMap& outMap) {
    outMap.constellations.clear();
    outMap.stars.clear();

    // Constellation 1: Origin
    Constellation origin;
    origin.id = 1;
    origin.name_key = "Origin Constellation";
    
    StarNode root;
    root.id = 1001;
    root.name_key = "star_origin_root";
    root.desc_key = "The starting point of your cosmic journey.";
    root.x = 0; root.y = 0;
    root.type = StarNodeType::Minor;
    root.icon_id = "icon_origin";
    root.modifiers.push_back({.value = 5.0f, .type = StatType::MaxHealth, .mode = ModifierMode::Flat});
    
    origin.star_ids.push_back(root.id);
    outMap.stars[root.id] = root;
    outMap.constellations.push_back(origin);

    // Constellation 2: Blade
    Constellation blade;
    blade.id = 2;
    blade.name_key = "Blade Constellation";
    
    StarNode blade1;
    blade1.id = 1002;
    blade1.name_key = "Blade Star";
    blade1.x = 50.0f; blade1.y = 50.0f;
    blade1.constellation_id = 2;
    blade1.prerequisites.push_back(1001); // Connect to root for graph test
    root.connections.push_back(1002);     // Update root connections
    outMap.stars[1001] = root;            // Save root back

    blade.star_ids.push_back(blade1.id);
    outMap.stars[blade1.id] = blade1;
    outMap.constellations.push_back(blade);

    // Constellation 3: Guard
    Constellation guard;
    guard.id = 3;
    guard.name_key = "Guard Constellation";

    StarNode guard1;
    guard1.id = 1003;
    guard1.name_key = "Guard Star 1";
    guard1.x = -50.0f; guard1.y = 50.0f;
    guard1.constellation_id = 3;
    guard1.prerequisites.push_back(1001);
    outMap.stars[1001].connections.push_back(1003); // Use map access to update root again

    StarNode guard2;
    guard2.id = 1004;
    guard2.name_key = "Guard Star 2";
    guard2.x = -60.0f; guard2.y = 60.0f;
    guard2.constellation_id = 3;
    guard2.prerequisites.push_back(1003);

    guard.star_ids.push_back(guard1.id);
    guard.star_ids.push_back(guard2.id);
    outMap.stars[guard1.id] = guard1;
    outMap.stars[guard2.id] = guard2;
    outMap.constellations.push_back(guard);
}

} // namespace NoMoreDay
