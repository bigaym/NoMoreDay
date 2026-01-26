#include "TestCommon.hpp"
#include "engine/physics/SpatialGrid.hpp" // For SpatialHashGrid
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp" // Added this include
#include "game/systems/combat/MonsterAffixSystem.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/world/EnemySpawnSystem.hpp"


using namespace NoMoreDay;

TEST_CASE("Monster Affix Persistence Test") {
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

TEST_CASE("Monster Affix Stat Mod Persistence Test") {
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

TEST_CASE("Monster Affix: Mirror Image Logic Test") {
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
  affix.hasOnHit = true;
  affix.mirrorCooldown = 0.0f; // Reset cooldown

  // Trigger Mirror Image via HP threshold
  auto &hp = registry.get<HealthComponent>(enemy);
  hp.current = 40.0f; // 40% HP

  CombatEvent evt;
  evt.target = enemy;

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
}

TEST_CASE("Monster Affix: Soul Eater Progression Test") {
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

TEST_CASE("Monster Affix: Suppressor Damage Reduction Test") {
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
