#include "TestCommon.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/AreaFieldDeliverySystem.hpp"
#include <type_traits>

namespace NoMoreDay {

TEST_CASE("[Unit] AreaFieldDelivery - Standard Layout and Trivially Destructible") {
  static_assert(std::is_standard_layout_v<AreaFieldComponent>);
  static_assert(std::is_trivially_destructible_v<AreaFieldComponent>);

  AreaFieldComponent comp{};
  CHECK(comp.owner == entt::entity{entt::null});
  CHECK(comp.remaining_duration == 0.0f);
  CHECK(comp.payload_count == 0);
  CHECK(comp.payloads.size() == AreaFieldComponent::kMaxFieldPayloads);
}

TEST_CASE("[Unit] AreaFieldDelivery - Lifetime and Destruction") {
  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 50.0f);

  auto fieldEnt = registry.create();
  registry.emplace<Position>(fieldEnt, 100.0f, 100.0f);
  auto &field = registry.emplace<AreaFieldComponent>(fieldEnt);
  field.remaining_duration = 1.0f;
  field.pulse_interval = 0.5f;

  // Tick 0.4s: field remains alive
  AreaFieldDeliverySystem::Update(registry, grid, 0.4f);
  CHECK(registry.valid(fieldEnt));
  CHECK(field.remaining_duration == doctest::Approx(0.6f));

  // Tick 0.7s: duration expires (0.6 - 0.7 < 0), field should be destroyed
  AreaFieldDeliverySystem::Update(registry, grid, 0.7f);
  CHECK_FALSE(registry.valid(fieldEnt));
}

TEST_CASE("[Unit] AreaFieldDelivery - Target Query and Pulse Delivery") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  DamageResolutionHooks hooks;
  hooks.execute = [](entt::registry &reg, const DamageRequest &req, entt::entity target) {
    return DamagePipeline::Execute(reg, req, target, false);
  };
  RegisterDamageResolutionHooks(hooks);

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 50.0f);

  auto player = registry.create();
  registry.emplace<Position>(player, 100.0f, 100.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.health = 100.0f;
  pStats.max_health = 100.0f;
  pStats.min_weapon_damage = 25.0f;
  pStats.max_weapon_damage = 35.0f;

  // Create an enemy in range (at 120, 100, distance 20 < radius 50)
  auto enemyInRange = registry.create();
  registry.emplace<Position>(enemyInRange, 120.0f, 100.0f);
  registry.emplace<EnemyTag>(enemyInRange);
  auto &eHp = registry.emplace<HealthComponent>(enemyInRange, 100.0f, 100.0f);
  auto &eStats = registry.emplace<CombatStats>(enemyInRange);
  eStats.health = 100.0f;
  eStats.max_health = 100.0f;

  // Create an enemy out of range (at 200, 200, distance > 50)
  auto enemyOutOfRange = registry.create();
  registry.emplace<Position>(enemyOutOfRange, 200.0f, 200.0f);
  registry.emplace<EnemyTag>(enemyOutOfRange);
  auto &eHpOut = registry.emplace<HealthComponent>(enemyOutOfRange, 100.0f, 100.0f);
  auto &eStatsOut = registry.emplace<CombatStats>(enemyOutOfRange);
  eStatsOut.health = 100.0f;
  eStatsOut.max_health = 100.0f;

  grid.rebuild(registry.view<Position>(), registry);

  // Create area field
  auto fieldEnt = registry.create();
  registry.emplace<Position>(fieldEnt, 100.0f, 100.0f);
  auto &field = registry.emplace<AreaFieldComponent>(fieldEnt);
  field.owner = player;
  field.source_skill_id = 11; // Heavenly Sword
  field.remaining_duration = 5.0f;
  field.pulse_interval = 0.5f;
  field.timer = 0.0f;
  field.radius = 50.0f;

  PayloadDefinition dmgPayload{};
  dmgPayload.type = PayloadType::Damage;
  dmgPayload.value_mult = 1.0f;
  dmgPayload.damage_tags = Tag::Physical | Tag::Area;
  field.payloads[0] = dmgPayload;
  field.payload_count = 1;

  // Advance by 0.2s: not reached pulse_interval (0.5s)
  AreaFieldDeliverySystem::Update(registry, grid, 0.2f);
  CHECK(eHp.current == 100.0f);
  CHECK(eHpOut.current == 100.0f);

  // Advance by 0.35s: total 0.55s, trigger pulse!
  AreaFieldDeliverySystem::Update(registry, grid, 0.35f);
  CHECK(eHp.current < 100.0f);       // Hit by area field
  CHECK(eHpOut.current == 100.0f);   // Out of range, unaffected
}

