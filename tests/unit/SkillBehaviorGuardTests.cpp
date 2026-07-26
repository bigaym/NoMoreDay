#include "TestCommon.hpp"
#include "SkillKeyNodeMatrixTestHelpers.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/EffectSystem.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>

namespace NoMoreDay {

TEST_CASE("[Unit] SkillBehaviorGuard - InitHooks idempotency avoids duplicate handlers") {
  entt::registry registry;
  CombatEventDispatcher::Clear();
  SkillSystem::ShutdownHooks();
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  SkillSystem::InitHooks();
  SkillSystem::InitHooks();

  auto caster = registry.create();
  registry.emplace<Position>(caster, 0.0f, 0.0f);
  registry.emplace<CombatStats>(caster).mana = 200.0f;
  auto &intent = registry.emplace<SwordIntentComponent>(caster);
  intent.stacks = 0;

  auto target = registry.create();
  registry.emplace<Position>(target, 8.0f, 0.0f);

  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateSkillHit(
                    caster, target, 1, Tag::Hit | Tag::Melee, false, 6101));

  CHECK(intent.stacks == 1);
}

TEST_CASE("[Unit] SkillBehaviorGuard - Trigger cooldown and depth guard") {
  entt::registry registry;
  CombatEventDispatcher::Clear();
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillSystem::InitHooks();

  auto caster = registry.create();
  registry.emplace<Position>(caster, 0.0f, 0.0f);
  registry.emplace<CombatStats>(caster).mana = 200.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(caster);
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[114] = 1; // Trigger node

  auto target = registry.create();
  registry.emplace<Position>(target, 8.0f, 0.0f);

  SUBCASE("Trigger cooldown blocks repeated dispatch") {
    const auto before = registry.storage<SkillExecution>().size();
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      caster, target, 1, Tag::Hit | Tag::Melee, false, 4001));

    const auto after_first = registry.storage<SkillExecution>().size();
    CHECK(after_first > before);
    const auto *runtime =
        registry.try_get<SkillContractRuntimeComponent>(caster);
    REQUIRE(runtime != nullptr);
    REQUIRE(runtime->trigger_cooldowns.contains(114));

    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      caster, target, 1, Tag::Hit | Tag::Melee, false, 4001));
    const auto after_second = registry.storage<SkillExecution>().size();
    CHECK(after_second == after_first);
  }

  SUBCASE("Skill 2 trigger contract dispatches and records cooldown") {
    active.specialized_slots[0].skill_id = 2;
    active.specialized_slots[0].allocated_points.clear();
    active.specialized_slots[0].allocated_points[233] = 1; // Skill 2 trigger node

    const auto before = registry.storage<SkillExecution>().size();
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      caster, target, 2, Tag::Hit | Tag::Projectile, false, 5001));

    const auto after = registry.storage<SkillExecution>().size();
    CHECK(after > before);

    const auto *runtime =
        registry.try_get<SkillContractRuntimeComponent>(caster);
    REQUIRE(runtime != nullptr);
    CHECK(runtime->trigger_cooldowns.contains(233));
  }

  SUBCASE("Trigger dispatch stores trigger effectiveness per cast") {
    const auto *nodeContract = SkillRegistry::Get().GetNodeContract(1, 114);
    REQUIRE(nodeContract != nullptr);
    const float expectedEffectiveness =
        (std::max)(0.0f, nodeContract->trigger.effectiveness);

    uint64_t maxCastIdBefore = 0;
    auto existingExecView = registry.view<SkillExecution>();
    for (const auto execEntity : existingExecView) {
      const auto &existingExec = existingExecView.get<SkillExecution>(execEntity);
      maxCastIdBefore = (std::max)(maxCastIdBefore, existingExec.cast_id);
    }

    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      caster, target, 1, Tag::Hit | Tag::Melee, false, 9101));

    uint64_t triggerCastId = 0;
    auto execView = registry.view<SkillExecution>();
    for (const auto execEntity : execView) {
      const auto &exec = execView.get<SkillExecution>(execEntity);
      if (exec.owner != caster || exec.skill_id != nodeContract->trigger.trigger_skill_id ||
          exec.trigger_depth != 1 || exec.cast_id <= maxCastIdBefore) {
        continue;
      }
      triggerCastId = exec.cast_id;
      break;
    }

    REQUIRE(triggerCastId != 0);
    CHECK(SkillSystem::GetTriggerEffectivenessForCast(triggerCastId) ==
          doctest::Approx(expectedEffectiveness));
    CHECK(SkillSystem::GetTriggerEffectivenessForCast(0) ==
          doctest::Approx(1.0f));
  }

  SUBCASE("Trigger depth guard blocks depth > 2") {
    auto &runtime =
        registry.get_or_emplace<SkillContractRuntimeComponent>(caster);
    runtime.trigger_cooldowns.clear();

    auto parent_exec_entity = registry.create();
    auto &parent_exec = registry.emplace<SkillExecution>(parent_exec_entity);
    parent_exec.skill_id = 1;
    parent_exec.owner = caster;
    parent_exec.cast_id = 9001;
    parent_exec.trigger_depth = 2;

    const auto before = registry.storage<SkillExecution>().size();
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      caster, target, 1, Tag::Hit | Tag::Melee, false, 9001));
    const auto after = registry.storage<SkillExecution>().size();
    CHECK(after == before);
  }
}

TEST_CASE("[Unit] SkillBehaviorGuard - Cast tracking handles concurrent read/write access") {
  entt::registry registry;
  CombatEventDispatcher::Clear();
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  const auto caster = registry.create();
  registry.emplace<Position>(caster, 0.0f, 0.0f);
  registry.emplace<CombatStats>(caster).mana = 200.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(caster);
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[114] = 1;

  const auto target = registry.create();
  registry.emplace<Position>(target, 8.0f, 0.0f);

  std::atomic<bool> writerDone{false};
  std::atomic<bool> readerFailed{false};

  std::thread writer([&]() {
    for (uint64_t castId = 31000; castId < 31128; ++castId) {
      CombatEventDispatcher::Dispatch(
          registry, CombatEventFactory::CreateSkillHit(
                        caster, target, 1, Tag::Hit | Tag::Melee, false, castId));
    }
    writerDone.store(true, std::memory_order_release);
  });

  auto readerTask = [&]() {
    uint64_t probeCastId = 31000;
    while (!writerDone.load(std::memory_order_acquire)) {
      const float triggerEffectiveness =
          SkillSystem::GetTriggerEffectivenessForCast(probeCastId);
      if (!std::isfinite(triggerEffectiveness) || triggerEffectiveness < 0.0f) {
        readerFailed.store(true, std::memory_order_relaxed);
        return;
      }
      probeCastId = (probeCastId == 31127) ? 31000 : probeCastId + 1;
    }
  };

  std::thread readerA(readerTask);
  std::thread readerB(readerTask);
  std::thread readerC(readerTask);

  writer.join();
  readerA.join();
  readerB.join();
  readerC.join();

  CHECK_FALSE(readerFailed.load(std::memory_order_relaxed));
}

