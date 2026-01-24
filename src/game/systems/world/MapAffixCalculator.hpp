#pragma once

#include "../../data/MapAffix.hpp"
#include <vector>
#include <entt/entt.hpp>

namespace NoMoreDay {

struct MosaicGrid; // Forward declaration

class MapAffixCalculator {
public:
    static int CalculateDifficultyScore(const std::vector<MapAffix>& affixes);
    
    struct Rewards {
        float rarityBonus;   // 1.0 = +100%
        float quantityBonus; // 1.0 = +100%
    };
    
    static Rewards CalculateRewards(int difficultyScore);
    
    // Calculates the probability multiplier for high-tier rolls (LP, Sockets)
    // based on the rarity bonus.
    static float CalculateLPProbabilityMultiplier(float rarityBonus);

    // Generates a list of MapAffixes based on the fragments in the grid.
    // This allows converting legacy/simple fragments into the new Affix system dynamically.
    static std::vector<MapAffix> GenerateAffixesFromGrid(const MosaicGrid& grid, entt::registry& registry);
};

} // namespace NoMoreDay
