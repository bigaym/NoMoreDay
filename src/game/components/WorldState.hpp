#pragma once
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "../data/MapAffix.hpp"
#include "../data/MosaicData.hpp" 
#include "Common.hpp"

namespace NoMoreDay {

// Serialization for BiomeID
inline void to_json(nlohmann::json& j, const BiomeID& p) {
    j = static_cast<uint8_t>(p);
}
inline void from_json(const nlohmann::json& j, BiomeID& p) {
    p = static_cast<BiomeID>(j.get<uint8_t>());
}

// Serialization for ResonanceResult (as it was missing in MosaicData.hpp)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ResonanceResult, 
    totalEnemyDensity, totalDropRate, totalLevelMod, resonanceChainCount,
    dominantElement, primaryBiome, hasBoss, hasMerchant, hasTreasure, isPerfectResonance
)

struct ActiveDimensionalState {
    // Identity
    bool isActive = false;
    uint32_t seed = 0;         // Map Generation Seed (Crucial for consistent terrain)
    
    // Configuration
    BiomeID biome = BiomeID::None;
    int depthLevel = 1;
    
    // The Calculated Affixes and Rewards
    ResonanceResult resonance; 
    std::vector<MapAffix> explicitAffixes; // Specific Gameplay Modifiers (Decaying)
    
    // Derived Rewards (Calculated once upon activation)
    int difficultyScore = 0;
    float calculatedRarity = 0.0f;   // 1.0 = +100%
    float calculatedQuantity = 0.0f; // 1.0 = +100%

    // Dungeon Structure
    int maxDepth = 3;          // Standard Mosaic = 3 Levels
    int currentDepth = 1;

    // Source Data (for UI viewing)
    // Note: Entities in MosaicGrid are runtime IDs and not serialized here.
    // Ideally we would snapshot the fragment item data, but for Phase 1/2 we omit.
    MosaicGrid sourceGrid;
    
    // State Tracking
    bool isBossKilled = false;
    bool isCompleted = false;
};

// Note: sourceGrid is excluded from serialization
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ActiveDimensionalState, 
    isActive, seed, biome, depthLevel, 
    resonance, 
    explicitAffixes, 
    difficultyScore, calculatedRarity, calculatedQuantity,
    maxDepth, currentDepth,
    isBossKilled, isCompleted
)

} // namespace NoMoreDay
