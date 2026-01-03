#pragma once
#include "Stats.hpp"
#include "Common.hpp"

namespace NoMoreDay {

// Projectile Component
// Represents a flying object that carries damage stats (Snapshot)
struct Projectile {
    // Snapshot of the attacker's stats at the moment of firing.
    // This ensures that if the attacker changes stats (e.g. equipment change, buff expiry)
    // while the projectile is in flight, the projectile's damage remains consistent.
    CombatStats snapshot;
    
    // Who fired this? (Entity ID) - Useful for kill credit, friendly fire checks
    entt::entity owner = entt::null;
    
    // Mechanics
    float lifeTime = 5.0f; // Max flight time
    float speed = 500.0f;
    float radius = 5.0f;   // Hitbox size
    bool pierce = false;   // Does it pass through enemies?
    int pierceCount = 0;   // How many enemies can it hit?

    // Pull mechanics
    bool hasPull = false;
    float pullStrength = 0.0f;

    // Tracking hits to prevent multi-hit on the same target
    // We use a small static-ish array or vector to keep track. 
    // For many projectiles, a vector is okay as most hits are few.
    std::vector<entt::entity> hitEntities;
};

// --- NEW: Special Projectile Behaviors ---
struct BoomerangComponent {
    enum Phase { Outward, Returning };
    Phase phase = Outward;
    float returnTimer = 0.6f; // Time until it turns back
    entt::entity owner = entt::null;
};

} // namespace NoMoreDay
