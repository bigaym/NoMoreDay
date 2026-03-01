#include "TestCommon.hpp"
#include "engine/physics/SpatialGrid.hpp" // For SpatialHashGrid
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/EliteModifierComponents.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp" // Added this include
#include "game/systems/combat/CombatEvents.hpp"
#include "game/systems/combat/EliteModifierSystem.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/world/EnemySpawnSystem.hpp"

#include <algorithm>


using namespace NoMoreDay;

TEST_CASE("[Unit] MonsterAffix - Persistence") {
  entt::registry registry;

  // 1. Create a dummy enemy with Berserker affix
  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 100.0f, 100.0f);
  registry.emplace<HealthComponent>(enemy, 100.0f, 100.0f);
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<EnemyStateComponent>(enemy, EnemyRace::UNDEAD,
                                        EnemyArchetype::FODDER);

  auto &stats = registry.emplace<CombatStats>(enemy);
  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::Berserker);
  affix.hasUpdate = false;

  // Initial state (not berserk)
  registry.emplace<StatsDirty>(enemy);
  StatsSystem::update(registry);
  float normalDamage = stats.min_weapon_damage;
  CHECK(normalDamage > 0.0f);

  // 2. Trigger Berserker (HP < 50%)
  {
    auto &hp = registry.get<HealthComponent>(enemy);
    hp.current = 10.0f; // 10% HP
  }

  // Create empty grid for test (no spatial queries needed for Berserker)
  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.1f, dummyGrid);
  StatsSystem::update(registry); // Must update to see changes from isBerserk

  // Verify Berserker is active
  CHECK(affix.isBerserk == true);
  // Berserker should make it 2.0x normal damage
  CHECK(stats.min_weapon_damage == doctest::Approx(normalDamage * 2.0f));
  float berserkDamage = stats.min_weapon_damage;

  // 3. Trigger Recalculate (Simulation of a buff being added)
  registry.emplace<StatsDirty>(enemy);
  StatsSystem::update(registry);

  // Verify if damage is still berserk
  CHECK(stats.min_weapon_damage == doctest::Approx(berserkDamage));
}

TEST_CASE("[Unit] MonsterAffix - Stat Modifier Persistence") {
  entt::registry registry;

  // 1. Create enemy with Fast affix (+50% MoveSpeed)
  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 100.0f, 100.0f);
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<EnemyStateComponent>(enemy, EnemyRace::UNDEAD,
                                        EnemyArchetype::FODDER);

  // Initial calculation to get base speed
  registry.emplace<CombatStats>(enemy);
  registry.emplace<StatsDirty>(enemy);
  StatsSystem::update(registry);

  auto &stats = registry.get<CombatStats>(enemy);
  float baseSpeed = stats.move_speed; // Undead base is 40.0f

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::Fast);

  // Trigger Recalculate to apply affix mods
  registry.emplace<StatsDirty>(enemy);
  StatsSystem::update(registry);

  float fastSpeed = baseSpeed * 1.5f;
  CHECK(stats.move_speed == doctest::Approx(fastSpeed));

  // 2. Trigger Recalculate again
  registry.emplace<StatsDirty>(enemy);
  StatsSystem::update(registry);

  // Verify if speed is still correct
  CHECK(stats.move_speed == doctest::Approx(fastSpeed));
}

TEST_CASE("[Unit] MonsterAffix - Mirror Image Logic") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 100.0f, 100.0f);
  registry.emplace<HealthComponent>(enemy, 100.0f, 100.0f);
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<EnemyStateComponent>(enemy, EnemyRace::UNDEAD,
                                        EnemyArchetype::FODDER);
  registry.emplace<CombatStats>(enemy);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::MirrorImage);
  affix.hasOnHit = false;
  affix.mirrorCooldown = 0.0f; // Reset cooldown

  // Trigger Mirror Image via HP threshold
  auto &hp = registry.get<HealthComponent>(enemy);
  hp.current = 40.0f; // 40% HP

  CombatEvent evt;
  evt.source = enemy;

  MonsterAffixSystem::OnEnemyTakeDamage(registry, evt);

  // Re-dispatch the same trigger condition in cooldown window.
  // A single event window should not duplicate clone spawn.
  MonsterAffixSystem::OnEnemyTakeDamage(registry, evt);

  // Check if clones were created
  auto cloneView = registry.view<CloneComponent>();
  int cloneCount = 0;
  for (auto c : cloneView) {
    cloneCount++;
    auto &comp = cloneView.get<CloneComponent>(c);
    CHECK(comp.parent == enemy);
  }

  CHECK(cloneCount == 2);
  CHECK(affix.mirrorCooldown > 0.0f);
}

