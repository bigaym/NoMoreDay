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

    Constellation origin;
    origin.id = 1;
    origin.name_key = "Origin Constellation";
    
    StarNode root;
    root.id = 1001;
    root.name_key = "Origin Star";
    root.desc_key = "The starting point of your cosmic journey.";
    root.x = 0; root.y = 0;
    root.type = StarNodeType::Minor;
    root.icon_id = "icon_origin";
    root.modifiers.push_back({.value = 5.0f, .type = StatType::MaxHealth, .mode = ModifierMode::Flat});
    
    origin.star_ids.push_back(root.id);
    outMap.stars[root.id] = root;
    outMap.constellations.push_back(origin);
}

} // namespace NoMoreDay