TEST_CASE("[Unit] AreaFieldDelivery - Enemy Field Relative Hostility and Self Protection") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  DamageResolutionHooks hooks;
  hooks.execute = [](entt::registry &reg, const DamageRequest &req, entt::entity target) {
    return DamagePipeline::Execute(reg, req, target, false);
  };
  RegisterDamageResolutionHooks(hooks);

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 50.0f);

  // Create an enemy caster (owner of the area field)
  auto enemyOwner = registry.create();
  registry.emplace<Position>(enemyOwner, 100.0f, 100.0f);
  registry.emplace<EnemyTag>(enemyOwner);
  auto &enemyOwnerHp = registry.emplace<HealthComponent>(enemyOwner, 100.0f, 100.0f);
  auto &enemyOwnerStats = registry.emplace<CombatStats>(enemyOwner);
  enemyOwnerStats.health = 100.0f;
  enemyOwnerStats.max_health = 100.0f;
  enemyOwnerStats.min_weapon_damage = 20.0f;
  enemyOwnerStats.max_weapon_damage = 30.0f;

  // Create another enemy near the field
  auto otherEnemy = registry.create();
  registry.emplace<Position>(otherEnemy, 110.0f, 100.0f);
  registry.emplace<EnemyTag>(otherEnemy);
  auto &otherEnemyHp = registry.emplace<HealthComponent>(otherEnemy, 100.0f, 100.0f);
  auto &otherEnemyStats = registry.emplace<CombatStats>(otherEnemy);
  otherEnemyStats.health = 100.0f;
  otherEnemyStats.max_health = 100.0f;

  // Create player in range
  auto player = registry.create();
  registry.emplace<Position>(player, 120.0f, 100.0f);
  auto &playerHp = registry.emplace<HealthComponent>(player, 100.0f, 100.0f);
  auto &playerStats = registry.emplace<CombatStats>(player);
  playerStats.health = 100.0f;
  playerStats.max_health = 100.0f;

  grid.rebuild(registry.view<Position>(), registry);

  // Create enemy-owned area field
  auto fieldEnt = registry.create();
  registry.emplace<Position>(fieldEnt, 100.0f, 100.0f);
  auto &field = registry.emplace<AreaFieldComponent>(fieldEnt);
  field.owner = enemyOwner;
  field.remaining_duration = 5.0f;
  field.pulse_interval = 0.2f;
  field.radius = 50.0f;

  PayloadDefinition dmgPayload{};
  dmgPayload.type = PayloadType::Damage;
  dmgPayload.value_mult = 1.0f;
  dmgPayload.damage_tags = Tag::Physical | Tag::Area;
  field.payloads[0] = dmgPayload;
  field.payload_count = 1;

  // Trigger pulse
  AreaFieldDeliverySystem::Update(registry, grid, 0.25f);

  // Player should be hit!
  CHECK(playerHp.current < 100.0f);
  // Enemy owner and other enemy should NOT be hit!
  CHECK(enemyOwnerHp.current == 100.0f);
  CHECK(otherEnemyHp.current == 100.0f);
}

} // namespace NoMoreDay