TEST_CASE("[Unit] SkillBehaviorGuard - Transmuter mutex and scope policy") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  SUBCASE("Transmuter mutex keeps only one active transmuter") {
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player).mana = 200.0f;
    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.slots[0].id = 8;
    active.slots[0].current_charges = 1;
    active.specialized_slots[0].skill_id = 8;
    active.specialized_slots[0].allocated_points[870] = 1;
    active.specialized_slots[0].allocated_points[871] = 1;

    CHECK(SkillSystem::TryCast(registry, player, 0, {10.0f, 0.0f}));
    const auto *runtime =
        registry.try_get<SkillContractRuntimeComponent>(player);
    REQUIRE(runtime != nullptr);
    REQUIRE(runtime->active_transmuter_node_by_skill.contains(8));
    CHECK(runtime->active_transmuter_node_by_skill.at(8) == 870);
  }

  SUBCASE("Scope policy entry supports channel and PhantomFlash windows") {
    auto player = registry.create();

    CHECK(
        SkillSystem::CanApplyScopePolicy(registry, player, 1, 1, ScopePolicy::SkillOnly));
    CHECK_FALSE(
        SkillSystem::CanApplyScopePolicy(registry, player, 1, 2, ScopePolicy::SkillOnly));

    CHECK_FALSE(SkillSystem::CanApplyScopePolicy(
        registry, player, 9, 9, ScopePolicy::GlobalWhileBuffActive));

    auto &chan = registry.emplace<ChannelingComponent>(player);
    chan.skill_id = 9;
    CHECK(SkillSystem::CanApplyScopePolicy(
        registry, player, 9, 9, ScopePolicy::GlobalWhileBuffActive));
    registry.remove<ChannelingComponent>(player);

    auto &pf = registry.emplace<PhantomFlashComponent>(player);
    pf.counter_window = 0.25f;
    pf.triggered = false;
    CHECK(SkillSystem::CanApplyScopePolicy(
        registry, player, 9, 9, ScopePolicy::GlobalWhileBuffActive));
    pf.triggered = true;
    CHECK_FALSE(SkillSystem::CanApplyScopePolicy(
        registry, player, 9, 9, ScopePolicy::GlobalWhileBuffActive));
  }

  SUBCASE("Keystone exclusion swaps active node even at zero free points") {
    auto player = registry.create();
    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.available_talent_points = 1;
    active.specialized_slots[0].skill_id = 2;
    // Satisfy prerequisites for both candidate keystones in exclusion group 1.
    active.specialized_slots[0].allocated_points[212] = 2;
    active.specialized_slots[0].allocated_points[200] = 4;

    CHECK(SkillSystem::AddTalentPoint(registry, player, 2, 213));
    CHECK(active.available_talent_points == 0);
    REQUIRE(active.specialized_slots[0].allocated_points.contains(213));

    CHECK(SkillSystem::AddTalentPoint(registry, player, 2, 214));
    CHECK(active.available_talent_points == 0);
    CHECK_FALSE(active.specialized_slots[0].allocated_points.contains(213));
    REQUIRE(active.specialized_slots[0].allocated_points.contains(214));
    CHECK(SkillSystem::IsNodeExcludedByMutualKeystone(registry, player, 2, 213));
    CHECK_FALSE(
        SkillSystem::IsNodeExcludedByMutualKeystone(registry, player, 2, 214));
  }

  SUBCASE("Expanded exclusion groups support keystone and transmuter swaps") {
    auto player = registry.create();
    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.available_talent_points = 1;
    active.specialized_slots[0].skill_id = 10;
    active.specialized_slots[0].allocated_points[1005] = 2;
    active.specialized_slots[0].allocated_points[1020] = 2;

    CHECK(SkillSystem::AddTalentPoint(registry, player, 10, 1007));
    CHECK(active.available_talent_points == 0);
    REQUIRE(active.specialized_slots[0].allocated_points.contains(1007));

    CHECK(SkillSystem::AddTalentPoint(registry, player, 10, 1021));
    CHECK(active.available_talent_points == 0);
    CHECK_FALSE(active.specialized_slots[0].allocated_points.contains(1007));
    REQUIRE(active.specialized_slots[0].allocated_points.contains(1021));
    CHECK(SkillSystem::IsNodeExcludedByMutualKeystone(registry, player, 10,
                                                      1007));
    CHECK_FALSE(SkillSystem::IsNodeExcludedByMutualKeystone(registry, player,
                                                            10, 1021));
  }

  SUBCASE("Effective tags apply only active transmuter tags") {
    auto player = registry.create();
    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 8;
    active.specialized_slots[0].allocated_points[870] = 1;
    active.specialized_slots[0].allocated_points[871] = 1;

    auto *tree = const_cast<SkillTreeDefinition *>(SkillRegistry::Get().GetSkillTree(8));
    REQUIRE(tree != nullptr);
    auto it870 = tree->nodes.find(870);
    auto it871 = tree->nodes.find(871);
    REQUIRE(it870 != tree->nodes.end());
    REQUIRE(it871 != tree->nodes.end());

    const Tag old870 = it870->second.add_tags;
    const Tag old871 = it871->second.add_tags;
    it870->second.add_tags = Tag::Fire;
    it871->second.add_tags = Tag::Cold;

    auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
    runtime.active_transmuter_node_by_skill[8] = 870;

    Tag tags = SkillSystem::GetEffectiveSkillTags(registry, player, 8);
    CHECK(HasTag(tags, Tag::Fire));
    CHECK_FALSE(HasTag(tags, Tag::Cold));

    runtime.active_transmuter_node_by_skill[8] = 871;
    tags = SkillSystem::GetEffectiveSkillTags(registry, player, 8);
    CHECK_FALSE(HasTag(tags, Tag::Fire));
    CHECK(HasTag(tags, Tag::Cold));

    runtime.active_transmuter_node_by_skill.erase(8);
    tags = SkillSystem::GetEffectiveSkillTags(registry, player, 8);
    CHECK(HasTag(tags, Tag::Fire));
    CHECK_FALSE(HasTag(tags, Tag::Cold));

    it870->second.add_tags = old870;
    it871->second.add_tags = old871;
  }
}

TEST_CASE("[Unit] SkillBehaviorGuard - SwordStep phase lifecycle follows buff") {
  entt::registry registry;
  systems::SpatialHashGrid grid(64, 64, 16);

  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<PhaseTag>(player);

  SUBCASE("PhaseTag removed when SwordStep buff is absent") {
    SkillSystem::Update(registry, grid, 0.016f);
    CHECK_FALSE(registry.any_of<PhaseTag>(player));
  }

  SUBCASE("PhaseTag retained when SwordStep buff is active") {
    auto &effects = registry.emplace<ActiveEffectsComponent>(player);
    BuffEffect swift;
    swift.id = "flowing_thrust_swift";
    swift.name = "Feng Xing";
    swift.duration = 1.0f;
    swift.remaining = 0.8f;
    effects.AddOrRefresh(swift);

    SkillSystem::Update(registry, grid, 0.016f);
    CHECK(registry.any_of<PhaseTag>(player));
  }
}

TEST_CASE("[Unit] SkillBehaviorGuard - Contract key nodes map to runtime state") {
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();

  SUBCASE("Skill 5 channel nodes populate runtime channel fields") {
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player).mana = 200.0f;
    registry.emplace<SwordIntentComponent>(player).stacks = 6;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 5;
    active.specialized_slots[0].allocated_points[570] = 2; // ElementFall -> cold
    active.specialized_slots[0].allocated_points[571] = 3; // ElementPen
    active.specialized_slots[0].allocated_points[552] = 2; // MindUnify

    SkillExecution exec;
    exec.skill_id = 5;
    exec.owner = player;
    exec.target_pos = {30.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(5);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    const auto *chan = registry.try_get<ChannelingComponent>(player);
    REQUIRE(chan != nullptr);
    CHECK(chan->conversion_tag == Tag::Cold);
    CHECK(chan->bonus_armor_pen == doctest::Approx(18.0f));
    CHECK(chan->bonus_damage_mult > 1.0f);
    CHECK(chan->bonus_crit_chance > 0.0f);
  }

  SUBCASE("Skill 6 key-node mapping drives SwordArray flags") {
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player);

    SkillExecution exec;
    exec.skill_id = 6;
    exec.owner = player;
    exec.target_pos = {20.0f, 0.0f};
    exec.active_nodes.set(630 % 100); // SlowPressure
    exec.active_nodes.set(633 % 100); // ExecuteField
    exec.active_nodes.set(652 % 100); // MindUnity

    auto cast = SkillBehaviorRegistry::GetCast(6);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<SwordArrayComponent>();
    REQUIRE(view.begin() != view.end());
    const auto arrayEntity = *view.begin();
    const auto &array = view.get<SwordArrayComponent>(arrayEntity);
    CHECK(array.has_slow);
    CHECK(array.has_execute);
    CHECK(array.gain_intent_on_tick);
    CHECK(array.execute_health_threshold_ratio == doctest::Approx(0.15f));
    CHECK(array.execute_damage_max_health_ratio == doctest::Approx(0.10f));
  }

  SUBCASE("Skill 9 key nodes set PhantomFlash runtime fields") {
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player).mana = 200.0f;
    registry.emplace<SwordIntentComponent>(player).stacks = 0;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 9;
    active.specialized_slots[0].allocated_points[930] = 1;
    active.specialized_slots[0].allocated_points[951] = 1;
    active.specialized_slots[0].allocated_points[952] = 2;
    active.specialized_slots[0].allocated_points[970] = 1;

    auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
    runtime.active_transmuter_node_by_skill[9] = 970;

    SkillExecution exec;
    exec.skill_id = 9;
    exec.owner = player;
    exec.target_pos = {10.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(9);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    const auto *pf = registry.try_get<PhantomFlashComponent>(player);
    REQUIRE(pf != nullptr);
    CHECK(pf->synergy_shadow_hide);
    CHECK(pf->flow_reset);
    CHECK(pf->intent_overflow == 2);
    CHECK(pf->enchant_tag == Tag::Cold);

    const auto *intent = registry.try_get<SwordIntentComponent>(player);
    REQUIRE(intent != nullptr);
    CHECK(intent->stacks >= 1);
  }

  SUBCASE("Skill 10 branch transmuters change SevenStarSlash cast radius") {
    const auto castWithBranch = [&](uint32_t node_id) {
      auto player = registry.create();
      registry.emplace<Position>(player, 0.0f, 0.0f);
      auto &stats = registry.emplace<CombatStats>(player);
      stats.mana = 200.0f;
      stats.min_weapon_damage = 30.0f;
      stats.max_weapon_damage = 40.0f;

      auto &active = registry.emplace<ActiveSkillsComponent>(player);
      active.specialized_slots[0].skill_id = 10;
      active.specialized_slots[0].allocated_points[node_id] = 1;

      auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
      runtime.active_transmuter_node_by_skill[10] = node_id;

      SkillExecution exec;
      exec.skill_id = 10;
      exec.owner = player;
      exec.target_pos = {16.0f, 0.0f};

      auto cast = SkillBehaviorRegistry::GetCast(10);
      REQUIRE(cast != nullptr);
      cast(registry, player, exec);

      const auto *invulnerable = registry.try_get<InvulnerableComponent>(player);
      REQUIRE(invulnerable != nullptr);
      return invulnerable->shieldRadius;
    };

    const float orbitRadius = castWithBranch(1021);
    const float starfallRadius = castWithBranch(1022);

    CHECK(orbitRadius == doctest::Approx(96.0f * 0.34f));
    CHECK(starfallRadius == doctest::Approx(96.0f * 0.50f));
    CHECK(starfallRadius > orbitRadius);
  }

  SUBCASE("Skill 11 selected branch nodes populate HeavenlySword field state") {
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player).mana = 200.0f;

    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::HeavenlySword;
    mastery.heavenly_attunement = BladeAttunement::Fire;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::SpiritBladeTier;
    resource.current = 4;
    resource.max = 10;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 11;
    active.specialized_slots[0].allocated_points[1102] = 2;
    active.specialized_slots[0].allocated_points[1113] = 1;

    SkillExecution exec;
    exec.skill_id = 11;
    exec.owner = player;
    exec.cast_id = 11011;
    exec.target_pos = {24.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(11);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<HeavenlySwordFieldComponent>();
    REQUIRE(view.begin() != view.end());
    const auto fieldEntity = *view.begin();
    const auto &field = view.get<HeavenlySwordFieldComponent>(fieldEntity);

    CHECK(field.owner == player);
    CHECK(field.cast_id == 11011);
    CHECK(field.spent_tiers == 4);
    CHECK(field.attunement == BladeAttunement::Fire);
    CHECK(field.has_cycle);
    CHECK(field.impact_damage_mult == doctest::Approx(1.632f));
    CHECK(field.field_damage_mult == doctest::Approx(1.32f));
    CHECK(field.radius == doctest::Approx(196.0f));
  }

  SUBCASE("Skill 12 branch forms populate BloodSea field state") {
    const auto castWithBranch = [&](uint32_t node_id) {
      auto player = registry.create();
      registry.emplace<Position>(player, 0.0f, 0.0f);
      registry.emplace<CombatStats>(player).mana = 200.0f;

      auto &mastery = registry.emplace<BladeMasteryComponent>(player);
      mastery.selected = BladeMasteryId::DemonBlade;
      mastery.blood_oath_active = true;

      auto &resource = registry.emplace<BladeResourceComponent>(player);
      resource.kind = BladeResourceKind::Bloodthirst;
      resource.current = 5;
      resource.max = 10;

      auto &active = registry.emplace<ActiveSkillsComponent>(player);
      active.specialized_slots[0].skill_id = 12;
      active.specialized_slots[0].allocated_points[node_id] = 1;

      SkillExecution exec;
      exec.skill_id = 12;
      exec.owner = player;
      exec.cast_id = 12000 + node_id;
      exec.target_pos = {18.0f, 0.0f};

      auto cast = SkillBehaviorRegistry::GetCast(12);
      REQUIRE(cast != nullptr);
      cast(registry, player, exec);

      auto view = registry.view<BloodSeaFieldComponent>();
      REQUIRE(view.begin() != view.end());
      const auto fieldEntity = *view.begin();
      return view.get<BloodSeaFieldComponent>(fieldEntity);
    };

    const auto torrentField = castWithBranch(1221);
    CHECK(torrentField.consumed_bloodthirst == 5);
    CHECK(torrentField.torrent_form);
    CHECK_FALSE(torrentField.ring_form);
    CHECK(torrentField.move_follow_speed == doctest::Approx(14.0f));
    CHECK(torrentField.radius == doctest::Approx(172.5f));
    CHECK(torrentField.damage_interval == doctest::Approx(0.2125f));
    CHECK(torrentField.leech_ratio == doctest::Approx(0.12f));

    const auto ringField = castWithBranch(1222);
    CHECK(ringField.consumed_bloodthirst == 5);
    CHECK_FALSE(ringField.torrent_form);
    CHECK(ringField.ring_form);
    CHECK(ringField.move_follow_speed == doctest::Approx(10.0f));
    CHECK(ringField.radius == doctest::Approx(120.0f));
    CHECK(ringField.damage_interval == doctest::Approx(0.25f));
    CHECK(ringField.leech_ratio == doctest::Approx(0.22f));
    CHECK(ringField.bonus_damage_mult == doctest::Approx(1.84f));
  }
}

