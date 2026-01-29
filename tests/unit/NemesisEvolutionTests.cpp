#include "TestCommon.hpp"
#include "game/components/Combat.hpp"
#include "game/data/PlayerCombatHistory.hpp"
#include "game/systems/combat/CombatHistorySystem.hpp"
#include "game/systems/nemesis/NemesisGenerator.hpp"

using namespace NoMoreDay;

TEST_CASE("[Unit] NemesisEvolution - Combat History Tracking") {
  entt::registry registry;
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<PlayerCombatHistory>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  auto target = registry.create();
  registry.emplace<EnemyTag>(target);
  registry.emplace<Position>(target, 100.0f, 0.0f); // 100 distance

  CombatHistorySystem::Init();

  SUBCASE("Damage Tracking") {
    CombatEvent evt;
    evt.source = player;
    evt.target = target;
    evt.value = 100.0f; // damage
    // evt.damageType = DamageType::Fire; // Not in struct, use tags
    evt.tags = Tag::Fire;

    auto &history = registry.get<PlayerCombatHistory>(player);
    // Engagement distance EMA check (alpha=0.1)
    // Initial avg is 300.0f?? Wait, default is 5.0f in struct definition.
    // Let's re-read the struct definition.
    // float avgEngagementDistance = 5.0f;
    // distance is 100.0f.
    // new avg = 0.1 * 100 + 0.9 * 5 = 10 + 4.5 = 14.5.
    // The test assumed 300 previously?
    // Let's check logic in CombatHistorySystem.cpp.
    // If I can't check cpp, rely on struct default.
    // Let's set it explicitly to be sure.
    history.avgEngagementDistance = 300.0f;

    // Re-run OnDealDamage to update it? No, do it before.
    CombatHistorySystem::OnDealDamage(registry, evt);

    CHECK(history.damageDealtFire == doctest::Approx(100.0f));
    CHECK(history.getTotalDamageTracking() == doctest::Approx(100.0f));

    // Engagement distance EMA check (alpha=0.1)
    // Initial avg is 300.0f. New dist is 100.0f.
    // New avg = 0.1 * 100 + 0.9 * 300 = 10 + 270 = 280
    CHECK(history.avgEngagementDistance == doctest::Approx(280.0f));
  }

  SUBCASE("Elite Kill Tracking") {
    CombatEvent evt;
    evt.source = player;
    evt.target = target;
    evt.tags = Tag::Elite;

    CombatHistorySystem::OnKill(registry, evt);

    auto &history = registry.get<PlayerCombatHistory>(player);
    CHECK(history.elitesKilled == 1);
  }

  // Cleanup
  CombatHistorySystem::Shutdown();
}

TEST_CASE("[Unit] NemesisEvolution - Affix Adaptation") {
  entt::registry registry;
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto &history = registry.emplace<PlayerCombatHistory>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  // Bias history towards Fire Damage
  history.damageDealtFire = 10000.0f;
  // history.damageDealtTotal = 10000.0f; // Removed, calculated dynamically

  // Bias towards long range
  history.avgEngagementDistance = 500.0f;

  // We can't easily test the private SelectAffixes directly without friend
  // class or public wrapper But we can test DetermineCounterResistances which
  // is public (based on previous view)

  // Note: NemesisGenerator::DetermineCounterResistances was modified to use
  // history
  Tag resistance = NemesisGenerator::DetermineCounterResistances(registry);

  // Expect Fire Resistance due to high Fire damage
  CHECK((resistance & Tag::Fire) == Tag::Fire);
}

TEST_CASE("[Unit] NemesisEvolution - Spawning Verification") {
  entt::registry registry;
  // Setup necessary singleton data if needed (NemesisDataStore is singleton)
  // CreateNemesisEntity is a static helper? No, it's public static in hpp?
  // Let's check NemesisGenerator.hpp. It seems CreateNemesisEntity is public.

  std::vector<MonsterAffixType> affixes = {MonsterAffixType::Fast, MonsterAffixType::Molten};
  Tag resistances = Tag::Fire;
  Position pos = {100.0f, 100.0f};

  auto entity = NemesisGenerator::CreateNemesisEntity(
      registry, FactionType::Undead, affixes, resistances, pos, 1);

  CHECK(registry.valid(entity));
  CHECK(registry.all_of<NemesisComponent>(entity));
  CHECK(registry.all_of<MonsterAffixComponent>(entity));
  CHECK(registry.all_of<EnemyTag>(entity));

  auto &comp = registry.get<MonsterAffixComponent>(entity);
  CHECK(comp.HasAffix(MonsterAffixType::Fast));
  CHECK(comp.HasAffix(MonsterAffixType::Molten));

  auto &nemComp = registry.get<NemesisComponent>(entity);
  CHECK(nemComp.evolved_affixes.size() == 2);
  CHECK(nemComp.evolved_affixes[0] == MonsterAffixType::Fast);
}
