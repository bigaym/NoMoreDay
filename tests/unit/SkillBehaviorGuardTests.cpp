#include "TestCommon.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/skill/SkillSystem.hpp"

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

} // namespace NoMoreDay
