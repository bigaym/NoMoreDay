#pragma once
#include <nlohmann/json.hpp>

namespace NoMoreDay {

/**
 * @brief Tracks player combat performance and style for the Nemesis system.
 * Uses rolling averages to adapt to recent playstyle changes.
 */
struct PlayerCombatHistory {
    // Damage Profile (Rolling Average)
    float damageDealtPhysical = 0.0f;
    float damageDealtFire = 0.0f;
    float damageDealtCold = 0.0f;
    float damageDealtLightning = 0.0f;
    float damageDealtPoison = 0.0f;
    
    // Playstyle Metrics
    float avgEngagementDistance = 5.0f; // Default to mid-range
    float avgKillTime = 10.0f;          // Default to moderate pace
    float burstDamagePeak = 0.0f;       // Single processing frame/tick max damage observed
    
    // Counters
    int elitesKilled = 0;
    int deathsToTraps = 0;

    // Helper to get total tracked damage for percentages
    float getTotalDamageTracking() const {
        return damageDealtPhysical + damageDealtFire + damageDealtCold + damageDealtLightning + damageDealtPoison;
    }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PlayerCombatHistory, 
        damageDealtPhysical, damageDealtFire, damageDealtCold, damageDealtLightning, damageDealtPoison,
        avgEngagementDistance, avgKillTime, burstDamagePeak,
        elitesKilled, deathsToTraps
    )
};

} // namespace NoMoreDay
