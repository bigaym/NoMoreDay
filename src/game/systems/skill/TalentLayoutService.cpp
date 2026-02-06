#include "game/systems/skill/TalentLayoutService.hpp"
#include <cmath>
#include <algorithm>
#include <map>
#include <vector>
#include <string>
#include <random>

namespace NoMoreDay {

void TalentLayoutService::computeNodePositions(TalentGraph& graph) {
    // 1. Profession Stars
    for (int i = 0; i < Constants::Astrolabe::PROFESSION_COUNT; ++i) {
        auto prof = static_cast<ProfessionID>(i);
        float angleDeg = getSectorCenterAngle(prof);
        float angleRad = angleDeg * DEG2RAD;
        float r = Constants::Astrolabe::ORBIT_R1;

        graph.professionStars[i].x = r * cos(angleRad);
        graph.professionStars[i].y = r * sin(angleRad);
        graph.professionStars[i].profession = prof;
    }

    // 2. Group nodes for layout
    using GroupKey = std::pair<ProfessionID, uint8_t>;
    std::map<GroupKey, std::vector<AstrolabeTalentNode*>> groupedNodes;
    for (auto& [id, node] : graph.nodes) {
        groupedNodes[{node.profession, node.tier}].push_back(&node);
    }

    // 3. Deterministic Layout (Sector-based)
    for (auto& [key, nodes] : groupedNodes) {
        ProfessionID prof = key.first;
        uint8_t tier = key.second;
        uint8_t count = (uint8_t)nodes.size();

        float r = getOrbitRadius(tier);
        
        for (auto* node : nodes) {
            float angleDeg = computeNodeAngle(prof, tier, node->sectorIndex, count);
            float angleRad = angleDeg * DEG2RAD;
            
            node->x = r * cos(angleRad);
            node->y = r * sin(angleRad);
        }
    }
}

float TalentLayoutService::getSectorCenterAngle(ProfessionID profession) {
    // Mapping based on UI spec diagram
    switch (profession) {
        case ProfessionID::BladeAscendant: return 270.0f; // Bottom
        case ProfessionID::Mage:           return 330.0f; // Bottom-Right
        case ProfessionID::Priest:         return 210.0f; // Bottom-Left
        case ProfessionID::Knight:         return 30.0f;  // Top-Right
        case ProfessionID::Ranger:         return 150.0f; // Top-Left
        case ProfessionID::Berserker:      return 90.0f;  // Top
        default: return 0.0f;
    }
}

float TalentLayoutService::getOrbitRadius(uint8_t tier) {
    using namespace Constants::Astrolabe;
    switch(tier) {
        case 1: return ORBIT_R2; // 300
        case 2: return ORBIT_R3; // 500
        case 3: return ORBIT_R4; // 750
        default: return ORBIT_R4 + (tier - 3) * 250.0f;
    }
}

float TalentLayoutService::computeNodeAngle(ProfessionID profession, uint8_t tier, uint8_t sectorIndex, uint8_t totalNodesInTier) {
    float centerAngle = getSectorCenterAngle(profession);
    float sectorSpan = Constants::Astrolabe::SECTOR_ANGLE; // 60
    float padding = Constants::Astrolabe::SECTOR_PADDING_DEG; // 5
    
    float usableSpan = sectorSpan - (2 * padding); // 50 deg
    
    // If only 1 node, put it in center
    if (totalNodesInTier <= 1) {
        return centerAngle;
    }
    
    // Distribute nodes within [center - span/2 + pad, center + span/2 - pad]
    // Start angle relative to sector center
    float startOffset = -usableSpan / 2.0f;
    float step = usableSpan / (totalNodesInTier - 1);
    
    float angleOffset = startOffset + (sectorIndex * step);
    return centerAngle + angleOffset;
}

} // namespace NoMoreDay
