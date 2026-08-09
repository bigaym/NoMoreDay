#pragma once
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "../data/MapAffix.hpp"
#include "../data/MosaicData.hpp" 
#include "Common.hpp"
#include "game/data/BiomeTypes.hpp"

namespace nlohmann {
template <>
struct adl_serializer<::Vector2> {
    static void to_json(json& j, const ::Vector2& p) {
        j = json{{"x", p.x}, {"y", p.y}};
    }

    static void from_json(const json& j, ::Vector2& p) {
        j.at("x").get_to(p.x);
        j.at("y").get_to(p.y);
    }
};
} // namespace nlohmann

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
    int selectedBaseLevel = 1; // User-selected base level from Town (Max: Player+10)
    
    // The Calculated Affixes and Rewards
    ResonanceResult resonance; 
    std::vector<MapAffix> explicitAffixes; // Specific Gameplay Modifiers (Decaying)
    std::vector<AggregatedAffix> aggregatedAffixes; // Cached aggregated view for UI
    
    // Derived Rewards (Calculated once upon activation)
    int difficultyScore = 0;
    float calculatedRarity = 0.0f;   // 1.0 = +100%
    float calculatedQuantity = 0.0f; // 1.0 = +100%

    // Dungeon Structure
    int maxDepth = 3;          // Standard Mosaic = 3 Levels
    int currentDepth = 1;

    // State Tracking
    int killCounter = 0;
    bool isBossKilled = false;
    bool isCompleted = false;
    Vector2 lastExitPosition{0.0f, 0.0f}; // Last known position before returning to town

    // Source Data (for UI viewing and Persistence)
    // gridSnapshots stores the POD data of fragments for decay calculation after reload.
    std::array<FragmentSnapshot, 9> gridSnapshots;
    
    // Runtime only grid (stores entities)
    MosaicGrid sourceGrid;
};

// JSON Serialization
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ActiveDimensionalState, 
    isActive, seed, biome, depthLevel, selectedBaseLevel, 
    resonance, 
    explicitAffixes, aggregatedAffixes,
    difficultyScore, calculatedRarity, calculatedQuantity,
    maxDepth, currentDepth,
    killCounter, isBossKilled, isCompleted, lastExitPosition,
    gridSnapshots
)

struct RiftCompletionPromptState {
    bool isPending = false;
};

inline bool HasInProgressRift(const ActiveDimensionalState& state) noexcept {
    return state.isActive && !state.isCompleted;
}

inline void ClearRiftInstanceData(ActiveDimensionalState& state) {
    state.isActive = false;
    state.isCompleted = true;
    state.seed = 0;
    state.biome = BiomeID::None;
    state.depthLevel = 1;
    state.maxDepth = 3;
    state.currentDepth = 1;
    state.killCounter = 0;
    state.isBossKilled = false;
    state.lastExitPosition = {0.0f, 0.0f};
    state.difficultyScore = 0;
    state.calculatedRarity = 0.0f;
    state.calculatedQuantity = 0.0f;
    state.resonance = ResonanceResult{};
    state.explicitAffixes.clear();
    state.aggregatedAffixes.clear();

    for (auto& snap : state.gridSnapshots) {
        snap = FragmentSnapshot{};
    }
    state.sourceGrid.Clear();
}

} // namespace NoMoreDay
