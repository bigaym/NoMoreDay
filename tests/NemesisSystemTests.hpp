#pragma once
#include "TestCommon.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/FactionComponent.hpp"
#include "game/components/NemesisComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Stats.hpp"
#include "game/data/NemesisDataStore.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/CombatEvents.hpp"
#include "game/systems/nemesis/FactionAggroSystem.hpp"
#include "game/systems/nemesis/NemesisGenerator.hpp"
#include <entt/entt.hpp>


namespace NoMoreDay {

TEST_CASE("Faction Aggro Accumulation") {
  PlayerFactionAggro aggro;

  SUBCASE("Initial state is zero for all factions") {
    for (size_t i = 0; i < static_cast<size_t>(FactionType::Count); ++i) {
      CHECK(aggro.GetAggro(static_cast<FactionType>(i)) == 0.0f);
    }
  }

  SUBCASE("Adding aggro increases faction value") {
    aggro.AddAggro(FactionType::Undead, 10.0f);
    CHECK(aggro.GetAggro(FactionType::Undead) == doctest::Approx(10.0f));

    aggro.AddAggro(FactionType::Undead, 5.0f);
    CHECK(aggro.GetAggro(FactionType::Undead) == doctest::Approx(15.0f));
  }

  SUBCASE("Threshold detection works correctly") {
    aggro.AddAggro(FactionType::Void, 99.0f);
    FactionType triggered;
    CHECK_FALSE(aggro.HasTriggeredNemesis(triggered));

    aggro.AddAggro(FactionType::Void, 1.0f);
    CHECK(aggro.HasTriggeredNemesis(triggered));
    CHECK(triggered == FactionType::Void);
  }

  SUBCASE("Reset clears aggro for specific faction") {
    aggro.AddAggro(FactionType::Corrupted, 50.0f);
    aggro.AddAggro(FactionType::Cultist, 30.0f);

    aggro.ResetAggro(FactionType::Corrupted);
    CHECK(aggro.GetAggro(FactionType::Corrupted) == 0.0f);
    CHECK(aggro.GetAggro(FactionType::Cultist) == doctest::Approx(30.0f));
  }
}

TEST_CASE("Nemesis Data Store Persistence") {
  auto &store = NemesisDataStore::Get();
  store.Reset();

  SUBCASE("Recording affixes stores in history") {
    store.RecordKillAffix("Fast");
    store.RecordKillAffix("Tanky");
    store.RecordKillAffix("Fast");
    store.RecordKillAffix("Fast");

    auto top = store.GetTopAffixes(2);
    REQUIRE(top.size() >= 1);
    CHECK(top[0] == "Fast"); // Most common
  }

  SUBCASE("Kill affix history has size limit") {
    store.Reset();
    for (int i = 0; i < 60; ++i) {
      store.RecordKillAffix("Affix" + std::to_string(i));
    }
    CHECK(store.kill_affix_history.size() ==
          NemesisDataStore::MAX_AFFIX_HISTORY);
  }

  SUBCASE("Nemesis ID generation is unique") {
    store.Reset();
    uint64_t id1 = store.GenerateNemesisId();
    uint64_t id2 = store.GenerateNemesisId();
    CHECK(id1 != id2);
  }
}

TEST_CASE("Faction Aggro System Integration") {
  LoggerScope scope;
  entt::registry registry;

  // Initialize dispatcher and FactionAggroSystem
  CombatEventDispatcher::Init();
  FactionAggroSystem::Init();

  // Create player
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<PlayerFactionAggro>(player);

  // Create enemy with faction
  auto enemy = registry.create();
  registry.emplace<FactionComponent>(enemy, FactionType::Undead);
  registry.emplace<EnemyRarityComponent>(enemy, EnemyRarityComponent::NORMAL);

  SUBCASE("OnKill event increases faction aggro") {
    auto &aggro = registry.get<PlayerFactionAggro>(player);
    float initial_aggro = aggro.GetAggro(FactionType::Undead);

    // Simulate kill event
    auto evt = CombatEventFactory::CreateOnKill(player, enemy);
    CombatEventDispatcher::Dispatch(registry, evt);

    CHECK(aggro.GetAggro(FactionType::Undead) > initial_aggro);
  }

  SUBCASE("Elite kills add more aggro") {
    auto &rarity = registry.get<EnemyRarityComponent>(enemy);
    rarity.rarity = EnemyRarityComponent::ELITE;
    rarity.affixes = {"Fast", "Tanky"};

    auto &aggro = registry.get<PlayerFactionAggro>(player);
    float initial_aggro = aggro.GetAggro(FactionType::Undead);

    auto evt = CombatEventFactory::CreateOnKill(player, enemy);
    CombatEventDispatcher::Dispatch(registry, evt);

    // Elite should add more than normal (1.0f vs 5.0f)
    CHECK(aggro.GetAggro(FactionType::Undead) >=
          initial_aggro + PlayerFactionAggro::AGGRO_ELITE);
  }

  FactionAggroSystem::Shutdown();
  CombatEventDispatcher::Clear();
}

TEST_CASE("Nemesis Generator - Counter Resistance Logic") {
  entt::registry registry;

  // Create player with high fire damage
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.damage_multipliers[static_cast<int>(DamageType::Fire)] =
      3.0f; // High fire
  stats.damage_multipliers[static_cast<int>(DamageType::Physical)] = 1.0f;

  // Spawn a nemesis
  Position spawnPos = {100.0f, 100.0f};
  auto nemesis =
      NemesisGenerator::SpawnNemesis(registry, FactionType::Void, spawnPos);

  REQUIRE(registry.valid(nemesis));

  SUBCASE("Nemesis entity has correct components") {
    CHECK(registry.any_of<NemesisComponent>(nemesis));
    CHECK(registry.any_of<NemesisTag>(nemesis));
    CHECK(registry.any_of<FactionComponent>(nemesis));
    CHECK(registry.any_of<CombatStats>(nemesis));
  }

  SUBCASE("Nemesis has counter resistance vs player's main damage") {
    auto &nemesis_stats = registry.get<CombatStats>(nemesis);
    // Should have high fire resistance (player's main damage type)
    CHECK(nemesis_stats.resistances[static_cast<int>(DamageType::Fire)] >=
          0.5f);
  }

  SUBCASE("Nemesis component stores faction and affixes") {
    auto &comp = registry.get<NemesisComponent>(nemesis);
    CHECK(comp.origin_faction == FactionType::Void);
    CHECK(!comp.evolved_affixes.empty());
  }
}

TEST_CASE("Nemesis Generator - Display Name Generation") {
  std::vector<std::string> affixes = {"Fast", "Vampiric"};
  std::string name =
      NemesisGenerator::GenerateDisplayName(FactionType::Undead, affixes);

  CHECK(!name.empty());
  // Name should contain Chinese characters (prefix from faction)
}

TEST_CASE("Nemesis Component - Stat Multiplier") {
  NemesisComponent comp;

  SUBCASE("Tier 1 has base stats") {
    comp.evolution_tier = 1;
    CHECK(comp.GetStatMultiplier() == doctest::Approx(1.0f));
  }

  SUBCASE("Higher tiers scale stats") {
    comp.evolution_tier = 3;
    CHECK(comp.GetStatMultiplier() ==
          doctest::Approx(1.4f)); // 1.0 + (3-1) * 0.2

    comp.evolution_tier = 6;
    CHECK(comp.GetStatMultiplier() ==
          doctest::Approx(2.0f)); // 1.0 + (6-1) * 0.2
  }
}

} // namespace NoMoreDay
