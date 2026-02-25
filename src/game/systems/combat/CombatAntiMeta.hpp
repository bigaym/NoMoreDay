#pragma once

#include "game/components/Stats.hpp"
#include "game/data/SkillContract.hpp"
#include "game/data/TagRegistry.hpp"

namespace NoMoreDay {

struct DiminishingReturnsConfig {
  bool enabled = true;
  float base = 0.35f;
  float scale = 0.45f;
};

struct CostAffixRuntimeConfig {
  const char *display_name = "None";
  const char *reward_text = "";
  const char *penalty_text = "";

  StatType reward_stat = StatType::Count;
  ModifierMode reward_mode = ModifierMode::Flat;
  float reward_value = 0.0f;

  StatType penalty_stat = StatType::Count;
  ModifierMode penalty_mode = ModifierMode::Flat;
  float penalty_value = 0.0f;

  Tag damage_more_source_tag = Tag::None;
  float damage_more_value = 0.0f;
};

class CombatAntiMeta {
public:
  static const DiminishingReturnsConfig &GetDiminishingReturnsConfig() noexcept;
  static float ApplyDiminishingReturns(float actual) noexcept;

  static const CostAffixRuntimeConfig &
  GetCostAffixConfig(CostAffixPreset preset) noexcept;

  static void ReloadForTests() noexcept;
};

} // namespace NoMoreDay
