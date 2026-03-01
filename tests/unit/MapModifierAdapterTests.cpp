#include "doctest.h"

#include "game/components/Stats.hpp"
#include "game/components/WorldState.hpp"
#include "game/data/MapAffix.hpp"
#include "game/systems/modifier/MapModifierAdapter.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"

TEST_CASE("[Unit] MapModifierAdapter - active map affixes produce expected enemy deltas") {
  NoMoreDay::ActiveDimensionalState state;
  state.isActive = true;
  state.resonance.totalEnemyDensity = 2.0f;
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::Enemy_ExtraHealth,
       NoMoreDay::MapAffixCategory::Debuff, 0.30f, 5, "test"});
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::Enemy_ExtraDamage,
       NoMoreDay::MapAffixCategory::Debuff, 0.50f, 5, "test"});
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::Enemy_Fast,
       NoMoreDay::MapAffixCategory::Debuff, 0.20f, 5, "test"});

  const auto delta = NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(state);

  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            100.0f, static_cast<uint32_t>(NoMoreDay::StatType::MaxHealth),
            delta) == doctest::Approx(143.0f));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            10.0f, static_cast<uint32_t>(NoMoreDay::StatType::PhysicalDamage),
            delta) == doctest::Approx(15.0f));
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            40.0f, static_cast<uint32_t>(NoMoreDay::StatType::MoveSpeed),
            delta) == doctest::Approx(48.0f));
}

TEST_CASE("[Unit] MapModifierAdapter - inactive map state yields empty delta") {
  NoMoreDay::ActiveDimensionalState state;
  state.isActive = false;
  state.resonance.totalEnemyDensity = 99.0f;
  state.explicitAffixes.push_back(
      {NoMoreDay::MapAffixType::Enemy_ExtraHealth,
       NoMoreDay::MapAffixCategory::Debuff, 0.30f, 5, "test"});

  const auto delta = NoMoreDay::MapModifierAdapter::EvaluateEnemyAffixDelta(state);
  CHECK(NoMoreDay::ModifierEvaluator::ApplyStat(
            100.0f, static_cast<uint32_t>(NoMoreDay::StatType::MaxHealth),
            delta) == doctest::Approx(100.0f));
}