TEST_CASE("[Unit] MonsterAffix - Mirror Image HP threshold does not consume one-shot while on cooldown") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 100.0f, 100.0f);
  registry.emplace<HealthComponent>(enemy, 40.0f, 100.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::MirrorImage);
  affix.hasOnHit = false;
  affix.mirrorTriggered = false;
  affix.mirrorCooldown = 1.0f;

  CombatEvent evt;
  evt.source = enemy;

  MonsterAffixSystem::OnEnemyTakeDamage(registry, evt);

  CHECK(affix.mirrorTriggered == false);
  CHECK(registry.view<CloneComponent>().empty());
}

TEST_CASE("[Unit] MonsterAffix - StormStrider OnTakeDamage behavior-op path still spawns ghost") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 100.0f, 100.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::StormStrider);
  affix.hasOnHit = false;

  CombatEvent evt;
  evt.source = enemy;

  bool ghostSpawned = false;
  for (int i = 0; i < 256; ++i) {
    MonsterAffixSystem::OnEnemyTakeDamage(registry, evt);
    if (!registry.view<LightningGhostTag>().empty()) {
      ghostSpawned = true;
      break;
    }
  }

  CHECK(ghostSpawned);
}

TEST_CASE("[Unit] MonsterAffix - Toxic OnDeath flow is driven by adapter events") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 100.0f, 100.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::Toxic);
  affix.hasOnDeath = false;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 120.0f, 120.0f);

  MonsterAffixSystem::OnEnemyDeath(registry, enemy);

  auto orbView = registry.view<VolatileOrbTag>();
  int orbCount = 0;
  for (const auto orb : orbView) {
    (void)orb;
    ++orbCount;
  }

  CHECK(orbCount == 3);
}

TEST_CASE("[Unit] MonsterAffix - Nullifier OnHit flow is driven by adapter events") {
  entt::registry registry;

  auto attacker = registry.create();
  auto &affix = registry.emplace<MonsterAffixComponent>(attacker);
  affix.AddAffix(MonsterAffixType::Nullifier);
  affix.hasOnHit = false;

  auto target = registry.create();
  auto &effects = registry.emplace<ActiveEffectsComponent>(target);

  BuffEffect buff;
  buff.id = "test-buff";
  buff.name = "Test Buff";
  buff.type = BuffType::AttackUp;
  buff.duration = 3.0f;
  buff.remaining = 3.0f;
  buff.is_debuff = false;
  effects.effects.push_back(buff);

  CombatEvent evt;
  evt.source = attacker;
  evt.target = target;

  MonsterAffixSystem::OnEnemyDealDamage(registry, evt);

  CHECK(effects.effects.empty());
}

TEST_CASE("[Unit] MonsterAffix - Entangler OnHit flow is driven by adapter events") {
  entt::registry registry;

  auto attacker = registry.create();
  auto &affix = registry.emplace<MonsterAffixComponent>(attacker);
  affix.AddAffix(MonsterAffixType::Entangler);
  affix.hasOnHit = false;

  auto target = registry.create();
  registry.emplace<PlayerTag>(target);
  auto &effects = registry.emplace<ActiveEffectsComponent>(target);
  auto &playerStats = registry.emplace<PlayerStats>(target);
  playerStats.isRooted = false;

  CombatEvent evt;
  evt.source = attacker;
  evt.target = target;

  bool rootedTriggered = false;
  for (int i = 0; i < 128; ++i) {
    MonsterAffixSystem::OnEnemyDealDamage(registry, evt);
    for (const auto &effect : effects.effects) {
      if (effect.type == BuffType::Root) {
        rootedTriggered = true;
        break;
      }
    }
    if (rootedTriggered) {
      break;
    }
  }

  CHECK(rootedTriggered);
  CHECK(playerStats.isRooted);
}

TEST_CASE("[Unit] MonsterAffix - Molten update behavior-op path still spawns hazard") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 64.0f, 64.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::Molten);
  affix.hasUpdate = false;
  affix.timers[0] = MonsterAffixRegistry::Params::MOLTEN_TICK_INTERVAL;

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  auto hazardView = registry.view<HazardComponent, LocalLevelTag>();
  int hazardCount = 0;
  for (const auto e : hazardView) {
    (void)e;
    ++hazardCount;
  }
  CHECK(hazardCount == 1);
}

