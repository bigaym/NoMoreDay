#pragma once
#include "game/contracts/CombatEvents.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay {

/**
 * @brief System to track player combat stats for the Nemesis Evolution system.
 * Uses Leaky Integator / EMA to track "recent" performance.
 */
class CombatHistorySystem {
public:
  static void Init();
  static void Shutdown();

  /**
   * @brief Decay damage accumulators over time to ensure they reflect "recent"
   * history.
   */
  static void Update(entt::registry &registry, float dt);

  static void OnDealDamage(entt::registry &registry,
                           const struct CombatEvent &evt);
  static void OnKill(entt::registry &registry, const struct CombatEvent &evt);

  static uint32_t s_handlerIdDealDamage;
  static uint32_t s_handlerIdKill;

  // Config
  static constexpr float HISTORY_DECAY_RATE =
      0.05f; // Decay 5% per second (approx)
  // Actually standard leaky bucket: Value *= (1.0f - Decay * dt)
  // If we want it to last ~30 seconds, Decay should be small.
  // e.g. 50% decay in 30s -> 0.5 = (1-k)^30.

  static constexpr float DISTANCE_EMA_ALPHA = 0.1f;
};

} // namespace NoMoreDay
