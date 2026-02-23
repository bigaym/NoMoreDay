#pragma once
#include "TestCommon.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include "engine/render/UIRenderer.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/SummonSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include <entt/entt.hpp>
#include <unordered_set>


namespace NoMoreDay {

TEST_CASE("[Integration] SkillSystem - Registry Loading") {
  LoggerScope scope;
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  SUBCASE("Load Flowing Thrust") {
    const auto *skill = registry.GetSkill(1);
    REQUIRE(skill != nullptr);
    CHECK(skill->name_key == "流云刺");
    CHECK(skill->mana_cost == 5.0f);
    CHECK(HasTag(skill->tags, Tag::Physical));
  }

  SUBCASE("Load Rending Wave") {
    const auto *skill = registry.GetSkill(2);
    REQUIRE(skill != nullptr);
    CHECK(HasTag(skill->tags, Tag::Projectile));
    CHECK(skill->max_charges == 3);
  }
}

TEST_CASE("[Integration] SkillSystem - Execution Logic") {
  LoggerScope scope;
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  CombatEventDispatcher::Init();
  systems::SpatialHashGrid grid(100, 100, 50);

  auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.mana = 100.0f;
  active.slots[0].id = 1;

  SUBCASE("Charges and Cooldown") {
    active.slots[0].current_charges = 1;
    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(active.slots[0].current_charges == 0);

    SkillSystem::Update(registry, grid, 0.5f);
    CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));
  }

  SUBCASE("State Machine & Callback") {
    auto oldCast = SkillBehaviorRegistry::GetCast(1);
    SkillBehaviorRegistry::RegisterCast(1, nullptr);

    bool effect_triggered = false;
    SkillSystem::RegisterEffect(
        1, [&](entt::registry &, entt::entity, SkillExecution &) {
          effect_triggered = true;
        });

    active.slots[0].current_charges = 1;
    SkillSystem::TryCast(registry, player, 0);
    SkillSystem::Update(registry, grid, 0.11f);
    CHECK(effect_triggered);

    SkillBehaviorRegistry::RegisterCast(1, oldCast);
  }
}

TEST_CASE("[Integration] SkillSystem - Sword Intent Logic") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  CombatEventDispatcher::Init();
  SkillSystem::InitHooks();
  systems::SpatialHashGrid grid(100, 100, 50);

  auto player = registry.create();
  auto &intent = registry.emplace<SwordIntentComponent>(player);
  intent.decay_interval = 1.0f;
  intent.grace_period = 0.1f;

  SUBCASE("Accumulation") {
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, entt::null, 1,
                      Tag::Melee | Tag::Physical | Tag::Hit, false));
    CHECK(intent.stacks == 1);
  }

  SUBCASE("Decay Logic") {
    intent.stacks = 5;
    intent.grace_period = 1.0f;
    intent.time_since_last_gain = 0.0f;

    // Update roughly 0.5s - shouldn't decay
    // Need to pass executor pointer as nullptr since it's now required in
    // Update signature? Let's check SkillSystem::Update signature. void
    // Update(entt::registry &registry, systems::SpatialHashGrid &grid, float
    // dt, tf::Executor *executor = nullptr); It seems it usually takes
    // executor.
    SkillSystem::Update(registry, grid, 0.5f);
    CHECK(intent.stacks == 5);

    // Update another 0.6s -> total 1.1s > 1.0s grace -> clear all
    SkillSystem::Update(registry, grid, 0.6f);
    CHECK(intent.stacks == 0);
  }

  SUBCASE("No Passive Gain") {
    intent.stacks = 0;
    intent.gain_rate = 1.0f;

    SkillSystem::Update(registry, grid, 2.0f); // 2 seconds
    CHECK(intent.stacks == 0);
  }

  SUBCASE("Empowered Cast") {
    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    active.slots[0].id = 1;
    active.slots[0].current_charges = 1;
    intent.stacks = 10;

    SkillSystem::TryCast(registry, player, 0);
    SkillSystem::Update(registry, grid, 0.11f);

    CHECK(intent.stacks == 0);
    // SkillExecution might be finished (removed) by now, so we can't reliably check it.
    // But stacks == 0 implies the hook ran.
  }
}

TEST_CASE("[Integration] SkillSpecialization - Talent Allocation Logic") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.available_talent_points = 10;
  active.specialized_slots[0].skill_id = 1;

  SUBCASE("Point Allocation and Reset") {
    CHECK(SkillSystem::AddTalentPoint(registry, player, 1, 100));
    CHECK(active.available_talent_points == 9);
    CHECK(SkillSystem::ResetTalents(registry, player, 1));
    CHECK(active.available_talent_points == 10);
  }
}

