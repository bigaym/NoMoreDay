#pragma once

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/FactionComponent.hpp"
#include "game/foundation/components/NemesisComponent.hpp"
#include "game/foundation/data/MonsterAffixRegistry.hpp"
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
                      const std::vector<MonsterAffixType> &affixes);

  /**
   * @brief Select affixes based on player kill history.
   */
  static std::vector<MonsterAffixType> SelectAffixes(entt::registry &registry);

public:
  /**
   * @brief Determine counter resistances based on player's main damage type.
   */
  static Tag DetermineCounterResistances(entt::registry &registry);

  /**
   * @brief Create the Nemesis entity with all components.
   * Public for testing.
   */
  static entt::entity
  CreateNemesisEntity(entt::registry &registry, FactionType faction,
                      const std::vector<MonsterAffixType> &affixes, Tag resistances,
                      const Position &pos, int evolution_tier);
};

} // namespace NoMoreDay
