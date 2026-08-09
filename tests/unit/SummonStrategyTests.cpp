#include "TestCommon.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/systems/skill/SummonCombatBridge.hpp"
#include "game/systems/skill/SkillSystem.hpp"

namespace NoMoreDay {
namespace {

struct DealDamageCaptureScope {
  CombatEvent captured{};
  bool hasEvent = false;
  uint32_t handler = 0;

  DealDamageCaptureScope() {
    handler = CombatEventDispatcher::Register(
        CombatEventType::OnDealDamage,
        [this](entt::registry &, const CombatEvent &evt) {
          captured = evt;
          hasEvent = true;
        },
        2000);
  }

  ~DealDamageCaptureScope() {
    CombatEventDispatcher::Unregister(CombatEventType::OnDealDamage, handler);
  }
};

entt::entity CreateTarget(entt::registry &registry, float x, float y,
                          float hp) {
  const auto target = registry.create();
  registry.emplace<EnemyTag>(target);
  registry.emplace<Position>(target, x, y);
  registry.emplace<HealthComponent>(target, hp, hp);
  registry.emplace<CombatStats>(target);
  return target;
}

} // namespace

TEST_CASE("[Unit] SummonStrategy - melee orbit events carry owner/summon/source_skill attribution") {
  TestSetupScope scope;
  entt::registry registry;

  const auto owner = registry.create();
  registry.emplace<Position>(owner, 0.0f, 0.0f);
  auto &ownerStats = registry.emplace<CombatStats>(owner);
  ownerStats.damage_multipliers[0] = 1.0f;

  const auto summon = registry.create();
  registry.emplace<SpiritSwordTag>(summon);
  registry.emplace<Position>(summon, 0.0f, 0.0f);
  auto &summonComp = registry.emplace<SummonComponent>(summon);
  summonComp.owner = owner;
  summonComp.skill_id = 3;
  summonComp.archetype_id = SummonArchetype::SpiritSword;
  auto &profile = registry.emplace<SummonCombatProfile>(summon);
  profile.inherit_mode = SummonInheritMode::Dynamic;
  profile.damage_scale = 1.0f;
  profile.proc_budget_cap = 10.0f;
  auto &runtime = registry.emplace<SummonRuntimeState>(summon);
  runtime.proc_budget = 10.0f;
  runtime.snapshot_stats = ownerStats;
  runtime.has_snapshot = true;

  const auto target = CreateTarget(registry, 5.0f, 0.0f, 200.0f);

  systems::SpatialHashGrid grid(64, 64, 20.0f);
  auto posView = registry.view<Position>();
  grid.rebuild(posView, registry);

  DealDamageCaptureScope capture;
  systems::SummonCombatBridge::ApplyMeleeOrbitContact(
      registry, summon, grid, registry.get<Position>(summon));

  CHECK(capture.hasEvent);
  CHECK(capture.captured.summon_owner == owner);
  CHECK(capture.captured.summon_entity == summon);
  CHECK(capture.captured.summon_source_skill == 3u);
  CHECK(registry.get<HealthComponent>(target).current < 200.0f);
}

TEST_CASE("[Unit] SummonStrategy - melee orbit uses profile-configured base damage") {
  TestSetupScope scope;
  entt::registry registry;

  const auto owner = registry.create();
  registry.emplace<Position>(owner, 0.0f, 0.0f);
  auto &ownerStats = registry.emplace<CombatStats>(owner);
  ownerStats.crit_chance = 0.0f;

  const auto summon = registry.create();
  registry.emplace<SpiritSwordTag>(summon);
  registry.emplace<Position>(summon, 0.0f, 0.0f);
  auto &summonComp = registry.emplace<SummonComponent>(summon);
  summonComp.owner = owner;
  summonComp.skill_id = 3;
  summonComp.archetype_id = SummonArchetype::SpiritSword;
  auto &profile = registry.emplace<SummonCombatProfile>(summon);
  profile.inherit_mode = SummonInheritMode::Dynamic;
  profile.damage_scale = 1.0f;
  profile.proc_budget_cap = 10.0f;
  profile.melee_orbit_hit_radius = 30.0f;
  profile.melee_orbit_base_damage = 5.0f;
  auto &runtime = registry.emplace<SummonRuntimeState>(summon);
  runtime.proc_budget = 10.0f;
  runtime.snapshot_stats = ownerStats;
  runtime.has_snapshot = true;

  const auto lowTarget = CreateTarget(registry, 5.0f, 0.0f, 200.0f);
  systems::SpatialHashGrid grid(64, 64, 20.0f);
  auto posView = registry.view<Position>();
  grid.rebuild(posView, registry);

  systems::SummonCombatBridge::ApplyMeleeOrbitContact(
      registry, summon, grid, registry.get<Position>(summon));
  const float lowDamage = 200.0f - registry.get<HealthComponent>(lowTarget).current;
  CHECK(lowDamage > 0.0f);

  registry.emplace<KilledTag>(lowTarget);
  profile.melee_orbit_base_damage = 60.0f;
  runtime.proc_budget = 10.0f;

  const auto highTarget = CreateTarget(registry, 5.0f, 0.0f, 200.0f);
  grid.rebuild(posView, registry);
  systems::SummonCombatBridge::ApplyMeleeOrbitContact(
      registry, summon, grid, registry.get<Position>(summon));
  const float highDamage =
      200.0f - registry.get<HealthComponent>(highTarget).current;

  CHECK(highDamage > lowDamage);
}

TEST_CASE("[Unit] SummonStrategy - inherit mode resolves snapshot/dynamic/mixed correctly") {
  TestSetupScope scope;
  entt::registry registry;

  const auto owner = registry.create();
  auto &ownerStats = registry.emplace<CombatStats>(owner);
  ownerStats.min_weapon_damage = 300.0f;
  ownerStats.max_weapon_damage = 300.0f;
  ownerStats.damage_multipliers[0] = 3.0f;

  const auto summon = registry.create();
  auto &summonComp = registry.emplace<SummonComponent>(summon);
  summonComp.owner = owner;
  summonComp.skill_id = 3;

  auto &runtime = registry.emplace<SummonRuntimeState>(summon);
  runtime.has_snapshot = true;
  runtime.snapshot_stats = ownerStats;
  runtime.snapshot_stats.min_weapon_damage = 100.0f;
  runtime.snapshot_stats.max_weapon_damage = 100.0f;
  runtime.snapshot_stats.damage_multipliers[0] = 1.0f;

  auto &profile = registry.emplace<SummonCombatProfile>(summon);
  profile.damage_scale = 1.0f;

  profile.inherit_mode = SummonInheritMode::Snapshot;
  const auto snapshotStats =
      systems::SummonCombatBridge::ResolveInheritedStats(registry, summon);
  CHECK(snapshotStats.min_weapon_damage == doctest::Approx(100.0f));
  CHECK(snapshotStats.damage_multipliers[0] == doctest::Approx(1.0f));

  profile.inherit_mode = SummonInheritMode::Dynamic;
  const auto dynamicStats =
      systems::SummonCombatBridge::ResolveInheritedStats(registry, summon);
  CHECK(dynamicStats.min_weapon_damage == doctest::Approx(300.0f));
  CHECK(dynamicStats.damage_multipliers[0] == doctest::Approx(3.0f));

  profile.inherit_mode = SummonInheritMode::Mixed;
  const auto mixedStats =
      systems::SummonCombatBridge::ResolveInheritedStats(registry, summon);
  CHECK(mixedStats.min_weapon_damage == doctest::Approx(180.0f));
  CHECK(mixedStats.damage_multipliers[0] == doctest::Approx(1.8f));
}

TEST_CASE("[Unit] SummonStrategy - spirit sword bridge casts through ShadowCast path") {
  TestSetupScope scope;
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  const auto owner = registry.create();
  registry.emplace<Position>(owner, 0.0f, 0.0f);
  auto &ownerStats = registry.emplace<CombatStats>(owner);
  ownerStats.min_weapon_damage = 120.0f;
  ownerStats.max_weapon_damage = 120.0f;

  const auto summon = registry.create();
  registry.emplace<SpiritSwordTag>(summon);
  registry.emplace<Position>(summon, 0.0f, 0.0f);
  auto &summonComp = registry.emplace<SummonComponent>(summon);
  summonComp.owner = owner;
  summonComp.skill_id = 3;
  summonComp.archetype_id = SummonArchetype::SpiritSword;
  auto &profile = registry.emplace<SummonCombatProfile>(summon);
  profile.inherit_mode = SummonInheritMode::Dynamic;
  profile.proc_budget_cap = 10.0f;
  auto &runtime = registry.emplace<SummonRuntimeState>(summon);
  runtime.proc_budget = 10.0f;
  runtime.snapshot_stats = ownerStats;
  runtime.has_snapshot = true;

  const auto target = CreateTarget(registry, 30.0f, 0.0f, 300.0f);
  const bool castOk = systems::SummonCombatBridge::CastSpiritSwordShadow(
      registry, summon, target, {0.0f, 0.0f}, false);
  CHECK(castOk);

  auto execView = registry.view<SkillExecution, ShadowCastTag>();
  CHECK(execView.begin() != execView.end());

  bool foundAttributedShadowOwner = false;
  for (const auto execEntity : execView) {
    const auto &exec = execView.get<SkillExecution>(execEntity);
    if (const auto *ctx = registry.try_get<SummonAttributionContext>(exec.owner)) {
      if (ctx->owner == owner && ctx->summon == summon &&
          ctx->source_skill_id == 3u) {
        foundAttributedShadowOwner = true;
      }
    }
  }
  CHECK(foundAttributedShadowOwner);
}

} // namespace NoMoreDay