TEST_CASE("[Integration] SkillSpecialization - Runtime state cleanup on reset/clear") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.available_talent_points = 0;

  SUBCASE("ResetTalents clears target skill runtime state only") {
    active.specialized_slots[0].skill_id = 1;
    active.specialized_slots[0].allocated_points[114] = 1;
    active.specialized_slots[0].allocated_points[170] = 1;
    active.specialized_slots[1].skill_id = 2;
    active.specialized_slots[1].allocated_points[233] = 1;

    auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
    runtime.active_transmuter_node_by_skill[1] = 170;
    runtime.active_transmuter_node_by_skill[2] = 270;
    runtime.trigger_cooldowns[114] = 1.5f;
    runtime.trigger_cooldowns[233] = 2.0f;

    CHECK(SkillSystem::ResetTalents(registry, player, 1));
    CHECK(active.available_talent_points == 2);
    CHECK_FALSE(runtime.active_transmuter_node_by_skill.contains(1));
    CHECK(runtime.active_transmuter_node_by_skill.contains(2));
    CHECK_FALSE(runtime.trigger_cooldowns.contains(114));
    CHECK(runtime.trigger_cooldowns.contains(233));
  }

  SUBCASE("ClearAllTalents clears all specialization runtime state") {
    active.specialized_slots[0].skill_id = 1;
    active.specialized_slots[0].allocated_points[114] = 1;
    active.specialized_slots[1].skill_id = 8;
    active.specialized_slots[1].allocated_points[870] = 1;
    active.specialized_slots[1].allocated_points[871] = 1;

    auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
    runtime.active_transmuter_node_by_skill[1] = 170;
    runtime.active_transmuter_node_by_skill[8] = 870;
    runtime.trigger_cooldowns[114] = 1.0f;
    runtime.trigger_cooldowns[831] = 2.0f;

    CHECK(SkillSystem::ClearAllTalents(registry, player));
    CHECK(active.available_talent_points == 3);
    CHECK(runtime.active_transmuter_node_by_skill.empty());
    CHECK(runtime.trigger_cooldowns.empty());
  }
}

TEST_CASE("[Integration] SkillLogic - Specialized Behaviors") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillSystem::InitHooks();
  systems::SpatialHashGrid grid(1000, 1000, 50);
  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<CombatStats>(player);

  SUBCASE("Boomerang") {
    auto proj_ent = registry.create();
    registry.emplace<Position>(proj_ent, 100.0f, 0.0f);
    registry.emplace<Velocity>(proj_ent, 100.0f, 0.0f);
    registry.emplace<Projectile>(proj_ent).owner = player;
    auto &bc = registry.emplace<BoomerangComponent>(proj_ent);
    bc.phase = BoomerangComponent::Outward;
    bc.returnTimer = 0.5f;

    // Total time needed: 0.5s (timer) + 0.2s (pause) = 0.7s. 
    // Two updates to ensure state transitions (Outward -> Paused -> Returning)
    ProjectileSystem::Update(registry, grid, 0.6f); // Outward -> Paused
    ProjectileSystem::Update(registry, grid, 0.3f); // Paused -> Returning
    CHECK(bc.phase == BoomerangComponent::Returning);
  }

  SUBCASE("Channeling - Infinite Blades") {
    auto &chan = registry.emplace<ChannelingComponent>(player);
    chan.skill_id = 5;
    chan.channel_timer = 1.0f;
    chan.tick_interval = 0.1f;
    chan.tick_timer = 0.1f;

    SkillSystem::Update(registry, grid, 0.15f);
    CHECK(!registry.view<Projectile>().empty());
  }

  SUBCASE("Blade Formation - Talent 321") {
    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 3;
    active.specialized_slots[0].allocated_points[321] = 1;

    auto &formation = registry.emplace<BladeFormationComponent>(player);
    formation.mana_on_hit = true; // Manually set flag normally set in DoCast

    registry.get<CombatStats>(player).mana = 10.0f;
    CombatEventDispatcher::Init();
    SkillSystem::InitHooks(); // Re-register behavior dispatcher
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(player, entt::null, 3,
                                                     Tag::Hit, false));
    CHECK(registry.get<CombatStats>(player).mana == 12.0f);
  }
}

TEST_CASE("[Integration] Projectile - Snapshotting Logic") {
  entt::registry registry;
  auto attacker = registry.create();
  auto defender = registry.create();
  auto proj_ent = registry.create();

  auto &a_stats = registry.emplace<CombatStats>(attacker);
  a_stats.damage_multipliers[0] = 1.5f;

  auto &proj = registry.emplace<Projectile>(proj_ent);
  proj.owner = attacker;
  proj.snapshot = a_stats;
  registry.emplace<CombatStats>(proj_ent, proj.snapshot);

  a_stats.damage_multipliers[0] = 0.1f; // Attacker weakened

  CHECK(registry.get<CombatStats>(proj_ent).damage_multipliers[0] ==
        doctest::Approx(1.5f));
}