TEST_CASE("[Unit] SkillBehaviorGuard - Demon Blade low-life nodes alter BloodSea state") {
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  const auto castField = [](const std::vector<std::pair<uint32_t, int>> &nodes) {
    entt::registry registry;

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &stats = registry.get<CombatStats>(player);
    stats.max_health = 200.0f;
    stats.health = 60.0f;

    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::DemonBlade;
    mastery.blood_oath_active = true;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 3;
    resource.max = 10;

    test::skill_keynode_matrix::ConfigureSpecialization(registry, player, 12,
                                                        nodes);

    SkillExecution exec;
    exec.skill_id = 12;
    exec.owner = player;
    exec.cast_id = 12030u + static_cast<uint64_t>(nodes.size());
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(12);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<BloodSeaFieldComponent>();
    REQUIRE(view.begin() != view.end());
    return view.get<BloodSeaFieldComponent>(*view.begin());
  };

  const auto baselineField = castField({{1201, 2}});
  const auto lowLifeField = castField({{1201, 2}, {1204, 2}, {1206, 2}});

  CHECK(lowLifeField.consumed_bloodthirst == baselineField.consumed_bloodthirst);
  CHECK(lowLifeField.bonus_damage_mult > baselineField.bonus_damage_mult);
}

TEST_CASE("[Unit] SkillBehaviorGuard - Demon Blade baseline nodes reshape BloodSea field") {
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  const auto castField = [](const std::vector<std::pair<uint32_t, int>> &nodes) {
    entt::registry registry;

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::DemonBlade;
    mastery.blood_oath_active = true;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 4;
    resource.max = 10;

    test::skill_keynode_matrix::ConfigureSpecialization(registry, player, 12,
                                                        nodes);

    SkillExecution exec;
    exec.skill_id = 12;
    exec.owner = player;
    exec.cast_id = 12040u + static_cast<uint64_t>(nodes.size());
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(12);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<BloodSeaFieldComponent>();
    REQUIRE(view.begin() != view.end());
    return view.get<BloodSeaFieldComponent>(*view.begin());
  };

  const auto baselineField = castField({});
  const auto shapedField = castField({{1200, 2}, {1201, 2}, {1202, 2}, {1203, 2}});

  CHECK(shapedField.consumed_bloodthirst == baselineField.consumed_bloodthirst);
  CHECK(shapedField.radius > baselineField.radius);
  CHECK(shapedField.bonus_damage_mult > baselineField.bonus_damage_mult);
  CHECK(shapedField.move_follow_speed > baselineField.move_follow_speed);
}

TEST_CASE("[Unit] SkillBehaviorGuard - Demon Blade sustain nodes increase BloodSea leech") {
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  const auto castField = [](const std::vector<std::pair<uint32_t, int>> &nodes) {
    entt::registry registry;

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &stats = registry.get<CombatStats>(player);
    stats.max_health = 200.0f;
    stats.health = 60.0f;

    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::DemonBlade;
    mastery.blood_oath_active = true;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 4;
    resource.max = 10;

    test::skill_keynode_matrix::ConfigureSpecialization(registry, player, 12,
                                                        nodes);

    SkillExecution exec;
    exec.skill_id = 12;
    exec.owner = player;
    exec.cast_id = 12050u + static_cast<uint64_t>(nodes.size());
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(12);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<BloodSeaFieldComponent>();
    REQUIRE(view.begin() != view.end());
    return view.get<BloodSeaFieldComponent>(*view.begin());
  };

  const auto baselineField = castField({{1213, 1}});
  const auto sustainField =
      castField({{1209, 2}, {1210, 2}, {1212, 2}, {1213, 1}});

  CHECK(sustainField.consumed_bloodthirst == baselineField.consumed_bloodthirst);
  CHECK(sustainField.leech_ratio > baselineField.leech_ratio);
}

TEST_CASE("[Unit] SkillBehaviorGuard - Demon Blade pursuit branch empowers close linked pressure") {
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  struct ScenarioResult {
    float close_target_health = 0.0f;
    float far_target_health = 0.0f;
    float return_empower_timer = 0.0f;
  };

  const auto runScenario = [](const std::vector<std::pair<uint32_t, int>> &nodes) {
    entt::registry registry;
    systems::SpatialHashGrid grid(1024, 1024, 64);

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &stats = registry.get<CombatStats>(player);
    stats.max_health = 200.0f;
    stats.health = 198.0f;

    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::DemonBlade;
    mastery.blood_oath_active = true;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 4;
    resource.max = 10;

    test::skill_keynode_matrix::ConfigureSpecialization(registry, player, 12,
                                                        nodes);

    const auto closeTarget =
        test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
    const auto farTarget =
        test::skill_keynode_matrix::CreateTarget(registry, {100.0f, 0.0f});
    grid.rebuild(registry.view<Position>(), registry);

    SkillExecution exec;
    exec.skill_id = 12;
    exec.owner = player;
    exec.cast_id = 12060u + static_cast<uint64_t>(nodes.size());
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(12);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<BloodSeaFieldComponent>();
    REQUIRE(view.begin() != view.end());
    const auto fieldEntity = *view.begin();

    SkillSystem::Update(registry, grid, 0.26f);
    test::skill_keynode_matrix::DispatchSkillHit(registry, player, closeTarget, 1,
                                                 exec.cast_id);

    const auto &field = registry.get<BloodSeaFieldComponent>(fieldEntity);
    return ScenarioResult{
        registry.get<HealthComponent>(closeTarget).current,
        registry.get<HealthComponent>(farTarget).current,
        field.return_empower_timer,
    };
  };

  const auto baseline = runScenario({{1213, 1}, {1217, 1}});
  const auto pursuit =
      runScenario({{1205, 2}, {1208, 2}, {1213, 1}, {1214, 2}, {1215, 2}, {1217, 1}});

  CHECK(pursuit.close_target_health < baseline.close_target_health);
  CHECK(pursuit.far_target_health <= baseline.far_target_health);
  CHECK(pursuit.return_empower_timer > doctest::Approx(0.0f));
}

TEST_CASE("[Unit] SkillBehaviorGuard - Demon Blade void branch extends miasma pressure") {
  CombatEventDispatcher::Init();
  SkillBehaviorRegistry::Initialize();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  struct ScenarioResult {
    float target_health = 0.0f;
    float damage_interval = 0.0f;
    float miasma_remaining = 0.0f;
  };

  const auto runScenario = [](const std::vector<std::pair<uint32_t, int>> &nodes) {
    entt::registry registry;

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::DemonBlade;
    mastery.blood_oath_active = true;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 4;
    resource.max = 10;

    test::skill_keynode_matrix::ConfigureSpecialization(registry, player, 12,
                                                        nodes);

    const auto target =
        test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});

    SkillExecution exec;
    exec.skill_id = 12;
    exec.owner = player;
    exec.cast_id = 12070u + static_cast<uint64_t>(nodes.size());
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(12);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<BloodSeaFieldComponent>();
    REQUIRE(view.begin() != view.end());
    const auto fieldEntity = *view.begin();

    test::skill_keynode_matrix::DispatchSkillHit(registry, player, target, 1,
                                                 exec.cast_id);

    const auto &field = registry.get<BloodSeaFieldComponent>(fieldEntity);
    const auto &effects = registry.get<ActiveEffectsComponent>(target);
    const auto it = std::find_if(effects.effects.begin(), effects.effects.end(),
                                 [](const BuffEffect &effect) {
                                   return effect.id == "blood_sea_miasma" &&
                                          effect.remaining > 0.0f;
                                 });
    REQUIRE(it != effects.effects.end());

    return ScenarioResult{registry.get<HealthComponent>(target).current,
                          field.damage_interval, it->remaining};
  };

  const auto baseline = runScenario({{1217, 1}, {1220, 1}, {1224, 2}});
  const auto voidBranch =
      runScenario({{1216, 2}, {1217, 1}, {1218, 2}, {1220, 1}, {1223, 2}, {1224, 2}});

  CHECK(voidBranch.target_health < baseline.target_health);
  CHECK(voidBranch.damage_interval < baseline.damage_interval);
  CHECK(voidBranch.miasma_remaining > baseline.miasma_remaining);
}

