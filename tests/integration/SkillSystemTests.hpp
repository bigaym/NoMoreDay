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