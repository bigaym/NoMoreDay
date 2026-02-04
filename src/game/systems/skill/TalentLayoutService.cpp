#include "game/systems/skill/TalentLayoutService.hpp"
#include <cmath>
#include <algorithm>
#include <map>
#include <vector>
#include <string>
#include <random>

namespace NoMoreDay {

struct LayoutRNG {
    std::mt19937 gen;
    LayoutRNG(uint32_t seed) : gen(seed) {}
    float nextFloat(float min, float max) {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(gen);
    }
};

void TalentLayoutService::computeNodePositions(TalentGraph& graph) {
    LayoutRNG rng(54321);

    // 1. Profession Stars
    for (int i = 0; i < Constants::Astrolabe::PROFESSION_COUNT; ++i) {
        auto prof = static_cast<ProfessionID>(i);
        float angleDeg = getSectorCenterAngle(prof);
        float angleRad = angleDeg * DEG2RAD;
        float r = 250.0f; // Inner ring further out

        graph.professionStars[i].x = r * cos(angleRad);
        graph.professionStars[i].y = r * sin(angleRad);
        graph.professionStars[i].profession = prof;
    }

    // 2. Group nodes
    using GroupKey = std::pair<ProfessionID, uint8_t>;
    std::map<GroupKey, std::vector<AstrolabeTalentNode*>> groupedNodes;
    for (auto& [id, node] : graph.nodes) {
        groupedNodes[{node.profession, node.tier}].push_back(&node);
    }

    const float GOLDEN_ANGLE = 137.508f * DEG2RAD;
    const float SPIRAL_C = 55.0f; // More spread internally
    const float MIN_NODE_DIST = 75.0f; // Safe breathing room

    std::vector<Vector2> allPlacedPositions;

    // 3. Organic Multi-Band Growth
    for (auto& [key, nodes] : groupedNodes) {
        ProfessionID prof = key.first;
        uint8_t tier = key.second;
        
        std::sort(nodes.begin(), nodes.end(), [](const AstrolabeTalentNode* a, const AstrolabeTalentNode* b) {
            return a->id < b->id;
        });

        // Group by prefix for clusters
        std::map<std::string, std::vector<AstrolabeTalentNode*>> clusters;
        std::vector<std::string> clusterOrder;
        for (auto* n : nodes) {
            std::string prefix = n->name_key.substr(0, 6);
            if (clusters.find(prefix) == clusters.end()) clusterOrder.push_back(prefix);
            clusters[prefix].push_back(n);
        }

        // Define Tier Bands (Tightened for better coordination)
        float minR = 0, maxR = 0;
        switch (tier) {
            case 1: minR = 220.0f;  maxR = 380.0f;  break; 
            case 2: minR = 480.0f;  maxR = 750.0f;  break; 
            case 3: minR = 850.0f;  maxR = 1200.0f; break;
            default: minR = 1300.0f; maxR = 1600.0f; break;
        }

        float centerAngle = getSectorCenterAngle(prof);
        float sectorSpan = Constants::Astrolabe::SECTOR_ANGLE; 
        float padding = 10.0f; // Less padding to allow clusters to fill width
        float minAngle = centerAngle - (sectorSpan / 2.0f) + padding;
        float maxAngle = centerAngle + (sectorSpan / 2.0f) - padding;

        for (size_t cIdx = 0; cIdx < clusterOrder.size(); ++cIdx) {
            const auto& prefix = clusterOrder[cIdx];
            auto& clusterNodes = clusters[prefix];
            
            float angleProgress = (clusterOrder.size() > 1) ? (float)cIdx / (clusterOrder.size() - 1) : 0.5f;
            float targetAngle = minAngle + angleProgress * (maxAngle - minAngle);
            
            bool anchorPlaced = false;
            Vector2 anchor;
            int attempts = 0;

            while(!anchorPlaced && attempts < 50) {
                float r = rng.nextFloat(minR, maxR);
                float a = targetAngle + rng.nextFloat(-8.0f, 8.0f); // More angular jitter
                anchor = { r * cos(a * DEG2RAD), r * sin(a * DEG2RAD) };
                
                bool collision = false;
                for(const auto& pos : allPlacedPositions) {
                    float dx = pos.x - anchor.x;
                    float dy = pos.y - anchor.y;
                    if(dx*dx + dy*dy < (MIN_NODE_DIST * 1.8f * MIN_NODE_DIST * 1.8f)) {
                        collision = true; break;
                    }
                }
                if(!collision) anchorPlaced = true;
                attempts++;
            }

            // Grow nodes around anchor
            for (size_t nIdx = 0; nIdx < clusterNodes.size(); ++nIdx) {
                auto* node = clusterNodes[nIdx];
                // Slightly tighter spirals
                float r = (SPIRAL_C * 0.8f) * sqrtf((float)nIdx + 0.5f);
                float theta = (float)nIdx * GOLDEN_ANGLE;
                
                float lx = r * cos(theta);
                float ly = r * sin(theta);
                
                node->x = anchor.x + lx;
                node->y = anchor.y + ly;
                allPlacedPositions.push_back({node->x, node->y});
            }
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
    // Step-based growth: R2 is base for Tier 1, then adds spacing
    float base = ORBIT_R2;
    float step = 120.0f; // Gap between major tiers
    
    return base + (tier - 1) * step;
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
