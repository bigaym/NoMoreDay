#pragma once

#include "game/components/EnemyComponent.hpp"
#include "game/components/FactionComponent.hpp"
#include "game/systems/combat/CombatEvents.hpp"
#include <entt/entt.hpp>


namespace NoMoreDay {

/**
 * @brief System that tracks faction aggro based on player kills.
 *
 * Registers with CombatEventDispatcher to listen for OnKill events,
 * accumulates aggro for the victim's faction, and triggers Nemesis
 * spawns when thresholds are reached.
 */
class FactionAggroSystem {
public:
  /**
   * @brief Initialize the system by registering event handlers.
   * Call once during game initialization.
   */
  static void Init();

  /**
   * @brief Update faction aggro state.
   * @param registry The ECS registry
   */
  static void Update(entt::registry &registry);

  /**
   * @brief Check if a Nemesis should be triggered.
   * @param registry The ECS registry
   * @param outFaction Output: the faction that triggered Nemesis
   * @return true if Nemesis should spawn
   */
  static bool ShouldSpawnNemesis(entt::registry &registry,
                                 FactionType &outFaction);

  /**
   * @brief Consume the Nemesis trigger (reset aggro for that faction).
   * Called after Nemesis is spawned.
   */
  static void ConsumeNemesisTrigger(entt::registry &registry,
                                    FactionType faction);

  /**
   * @brief Cleanup (unregister handlers).
   */
  static void Shutdown();

private:
  /**
   * @brief Handler for OnKill events.
   */
  static void OnKillHandler(entt::registry &registry, const CombatEvent &evt);

  static uint32_t s_handler_id;
};

} // namespace NoMoreDay
