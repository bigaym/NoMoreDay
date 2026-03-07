#include "SkillKeyNodeMatrixTestHelpers.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Progression.hpp"
#include "game/components/Projectile.hpp"
#include "game/data/BladeMasteryRegistry.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include <array>

namespace NoMoreDay {
namespace sknm = test::skill_keynode_matrix;

namespace test::skill_keynode_matrix::integration {

inline void InitContext(entt::registry &) {
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  CombatEventDispatcher::Clear();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();
}

inline void RunTicks(entt::registry &registry, systems::SpatialHashGrid &grid,
                     int count, float dt) {
  for (int i = 0; i < count; ++i) {
    SkillSystem::Update(registry, grid, dt);
  }
}

inline bool HasArrayFlags(entt::registry &registry) {
  auto view = registry.view<SwordArrayComponent>();
  if (view.begin() == view.end()) {
    return false;
  }
  const auto ent = *view.begin();
  const auto &array = view.get<SwordArrayComponent>(ent);
  return array.has_slow && array.has_execute && array.gain_intent_on_tick;
}

inline bool HasBoomerangProjectiles(entt::registry &registry, int min_count) {
  auto view = registry.view<Projectile, BoomerangComponent>();
  int count = 0;
  for (const auto entity : view) {
    (void)entity;
    ++count;
  }
  return count >= min_count;
}

} // namespace test::skill_keynode_matrix::integration

TEST_CASE("[Integration] SkillKeyNodeMatrix - Per-skill runtime scenarios (1..10)") {
  const auto fixture_nodes = sknm::LoadFixtureKeyNodes();
  REQUIRE(fixture_nodes.size() == 10);

  for (uint32_t skill_id = 1; skill_id <= 10; ++skill_id) {
    CAPTURE(skill_id);
    REQUIRE(fixture_nodes.contains(skill_id));

    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    systems::SpatialHashGrid grid(1024, 1024, 64);

    const auto caster = sknm::CreateCaster(registry, 2000.0f);
    const auto target = sknm::CreateTarget(registry, {120.0f, 0.0f});
    sknm::ConfigureSkillSlot(registry, caster, skill_id, 0, 2);
    sknm::ConfigureSpecialization(
        registry, caster, skill_id,
        sknm::AsAllocatedPoints(fixture_nodes.at(skill_id), 1));

    if (skill_id == 10) {
      REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
          "assets/data/blade_masteries.json"));
      auto &stats = registry.get_or_emplace<PlayerStats>(caster);
      stats.level = 50;
      auto &astrolabe = registry.get_or_emplace<AstrolabeComponent>(caster);
      astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
      systems::BladeMasteryService::RefreshPlayerState(registry, caster);
      REQUIRE(systems::BladeMasteryService::SelectMastery(
          registry, caster, BladeMasteryId::SwordSaint));
      REQUIRE(systems::BladeResourceService::Gain(registry, caster, 3, skill_id));
    }

    CHECK(SkillSystem::TryCast(registry, caster, 0, {120.0f, 0.0f}));
    test::skill_keynode_matrix::integration::RunTicks(registry, grid, 3, 0.08f);

    switch (skill_id) {
    case 1:
      {
        const bool has_phase = registry.any_of<PhaseTag>(caster);
        const bool has_swift = sknm::HasEffectById(
            registry, caster, "flowing_thrust_swift");
        const bool has_signal = has_phase || has_swift;
        CHECK(has_signal);
      }
      break;
    case 2:
      {
        auto proj_view = registry.view<Projectile>();
        CHECK(proj_view.begin() != proj_view.end());
      }
      break;
    case 3:
      CHECK(registry.all_of<BladeFormationComponent>(caster));
      break;
    case 4:
      REQUIRE(registry.all_of<BladeWardComponent>(caster));
      CHECK(registry.get<BladeWardComponent>(caster).trigger_counter);
      break;
    case 5:
      REQUIRE(registry.all_of<ChannelingComponent>(caster));
      CHECK(registry.get<ChannelingComponent>(caster).skill_id == 5);
      break;
    case 6:
      CHECK(test::skill_keynode_matrix::integration::HasArrayFlags(registry));
      break;
    case 7:
      REQUIRE(registry.all_of<ChannelingComponent>(caster));
      CHECK(registry.get<ChannelingComponent>(caster).skill_id == 7);
      break;
    case 8:
      CHECK(test::skill_keynode_matrix::integration::HasBoomerangProjectiles(
          registry, 3));
      break;
    case 9:
      REQUIRE(registry.all_of<PhantomFlashComponent>(caster));
      CHECK(registry.get<PhantomFlashComponent>(caster).flow_reset);
      break;
    case 10:
      CHECK(registry.any_of<InvulnerableComponent>(caster));
      break;
    default:
      FAIL("Unexpected skill id in key-node matrix test");
      break;
    }

    (void)target;
  }
}