TEST_CASE("[Unit] MonsterAffix - Vampiric on-hit behavior-op path heals attacker") {
  entt::registry registry;

  auto attacker = registry.create();
  auto &affix = registry.emplace<MonsterAffixComponent>(attacker);
  affix.AddAffix(MonsterAffixType::Vampiric);
  affix.hasOnHit = false;

  auto &hp = registry.emplace<HealthComponent>(attacker, 20.0f, 100.0f);

  auto target = registry.create();
  registry.emplace<PlayerTag>(target);

  CombatEvent evt;
  evt.source = attacker;
  evt.target = target;
  CombatEventFactory::SetDamagePayload(evt, 40.0f);

  MonsterAffixSystem::OnEnemyDealDamage(registry, evt);

  CHECK(hp.current == doctest::Approx(40.0f));
}

TEST_CASE("[Unit] MonsterAffix - Void on-hit behavior-op path applies bonus once without recursion") {
  entt::registry registry;

  auto attacker = registry.create();
  auto &affix = registry.emplace<MonsterAffixComponent>(attacker);
  affix.AddAffix(MonsterAffixType::Void);
  affix.hasOnHit = false;

  auto target = registry.create();
  auto &hp = registry.emplace<HealthComponent>(target, 100.0f, 100.0f);

  CombatEvent evt;
  evt.source = attacker;
  evt.target = target;
  CombatEventFactory::SetDamagePayload(evt, 40.0f);

  MonsterAffixSystem::OnEnemyDealDamage(registry, evt);

  const float expectedBonus =
      (std::max)(40.0f * MonsterAffixRegistry::Params::VOID_ON_HIT_BONUS_RATIO,
                 MonsterAffixRegistry::Params::VOID_ON_HIT_MIN_BONUS_DAMAGE);
  CHECK(hp.current == doctest::Approx(100.0f - expectedBonus));
}

TEST_CASE("[Unit] MonsterAffix - Teleporter update behavior-op path still starts blink") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 0.0f, 0.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::Teleporter);
  affix.hasUpdate = false;
  affix.timers[0] = MonsterAffixRegistry::Params::TELEPORT_COOLDOWN;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 1000.0f, 0.0f);

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  CHECK(registry.all_of<TeleportationComponent>(enemy));
}

TEST_CASE("[Unit] MonsterAffix - Storm update behavior-op path spawns lightning marker") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 0.0f, 0.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::Storm);
  affix.hasUpdate = false;
  affix.timers[0] = MonsterAffixRegistry::Params::STORM_UPDATE_INTERVAL;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 128.0f, 64.0f);

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  auto ghostView = registry.view<LightningGhostTag, LightningGhostComponent>();
  int ghostCount = 0;
  for (const auto ghost : ghostView) {
    (void)ghost;
    ++ghostCount;
  }

  CHECK(ghostCount == 1);
}

TEST_CASE("[Unit] MonsterAffix - Frozen update behavior-op path still spawns orb") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 64.0f, 64.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::Frozen);
  affix.hasUpdate = false;
  affix.timers[0] = MonsterAffixRegistry::Params::FROZEN_ORB_INTERVAL;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 128.0f, 64.0f);

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  auto orbView = registry.view<FrozenOrbTag, HazardComponent, LocalLevelTag>();
  int orbCount = 0;
  for (const auto orb : orbView) {
    (void)orb;
    ++orbCount;
  }

  CHECK(orbCount == 1);
}

TEST_CASE("[Unit] MonsterAffix - ManaSiphon update behavior-op path drains player mana") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 0.0f, 0.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::ManaSiphon);
  affix.hasUpdate = false;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 150.0f, 0.0f);
  auto &playerStats = registry.emplace<CombatStats>(player);
  playerStats.mana = 100.0f;

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 1.0f, dummyGrid);

  CHECK(registry.all_of<ResourceDrainComponent>(enemy));
  CHECK(playerStats.mana < 100.0f);
  CHECK(playerStats.mana >= 0.0f);
}

