#pragma once

#include "game/components/EndgameModifiers.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>

namespace NoMoreDay::systems {

namespace EndgameModifierIds {
inline constexpr uint32_t ExtraDamage = 1001u;
inline constexpr uint32_t ResistanceRend = 1002u;
inline constexpr uint32_t AilmentAmplification = 1003u;
inline constexpr uint32_t ArmorBreaker = 1004u;
inline constexpr uint32_t EnduringWard = 1005u;
} // namespace EndgameModifierIds

struct EndgameModifierContract {
  uint32_t id = 0;
  std::string name;

  // Source-side offense effects
  float outgoing_damage_more = 0.0f;
  float outgoing_resistance_reduction = 0.0f;
  float outgoing_armor_reduction = 0.0f;
  float outgoing_global_damage_reduction_reduction = 0.0f;
  float outgoing_ailment_magnitude_more = 0.0f;
  float outgoing_ailment_duration_more = 0.0f;

  // Target-side defense effects
  float incoming_damage_taken_more = 0.0f;
  float incoming_resistance_bonus = 0.0f;
  float incoming_armor_bonus = 0.0f;
  float incoming_global_damage_reduction_bonus = 0.0f;
  float incoming_ailment_taken_more = 0.0f;
  float incoming_ailment_duration_bonus = 0.0f;
};

struct EndgameModifierAggregate {
  float outgoing_damage_more = 0.0f;
  float outgoing_resistance_reduction = 0.0f;
  float outgoing_armor_reduction = 0.0f;
  float outgoing_global_damage_reduction_reduction = 0.0f;
  float outgoing_ailment_magnitude_more = 0.0f;
  float outgoing_ailment_duration_more = 0.0f;

  float incoming_damage_taken_more = 0.0f;
  float incoming_resistance_bonus = 0.0f;
  float incoming_armor_bonus = 0.0f;
  float incoming_global_damage_reduction_bonus = 0.0f;
  float incoming_ailment_taken_more = 0.0f;
  float incoming_ailment_duration_bonus = 0.0f;
};

struct EndgameModifierTrace {
  static constexpr size_t kMaxTrackedIds = 16;
  std::array<uint32_t, kMaxTrackedIds> source_ids{};
  size_t source_count = 0;
  std::array<uint32_t, kMaxTrackedIds> target_ids{};
  size_t target_count = 0;
};

struct EndgameModifierResolution {
  EndgameModifierAggregate aggregate{};
  EndgameModifierTrace trace{};
};

class EndgameModifierRegistry {
public:
  static EndgameModifierRegistry &Get();

  [[nodiscard]] bool EnsureLoaded();
  [[nodiscard]] bool LoadFromFile(
      const std::string &path = "assets/data/endgame_modifier_contracts.json");
  void ResetForTests();

  [[nodiscard]] const EndgameModifierContract *Find(uint32_t id) const;
  [[nodiscard]] size_t Size() const noexcept { return contracts_.size(); }

  [[nodiscard]] EndgameModifierResolution
  ResolveForEntities(const entt::registry &registry, entt::entity source,
                     entt::entity target) const;

private:
  void LoadBuiltins();

  std::unordered_map<uint32_t, EndgameModifierContract> contracts_;
  bool loaded_ = false;
};

} // namespace NoMoreDay::systems