TEST_CASE("[Integration] SkillKeyNodeMatrix - Trigger chain matrix covers all trigger nodes") {
  const auto compact = sknm::LoadCompactContractBuckets();
  CHECK(compact.trigger_node_by_skill.size() == 10);

  int scenario_count = 0;
  for (const auto &[skill_id, trigger_node] : compact.trigger_node_by_skill) {
    CAPTURE(skill_id);
    CAPTURE(trigger_node);
    REQUIRE(compact.trigger_skill_by_node.contains(trigger_node));
    const uint32_t trigger_skill = compact.trigger_skill_by_node.at(trigger_node);

    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    const auto caster = sknm::CreateCaster(registry, 1500.0f);
    const auto target = sknm::CreateTarget(registry, {30.0f, 0.0f});
    sknm::ConfigureSpecialization(registry, caster, skill_id,
                                  {{trigger_node, 1}});

    const auto before = registry.storage<SkillExecution>().size();
    sknm::DispatchSkillHit(registry, caster, target, skill_id,
                           static_cast<uint64_t>(7000 + skill_id));
    const auto after = registry.storage<SkillExecution>().size();
    CHECK(after > before);

    const auto *runtime =
        registry.try_get<SkillContractRuntimeComponent>(caster);
    REQUIRE(runtime != nullptr);
    CHECK(runtime->trigger_cooldowns.contains(trigger_node));

    bool found_triggered_skill = false;
    auto view = registry.view<SkillExecution>();
    for (const auto exec_entity : view) {
      const auto &exec = view.get<SkillExecution>(exec_entity);
      if (exec.skill_id == trigger_skill && exec.trigger_depth == 1) {
        found_triggered_skill = true;
        break;
      }
    }
    CHECK(found_triggered_skill);
    ++scenario_count;
  }

  CHECK(scenario_count == 10);
}

TEST_CASE("[Integration] SkillKeyNodeMatrix - Cross-skill and visual-signal guard matrix >= 12") {
  const auto compact = sknm::LoadCompactContractBuckets();
  int scenario_count = 0;

  for (const auto &[skill_id, trigger_node] : compact.trigger_node_by_skill) {
    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    const auto caster = sknm::CreateCaster(registry, 1200.0f);
    const auto target = sknm::CreateTarget(registry, {16.0f, 0.0f});
    sknm::ConfigureSpecialization(registry, caster, skill_id,
                                  {{trigger_node, 1}});

    const auto before = registry.storage<SkillExecution>().size();
    sknm::DispatchSkillHit(registry, caster, target, skill_id,
                           static_cast<uint64_t>(8200 + skill_id));
    const auto after = registry.storage<SkillExecution>().size();
    CHECK(after > before);
    ++scenario_count;
  }

  {
    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    systems::SpatialHashGrid grid(1024, 1024, 64);
    const auto caster = sknm::CreateCaster(registry, 1000.0f);
    sknm::ConfigureSkillSlot(registry, caster, 1, 0, 1);
    sknm::ConfigureSpecialization(registry, caster, 1, {{130, 1}});
    CHECK(SkillSystem::TryCast(registry, caster, 0, {80.0f, 0.0f}));
    test::skill_keynode_matrix::integration::RunTicks(registry, grid, 8, 0.08f);
    auto shadow_view = registry.view<ShadowComponent>();
    CHECK(shadow_view.begin() != shadow_view.end());
    ++scenario_count;
  }

  {
    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    systems::SpatialHashGrid grid(1024, 1024, 64);
    const auto caster = sknm::CreateCaster(registry, 1000.0f);
    sknm::ConfigureSkillSlot(registry, caster, 6, 0, 1);
    sknm::ConfigureSpecialization(registry, caster, 6,
                                  {{630, 1}, {633, 1}, {652, 1}, {670, 1}});
    CHECK(SkillSystem::TryCast(registry, caster, 0, {20.0f, 0.0f}));
    test::skill_keynode_matrix::integration::RunTicks(registry, grid, 10, 0.08f);
    CHECK(test::skill_keynode_matrix::integration::HasArrayFlags(registry));
    ++scenario_count;
  }

  {
    entt::registry registry;
    test::skill_keynode_matrix::integration::InitContext(registry);
    systems::SpatialHashGrid grid(1024, 1024, 64);
    const auto caster = sknm::CreateCaster(registry, 1000.0f);
    sknm::ConfigureSkillSlot(registry, caster, 8, 0, 1);
    sknm::ConfigureSpecialization(registry, caster, 8, {{871, 1}});
    CHECK(SkillSystem::TryCast(registry, caster, 0, {120.0f, 0.0f}));
    test::skill_keynode_matrix::integration::RunTicks(registry, grid, 8, 0.08f);
    CHECK(sknm::HasEffectById(registry, caster, "blade_boomerang_guard_qi"));
    ++scenario_count;
  }

  CHECK(scenario_count >= 12);
}

} // namespace NoMoreDay
