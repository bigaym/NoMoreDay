#include "TestCommon.hpp"
#include "SkillKeyNodeMatrixTestHelpers.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

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

} // namespace NoMoreDay
