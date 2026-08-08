#include "game/data/TalentLoader.hpp"
#include "game/data/TalentLayoutService.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace NoMoreDay {

bool TalentLoader::LoadProfessionTalents(const std::string& path, TalentGraph& outGraph) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[TalentLoader] Failed to open file: " << path << std::endl;
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        outGraph.Clear();

        // Load Profession Stars
        if (j.contains("profession_stars")) {
            auto stars = j["profession_stars"];
            for (const auto& starJson : stars) {
                ProfessionStar star;
                from_json(starJson, star);
                // Ensure index safety
                uint8_t idx = static_cast<uint8_t>(star.profession);
                if (idx < 6) {
                    outGraph.professionStars[idx] = star;
                }
            }
        }

        // Load Nodes
        if (j.contains("nodes")) {
            for (const auto& nodeJson : j["nodes"]) {
                AstrolabeTalentNode node;
                from_json(nodeJson, node);
                outGraph.nodes[node.id] = node;
            }
        }

        // Compute Layout
        TalentLayoutService::computeNodePositions(outGraph);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[TalentLoader] JSON Parse Error: " << e.what() << std::endl;
        return false;
    }
}

bool TalentLoader::LoadAstrolabe(const std::string& path, AstrolabeMap& outMap) {
    // Stubbed
    return false;
}

void TalentLoader::CreateDefaultMap(AstrolabeMap& outMap) {
    // Stubbed
}

} // namespace NoMoreDay