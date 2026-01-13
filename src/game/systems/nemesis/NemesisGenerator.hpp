#pragma once

#include "game/components/Common.hpp"
#include "game/components/FactionComponent.hpp"
#include "game/components/NemesisComponent.hpp"
#include <entt/entt.hpp>
#include <string>
#include <vector>


namespace NoMoreDay {

/**
 * @brief Generator for dynamically creating Nemesis entities.
 *
 * Analyzes player kill history and damage types to synthesize
 * a Nemesis that counters the player's build.
 */
class NemesisGenerator {
public:
  /**
   * @brief Spawn a Nemesis if conditions are met.
   * @param registry The ECS registry
   * @param spawnPos Position to spawn the Nemesis
   * @return true if a Nemesis was spawned
   */
  static bool SpawnNemesisIfReady(entt::registry &registry,
                                  const Position &spawnPos);

  /**
   * @brief Force spawn a Nemesis for a specific faction.
   * @param registry The ECS registry
   * @param faction The faction to spawn for
   * @param spawnPos Position to spawn
   * @return The created Nemesis entity
   */
  static entt::entity SpawnNemesis(entt::registry &registry,
                                   FactionType faction,
                                   const Position &spawnPos);

  /**
   * @brief Generate a display name for a Nemesis based on faction and affixes.
   */
  static std::string
  GenerateDisplayName(FactionType faction,
                      const std::vector<std::string> &affixes);

private:
  /**
   * @brief Select affixes based on player kill history.
   */
  static std::vector<std::string> SelectAffixes();

  /**
   * @brief Determine counter resistances based on player's main damage type.
   */
  static Tag DetermineCounterResistances(entt::registry &registry);

  /**
   * @brief Create the Nemesis entity with all components.
   */
  static entt::entity
  CreateNemesisEntity(entt::registry &registry, FactionType faction,
                      const std::vector<std::string> &affixes, Tag resistances,
                      const Position &pos, int evolution_tier);
};

} // namespace NoMoreDay
