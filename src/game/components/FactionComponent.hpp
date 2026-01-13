#pragma once

#include <array>
#include <cstdint>

namespace NoMoreDay {

/**
 * @brief Faction types for the Nemesis system.
 *
 * These correspond to the 4 major factions defined in the design document.
 * Each faction can accumulate aggro leading to Nemesis spawns.
 */
enum class FactionType : uint8_t {
  Undead = 0,    // 亡灵
  Void = 1,      // 异魔
  Corrupted = 2, // 腐化
  Cultist = 3,   // 邪教
  Count
};

/**
 * @brief Returns the display name for a faction type.
 */
inline const char *FactionTypeName(FactionType type) {
  switch (type) {
  case FactionType::Undead:
    return "亡灵";
  case FactionType::Void:
    return "异魔";
  case FactionType::Corrupted:
    return "腐化";
  case FactionType::Cultist:
    return "邪教";
  default:
    return "Unknown";
  }
}

/**
 * @brief Component attached to enemy entities to mark their faction.
 */
struct FactionComponent {
  FactionType faction = FactionType::Undead;

  FactionComponent() = default;
  explicit FactionComponent(FactionType f) : faction(f) {}
};

/**
 * @brief Component attached to the player entity to track faction aggro.
 *
 * When any faction's aggro reaches NEMESIS_THRESHOLD, a Nemesis of that
 * faction will be spawned in the next map.
 */
struct PlayerFactionAggro {
  std::array<float, static_cast<size_t>(FactionType::Count)> aggro{};

  static constexpr float NEMESIS_THRESHOLD = 100.0f;

  // Aggro values per enemy type
  static constexpr float AGGRO_NORMAL = 1.0f;
  static constexpr float AGGRO_ELITE = 5.0f;
  static constexpr float AGGRO_BOSS = 15.0f;

  /**
   * @brief Add aggro to a specific faction.
   * @return true if this addition caused the faction to reach Nemesis threshold
   */
  bool AddAggro(FactionType faction, float amount) {
    size_t idx = static_cast<size_t>(faction);
    if (idx >= aggro.size())
      return false;

    bool wasBelow = aggro[idx] < NEMESIS_THRESHOLD;
    aggro[idx] += amount;
    return wasBelow && aggro[idx] >= NEMESIS_THRESHOLD;
  }

  /**
   * @brief Get aggro value for a specific faction.
   */
  float GetAggro(FactionType faction) const {
    size_t idx = static_cast<size_t>(faction);
    return (idx < aggro.size()) ? aggro[idx] : 0.0f;
  }

  /**
   * @brief Check if any faction has reached Nemesis threshold.
   * @param outFaction Output parameter for the triggered faction
   * @return true if a faction has reached threshold
   */
  bool HasTriggeredNemesis(FactionType &outFaction) const {
    for (size_t i = 0; i < aggro.size(); ++i) {
      if (aggro[i] >= NEMESIS_THRESHOLD) {
        outFaction = static_cast<FactionType>(i);
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Reset aggro for a faction after Nemesis is spawned.
   */
  void ResetAggro(FactionType faction) {
    size_t idx = static_cast<size_t>(faction);
    if (idx < aggro.size()) {
      aggro[idx] = 0.0f;
    }
  }

  PlayerFactionAggro() = default;
};

} // namespace NoMoreDay
