#include "game/systems/stats/AttributePipeline.hpp"
#include "TestCommon.hpp"

namespace NoMoreDay {

TEST_CASE("AttributePipeline: Tag Filter") {
  // Setup
  StatModifier mod;
  mod.required_tags = Tag::Melee;

  // Case 1: No Context Tags -> Inactive
  CHECK_FALSE(mod.IsActive(Tag::None));
  CHECK_FALSE(mod.IsActive(Tag::Fire));

  // Case 2: Matching Tags -> Active
  CHECK(mod.IsActive(Tag::Melee));
  CHECK(mod.IsActive(Tag::Melee | Tag::Fire));

  // Case 3: Tag::None requirement -> Always Active
  mod.required_tags = Tag::None;
  CHECK(mod.IsActive(Tag::None));
  CHECK(mod.IsActive(Tag::Melee));
}

TEST_CASE("AttributePipeline: Struct Layout") {
  // Verify StatModifier is 24 bytes (or close/aligned)
  CHECK(sizeof(StatModifier) <= 32);
  CHECK(alignof(StatModifier) == 8);
}

TEST_CASE("AttributePipeline: Calculation Logic") {
  // Mock Data
  CombatStats stats;
  // Set Base values (Simulating Phase 0)
  stats.effective_strength = 10.0f; // Base Strength
  stats.armor = 100.0f;             // Base Armor from Gear
  stats.max_health = 1000.0f;       // Base HP
  stats.dodge_rating = 0.0f;

  std::vector<StatModifier> mods;

  // Add Modifiers
  // 1. +5 Strength (Flat)
  {
    StatModifier m;
    m.type = StatType::Strength;
    m.mode = ModifierMode::Flat;
    m.value = 5.0f;
    mods.push_back(m);
  }
  // 2. +10% Strength (Inc)
  {
    StatModifier m;
    m.type = StatType::Strength;
    m.mode = ModifierMode::PercentAdd;
    m.value = 0.1f;
    mods.push_back(m);
  }
  // 3. +200 Armor (Flat)
  {
    StatModifier m;
    m.type = StatType::Armor;
    m.mode = ModifierMode::Flat;
    m.value = 200.0f;
    mods.push_back(m);
  }

  // Manually Run Phase 2 (Primary + Conversion)
  // 1. Resolve Strength: (10 + 5) * 1.1 = 16.5
  // 2. Convert Strength to Armor: 16.5 * 2.0 = 33.0
  AttributePipeline::Phase2_ResolvePrimary(stats, mods, 1);

  CHECK(stats.effective_strength == doctest::Approx(16.5f));

  // Manually Run Phase 3 (Secondary)
  // 1. Resolve Armor: (Base 100 + Conv 33 + Flat 200) * 1 = 333
  AttributePipeline::Phase3_ResolveSecondary(stats, mods);

  CHECK(stats.armor == doctest::Approx(333.0f));

  // Manually Run Phase 4 (Bake)
  // Level 1.
  // DR = Armor / (Armor + 50*1) = 333 / 383 = 0.869
  AttributePipeline::Phase4_BakeEffective(stats, 1);

  CHECK(stats.effective_armor_dr == doctest::Approx(0.869f).epsilon(0.01f));
}

} // namespace NoMoreDay