TEST_CASE("[Integration] SkillSystem - Phantom Flash Counter Uses Pipeline") {
  entt::registry registry;
  CombatEventDispatcher::Clear();
  SkillSystem::InitHooks();

  auto victim = registry.create();
  registry.emplace<Position>(victim, 0.0f, 0.0f);
  registry.emplace<CombatStats>(victim);
  registry.emplace<HealthComponent>(victim, 100.0f, 100.0f);
  auto &pf = registry.emplace<PhantomFlashComponent>(victim);
  pf.counter_window = 0.5f;
  pf.triggered = false;

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 5.0f, 0.0f);
  registry.emplace<CombatStats>(attacker);
  registry.emplace<HealthComponent>(attacker, 100.0f, 100.0f);

  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateTakeDamage(
                    victim, attacker, 9, Tag::Melee, 10.0f, false));

  CHECK(pf.triggered);
  CHECK(registry.get<HealthComponent>(attacker).current < 100.0f);

  auto victim2 = registry.create();
  registry.emplace<Position>(victim2, 0.0f, 0.0f);
  registry.emplace<CombatStats>(victim2);
  auto &pf2 = registry.emplace<PhantomFlashComponent>(victim2);
  pf2.counter_window = 0.5f;
  pf2.triggered = false;

  auto noStatsAttacker = registry.create();
  registry.emplace<Position>(noStatsAttacker, 5.0f, 0.0f);
  registry.emplace<HealthComponent>(noStatsAttacker, 80.0f, 80.0f);

  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateTakeDamage(
                    victim2, noStatsAttacker, 9, Tag::Melee, 10.0f, false));

  CHECK(pf2.triggered);
  CHECK(registry.get<HealthComponent>(noStatsAttacker).current ==
        doctest::Approx(80.0f));
}

TEST_CASE(
    "[Integration] SkillBehaviorRegistry - Blade Ascendant key branches run") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  CombatEventDispatcher::Clear();
  SkillSystem::InitHooks();

  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<Velocity>(player, 0.0f, 0.0f);
  registry.emplace<CombatStats>(player).mana = 500.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.specialized_slots[0].skill_id = 2;
  active.specialized_slots[0].allocated_points[230] = 1; // Rending boomerang
  active.specialized_slots[1].skill_id = 8;
  active.specialized_slots[1].allocated_points[813] = 1; // Boomerang split

  for (uint32_t skill_id = 1; skill_id <= 9; ++skill_id) {
    CAPTURE(skill_id);
    CHECK(SkillBehaviorRegistry::HasBehavior(skill_id));
  }

  SUBCASE("Rending Wave boomerang branch creates boomerang projectiles") {
    SkillExecution exec;
    exec.skill_id = 2;
    exec.owner = player;
    exec.cast_id = 501;
    exec.target_pos = {120.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(2);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto boomerangs = registry.view<Projectile, BoomerangComponent>();
    CHECK(boomerangs.begin() != boomerangs.end());
  }

  SUBCASE("Blade Formation giant sword branch toggles giant state") {
    SkillExecution exec;
    exec.skill_id = 3;
    exec.owner = player;
    exec.cast_id = 502;
    exec.target_pos = {40.0f, 0.0f};
    exec.active_nodes.set(330 % 100);

    auto cast = SkillBehaviorRegistry::GetCast(3);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    REQUIRE(registry.all_of<BladeFormationComponent>(player));
    CHECK(registry.get<BladeFormationComponent>(player).has_giant_sword);
  }

  SUBCASE("Blade Boomerang phantom spin branch emits multi projectiles") {
    SkillExecution exec;
    exec.skill_id = 8;
    exec.owner = player;
    exec.cast_id = 503;
    exec.target_pos = {150.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(8);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto projectiles = registry.view<Projectile, BoomerangComponent>();
    int count = 0;
    for (auto entity : projectiles) {
      (void)entity;
      ++count;
    }
    CHECK(count >= 3);
  }

  SUBCASE("Phantom Flash main branch enters counter window") {
    SkillExecution exec;
    exec.skill_id = 9;
    exec.owner = player;
    exec.cast_id = 504;
    exec.target_pos = {80.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(9);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    REQUIRE(registry.all_of<PhantomFlashComponent>(player));
    CHECK(registry.get<PhantomFlashComponent>(player).counter_window > 0.0f);
  }
}

TEST_CASE("[Integration] SkillSystem - Boomerang Catch node restores resources") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  systems::SpatialHashGrid grid(100, 100, 50);

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.mana = 10.0f;
  stats.max_mana = 100.0f;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 8;
  active.slots[0].cooldown = 2.0f;
  active.specialized_slots[0].skill_id = 8;
  active.specialized_slots[0].allocated_points[831] = 2;

  auto projEnt = registry.create();
  registry.emplace<Position>(projEnt, 0.5f, 0.0f);
  registry.emplace<Velocity>(projEnt, -10.0f, 0.0f);
  auto &proj = registry.emplace<Projectile>(projEnt);
  proj.owner = player;
  proj.radius = 12.0f;
  proj.speed = 400.0f;
  proj.lifeTime = 2.0f;
  registry.emplace<SkillComponent>(projEnt, 8u, player);
  auto &boom = registry.emplace<BoomerangComponent>(projEnt);
  boom.owner = player;
  boom.phase = BoomerangComponent::Returning;
  boom.returnSpeed = 400.0f;

  ProjectileSystem::Update(registry, grid, 0.016f);

  CHECK(stats.mana > 10.0f);
  CHECK(active.slots[0].cooldown < 2.0f);
  CHECK_FALSE(registry.valid(projEnt));
}

TEST_CASE("[Integration] SkillSystem - BladeWard interception counter loop") {
  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 50);

  auto defender = registry.create();
  registry.emplace<PlayerTag>(defender);
  registry.emplace<Position>(defender, 0.0f, 0.0f);
  registry.emplace<CombatStats>(defender);
  registry.emplace<SwordIntentComponent>(defender).stacks = 0;
  auto &ward = registry.emplace<BladeWardComponent>(defender);
  ward.sword_count = 2;
  ward.interception_chance = 1.0f;
  ward.trigger_counter = true;
  ward.has_blink_counter = true;
  ward.has_agile_counter = true;
  ward.has_rainbow_qi = true;

  auto attacker = registry.create();
  registry.emplace<EnemyTag>(attacker);
  registry.emplace<Position>(attacker, 2.0f, 0.0f);
  registry.emplace<CombatStats>(attacker);
  registry.emplace<HealthComponent>(attacker, 100.0f, 100.0f);

  auto projEnt = registry.create();
  registry.emplace<Position>(projEnt, 0.0f, 0.0f);
  registry.emplace<Velocity>(projEnt, 0.0f, 0.0f);
  auto &proj = registry.emplace<Projectile>(projEnt);
  proj.owner = attacker;
  proj.radius = 20.0f;
  proj.speed = 0.0f;
  proj.lifeTime = 1.0f;
  registry.emplace<SkillComponent>(projEnt, 2u, attacker);

  ProjectileSystem::Update(registry, grid, 0.016f);

  CHECK(registry.get<HealthComponent>(attacker).current < 100.0f);
  CHECK(registry.get<SwordIntentComponent>(defender).stacks >= 1);
}

} // namespace NoMoreDay

