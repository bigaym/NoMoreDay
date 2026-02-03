#pragma once
#include <string>
#include <memory>
#include "game/data/TalentData.hpp"

namespace NoMoreDay {

class TalentLoader {
public:
    static bool LoadAstrolabe(const std::string& path, AstrolabeMap& outMap);
    static bool LoadProfessionTalents(const std::string& path, TalentGraph& outGraph);
    
    // Create a default map for testing/initial development
    static void CreateDefaultMap(AstrolabeMap& outMap);
};

} // namespace NoMoreDay
