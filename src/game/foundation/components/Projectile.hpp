#pragma once
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"


namespace NoMoreDay {

// Projectile Component
// Represents a flying object that carries damage stats (Snapshot)
struct Projectile {
  // Snapshot of the attacker's stats at the moment of firing.
  // This ensures that if the attacker changes stats (e.g. equipment change,
  // buff expiry) while the projectile is in flight, the projectile's damage
  // remains consistent.
  CombatStats snapshot;

  // Who fired this? (Entity ID) - Useful for kill credit, friendly fire checks
  entt::entity owner = entt::null;
  uint64_t cast_id =
      0; // NEW: Unique ID for the cast that spawned this projectile

  // Mechanics
  float lifeTime = 5.0f; // Max flight time
  float speed = 500.0f;
  float radius = 5.0f;   // Hitbox size
  float arcWidth = 0.0f; // Visual arc width in degrees (0 = use system default)
  int visualType = 0;    // 0 = Fan, 1 = Circle, 2 = Beam
  bool pierce = false;   // Does it pass through enemies?
  int pierceCount = 0;   // How many enemies can it hit?
  bool hasRendered =
      false; // Flag to prevent first-frame destruction before visibility
  bool hitLimitReached =
      false; // Persistent flag to ensure destruction after rendering

  // Pull mechanics
  bool hasPull = false;
  float pullStrength = 0.0f;

  // Tracking hits to prevent multi-hit on the same target
  // We use a small static-ish array or vector to keep track.
  // For many projectiles, a vector is okay as most hits are few.
  std::vector<entt::entity> hitEntities;

  // --- NEW: Lifecycle Callbacks (Phase 2) ---
  enum class OnDeathBehavior : uint8_t {
    None = 0,
    Split,   // Split into multiple projectiles in a cone
    Explode, // Explode radially
    Hover    // Stop and deal Area Damage
  };
  OnDeathBehavior on_death = OnDeathBehavior::None;

  // Split Config
  uint8_t split_count = 3;
  float split_damage_mult = 0.5f;
  float split_speed_mult = 0.8f;
  float split_radius_mult = 0.6f;
  float split_spread = 0.6f; // Radians

  // Explode Config
  uint8_t explode_count = 8;
  float explode_damage_mult = 0.4f;

  // Hover Config
  float hover_duration = 1.0f;
  float hover_tick_rate = 0.2f;
  float hover_damage_mult = 0.3f;
};

// --- NEW: Special Projectile Behaviors ---
struct BoomerangComponent {
  enum Phase { Outward, Paused, Returning };
  Phase phase = Outward;
  float returnTimer = 0.5f; // Time until it pauses/turns
  float pauseTimer = 0.0f;  // Time to stay at apex
  entt::entity owner = entt::null;

  // Improved return logic
  float returnSpeed = 0.0f;               // If 0, use projectile speed
  entt::entity returnTarget = entt::null; // If null, return to owner
};

struct HomingTag {};

} // namespace NoMoreDay
