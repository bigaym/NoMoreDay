#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/MobilityDeliverySystem.hpp"
#include "game/systems/skill/BoomerangDeliverySystem.hpp"
#include "game/systems/skill/OrbitingSentinelDeliverySystem.hpp"
#include "game/systems/skill/AreaFieldDeliverySystem.hpp"
#include "game/systems/skill/BeamChannelDeliverySystem.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"

namespace NoMoreDay {

TEST_CASE("[Integration] DeliveryArchetypes - Mobility Pipeline and PhantasmClone") {
  TestSetupScope scope;
  entt::registry registry;

  const auto player = registry.create();
  registry.emplace<Position>(player, 100.0f, 100.0f);
  registry.emplace<Velocity>(player, 0.0f, 0.0f);

  auto &mob = registry.emplace<MobilityComponent>(player);
  mob.target_entity = player;
  mob.direction = {1.0f, 0.0f};
  mob.speed = 400.0f;
  mob.duration = 0.2f;
  mob.timer = 0.0f;
  mob.spawn_clone_node = 130;

  // Step 1: Advance halfway
  MobilityDeliverySystem::Update(registry, 0.1f);
  const auto &pos = registry.get<Position>(player);
  CHECK(pos.x > 130.0f);
  CHECK(registry.all_of<MobilityComponent>(player));

  // Step 2: Complete dash, verify clone spawned and MobilityComponent removed
  MobilityDeliverySystem::Update(registry, 0.15f);
  CHECK_FALSE(registry.all_of<MobilityComponent>(player));

  // Verify PhantasmClone entity was created
  auto clone_view = registry.view<PhantasmCloneComponent>();
  CHECK(clone_view.begin() != clone_view.end());

  entt::entity clone_ent = clone_view.front();
  auto &clone = clone_view.get<PhantasmCloneComponent>(clone_ent);
  CHECK(clone.creator == player);
  CHECK_FALSE(clone.has_cast);

  // Step 3: Advance past delay_before_cast
  MobilityDeliverySystem::Update(registry, 0.25f);
  CHECK(clone.has_cast);

  // Step 4: Advance past lifetime, verify clone is destroyed
  MobilityDeliverySystem::Update(registry, 2.0f);
  CHECK_FALSE(registry.valid(clone_ent));
}

TEST_CASE("[Integration] DeliveryArchetypes - Boomerang State Machine") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(128, 128, 32.0f);

  const auto owner = registry.create();
  registry.emplace<Position>(owner, 0.0f, 0.0f);
  auto &active = registry.emplace<ActiveSkillsComponent>(owner);
  active.slots[0] = SkillSlot{.id = 8, .cooldown = 2.0f, .current_charges = 0};

  const auto proj = registry.create();
  registry.emplace<Position>(proj, 50.0f, 0.0f);
  registry.emplace<Velocity>(proj, 500.0f, 0.0f);

  auto &bc = registry.emplace<BoomerangComponent>(proj);
  bc.owner = owner;
  bc.skill_id = 8;
  bc.phase = BoomerangPhase::Outward;
  bc.returnTimer = 0.1f;
  bc.hover_duration = 0.2f;
  bc.catch_by_owner = true;

  // Step 1: Reaches apex
  BoomerangDeliverySystem::Update(registry, grid, 0.15f);
  CHECK(bc.phase == BoomerangPhase::HoverApex);

  // Step 2: Finishes hover, transitions to Returning
  BoomerangDeliverySystem::Update(registry, grid, 0.25f);
  CHECK(bc.phase == BoomerangPhase::Returning);

  // Step 3: Close enough to owner, caught and triggers cooldown refund
  registry.get<Position>(proj).x = 10.0f; // within catch threshold (32px)
  BoomerangDeliverySystem::Update(registry, grid, 0.01f);
  CHECK_FALSE(registry.valid(proj)); // destroyed on catch
  CHECK(active.slots[0].cooldown < 2.0f); // refunded cooldown
}

TEST_CASE("[Integration] DeliveryArchetypes - Orbiting Sentinel Rotation") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(128, 128, 32.0f);

  const auto anchor = registry.create();
  registry.emplace<Position>(anchor, 100.0f, 100.0f);

  const auto sentinel = registry.create();
  registry.emplace<Position>(sentinel, 160.0f, 100.0f);

  auto &sc = registry.emplace<OrbitingSentinelComponent>(sentinel);
  sc.anchor_entity = anchor;
  sc.orbit_radius = 60.0f;
  sc.angular_velocity = 180.0f; // 180 deg/sec
  sc.current_angle = 0.0f;

  // Step: update 0.5s -> 90 degrees
  OrbitingSentinelDeliverySystem::Update(registry, grid, 0.5f);
  CHECK(sc.current_angle == doctest::Approx(90.0f));

  const auto &pos = registry.get<Position>(sentinel);
  CHECK(pos.x == doctest::Approx(100.0f).epsilon(1.0f));
  CHECK(pos.y == doctest::Approx(160.0f).epsilon(1.0f));
}