TEST_CASE("[Unit] MonsterAffix - Berserker update behavior-op path still activates berserk") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 100.0f, 100.0f);
  registry.emplace<HealthComponent>(enemy, 10.0f, 100.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::Berserker);
  affix.hasUpdate = false;
  affix.isBerserk = false;

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  CHECK(affix.isBerserk);
  CHECK(registry.all_of<StatsDirty>(enemy));
}

TEST_CASE("[Unit] MonsterAffix - VoidZone update behavior-op path still spawns hazard") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 0.0f, 0.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::VoidZone);
  affix.hasUpdate = false;
  affix.timers[0] = MonsterAffixRegistry::Params::VOIDZONE_SPAWN_INTERVAL_MAX;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 80.0f, 40.0f);

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  auto zoneView = registry.view<VoidZoneTag, HazardComponent, LocalLevelTag>();
  int zoneCount = 0;
  for (const auto zone : zoneView) {
    (void)zone;
    ++zoneCount;
  }

  CHECK(zoneCount == 1);
}

TEST_CASE("[Unit] MonsterAffix - Shielding update behavior-op path initializes phase shield") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 0.0f, 0.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::Shielding);
  affix.hasUpdate = false;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 20.0f, 0.0f);

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  CHECK(registry.all_of<PhaseShieldComponent>(enemy));
}

TEST_CASE("[Unit] MonsterAffix - Vortex update behavior-op path activates force field") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 32.0f, 32.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::Vortex);
  affix.hasUpdate = false;
  affix.timers[0] = MonsterAffixRegistry::Params::VORTEX_INTERVAL;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 64.0f, 32.0f);

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  REQUIRE(registry.all_of<ForceFieldComponent>(enemy));
  const auto &forceField = registry.get<ForceFieldComponent>(enemy);
  CHECK(forceField.activeDuration ==
        doctest::Approx(MonsterAffixRegistry::Params::VORTEX_DURATION));
}

TEST_CASE("[Unit] MonsterAffix - Waller update behavior-op path still spawns dynamic obstacles") {
  entt::registry registry;

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 0.0f, 0.0f);

  auto &affix = registry.emplace<MonsterAffixComponent>(enemy);
  affix.AddAffix(MonsterAffixType::Waller);
  affix.hasUpdate = false;
  affix.timers[0] = MonsterAffixRegistry::Params::WALLER_COOLDOWN;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 120.0f, 0.0f);

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  auto obstacleView = registry.view<DynamicObstacleComponent>();
  int obstacleCount = 0;
  for (const auto obstacle : obstacleView) {
    (void)obstacle;
    ++obstacleCount;
  }

  CHECK(obstacleCount == 3);
}

TEST_CASE("[Unit] MonsterAffix - Soul Eater Progression") {
  entt::registry registry;

  auto eater = registry.create();
  registry.emplace<Position>(eater, 100.0f, 100.0f);
  registry.emplace<EnemyTag>(eater);
  registry.emplace<EnemyStateComponent>(eater, EnemyRace::UNDEAD,
                                        EnemyArchetype::FODDER);
  registry.emplace<EnemyRarityComponent>(eater, EnemyRarityComponent::ELITE);
  registry.emplace<CombatStats>(eater);

  auto &affix = registry.emplace<MonsterAffixComponent>(eater);
  affix.AddAffix(MonsterAffixType::SoulEater);

  // Manually add component as ProcessSoulEater would do in Update
  auto &soulEaterComp = registry.emplace<SoulEaterComponent>(eater);

  // Initial stats
  StatsSystem::update(registry);
  auto &stats = registry.get<CombatStats>(eater);
  float initialDmg = stats.damage_multipliers[0];

  // Simulate nearby enemy death
  auto victim = registry.create();
  registry.emplace<Position>(victim, 110.0f, 110.0f);

  MonsterAffixSystem::OnEnemyDeath(registry, victim);

  // Check if stack gained
  auto &soulEater = registry.get<SoulEaterComponent>(eater);
  CHECK(soulEater.stacks == 1);

  // Check if stats recalculation was triggered
  CHECK(registry.all_of<StatsDirty>(eater));

  StatsSystem::update(registry);

  // Verify damage bonus (5% per stack)
  CHECK(stats.damage_multipliers[0] == doctest::Approx(initialDmg + 0.05f));
}

