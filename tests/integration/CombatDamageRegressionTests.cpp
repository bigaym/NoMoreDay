#pragma once

#include "TestCommon.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Combat.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"

namespace NoMoreDay {

TEST_CASE("[Integration] CombatDamageRegression - Player melee HP delta matches pipeline") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(128, 128, 32.0f);

  const auto attacker = registry.create();
  registry.emplace<PlayerTag>(attacker);
  auto &input = registry.emplace<InputComponent>(attacker);
  input.attack = true;
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  registry.emplace<AttackState>(attacker).cooldownTimer = 0.0f;
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.min_weapon_damage = 64.0f;
  attackerStats.max_weapon_damage = 64.0f;
  attackerStats.crit_chance = 0.0f;
  attackerStats.accuracy = 1.0f;

  const auto target = registry.create();
  registry.emplace<EnemyTag>(target);
  registry.emplace<Position>(target, 20.0f, 0.0f);
  registry.emplace<HealthComponent>(target, 300.0f, 300.0f);
  auto &targetStats = registry.emplace<CombatStats>(target);
  targetStats.dodge_chance = 0.0f;
  targetStats.block_chance = 0.0f;

  DamageRequest expectedReq;
  expectedReq.attacker = attacker;
  expectedReq.defender = target;
  expectedReq.skill_id = 0;
  expectedReq.base_pool.Add(Tag::Physical, attackerStats.min_weapon_damage);
  expectedReq.additional_tags = Tag::Melee | Tag::Hit;
  expectedReq.is_simulation = true;
  const float expectedDamage =
      DamagePipeline::Calculate(registry, expectedReq).total_damage;

  const float hpBefore = registry.get<HealthComponent>(target).current;
  grid.rebuild(registry.view<Position>(), registry);

  Camera2D camera = {};
  camera.zoom = 1.0f;
  CombatSystem::update(registry, grid, camera, 0.016f);

  const float hpAfter = registry.get<HealthComponent>(target).current;
  CHECK((hpBefore - hpAfter) ==
        doctest::Approx(expectedDamage).epsilon(0.0001f));
}

TEST_CASE("[Integration] CombatDamageRegression - AI attack HP delta matches pipeline") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(128, 128, 32.0f);

  const auto target = registry.create();
  registry.emplace<PlayerTag>(target);
  registry.emplace<Position>(target, 10.0f, 0.0f);
  registry.emplace<HealthComponent>(target, 400.0f, 400.0f);
  auto &targetStats = registry.emplace<CombatStats>(target);
  targetStats.dodge_chance = 0.0f;
  targetStats.block_chance = 0.0f;

  const auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  auto &ai = registry.emplace<AIComponent>(enemy);
  ai.aiType = AIType::ATTACK;
  ai.attackRange = 50.0f;
  ai.target = target;
  registry.emplace<Position>(enemy, 0.0f, 0.0f);
  registry.emplace<AttackState>(enemy).cooldownTimer = 0.0f;
  auto &enemyStats = registry.emplace<CombatStats>(enemy);
  enemyStats.min_weapon_damage = 45.0f;
  enemyStats.max_weapon_damage = 45.0f;
  enemyStats.crit_chance = 0.0f;
  enemyStats.accuracy = 1.0f;

  DamageRequest expectedReq;
  expectedReq.attacker = enemy;
  expectedReq.defender = target;
  expectedReq.skill_id = 0;
  expectedReq.base_pool.Add(Tag::Physical, 45.0f);
  expectedReq.additional_tags = Tag::Melee | Tag::Hit;
  expectedReq.is_simulation = true;
  const float expectedDamage =
      DamagePipeline::Calculate(registry, expectedReq).total_damage;

  const float hpBefore = registry.get<HealthComponent>(target).current;
  Camera2D camera = {};
  camera.zoom = 1.0f;
  CombatSystem::update(registry, grid, camera, 0.016f);
  const float hpAfter = registry.get<HealthComponent>(target).current;

  CHECK((hpBefore - hpAfter) ==
        doctest::Approx(expectedDamage).epsilon(0.0001f));
}

TEST_CASE("[Integration] CombatDamageRegression - Projectile hit HP delta matches pipeline") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(128, 128, 32.0f);

  const auto attacker = registry.create();
  registry.emplace<PlayerTag>(attacker);
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.crit_chance = 0.0f;

  const auto target = registry.create();
  registry.emplace<EnemyTag>(target);
  registry.emplace<Position>(target, 10.0f, 0.0f);
  registry.emplace<HealthComponent>(target, 150.0f, 150.0f);
  auto &targetStats = registry.emplace<CombatStats>(target);
  targetStats.block_chance = 0.0f;
  targetStats.dodge_chance = 0.0f;

  const auto projectile = registry.create();
  registry.emplace<Position>(projectile, 10.0f, 0.0f);
  registry.emplace<Velocity>(projectile, 0.0f, 0.0f);
  auto &proj = registry.emplace<Projectile>(projectile);
  proj.owner = attacker;
  proj.speed = 0.0f;
  proj.radius = 18.0f;
  proj.lifeTime = 1.0f;
  proj.snapshot = attackerStats;
  registry.emplace<CombatStats>(projectile, proj.snapshot);
  auto &skill = registry.emplace<SkillComponent>(projectile);
  skill.skill_id = 0u;
  skill.owner = attacker;

  DamagePool emptyPool;
  const auto simulated = DamagePipeline::Calculate(
      registry, projectile, target, 0, emptyPool, Tag::Projectile | Tag::Hit,
      projectile, true);
  const float expectedDamage = simulated.total_damage;

  const float hpBefore = registry.get<HealthComponent>(target).current;
  ProjectileSystem::Update(registry, grid, 0.016f);
  const float hpAfter = registry.get<HealthComponent>(target).current;

  CHECK((hpBefore - hpAfter) ==
        doctest::Approx(expectedDamage).epsilon(0.0001f));
}

} // namespace NoMoreDay