TEST_CASE("[Unit] SkillBehaviorGuard - Trigger matrix smoke for remaining key nodes") {
  auto &skill_registry = SkillRegistry::Get();
  skill_registry.LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();

  const std::array<std::pair<uint32_t, uint32_t>, 7> trigger_matrix = {{
      {3u, 373u},
      {4u, 451u},
      {5u, 533u},
      {6u, 633u},
      {7u, 713u},
      {8u, 831u},
      {9u, 951u},
  }};

  for (const auto &[skill_id, trigger_node] : trigger_matrix) {
    CAPTURE(skill_id);
    CAPTURE(trigger_node);

    entt::registry registry;
    CombatEventDispatcher::Clear();
    SkillSystem::ShutdownHooks();
    SkillSystem::InitHooks();

    const auto caster =
        test::skill_keynode_matrix::CreateCaster(registry, 800.0f);
    const auto target = test::skill_keynode_matrix::CreateTarget(registry);
    test::skill_keynode_matrix::ConfigureSpecialization(
        registry, caster, skill_id, {{trigger_node, 1}});

    const auto before = registry.storage<SkillExecution>().size();
    test::skill_keynode_matrix::DispatchSkillHit(
        registry, caster, target, skill_id, static_cast<uint64_t>(9900 + skill_id));
    const auto after = registry.storage<SkillExecution>().size();
    CHECK(after > before);

    const auto *runtime =
        registry.try_get<SkillContractRuntimeComponent>(caster);
    REQUIRE(runtime != nullptr);
    CHECK(runtime->trigger_cooldowns.contains(trigger_node));
  }
}

TEST_CASE("[Unit] SkillBehaviorGuard - Trigger and synergy nodes cause observable outcomes") {
  CombatEventDispatcher::Init();
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();

  SUBCASE("Skill 10 sword-step synergy grants mirage buff after cast") {
    entt::registry registry;

    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    auto &stats = registry.emplace<CombatStats>(player);
    stats.mana = 200.0f;
    stats.min_weapon_damage = 30.0f;
    stats.max_weapon_damage = 40.0f;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 10;
    active.specialized_slots[0].allocated_points[1017] = 1;

    auto &effects = registry.emplace<ActiveEffectsComponent>(player);
    BuffEffect swift;
    swift.id = "flowing_thrust_swift";
    swift.name = "Feng Xing";
    swift.duration = 1.0f;
    swift.remaining = 1.0f;
    effects.AddOrRefresh(swift);

    SkillExecution exec;
    exec.skill_id = 10;
    exec.owner = player;
    exec.target_pos = {16.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(10);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    CHECK(test::skill_keynode_matrix::HasEffectById(
        registry, player, "seven_star_sword_step_mirage"));
  }

  SUBCASE("Skill 11 trigger node spends tiers into immediate echo strikes") {
    entt::registry registry;

    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player).mana = 200.0f;

    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::HeavenlySword;
    mastery.heavenly_attunement = BladeAttunement::Fire;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::SpiritBladeTier;
    resource.current = 3;
    resource.max = 10;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 11;
    active.specialized_slots[0].allocated_points[1111] = 1;

    const auto target = test::skill_keynode_matrix::CreateTarget(registry, {20.0f, 0.0f});
    (void)target;

    SkillExecution exec;
    exec.skill_id = 11;
    exec.owner = player;
    exec.target_pos = {20.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(11);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<HeavenlySwordFieldComponent>();
    REQUIRE(view.begin() != view.end());
    const auto &field = view.get<HeavenlySwordFieldComponent>(*view.begin());

    CHECK(field.spent_tiers == 3);
    CHECK(field.has_trigger_echo);
    CHECK(field.echo_strikes_triggered == 3);
  }

  SUBCASE("Skill 11 array synergy converts linked hit into echo strike") {
    entt::registry registry;
    CombatEventDispatcher::Init();
    SkillBehaviorRegistry::Initialize();
    SkillSystem::ShutdownHooks();
    SkillSystem::InitHooks();

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::HeavenlySword;
    mastery.heavenly_attunement = BladeAttunement::Lightning;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::SpiritBladeTier;
    resource.current = 2;
    resource.max = 10;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 11;
    active.specialized_slots[0].allocated_points[1117] = 1;

    const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});

    SkillExecution exec;
    exec.skill_id = 11;
    exec.owner = player;
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(11);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<HeavenlySwordFieldComponent>();
    REQUIRE(view.begin() != view.end());
    const auto fieldEntity = *view.begin();

    const auto beforeTargetHealth = registry.get<HealthComponent>(target).current;
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, target, 3,
                      Tag::Hit | Tag::Melee | Tag::SwordSkill | Tag::Physical,
                      false, 1117001u));

    const auto &field = view.get<HeavenlySwordFieldComponent>(fieldEntity);
    CHECK(field.has_array_synchrony);
    CHECK(field.linked_hit_count == 1);
    CHECK(field.echo_strikes_triggered == 1);
    CHECK(field.linked_cut_cooldown == doctest::Approx(0.15f));
    CHECK(registry.get<HealthComponent>(target).current < beforeTargetHealth);
  }

  SUBCASE("Skill 12 trigger node causes opening burst pulse on cast") {
    entt::registry registry;

    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<CombatStats>(player).mana = 200.0f;

    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::DemonBlade;
    mastery.blood_oath_active = true;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 4;
    resource.max = 10;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 12;
    active.specialized_slots[0].allocated_points[1211] = 1;

    const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
    const auto beforeTargetHealth = registry.get<HealthComponent>(target).current;

    SkillExecution exec;
    exec.skill_id = 12;
    exec.owner = player;
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(12);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<BloodSeaFieldComponent>();
    REQUIRE(view.begin() != view.end());
    const auto &field = view.get<BloodSeaFieldComponent>(*view.begin());

    CHECK(field.has_trigger_burst);
    CHECK(field.pulses_triggered == 1);
    CHECK(registry.get<HealthComponent>(target).current < beforeTargetHealth);
  }

  SUBCASE("Skill 12 synergy converts linked hit into extra pulse") {
    entt::registry registry;
    CombatEventDispatcher::Init();
    SkillBehaviorRegistry::Initialize();
    SkillSystem::ShutdownHooks();
    SkillSystem::InitHooks();

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::DemonBlade;
    mastery.blood_oath_active = true;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 3;
    resource.max = 10;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 12;
    active.specialized_slots[0].allocated_points[1217] = 1;

    SkillExecution exec;
    exec.skill_id = 12;
    exec.owner = player;
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(12);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
    const auto beforeTargetHealth = registry.get<HealthComponent>(target).current;

    auto view = registry.view<BloodSeaFieldComponent>();
    REQUIRE(view.begin() != view.end());
    const auto fieldEntity = *view.begin();

    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, target, 1,
                      Tag::Hit | Tag::Melee | Tag::SwordSkill | Tag::Physical,
                      false, 1217001u));

    const auto &field = view.get<BloodSeaFieldComponent>(fieldEntity);
    CHECK(field.has_linked_synergy);
    CHECK(field.linked_hit_count == 1);
    CHECK(field.pulses_triggered == 1);
    CHECK(field.linked_pulse_cooldown == doctest::Approx(0.2f));
    CHECK(registry.get<HealthComponent>(target).current < beforeTargetHealth);
  }
}