TEST_CASE("[Unit] MonsterAffix - Suppressor Damage Reduction") {
  entt::registry registry;

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 500.0f, 500.0f); // Far away
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.crit_chance = 0.0f; // Disable random crits for test stability

  auto defender = registry.create();
  registry.emplace<Position>(defender, 100.0f,
                             100.0f); // Distance = 565px (> 300px threshold)
  registry.emplace<EnemyTag>(defender);
  registry.emplace<CombatStats>(defender);

  auto &suppressor = registry.emplace<SuppressorComponent>(defender);
  suppressor.threshold = 300.0f;
  suppressor.damageReduction = 0.9f;

  DamagePool pool;
  pool.values[0] = 100.0f; // 100 Phys damage

  // We use a clean environment - no stats, no modifiers.
  // DamagePipeline should use defaults if stats are missing.

  // Calculate damage (NOT simulation, but with controlled entities)
  auto result = DamagePipeline::Calculate(registry, attacker, defender, 0, pool,
                                          Tag::Hit);

  // Expect 90% reduction (100 -> 10)
  CHECK(result.total_damage == doctest::Approx(10.0f));

  // Move closer (within 300px)
  registry.replace<Position>(attacker, 200.0f, 200.0f); // Distance = 141px

  result = DamagePipeline::Calculate(registry, attacker, defender, 0, pool,
                                     Tag::Hit);

  // Expect full damage (100)
  CHECK(result.total_damage == doctest::Approx(100.0f));
}

TEST_CASE("[Unit] MonsterAffix - Suppressor update behavior-op path initializes suppressor component") {
  entt::registry registry;

  auto defender = registry.create();
  registry.emplace<Position>(defender, 100.0f, 100.0f);
  auto &affix = registry.emplace<MonsterAffixComponent>(defender);
  affix.AddAffix(MonsterAffixType::Suppressor);
  affix.hasUpdate = false;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 150.0f, 150.0f);

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  REQUIRE(registry.all_of<SuppressorComponent>(defender));

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 500.0f, 500.0f);
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.crit_chance = 0.0f;
  registry.emplace<CombatStats>(defender);

  DamagePool pool;
  pool.values[0] = 100.0f;

  const auto result =
      DamagePipeline::Calculate(registry, attacker, defender, 0, pool, Tag::Hit);
  CHECK(result.total_damage == doctest::Approx(10.0f));
}

TEST_CASE("[Unit] MonsterAffix - Avenger on-death behavior-op path grants nearby stack") {
  entt::registry registry;

  auto avenger = registry.create();
  registry.emplace<Position>(avenger, 100.0f, 100.0f);
  registry.emplace<EnemyTag>(avenger);

  auto &affix = registry.emplace<MonsterAffixComponent>(avenger);
  affix.AddAffix(MonsterAffixType::Avenger);
  affix.hasOnDeath = false;

  auto victim = registry.create();
  registry.emplace<Position>(victim, 120.0f, 120.0f);
  registry.emplace<EnemyTag>(victim);

  MonsterAffixSystem::OnEnemyDeath(registry, victim);

  REQUIRE(registry.all_of<AvengerComponent>(avenger));
  const auto &avengerComp = registry.get<AvengerComponent>(avenger);
  CHECK(avengerComp.avengerStacks == 1);
}

TEST_CASE("[Unit] MonsterAffix - OnDeath applies SoulEater and Avenger once each for mixed reactor") {
  entt::registry registry;

  auto reactor = registry.create();
  registry.emplace<Position>(reactor, 100.0f, 100.0f);

  auto &reactorAffix = registry.emplace<MonsterAffixComponent>(reactor);
  reactorAffix.AddAffix(MonsterAffixType::SoulEater);
  reactorAffix.AddAffix(MonsterAffixType::Avenger);
  reactorAffix.hasOnDeath = false;

  auto &soulEater = registry.emplace<SoulEaterComponent>(reactor);
  soulEater.stacks = 0;
  auto &avenger = registry.emplace<AvengerComponent>(reactor);
  avenger.avengerStacks = 0;

  auto victim = registry.create();
  registry.emplace<Position>(victim, 120.0f, 120.0f);

  MonsterAffixSystem::OnEnemyDeath(registry, victim);

  CHECK(registry.get<SoulEaterComponent>(reactor).stacks == 1);
  CHECK(registry.get<AvengerComponent>(reactor).avengerStacks == 1);
}

