#pragma once

#include "game/components/FactionComponent.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/data/TagRegistry.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Component to mark an entity as a Nemesis.
 *
 * A Nemesis is a special elite enemy that is dynamically generated
 * based on the player's kill history and build. It persists across
 * runs and evolves each time it is defeated.
 */
struct NemesisComponent {
  /// The faction this Nemesis originates from
  FactionType origin_faction = FactionType::Undead;

  /// Affixes synthesized from player kill history
  std::vector<MonsterAffixType> evolved_affixes;

  /// Counter-resistances based on player's main damage type
  Tag counter_resistances = Tag::None;

  /// Evolution tier (increments each time player defeats this Nemesis)
  int evolution_tier = 1;

  /// Unique ID for persistence across runs
  uint64_t nemesis_id = 0;

  /// Display name (generated based on affixes)
  std::string display_name;

  /// Gold drop value (tier * 1000)
  uint32_t gold_value = 0;

  NemesisComponent() = default;

  NemesisComponent(FactionType faction, int tier = 1)
      : origin_faction(faction), evolution_tier(tier) {}

  /**
   * @brief Calculate stat multiplier based on evolution tier.
   * Each tier increases stats by 20%.
   */
  float GetStatMultiplier() const { return 1.0f + (evolution_tier - 1) * 0.2f; }
};

/**
 * @brief Tag component to mark an entity as a Nemesis for quick filtering.
 */
struct NemesisTag {};

/**
 * @brief Data structure for persisting Nemesis state across runs.
 */
struct NemesisData {
  uint64_t nemesis_id = 0;
  FactionType faction = FactionType::Undead;
  std::vector<MonsterAffixType> affixes;
  Tag resistances = Tag::None;
  int evolution_tier = 1;
  std::string display_name;

  /// Whether this Nemesis is currently active (spawned but not killed)
  bool is_active = false;
};

} // namespace NoMoreDay
