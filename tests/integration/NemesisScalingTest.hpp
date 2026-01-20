#include "TestCommon.hpp"
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/NemesisComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp"
#include "game/systems/nemesis/NemesisGenerator.hpp"
#include <entt/entt.hpp>

using namespace NoMoreDay;

TEST_CASE("Nemesis Scaling and Phase Shield") {
  entt::registry registry;
  MonsterAffixSystem::Init();

  // Setup: Tier 10 Nemesis with Molten and Shielding
  std::vector<std::string> affixes = {"Molten", "Shielding"};
  Tag resistances = Tag::None;
  Position spawnPos = {0, 0};
  int tier = 10;

  entt::entity nemesis = NemesisGenerator::CreateNemesisEntity(
      registry, FactionType::Void, affixes, resistances, spawnPos, tier);

  REQUIRE(registry.valid(nemesis));
  REQUIRE(registry.any_of<NemesisComponent>(nemesis));
  REQUIRE(registry.get<NemesisComponent>(nemesis).evolution_tier == 10);

  // 1. Verify Scaling (Molten)
  // We can't easily check internal variables of ProcessMolten, but we can check
  // the spawned Hazard. Run Update to trigger Molten spawn Molten spawns every
  // interval. Force timer to interval.
  if (auto *affix = registry.try_get<MonsterAffixComponent>(nemesis)) {
    // Molten is index 0, Shielding is index 1
    affix->timers[0] = MonsterAffixRegistry::Params::MOLTEN_TICK_INTERVAL;
    affix->timers[1] = 3.0f; // Force Shielding cooldown
  }

  MonsterAffixSystem::Update(registry, 0.1f);

  // Find the spawned Hazard
  bool hazardFound = false;
  float hazardRadius = 0.0f;
  auto hazardView = registry.view<HazardComponent, Radius>();
  for (auto [e, haz, rad] : hazardView.each()) {
    if (haz.owner == nemesis && haz.damageType == DamageType::Fire) {
      hazardFound = true;
      hazardRadius = rad.value;
      // Verify Damage Scaling
      // Base damage = 10 * 0.5 = 5.
      // Tier 10 scale = 1.0 + (9 * 0.1) = 1.9.
      // Expected damage per tick = 5 * 1.9 = 9.5.
      // Hazard stores damagePerTick.
      float expectedDmg = MonsterAffixRegistry::Params::MOLTEN_TRAIL_DAMAGE *
                          MonsterAffixRegistry::Params::MOLTEN_TICK_INTERVAL *
                          1.9f;
      CHECK(haz.damagePerTick == doctest::Approx(expectedDmg).epsilon(0.01f));
    }
  }
  CHECK(hazardFound);
  // Verify Radius Scaling
  // Base radius 20. Tier 10 scale 1.9. Expected 38.
  float expectedRad = MonsterAffixRegistry::Params::MOLTEN_TRAIL_RADIUS * 1.9f;
  CHECK(hazardRadius == doctest::Approx(expectedRad).epsilon(0.01f));

  // With the Refactored MonsterAffixComponent (timers array), Molten (index 0)
  // and Shielding (index 1) now have independent timers.
  // Verify Shielding logic triggers:
  CHECK(registry.all_of<PhaseShieldComponent>(nemesis));

  // Also verify that both Molten trail spawned and Shielding check passed.
  // If we run another update with enough time:
  if (auto *affix = registry.try_get<MonsterAffixComponent>(nemesis)) {
    affix->timers[1] = 3.1f; // Force Shielding
  }
  MonsterAffixSystem::Update(registry, 0.1f);

  // The test below for PhaseShield remains valid.

  // Run update to add PhaseShieldComponent
  MonsterAffixSystem::Update(registry, 0.1f);

  CHECK(registry.any_of<PhaseShieldComponent>(nemesis));

  // Trigger Phase Shield
  auto &ps = registry.get<PhaseShieldComponent>(nemesis);
  auto &hp = registry.get<HealthComponent>(nemesis);

  // Deal burst damage > 30% HP
  float burstDmg = hp.max * 0.35f;
  CombatEvent evt;
  evt.type = CombatEventType::OnTakeDamage;
  evt.target = nemesis;
  evt.value = burstDmg;

  // Manually trigger OnEnemyTakeDamage (since we don't have full event dispatch
  // in test) Wait, OnEnemyTakeDamage is static private? No, it's used by lambda
  // in Init(). I need to call it. It is private. But I can use
  // `CombatEventDispatcher` if I initialized it. `MonsterAffixSystem::Init`
  // registers it. So I can `CombatEventDispatcher::Dispatch(evt, registry)`.

  CombatEventDispatcher::Dispatch(registry, evt);

  // Verify Invulnerability
  CHECK(registry.any_of<InvulnerableComponent>(nemesis));
}