TEST_CASE("[Unit] MonsterAffix - SoulLink update behavior-op path initializes soul-link component") {
  entt::registry registry;

  auto linker = registry.create();
  registry.emplace<Position>(linker, 100.0f, 100.0f);
  auto &affix = registry.emplace<MonsterAffixComponent>(linker);
  affix.AddAffix(MonsterAffixType::SoulLink);
  affix.hasUpdate = false;

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 120.0f, 120.0f);

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  CHECK(registry.all_of<SoulLinkComponent>(linker));
}

TEST_CASE("[Unit] MonsterAffix - Suppressor op-path update and component fallback damage apply exactly once") {
  entt::registry registry;

  auto defender = registry.create();
  registry.emplace<Position>(defender, 100.0f, 100.0f);
  auto &affix = registry.emplace<MonsterAffixComponent>(defender);
  affix.AddAffix(MonsterAffixType::Suppressor);
  affix.AddAffix(MonsterAffixType::Suppressor); // duplicate affix hardening case
  affix.hasUpdate = false;
  registry.emplace<EnemyTag>(defender);
  registry.emplace<CombatStats>(defender);

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 120.0f, 120.0f);

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  REQUIRE(registry.all_of<SuppressorComponent>(defender));

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 500.0f, 500.0f);
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.crit_chance = 0.0f;

  DamagePool pool;
  pool.values[0] = 100.0f;

  const auto result =
      DamagePipeline::Calculate(registry, attacker, defender, 0, pool, Tag::Hit);

  CHECK(result.total_damage == doctest::Approx(10.0f));
}

TEST_CASE("[Unit] MonsterAffix - SoulLink op-path update and component fallback damage apply exactly once") {
  entt::registry registry;

  auto linker = registry.create();
  registry.emplace<Position>(linker, 100.0f, 100.0f);
  registry.emplace<EnemyTag>(linker);
  registry.emplace<HealthComponent>(linker, 100.0f, 100.0f);
  auto &affix = registry.emplace<MonsterAffixComponent>(linker);
  affix.AddAffix(MonsterAffixType::SoulLink);
  affix.AddAffix(MonsterAffixType::SoulLink); // duplicate affix hardening case
  affix.hasUpdate = false;

  auto allyA = registry.create();
  registry.emplace<Position>(allyA, 110.0f, 100.0f);
  registry.emplace<EnemyTag>(allyA);
  registry.emplace<HealthComponent>(allyA, 100.0f, 100.0f);

  auto allyB = registry.create();
  registry.emplace<Position>(allyB, 90.0f, 100.0f);
  registry.emplace<EnemyTag>(allyB);
  registry.emplace<HealthComponent>(allyB, 100.0f, 100.0f);

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 120.0f, 120.0f);

  NoMoreDay::systems::SpatialHashGrid dummyGrid(10, 10, 100.0f);
  MonsterAffixSystem::Update(registry, 0.0f, dummyGrid);

  REQUIRE(registry.all_of<SoulLinkComponent>(linker));
  REQUIRE(registry.all_of<SoulLinkTag>(linker));

  auto &link = registry.get<SoulLinkComponent>(linker);
  link.linkedEntities = {allyA, allyB};

  const bool distributed =
      EliteModifierSystem::DistributeDamageToLinkGroup(registry, linker, 30.0f);
  REQUIRE(distributed);
  CHECK(registry.get<HealthComponent>(linker).current == doctest::Approx(90.0f));
  CHECK(registry.get<HealthComponent>(allyA).current == doctest::Approx(90.0f));
  CHECK(registry.get<HealthComponent>(allyB).current == doctest::Approx(90.0f));

  auto fallbackOnly = registry.create();
  registry.emplace<EnemyTag>(fallbackOnly);
  registry.emplace<HealthComponent>(fallbackOnly, 100.0f, 100.0f);
  auto fallbackAlly = registry.create();
  registry.emplace<EnemyTag>(fallbackAlly);
  registry.emplace<HealthComponent>(fallbackAlly, 100.0f, 100.0f);
  auto &fallbackLink = registry.emplace<SoulLinkComponent>(fallbackOnly);
  fallbackLink.linkedEntities = {fallbackAlly};

  const bool fallbackDistributed = EliteModifierSystem::DistributeDamageToLinkGroup(
      registry, fallbackOnly, 20.0f);
  REQUIRE(fallbackDistributed);
  CHECK(registry.get<HealthComponent>(fallbackOnly).current ==
        doctest::Approx(90.0f));
  CHECK(registry.get<HealthComponent>(fallbackAlly).current ==
        doctest::Approx(90.0f));
}