TEST_CASE("[Integration] DeliveryArchetypes - BeamChannel Barrage Emitter") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(128, 128, 32.0f);

  const auto caster = registry.create();
  registry.emplace<Position>(caster, 0.0f, 0.0f);

  auto &beam = registry.emplace<BeamChannelComponent>(caster);
  beam.mode = BeamChannelMode::BarrageEmitter;
  beam.target_pos = {200.0f, 0.0f};
  beam.tick_interval = 0.1f;
  beam.tick_timer = 0.0f;
  beam.max_channel_time = 1.0f;

  BeamChannelDeliverySystem::Update(registry, grid, 0.05f);

  // Should have spawned barrage projectiles
  auto proj_view = registry.view<Projectile>();
  CHECK(proj_view.begin() != proj_view.end());
}

TEST_CASE("[Integration] DeliveryArchetypes - DirectStrike Hitbox and CompactEntitySet") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(128, 128, 32.0f);

  const auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<CombatStats>(player);

  const auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 30.0f, 0.0f);
  auto &enemyStats = registry.emplace<CombatStats>(enemy);
  enemyStats.health = 100.0f;
  enemyStats.max_health = 100.0f;

  const auto strikeEnt = registry.create();
  registry.emplace<Position>(strikeEnt, 0.0f, 0.0f);
  auto &ds = registry.emplace<DirectStrikeComponent>(strikeEnt);
  ds.owner = player;
  ds.skill_id = 1;
  ds.radius = 50.0f;
  ds.lifetime = 0.2f;

  ProjectileSystem::Update(registry, grid, 0.05f);

  // Verify enemy was hit and recorded in hit_entities
  CHECK(ds.hit_entities.contains(enemy));
  CHECK(ds.hit_entities.size() == 1);
}

TEST_CASE("[Integration] DeliveryArchetypes - SkyfallImpact Spawns AreaField") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(128, 128, 32.0f);

  const auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);

  const auto skyfallEnt = registry.create();
  registry.emplace<Position>(skyfallEnt, 100.0f, 100.0f);
  auto &skyfall = registry.emplace<SkyfallImpactComponent>(skyfallEnt);
  skyfall.owner = player;
  skyfall.skill_id = 6;
  skyfall.delay_before_impact = 0.1f;
  skyfall.impact_radius = 80.0f;
  skyfall.leave_field_skill_id = 6;

  // Before delay: Skyfall exists, AreaField does not
  AreaFieldDeliverySystem::Update(registry, grid, 0.05f);
  CHECK(registry.all_of<SkyfallImpactComponent>(skyfallEnt));
  CHECK_FALSE(registry.all_of<AreaFieldComponent>(skyfallEnt));

  // After delay: Impact triggers and morphs into AreaFieldComponent
  AreaFieldDeliverySystem::Update(registry, grid, 0.1f);
  CHECK(registry.all_of<AreaFieldComponent>(skyfallEnt));
  CHECK_FALSE(registry.all_of<SkyfallImpactComponent>(skyfallEnt));

  const auto &field = registry.get<AreaFieldComponent>(skyfallEnt);
  CHECK(field.source_skill_id == 6);
  CHECK(field.radius == doctest::Approx(80.0f));
}

TEST_CASE("[Integration] DeliveryArchetypes - StickyDetonation and ReactiveWard Lifecycle") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(128, 128, 32.0f);

  // ReactiveWard timeout removal
  const auto wardEnt = registry.create();
  auto &rw = registry.emplace<ReactiveWardComponent>(wardEnt);
  rw.ward_duration = 0.2f;
  rw.counter_window = 0.1f;

  SkillSystem::Update(registry, grid, 0.1f);
  CHECK(registry.all_of<ReactiveWardComponent>(wardEnt));

  SkillSystem::Update(registry, grid, 0.15f);
  CHECK_FALSE(registry.all_of<ReactiveWardComponent>(wardEnt));

  // StickyDetonation explosion
  const auto attacker = registry.create();
  registry.emplace<PlayerTag>(attacker);
  registry.emplace<Position>(attacker, 0.0f, 0.0f);

  const auto markedEnemy = registry.create();
  registry.emplace<EnemyTag>(markedEnemy);
  registry.emplace<Position>(markedEnemy, 10.0f, 0.0f);
  registry.emplace<CombatStats>(markedEnemy);
  auto &sd = registry.emplace<StickyDetonationComponent>(markedEnemy);
  sd.attacker = attacker;
  sd.source_skill_id = 2;
  sd.timer = 0.1f;
  sd.explode_radius = 50.0f;

  ProjectileSystem::Update(registry, grid, 0.15f);
  CHECK_FALSE(registry.all_of<StickyDetonationComponent>(markedEnemy));
}

} // namespace NoMoreDay