TEST_CASE("[Unit] SkillBehaviorGuard - Update advances mastery field refunds ticks and cooldowns") {
  CombatEventDispatcher::Init();
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  SUBCASE("Skill 11 cycle refunds are capped and linked cooldown decays across updates") {
    entt::registry registry;
    systems::SpatialHashGrid grid(100, 100, 50);

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::HeavenlySword;
    mastery.heavenly_attunement = BladeAttunement::Lightning;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::SpiritBladeTier;
    resource.current = 0;
    resource.max = 10;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 11;
    active.specialized_slots[0].allocated_points[1113] = 1;
    active.specialized_slots[0].allocated_points[1117] = 1;

    const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});

    SkillExecution exec;
    exec.skill_id = 11;
    exec.owner = player;
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(11);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<HeavenlySwordFieldComponent>();
    REQUIRE(view.begin() != view.end());
    const auto fieldEntity = *view.begin();
    const auto fieldState = [&]() -> HeavenlySwordFieldComponent & {
      return registry.get<HeavenlySwordFieldComponent>(fieldEntity);
    };
    REQUIRE(fieldState().has_cycle);
    REQUIRE(fieldState().has_array_synchrony);

    grid.rebuild(registry.view<Position>(), registry);
    SkillSystem::Update(registry, grid, 0.10f);
    CHECK(resource.current >= 1);
    CHECK(fieldState().cycle_refunds_granted == 0);
    CHECK(fieldState().cycle_refund_timer == doctest::Approx(0.9f));

    grid.rebuild(registry.view<Position>(), registry);
    SkillSystem::Update(registry, grid, 1.01f);
    CHECK(fieldState().cycle_refunds_granted == 1);
    CHECK(fieldState().cycle_refund_timer == doctest::Approx(1.0f));

    grid.rebuild(registry.view<Position>(), registry);
    SkillSystem::Update(registry, grid, 1.01f);
    CHECK(fieldState().cycle_refunds_granted == 2);

    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, target, 3,
                      Tag::Hit | Tag::Melee | Tag::SwordSkill | Tag::Physical,
                      false, 21117001u));
    CHECK(fieldState().linked_cut_cooldown == doctest::Approx(0.15f));
    CHECK(fieldState().echo_strikes_triggered == 1);

    const auto echoAfterFirstHit = fieldState().echo_strikes_triggered;
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, target, 3,
                      Tag::Hit | Tag::Melee | Tag::SwordSkill | Tag::Physical,
                      false, 21117002u));
    CHECK(fieldState().echo_strikes_triggered == echoAfterFirstHit);

    grid.rebuild(registry.view<Position>(), registry);
    SkillSystem::Update(registry, grid, 0.10f);
    CHECK(fieldState().linked_cut_cooldown == doctest::Approx(0.05f));

    grid.rebuild(registry.view<Position>(), registry);
    SkillSystem::Update(registry, grid, 0.06f);
    CHECK(fieldState().linked_cut_cooldown == doctest::Approx(0.0f));

    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, target, 3,
                      Tag::Hit | Tag::Melee | Tag::SwordSkill | Tag::Physical,
                      false, 21117003u));
    CHECK(fieldState().echo_strikes_triggered == echoAfterFirstHit + 1);
    CHECK(fieldState().linked_cut_cooldown == doctest::Approx(0.15f));
  }

  SUBCASE("Skill 12 linked pulse cooldown decays and periodic updates keep pulsing") {
    entt::registry registry;
    systems::SpatialHashGrid grid(100, 100, 50);

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &playerStats = registry.get<CombatStats>(player);
    playerStats.max_health = 200.0f;
    playerStats.health = 60.0f;

    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::DemonBlade;
    mastery.blood_oath_active = true;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 3;
    resource.max = 10;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 12;
    active.specialized_slots[0].allocated_points[1217] = 1;

    const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});

    SkillExecution exec;
    exec.skill_id = 12;
    exec.owner = player;
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(12);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<BloodSeaFieldComponent>();
    REQUIRE(view.begin() != view.end());
    const auto fieldEntity = *view.begin();
    const auto fieldState = [&]() -> BloodSeaFieldComponent & {
      return registry.get<BloodSeaFieldComponent>(fieldEntity);
    };
    REQUIRE(fieldState().has_linked_synergy);

    const auto startHealth = registry.get<HealthComponent>(target).current;
    const float ownerHealthBeforeLinked = playerStats.health;
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, target, 1,
                      Tag::Hit | Tag::Melee | Tag::SwordSkill | Tag::Physical,
                      false, 21217001u));
    CHECK(fieldState().pulses_triggered == 1);
    CHECK(fieldState().linked_pulse_cooldown == doctest::Approx(0.2f));
    CHECK(registry.get<HealthComponent>(target).current < startHealth);
    CHECK(playerStats.health > ownerHealthBeforeLinked);

    const auto pulsesAfterFirstLink = fieldState().pulses_triggered;
    grid.rebuild(registry.view<Position>(), registry);
    SkillSystem::Update(registry, grid, 0.10f);
    CHECK(fieldState().linked_pulse_cooldown == doctest::Approx(0.1f));
    CHECK(fieldState().pulses_triggered == pulsesAfterFirstLink + 1);

    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, target, 1,
                      Tag::Hit | Tag::Melee | Tag::SwordSkill | Tag::Physical,
                      false, 21217002u));
    CHECK(fieldState().pulses_triggered == pulsesAfterFirstLink + 1);

    const float ownerHealthBeforeCooldownClear = playerStats.health;
    const auto targetHealthBeforeCooldownClear = registry.get<HealthComponent>(target).current;
    grid.rebuild(registry.view<Position>(), registry);
    SkillSystem::Update(registry, grid, 0.11f);
    CHECK(fieldState().linked_pulse_cooldown == doctest::Approx(0.0f));
    CHECK(fieldState().pulses_triggered == pulsesAfterFirstLink + 1);
    CHECK(playerStats.health == doctest::Approx(ownerHealthBeforeCooldownClear));
    CHECK(registry.get<HealthComponent>(target).current ==
          doctest::Approx(targetHealthBeforeCooldownClear));

    const float ownerHealthBeforeSecondTick = playerStats.health;
    const auto targetHealthBeforeSecondTick = registry.get<HealthComponent>(target).current;
    grid.rebuild(registry.view<Position>(), registry);
    SkillSystem::Update(registry, grid, 0.15f);
    CHECK(fieldState().pulses_triggered == pulsesAfterFirstLink + 2);
    CHECK(playerStats.health > ownerHealthBeforeSecondTick);
    CHECK(registry.get<HealthComponent>(target).current < targetHealthBeforeSecondTick);

    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, target, 1,
                      Tag::Hit | Tag::Melee | Tag::SwordSkill | Tag::Physical,
                      false, 21217003u));
    CHECK(fieldState().pulses_triggered == pulsesAfterFirstLink + 3);
    CHECK(fieldState().linked_pulse_cooldown == doctest::Approx(0.2f));
  }
}

TEST_CASE("[Unit] SkillBehaviorGuard - Deeper mastery update branches apply debuffs and cadence differences") {
  CombatEventDispatcher::Init();
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  SUBCASE("Skill 11 field tick applies attunement resist shred with extra razing") {
    entt::registry registry;
    systems::SpatialHashGrid grid(100, 100, 50);

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::HeavenlySword;
    mastery.heavenly_attunement = BladeAttunement::Fire;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::SpiritBladeTier;
    resource.current = 2;
    resource.max = 10;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 11;
    active.specialized_slots[0].allocated_points[1124] = 2;

    const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
    const auto targetHealthBeforeTick = registry.get<HealthComponent>(target).current;

    SkillExecution exec;
    exec.skill_id = 11;
    exec.owner = player;
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(11);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<HeavenlySwordFieldComponent>();
    REQUIRE(view.begin() != view.end());
    const auto fieldEntity = *view.begin();
    const auto fieldState = [&]() -> HeavenlySwordFieldComponent & {
      return registry.get<HeavenlySwordFieldComponent>(fieldEntity);
    };

    CHECK(fieldState().attunement == BladeAttunement::Fire);
    CHECK(fieldState().extra_resist_reduction == doctest::Approx(4.0f));

    grid.rebuild(registry.view<Position>(), registry);
    SkillSystem::Update(registry, grid, 0.01f);

    CHECK(registry.get<HealthComponent>(target).current < targetHealthBeforeTick);
    REQUIRE(test::skill_keynode_matrix::HasEffectById(
        registry, target, "heavenly_sword_field_resist"));

    auto &effects = registry.get<ActiveEffectsComponent>(target);
    const auto *debuff = effects.Get("heavenly_sword_field_resist");
    REQUIRE(debuff != nullptr);
    REQUIRE_FALSE(debuff->modifiers.empty());
    CHECK(debuff->modifiers.front().type == StatType::ResistFire);
    CHECK(debuff->modifiers.front().value == doctest::Approx(-10.0f));
  }

  SUBCASE("Skill 12 torrent and ring branches diverge in linked cooldown and pulse cadence") {
    struct BranchOutcome {
      float initial_linked_cooldown = 0.0f;
      int pulses_after_first_update = 0;
      int pulses_after_second_update = 0;
      float cooldown_after_second_update = 0.0f;
    };

    const auto exerciseBranch = [&](const uint32_t node_id) {
      entt::registry registry;
      systems::SpatialHashGrid grid(100, 100, 50);

      const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
      auto &mastery = registry.emplace<BladeMasteryComponent>(player);
      mastery.selected = BladeMasteryId::DemonBlade;
      mastery.blood_oath_active = true;

      auto &resource = registry.emplace<BladeResourceComponent>(player);
      resource.kind = BladeResourceKind::Bloodthirst;
      resource.current = 3;
      resource.max = 10;

      auto &active = registry.emplace<ActiveSkillsComponent>(player);
      active.specialized_slots[0].skill_id = 12;
      active.specialized_slots[0].allocated_points[1217] = 1;
      active.specialized_slots[0].allocated_points[node_id] = 1;

      const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});

      SkillExecution exec;
      exec.skill_id = 12;
      exec.owner = player;
      exec.target_pos = {18.0f, 0.0f};

      auto cast = SkillBehaviorRegistry::GetCast(12);
      REQUIRE(cast != nullptr);
      cast(registry, player, exec);

      auto view = registry.view<BloodSeaFieldComponent>();
      REQUIRE(view.begin() != view.end());
      const auto fieldEntity = *view.begin();
      const auto fieldState = [&]() -> BloodSeaFieldComponent & {
        return registry.get<BloodSeaFieldComponent>(fieldEntity);
      };

      CombatEventDispatcher::Dispatch(
          registry, CombatEventFactory::CreateSkillHit(
                        player, target, 1,
                        Tag::Hit | Tag::Melee | Tag::SwordSkill | Tag::Physical,
                        false, 30120001u + node_id));

      BranchOutcome outcome;
      outcome.initial_linked_cooldown = fieldState().linked_pulse_cooldown;

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.10f);
      outcome.pulses_after_first_update = fieldState().pulses_triggered;

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.22f);
      outcome.pulses_after_second_update = fieldState().pulses_triggered;
      outcome.cooldown_after_second_update = fieldState().linked_pulse_cooldown;

      return outcome;
    };

    const auto torrent = exerciseBranch(1221);
    const auto ring = exerciseBranch(1222);

    CHECK(torrent.initial_linked_cooldown == doctest::Approx(0.12f));
    CHECK(ring.initial_linked_cooldown == doctest::Approx(0.2f));
    CHECK(torrent.pulses_after_first_update == 2);
    CHECK(ring.pulses_after_first_update == 2);
    CHECK(torrent.pulses_after_second_update == 3);
    CHECK(ring.pulses_after_second_update == 2);
    CHECK(torrent.cooldown_after_second_update == doctest::Approx(0.0f));
    CHECK(ring.cooldown_after_second_update == doctest::Approx(0.0f));
  }
}