TEST_CASE("[Bugfix] SkillSystem - UAF Reproduction / Reallocation Safety") {
  // This test ensures that if a hook adds many new entities with SkillExecution
  // (triggering a pool reallocation), the main loop in UpdateStates doesn't
  // crash or access invalid memory.

  entt::registry registry;
  SkillSystem::InitHooks();
  systems::SpatialHashGrid grid(100, 100, 50);

  // 1. Setup a single preparing skill
  auto entity = registry.create();
  auto &exec = registry.emplace<SkillExecution>(entity);
  exec.skill_id = 999; // Dummy ID
  exec.state = SkillState::Preparing;
  exec.timer = 0.0f;
  exec.owner = entity;

  // 2. Add a malicious hook that forces reallocation
  SkillSystem::ClearHooks();
  SkillSystem::AddPreCastHook([](entt::registry &r, entt::entity e,
                                 SkillExecution &ex) {
    // Force reallocation of SkillExecution pool by adding MANY new entities
    // Default page size for EnTT is often 4096 or similar, so adding enough
    // should trigger it. Or just `reserve` significantly more if possible, but
    // creating is surer.
    for (int i = 0; i < 2000; ++i) {
      auto newEnt = r.create();
      auto &newExec = r.emplace<SkillExecution>(newEnt);
      newExec.skill_id = 1;
      newExec.timer = 100.0f; // Don't trigger recursively immediately
    }

    // Also modify the original execution to verify write access still works (if
    // we have reference) If UAF occurs, 'ex' might be dangling here if it was a
    // reference to the old pool! Wait, 'ex' is passed by reference from
    // UpdateStates. If UpdateStates holds a reference to an element in the
    // pool, and the pool moves, 'ex' is invalid. We can't easily detect the
    // crash inside the hook (it happens when we access ex OR when the loop
    // continues).

    ex.timer =
        5.0f; // Write to potentially dangling reference?
              // Actually, EnTT reference stability depends on storage type.
              // Standard component view invalidates references on reallocation.
  });

  // 3. Trigger Update
  // If UAF is present, this might crash or corrupt memory (detected by
  // ASAN/valgrind, or crash)
  SkillSystem::UpdateStates(registry, 0.1f);
}
