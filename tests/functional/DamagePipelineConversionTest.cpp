#include "TestCommon.hpp"
#include "doctest.h"
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"

namespace NoMoreDay {

TEST_CASE("[Functional] DamagePipeline - Iterative Conversion Chain") {
  TestSetupScope scope;
  entt::registry registry;
  auto attacker = registry.create();
  auto defender = registry.create();

  registry.emplace<CombatStats>(attacker);
  registry.get<CombatStats>(attacker).min_weapon_damage = 0.0f;
  registry.get<CombatStats>(attacker).max_weapon_damage = 0.0f;

  auto &defStats = registry.emplace<CombatStats>(defender);
  defStats.cached_area_level = 1;

  DamagePool base;
  base.values[0] = 100.0f; // Physical

  SUBCASE("Physical -> Lightning -> Cold") {
    auto &mods = registry.emplace<GlobalModifierComponent>(attacker);

    DamageModifier conv1;
    conv1.source_tag = Tag::Physical;
    conv1.target_tag = Tag::Lightning;
    conv1.type = ModifierType::Convert;
    conv1.value = 0.5f;
    mods.modifiers.push_back(conv1);

    DamageModifier conv2;
    conv2.source_tag = Tag::Lightning;
    conv2.target_tag = Tag::Cold;
    conv2.type = ModifierType::Convert;
    conv2.value = 1.0f;
    mods.modifiers.push_back(conv2);

    auto res = DamagePipeline::Calculate(registry, attacker, defender, 0, base,
                                         Tag::Hit);

    // Result: 50 Physical, 50 Cold (Lightning all converted to Cold)
    CHECK(res.final_pool.values[0] == doctest::Approx(50.0f));
    CHECK(res.final_pool.values[3] ==
          doctest::Approx(0.0f)); // Lightning all converted
    CHECK(res.final_pool.values[2] == doctest::Approx(50.0f)); // To Cold
  }

  SUBCASE("Conversion Loop Prevention") {
    auto &mods = registry.emplace<GlobalModifierComponent>(attacker);

    // Cold -> Fire (LEGAL based on CONVERSION_ORDER {0,3,2,1,4,5})
    DamageModifier conv1;
    conv1.source_tag = Tag::Cold;
    conv1.target_tag = Tag::Fire;
    conv1.type = ModifierType::Convert;
    conv1.value = 1.0f;
    mods.modifiers.push_back(conv1);

    // Fire -> Cold (ILLEGAL - reverse order)
    DamageModifier conv2;
    conv2.source_tag = Tag::Fire;
    conv2.target_tag = Tag::Cold;
    conv2.type = ModifierType::Convert;
    conv2.value = 1.0f;
    mods.modifiers.push_back(conv2);

    DamagePool coldPool;
    coldPool.values[2] = 100.0f; // Cold

    auto res = DamagePipeline::Calculate(registry, attacker, defender, 0,
                                         coldPool, Tag::Hit);

    // Cold should convert to Fire (legal)
    CHECK(res.final_pool.values[2] == doctest::Approx(0.0f));
    CHECK(res.final_pool.values[1] == doctest::Approx(100.0f));

    // Starting from Fire - it should NOT convert back to Cold
    DamagePool firePool;
    firePool.values[1] = 100.0f; // Fire
    auto res2 = DamagePipeline::Calculate(registry, attacker, defender, 0,
                                          firePool, Tag::Hit);
    CHECK(res2.final_pool.values[1] == doctest::Approx(100.0f));
    CHECK(res2.final_pool.values[2] == doctest::Approx(0.0f));
  }
}

TEST_CASE("[Functional] DamagePipeline - Unified More Multipliers") {
  TestSetupScope scope;
  entt::registry registry;
  auto attacker = registry.create();
  auto defender = registry.create();

  registry.emplace<CombatStats>(attacker);
  registry.emplace<CombatStats>(defender).cached_area_level = 1;

  DamagePool base;
  base.values[0] = 100.0f;

  SUBCASE("Multiple More multipliers application") {
    auto &mods = registry.emplace<GlobalModifierComponent>(attacker);

    // 20% More Global
    DamageModifier more1;
    more1.type = ModifierType::More;
    more1.value = 0.2f;
    more1.source_tag = Tag::None;
    mods.modifiers.push_back(more1);

    // 50% More Physical
    DamageModifier more2;
    more2.type = ModifierType::More;
    more2.value = 0.5f;
    more2.source_tag = Tag::Physical;
    mods.modifiers.push_back(more2);

    // Calculation: 100 * 1.2 * 1.5 = 180
    auto res = DamagePipeline::Calculate(registry, attacker, defender, 0, base,
                                         Tag::Hit);
    CHECK(res.total_damage == doctest::Approx(180.0f));
  }
}

} // namespace NoMoreDay