TEST_CASE("[Unit] SkillBehaviorGuard - Deep dive cadence and miasma refresh") {
  CombatEventDispatcher::Init();
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  SkillSystem::InitHooks();

  SUBCASE("Skill 11 node 1116 advances the second field tick cadence") {
    struct TickOutcome {
      float damage_interval = 0.0f;
      float health_after_first_tick = 0.0f;
      float health_after_second_window = 0.0f;
    };

    const auto exerciseBranch = [&](const int field_resonance_points) {
      entt::registry registry;
      systems::SpatialHashGrid grid(100, 100, 50);

      const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
      auto &mastery = registry.emplace<BladeMasteryComponent>(player);
      mastery.selected = BladeMasteryId::HeavenlySword;
      mastery.heavenly_attunement = BladeAttunement::Lightning;

      auto &resource = registry.emplace<BladeResourceComponent>(player);
      resource.kind = BladeResourceKind::SpiritBladeTier;
      resource.current = 2;
      resource.max = 10;

      auto &active = registry.emplace<ActiveSkillsComponent>(player);
      active.specialized_slots[0].skill_id = 11;
      if (field_resonance_points > 0) {
        active.specialized_slots[0].allocated_points[1116] = field_resonance_points;
      }

      const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
      const auto initialHealth = registry.get<HealthComponent>(target).current;

      SkillExecution exec;
      exec.skill_id = 11;
      exec.owner = player;
      exec.target_pos = {18.0f, 0.0f};

      auto cast = SkillBehaviorRegistry::GetCast(11);
      REQUIRE(cast != nullptr);
      cast(registry, player, exec);

      auto view = registry.view<HeavenlySwordFieldComponent>();
      REQUIRE(view.begin() != view.end());
      const auto fieldEntity = *view.begin();
      const auto fieldState = [&]() -> HeavenlySwordFieldComponent & {
        return registry.get<HeavenlySwordFieldComponent>(fieldEntity);
      };

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.01f);
      const auto healthAfterFirstTick = registry.get<HealthComponent>(target).current;

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.43f);

      TickOutcome outcome;
      outcome.damage_interval = fieldState().damage_interval;
      outcome.health_after_first_tick = healthAfterFirstTick - initialHealth;
      outcome.health_after_second_window =
          registry.get<HealthComponent>(target).current - initialHealth;
      return outcome;
    };

    const auto base = exerciseBranch(0);
    const auto resonant = exerciseBranch(2);

    CHECK(base.damage_interval == doctest::Approx(0.5f));
    CHECK(resonant.damage_interval == doctest::Approx(0.42f));
    CHECK(base.health_after_first_tick < 0.0f);
    CHECK(resonant.health_after_first_tick < 0.0f);
    CHECK(base.health_after_second_window == doctest::Approx(base.health_after_first_tick));
    CHECK(resonant.health_after_second_window < resonant.health_after_first_tick);
  }

  SUBCASE("Skill 12 node 1224 applies dual resist shred and refreshes miasma duration") {
    entt::registry registry;
    systems::SpatialHashGrid grid(100, 100, 50);

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::DemonBlade;
    mastery.blood_oath_active = true;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 3;
    resource.max = 10;

    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 12;
    active.specialized_slots[0].allocated_points[1217] = 1;
    active.specialized_slots[0].allocated_points[1224] = 2;

    const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});

    SkillExecution exec;
    exec.skill_id = 12;
    exec.owner = player;
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(12);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, target, 1,
                      Tag::Hit | Tag::Melee | Tag::SwordSkill | Tag::Physical,
                      false, 41224001u));

    REQUIRE(test::skill_keynode_matrix::HasEffectById(
        registry, target, "blood_sea_miasma"));
    REQUIRE(registry.all_of<ActiveEffectsComponent>(player));
    auto &playerEffects = registry.get<ActiveEffectsComponent>(player);
    auto *bloodSeaBuff = playerEffects.Get("blood_sea_active");
    REQUIRE(bloodSeaBuff != nullptr);
    const float initialBloodSeaDuration = bloodSeaBuff->duration;

    auto &effects = registry.get<ActiveEffectsComponent>(target);
    auto *debuff = effects.Get("blood_sea_miasma");
    REQUIRE(debuff != nullptr);
    REQUIRE(debuff->modifiers.size() >= 2);
    CHECK(debuff->modifiers[0].type == StatType::ResistPhysical);
    CHECK(debuff->modifiers[0].value == doctest::Approx(-4.0f));
    CHECK(debuff->modifiers[1].type == StatType::ResistShadow);
    CHECK(debuff->modifiers[1].value == doctest::Approx(-4.0f));
    CHECK(debuff->remaining == doctest::Approx(1.0f));

    debuff->remaining = 0.25f;
    registry.get<Position>(target) = Position{500.0f, 0.0f};
    grid.rebuild(registry.view<Position>(), registry);
    StatsSystem::UpdateBuffs(registry, 0.21f);
    SkillSystem::Update(registry, grid, 0.21f);
    systems::EffectSystem::update(registry, 0.21f);

    bloodSeaBuff = playerEffects.Get("blood_sea_active");
    REQUIRE(bloodSeaBuff != nullptr);
    CHECK(bloodSeaBuff->duration == doctest::Approx(initialBloodSeaDuration));
    CHECK(bloodSeaBuff->remaining < bloodSeaBuff->duration);

    registry.get<Position>(target) = Position{18.0f, 0.0f};
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, target, 1,
                      Tag::Hit | Tag::Melee | Tag::SwordSkill | Tag::Physical,
                      false, 41224002u));
    debuff = effects.Get("blood_sea_miasma");
    REQUIRE(debuff != nullptr);
    CHECK(debuff->remaining == doctest::Approx(1.0f));
    CHECK(debuff->modifiers[0].value == doctest::Approx(-4.0f));
    CHECK(debuff->modifiers[1].value == doctest::Approx(-4.0f));

    REQUIRE(registry.all_of<ActiveEffectsComponent>(player));
    bloodSeaBuff = playerEffects.Get("blood_sea_active");
    REQUIRE(bloodSeaBuff != nullptr);
    CHECK_FALSE(bloodSeaBuff->is_debuff);
    CHECK(bloodSeaBuff->remaining > 4.0f);
    CHECK(!bloodSeaBuff->name.empty());
    CHECK(!bloodSeaBuff->description.empty());
  }

  SUBCASE("Skill 11 attunement switches heavenly_sword_field_resist stat type") {
    struct AttunementOutcome {
      StatType resist_type = StatType::ResistAll;
      float resist_value = 0.0f;
    };

    const auto exerciseAttunement = [&](const BladeAttunement attunement) {
      entt::registry registry;
      systems::SpatialHashGrid grid(100, 100, 50);

      const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
      auto &mastery = registry.emplace<BladeMasteryComponent>(player);
      mastery.selected = BladeMasteryId::HeavenlySword;
      mastery.heavenly_attunement = attunement;

      auto &resource = registry.emplace<BladeResourceComponent>(player);
      resource.kind = BladeResourceKind::SpiritBladeTier;
      resource.current = 2;
      resource.max = 10;

      auto &active = registry.emplace<ActiveSkillsComponent>(player);
      active.specialized_slots[0].skill_id = 11;
      active.specialized_slots[0].allocated_points[1124] = 1;

      const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});

      SkillExecution exec;
      exec.skill_id = 11;
      exec.owner = player;
      exec.target_pos = {18.0f, 0.0f};

      auto cast = SkillBehaviorRegistry::GetCast(11);
      REQUIRE(cast != nullptr);
      cast(registry, player, exec);

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.01f);

      auto &effects = registry.get<ActiveEffectsComponent>(target);
      auto *debuff = effects.Get("heavenly_sword_field_resist");
      REQUIRE(debuff != nullptr);
      REQUIRE_FALSE(debuff->modifiers.empty());

      AttunementOutcome outcome;
      outcome.resist_type = debuff->modifiers.front().type;
      outcome.resist_value = debuff->modifiers.front().value;
      return outcome;
    };

    const auto lightning = exerciseAttunement(BladeAttunement::Lightning);
    const auto frost = exerciseAttunement(BladeAttunement::Frost);
    const auto fire = exerciseAttunement(BladeAttunement::Fire);
    const auto none = exerciseAttunement(BladeAttunement::None);

    CHECK(lightning.resist_type == StatType::ResistLightning);
    CHECK(frost.resist_type == StatType::ResistCold);
    CHECK(fire.resist_type == StatType::ResistFire);
    CHECK(none.resist_type == StatType::ResistAll);
    CHECK(lightning.resist_value == doctest::Approx(-8.0f));
    CHECK(frost.resist_value == doctest::Approx(-8.0f));
    CHECK(fire.resist_value == doctest::Approx(-8.0f));
    CHECK(none.resist_value == doctest::Approx(-8.0f));
  }

  SUBCASE("Skill 12 1221+1224 refreshes miasma earlier than 1222+1224 periodic pulse") {
    struct BranchOutcome {
      float damage_interval = 0.0f;
      float remaining_after_same_window = 0.0f;
      float target_health_after_same_window = 0.0f;
    };

    const auto exerciseBranch = [&](const uint32_t branch_node_id) {
      entt::registry registry;
      systems::SpatialHashGrid grid(100, 100, 50);

      const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
      auto &mastery = registry.emplace<BladeMasteryComponent>(player);
      mastery.selected = BladeMasteryId::DemonBlade;
      mastery.blood_oath_active = true;

      auto &resource = registry.emplace<BladeResourceComponent>(player);
      resource.kind = BladeResourceKind::Bloodthirst;
      resource.current = 3;
      resource.max = 10;

      auto &active = registry.emplace<ActiveSkillsComponent>(player);
      active.specialized_slots[0].skill_id = 12;
      active.specialized_slots[0].allocated_points[1224] = 2;
      active.specialized_slots[0].allocated_points[branch_node_id] = 1;

      const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});

      SkillExecution exec;
      exec.skill_id = 12;
      exec.owner = player;
      exec.target_pos = {18.0f, 0.0f};

      auto cast = SkillBehaviorRegistry::GetCast(12);
      REQUIRE(cast != nullptr);
      cast(registry, player, exec);

      auto view = registry.view<BloodSeaFieldComponent>();
      REQUIRE(view.begin() != view.end());
      const auto fieldEntity = *view.begin();
      const auto fieldState = [&]() -> BloodSeaFieldComponent & {
        return registry.get<BloodSeaFieldComponent>(fieldEntity);
      };

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.01f);

      auto &effects = registry.get<ActiveEffectsComponent>(target);
      auto *debuff = effects.Get("blood_sea_miasma");
      REQUIRE(debuff != nullptr);
      debuff->remaining = 0.05f;

      const auto healthBeforeWindow = registry.get<HealthComponent>(target).current;
      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.22f);

      debuff = effects.Get("blood_sea_miasma");
      REQUIRE(debuff != nullptr);

      BranchOutcome outcome;
      outcome.damage_interval = fieldState().damage_interval;
      outcome.remaining_after_same_window = debuff->remaining;
      outcome.target_health_after_same_window =
          registry.get<HealthComponent>(target).current - healthBeforeWindow;
      return outcome;
    };

    const auto torrent = exerciseBranch(1221);
    const auto ring = exerciseBranch(1222);

    CHECK(torrent.damage_interval == doctest::Approx(0.2125f));
    CHECK(ring.damage_interval == doctest::Approx(0.25f));
    CHECK(torrent.remaining_after_same_window == doctest::Approx(1.0f));
    CHECK(ring.remaining_after_same_window == doctest::Approx(0.05f));
    CHECK(torrent.target_health_after_same_window < 0.0f);
    CHECK(ring.target_health_after_same_window == doctest::Approx(0.0f));
  }

  SUBCASE("Skill 11 nodes 1122 and 1123 add attunement-specific extra debuff state") {
    struct DebuffOutcome {
      size_t modifier_count = 0;
      StatType primary_resist_type = StatType::ResistAll;
      float primary_resist_value = 0.0f;
      bool has_move_speed_slow = false;
    };

    const auto exerciseCase = [&](const BladeAttunement attunement,
                                  const bool withPolarization,
                                  const bool withFrozenDominion,
                                  const bool withSolarIncineration) {
      entt::registry registry;
      systems::SpatialHashGrid grid(100, 100, 50);

      const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
      auto &mastery = registry.emplace<BladeMasteryComponent>(player);
      mastery.selected = BladeMasteryId::HeavenlySword;
      mastery.heavenly_attunement = attunement;

      auto &resource = registry.emplace<BladeResourceComponent>(player);
      resource.kind = BladeResourceKind::SpiritBladeTier;
      resource.current = 2;
      resource.max = 10;

      auto &active = registry.emplace<ActiveSkillsComponent>(player);
      active.specialized_slots[0].skill_id = 11;
      active.specialized_slots[0].allocated_points[1124] = 1;
      if (withPolarization) {
        active.specialized_slots[0].allocated_points[1120] = 1;
      }
      if (withFrozenDominion) {
        active.specialized_slots[0].allocated_points[1122] = 1;
      }
      if (withSolarIncineration) {
        active.specialized_slots[0].allocated_points[1123] = 1;
      }

      const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});

      SkillExecution exec;
      exec.skill_id = 11;
      exec.owner = player;
      exec.target_pos = {18.0f, 0.0f};

      auto cast = SkillBehaviorRegistry::GetCast(11);
      REQUIRE(cast != nullptr);
      cast(registry, player, exec);

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.01f);

      auto &effects = registry.get<ActiveEffectsComponent>(target);
      auto *debuff = effects.Get("heavenly_sword_field_resist");
      REQUIRE(debuff != nullptr);
      REQUIRE_FALSE(debuff->modifiers.empty());

      DebuffOutcome outcome;
      outcome.modifier_count = debuff->modifiers.size();
      outcome.primary_resist_type = debuff->modifiers.front().type;
      outcome.primary_resist_value = debuff->modifiers.front().value;
      for (const auto &modifier : debuff->modifiers) {
        if (modifier.type == StatType::MoveSpeed &&
            modifier.mode == ModifierMode::PercentAdd &&
            modifier.value == doctest::Approx(-12.0f)) {
          outcome.has_move_speed_slow = true;
        }
      }
      return outcome;
    };

    const auto frostDominion =
        exerciseCase(BladeAttunement::Frost, true, true, false);
    const auto fireIncineration =
        exerciseCase(BladeAttunement::Fire, true, false, true);

    CHECK(frostDominion.primary_resist_type == StatType::ResistCold);
    CHECK(frostDominion.primary_resist_value == doctest::Approx(-8.0f));
    CHECK(frostDominion.modifier_count == 2);
    CHECK(frostDominion.has_move_speed_slow);

    CHECK(fireIncineration.primary_resist_type == StatType::ResistFire);
    CHECK(fireIncineration.primary_resist_value == doctest::Approx(-8.0f));
    CHECK(fireIncineration.modifier_count == 1);
    CHECK_FALSE(fireIncineration.has_move_speed_slow);
  }

  SUBCASE("Skill 12 nodes 1220 and 1224 combine void scaling with stronger dual shred") {
    struct VoidMiasmaOutcome {
      float resist_shred = 0.0f;
      float bonus_damage_mult = 0.0f;
      float physical_ratio = 0.0f;
      float remaining_after_refresh_window = 0.0f;
      float health_delta_after_refresh_window = 0.0f;
      float modifier_value_0 = 0.0f;
      float modifier_value_1 = 0.0f;
    };

    const auto exerciseCase = [&](const bool withVoidKeystone) {
      entt::registry registry;
      systems::SpatialHashGrid grid(100, 100, 50);

      const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
      auto &mastery = registry.emplace<BladeMasteryComponent>(player);
      mastery.selected = BladeMasteryId::DemonBlade;
      mastery.blood_oath_active = true;

      auto &resource = registry.emplace<BladeResourceComponent>(player);
      resource.kind = BladeResourceKind::Bloodthirst;
      resource.current = 3;
      resource.max = 10;

      auto &active = registry.emplace<ActiveSkillsComponent>(player);
      active.specialized_slots[0].skill_id = 12;
      active.specialized_slots[0].allocated_points[1221] = 1;
      active.specialized_slots[0].allocated_points[1224] = 2;
      if (withVoidKeystone) {
        active.specialized_slots[0].allocated_points[1220] = 1;
      }

      const auto target = test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});

      SkillExecution exec;
      exec.skill_id = 12;
      exec.owner = player;
      exec.target_pos = {18.0f, 0.0f};

      auto cast = SkillBehaviorRegistry::GetCast(12);
      REQUIRE(cast != nullptr);
      cast(registry, player, exec);

      auto view = registry.view<BloodSeaFieldComponent>();
      REQUIRE(view.begin() != view.end());
      const auto fieldEntity = *view.begin();
      const auto &field = view.get<BloodSeaFieldComponent>(fieldEntity);

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.01f);

      auto &effects = registry.get<ActiveEffectsComponent>(target);
      auto *debuff = effects.Get("blood_sea_miasma");
      REQUIRE(debuff != nullptr);
      debuff->remaining = 0.05f;

      const auto healthBeforeRefreshWindow = registry.get<HealthComponent>(target).current;
      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.22f);

      debuff = effects.Get("blood_sea_miasma");
      REQUIRE(debuff != nullptr);

      VoidMiasmaOutcome outcome;
      outcome.resist_shred = field.resist_shred;
      outcome.bonus_damage_mult = field.bonus_damage_mult;
      outcome.physical_ratio = field.has_void_keystone ? 0.45f : 0.6f;
      outcome.remaining_after_refresh_window = debuff->remaining;
      outcome.health_delta_after_refresh_window =
          registry.get<HealthComponent>(target).current - healthBeforeRefreshWindow;
      outcome.modifier_value_0 = debuff->modifiers[0].value;
      outcome.modifier_value_1 = debuff->modifiers[1].value;
      return outcome;
    };

    const auto base = exerciseCase(false);
    const auto voidMiasma = exerciseCase(true);

    CHECK(base.resist_shred == doctest::Approx(4.0f));
    CHECK(voidMiasma.resist_shred == doctest::Approx(8.0f));
    CHECK(base.modifier_value_0 == doctest::Approx(-4.0f));
    CHECK(base.modifier_value_1 == doctest::Approx(-4.0f));
    CHECK(voidMiasma.modifier_value_0 == doctest::Approx(-8.0f));
    CHECK(voidMiasma.modifier_value_1 == doctest::Approx(-8.0f));
    CHECK(base.bonus_damage_mult == doctest::Approx(1.36f));
    CHECK(voidMiasma.bonus_damage_mult == doctest::Approx(1.6048f));
    CHECK(base.physical_ratio == doctest::Approx(0.6f));
    CHECK(voidMiasma.physical_ratio == doctest::Approx(0.45f));
    CHECK(base.remaining_after_refresh_window == doctest::Approx(1.0f));
    CHECK(voidMiasma.remaining_after_refresh_window == doctest::Approx(1.0f));
    CHECK(voidMiasma.health_delta_after_refresh_window < base.health_delta_after_refresh_window);
  }

  SUBCASE("Heavenly Sword impact nodes add center control and delayed follow-up") {
    struct ImpactOutcome {
      float distant_health_after_cast = 0.0f;
      float center_health_after_cast = 0.0f;
      float elite_health_after_first_tick = 0.0f;
      float elite_health_after_delayed_window = 0.0f;
      bool has_center_slow = false;
    };

    const auto exerciseCase = [&](const std::vector<std::pair<uint32_t, int>> &nodes) {
      entt::registry registry;
      systems::SpatialHashGrid grid(100, 100, 50);

      const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
      auto &mastery = registry.emplace<BladeMasteryComponent>(player);
      mastery.selected = BladeMasteryId::HeavenlySword;
      mastery.heavenly_attunement = BladeAttunement::Lightning;

      auto &resource = registry.emplace<BladeResourceComponent>(player);
      resource.kind = BladeResourceKind::SpiritBladeTier;
      resource.current = 3;
      resource.max = 10;

      test::skill_keynode_matrix::ConfigureSpecialization(registry, player, 11, nodes);

      const auto centerTarget =
          test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
      const auto eliteTarget =
          test::skill_keynode_matrix::CreateTarget(registry, {22.0f, 0.0f});
      registry.get<HealthComponent>(eliteTarget).max = 5000.0f;
      registry.get<HealthComponent>(eliteTarget).current = 5000.0f;
      registry.emplace<EnemyRarityComponent>(eliteTarget, EnemyRarityComponent::ELITE);
      const auto distantTarget =
          test::skill_keynode_matrix::CreateTarget(registry, {140.0f, 0.0f});

      SkillExecution exec;
      exec.skill_id = 11;
      exec.owner = player;
      exec.cast_id = 41108001u + static_cast<uint64_t>(nodes.size());
      exec.target_pos = {18.0f, 0.0f};

      auto cast = SkillBehaviorRegistry::GetCast(11);
      REQUIRE(cast != nullptr);
      cast(registry, player, exec);

      ImpactOutcome outcome;
      outcome.distant_health_after_cast =
          registry.get<HealthComponent>(distantTarget).current;
      outcome.center_health_after_cast =
          registry.get<HealthComponent>(centerTarget).current;

      if (registry.all_of<ActiveEffectsComponent>(centerTarget)) {
        const auto &effects = registry.get<ActiveEffectsComponent>(centerTarget);
        for (const auto &effect : effects.effects) {
          for (const auto &modifier : effect.modifiers) {
            if (modifier.type == StatType::MoveSpeed &&
                modifier.mode == ModifierMode::PercentAdd && modifier.value < 0.0f) {
              outcome.has_center_slow = true;
            }
          }
        }
      }

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.01f);
      outcome.elite_health_after_first_tick =
          registry.get<HealthComponent>(eliteTarget).current;

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.26f);
      outcome.elite_health_after_delayed_window =
          registry.get<HealthComponent>(eliteTarget).current;
      return outcome;
    };

    const auto baseline = exerciseCase({});
    const auto impactNodes =
        exerciseCase({{1100, 2}, {1104, 2}, {1105, 2}, {1106, 2}, {1107, 1}, {1108, 2}});

    CHECK(impactNodes.distant_health_after_cast < baseline.distant_health_after_cast);
    CHECK(impactNodes.center_health_after_cast < baseline.center_health_after_cast);
    CHECK(impactNodes.elite_health_after_first_tick < baseline.elite_health_after_first_tick);
    CHECK(impactNodes.elite_health_after_delayed_window <
          impactNodes.elite_health_after_first_tick);
    CHECK(impactNodes.has_center_slow);
  }

  SUBCASE("Heavenly Sword cycle nodes hasten swords empower refunds and punish afflicted targets") {
    struct CycleOutcome {
      float formation_attack_interval = 0.0f;
      float sword_attack_interval = 0.0f;
      float blade_formation_damage_scale = 0.0f;
      float second_blade_formation_damage_scale = 0.0f;
      float infinite_blades_damage_mult = 0.0f;
      float afflicted_target_health = 0.0f;
      float afflicted_remaining = 0.0f;
      float formation_attack_interval_after_cleanup = 0.0f;
      float sword_attack_interval_after_cleanup = 0.0f;
      float infinite_blades_tick_interval_after_cleanup = 0.0f;
    };

    const auto exerciseCase = [&](const std::vector<std::pair<uint32_t, int>> &nodes) {
      entt::registry registry;
      systems::SpatialHashGrid grid(100, 100, 50);

      const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
      auto &mastery = registry.emplace<BladeMasteryComponent>(player);
      mastery.selected = BladeMasteryId::HeavenlySword;
      mastery.heavenly_attunement = BladeAttunement::Fire;

      auto &resource = registry.emplace<BladeResourceComponent>(player);
      resource.kind = BladeResourceKind::SpiritBladeTier;
      resource.current = 2;
      resource.max = 10;

      test::skill_keynode_matrix::ConfigureSpecialization(registry, player, 11, nodes);

      SkillExecution formationExec;
      formationExec.skill_id = 3;
      formationExec.owner = player;
      auto formationCast = SkillBehaviorRegistry::GetCast(3);
      REQUIRE(formationCast != nullptr);
      formationCast(registry, player, formationExec);

      const auto afflictedTarget =
          test::skill_keynode_matrix::CreateTarget(registry, {18.0f, 0.0f});
      auto &effects = registry.emplace<ActiveEffectsComponent>(afflictedTarget);
      BuffEffect ignite;
      ignite.id = "test_ignite";
      ignite.name = "Test Ignite";
      ignite.type = BuffType::Burn;
      ignite.duration = 3.0f;
      ignite.remaining = 0.2f;
      ignite.is_debuff = true;
      effects.AddOrRefresh(ignite);

      SkillExecution descentExec;
      descentExec.skill_id = 11;
      descentExec.owner = player;
      descentExec.cast_id = 41118001u + static_cast<uint64_t>(nodes.size());
      descentExec.target_pos = {18.0f, 0.0f};

      auto descentCast = SkillBehaviorRegistry::GetCast(11);
      REQUIRE(descentCast != nullptr);
      descentCast(registry, player, descentExec);

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 1.01f);

      SkillExecution empoweredFormation;
      empoweredFormation.skill_id = 3;
      empoweredFormation.owner = player;
      formationCast(registry, player, empoweredFormation);

      SkillExecution infiniteExec;
      infiniteExec.skill_id = 5;
      infiniteExec.owner = player;
      infiniteExec.target_pos = {18.0f, 0.0f};
      auto infiniteCast = SkillBehaviorRegistry::GetCast(5);
      REQUIRE(infiniteCast != nullptr);
      infiniteCast(registry, player, infiniteExec);

      grid.rebuild(registry.view<Position>(), registry);
      SkillSystem::Update(registry, grid, 0.01f);

      CycleOutcome outcome;
      const auto &formation = registry.get<BladeFormationComponent>(player);
      outcome.formation_attack_interval = formation.attack_interval;

      auto swordView = registry.view<SpiritSwordTag, SpiritSwordAI, SummonCombatProfile>();
      REQUIRE(swordView.begin() != swordView.end());
      const auto sword = *swordView.begin();
      outcome.sword_attack_interval = swordView.get<SpiritSwordAI>(sword).attack_interval;
      outcome.blade_formation_damage_scale =
          swordView.get<SummonCombatProfile>(sword).damage_scale;

      SkillExecution secondFormation;
      secondFormation.skill_id = 3;
      secondFormation.owner = player;
      formationCast(registry, player, secondFormation);
      outcome.second_blade_formation_damage_scale =
          swordView.get<SummonCombatProfile>(sword).damage_scale;

      REQUIRE(registry.all_of<ChannelingComponent>(player));
      outcome.infinite_blades_damage_mult =
          registry.get<ChannelingComponent>(player).bonus_damage_mult;
      outcome.afflicted_target_health =
          registry.get<HealthComponent>(afflictedTarget).current;
      auto &refreshedEffects = registry.get<ActiveEffectsComponent>(afflictedTarget);
      auto *refreshedIgnite = refreshedEffects.Get("test_ignite");
      REQUIRE(refreshedIgnite != nullptr);
      outcome.afflicted_remaining = refreshedIgnite->remaining;

      mastery.selected = BladeMasteryId::SwordSaint;
      SkillSystem::ShutdownHooks();
      systems::BladeMasteryService::RefreshPlayerState(registry, player);
      outcome.formation_attack_interval_after_cleanup = formation.attack_interval;
      outcome.sword_attack_interval_after_cleanup = swordView.get<SpiritSwordAI>(sword).attack_interval;
      outcome.infinite_blades_tick_interval_after_cleanup =
          registry.get<ChannelingComponent>(player).tick_interval;
      return outcome;
    };

    const auto baseline = exerciseCase({{1113, 1}});
    const auto cycleNodes = exerciseCase({{1112, 2}, {1113, 1}, {1114, 2}, {1118, 2}});

    CHECK(cycleNodes.formation_attack_interval < baseline.formation_attack_interval);
    CHECK(cycleNodes.sword_attack_interval < baseline.sword_attack_interval);
    CHECK(cycleNodes.blade_formation_damage_scale > baseline.blade_formation_damage_scale);
    CHECK(cycleNodes.second_blade_formation_damage_scale ==
          doctest::Approx(baseline.blade_formation_damage_scale));
    CHECK(cycleNodes.infinite_blades_damage_mult ==
          doctest::Approx(baseline.infinite_blades_damage_mult));
    CHECK(cycleNodes.afflicted_target_health < baseline.afflicted_target_health);
    CHECK(cycleNodes.afflicted_remaining > baseline.afflicted_remaining);
    CHECK(cycleNodes.formation_attack_interval_after_cleanup ==
          doctest::Approx(baseline.formation_attack_interval));
    CHECK(cycleNodes.sword_attack_interval_after_cleanup ==
          doctest::Approx(baseline.sword_attack_interval));
    CHECK(cycleNodes.infinite_blades_tick_interval_after_cleanup ==
          doctest::Approx(0.5f));
  }
}

} // namespace NoMoreDay
