#include "SkillKeyNodeMatrixTestHelpers.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include <array>

namespace NoMoreDay {
namespace sknm = test::skill_keynode_matrix;

TEST_CASE("[Unit] SkillKeyNodeMatrix - Fixture completeness matches compact contract") {
  const auto fixture_nodes = sknm::LoadFixtureKeyNodes();
  const auto expected_nodes = sknm::ExpectedKeyNodesBySkill();
  const auto fixture_layers = sknm::LoadFixtureLayers();
  const auto compact = sknm::LoadCompactContractBuckets();

  CHECK(fixture_nodes.size() == expected_nodes.size());
  CHECK(fixture_layers.size() == expected_nodes.size());

  const std::unordered_set<std::string> valid_layers = {
      "CastState", "RuntimeTick", "CombatEvent",
      "TagConversion", "GuardPolicy", "VisualSignalGuard"};

  size_t total_fixture_nodes = 0;
  for (const uint32_t skill_id : sknm::MatrixSkillIds()) {
    CAPTURE(skill_id);
    REQUIRE(fixture_nodes.contains(skill_id));
    REQUIRE(expected_nodes.contains(skill_id));
    REQUIRE(fixture_layers.contains(skill_id));

    const auto &fixture = fixture_nodes.at(skill_id);
    auto expected = expected_nodes.at(skill_id);
    std::sort(expected.begin(), expected.end());
    CHECK(fixture == expected);
    total_fixture_nodes += fixture.size();

    const auto &layers = fixture_layers.at(skill_id);
    CHECK_FALSE(layers.empty());
    for (const auto &layer : layers) {
      CHECK(valid_layers.contains(layer));
    }

    REQUIRE(compact.key_nodes_by_skill.contains(skill_id));
    const std::set<uint32_t> fixture_set(fixture.begin(), fixture.end());
    CHECK(fixture_set == compact.key_nodes_by_skill.at(skill_id));
  }

  CHECK(total_fixture_nodes == sknm::ExpectedTotalKeyNodeCount());
  CHECK(compact.all_key_nodes.size() == sknm::ExpectedTotalKeyNodeCount());
}

TEST_CASE("[Unit] SkillKeyNodeMatrix - Contract role and effect assertions cover all key nodes") {
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");
  const auto compact = sknm::LoadCompactContractBuckets();

  size_t checked_nodes = 0;
  for (const uint32_t skill_id : sknm::MatrixSkillIds()) {
    CAPTURE(skill_id);
    REQUIRE(compact.key_nodes_by_skill.contains(skill_id));

    for (const uint32_t node_id : compact.key_nodes_by_skill.at(skill_id)) {
      CAPTURE(node_id);
      const auto *node_contract = registry.GetNodeContract(skill_id, node_id);
      REQUIRE(node_contract != nullptr);

      if (compact.trigger_nodes.contains(node_id)) {
        const bool is_expected_role = (node_contract->role == SpecNodeRole::Trigger ||
                                       node_contract->role == SpecNodeRole::Passive ||
                                       node_contract->role == SpecNodeRole::Keystone);
        CHECK(is_expected_role);
        REQUIRE(compact.trigger_skill_by_node.contains(node_id));
        CHECK(node_contract->trigger.trigger_skill_id ==
              compact.trigger_skill_by_node.at(node_id));
        
        if (node_contract->role == SpecNodeRole::Trigger) {
          CHECK(node_contract->trigger.internal_cooldown > 0.0f);
        }
      }
      if (compact.transmuter_nodes.contains(node_id)) {
        CHECK(node_contract->role == SpecNodeRole::Transmuter);
      }
      if (compact.synergy_nodes.contains(node_id)) {
        CHECK(node_contract->role == SpecNodeRole::Synergy);
      }
      if (compact.sword_intent_nodes.contains(node_id)) {
        CHECK(SkillSystem::NodeAffectsSwordIntent(
            entt::registry{}, skill_id, node_id));
      }
      if (compact.sword_step_nodes.contains(node_id)) {
        CHECK(SkillSystem::NodeAffectsSwordStep(
            entt::registry{}, skill_id, node_id));
      }

      ++checked_nodes;
    }
  }

  CHECK(checked_nodes == sknm::ExpectedTotalKeyNodeCount());
}

TEST_CASE("[Unit] SkillKeyNodeMatrix - Trigger guards and transmuter mutex matrix") {
  auto &skill_registry = SkillRegistry::Get();
  skill_registry.LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  const auto compact = sknm::LoadCompactContractBuckets();

  SUBCASE("All trigger nodes enforce cooldown guard deterministically") {
    for (const auto &[skill_id, trigger_node] : compact.trigger_node_by_skill) {
      CAPTURE(skill_id);
      CAPTURE(trigger_node);

      const auto *node_contract = skill_registry.GetNodeContract(skill_id, trigger_node);
      REQUIRE(node_contract != nullptr);
      if (node_contract->role != SpecNodeRole::Trigger) {
        continue;
      }

      entt::registry registry;
      CombatEventDispatcher::Clear();
      SkillSystem::ShutdownHooks();
      SkillSystem::InitHooks();

      const auto caster = sknm::CreateCaster(registry);
      const auto target = sknm::CreateTarget(registry, {8.0f, 0.0f});
      sknm::ConfigureSpecialization(registry, caster, skill_id,
                                    {{trigger_node, 1}});

      const auto before = registry.storage<SkillExecution>().size();
      sknm::DispatchSkillHit(registry, caster, target, skill_id,
                             static_cast<uint64_t>(5000 + skill_id));
      const auto after_first = registry.storage<SkillExecution>().size();
      CHECK(after_first > before);

      const auto *runtime =
          registry.try_get<SkillContractRuntimeComponent>(caster);
      REQUIRE(runtime != nullptr);
      REQUIRE(runtime->trigger_cooldowns.contains(trigger_node));

      sknm::DispatchSkillHit(registry, caster, target, skill_id,
                             static_cast<uint64_t>(5000 + skill_id));
      const auto after_second = registry.storage<SkillExecution>().size();
      CHECK(after_second == after_first);
    }
  }

  SUBCASE("Trigger depth guard blocks dispatch when parent depth is exhausted") {
    for (const auto &[skill_id, trigger_node] : compact.trigger_node_by_skill) {
      CAPTURE(skill_id);
      CAPTURE(trigger_node);

      entt::registry registry;
      CombatEventDispatcher::Clear();
      SkillSystem::ShutdownHooks();
      SkillSystem::InitHooks();

      const auto caster = sknm::CreateCaster(registry);
      const auto target = sknm::CreateTarget(registry, {8.0f, 0.0f});
      sknm::ConfigureSpecialization(registry, caster, skill_id,
                                    {{trigger_node, 1}});

      auto parent_exec_entity = registry.create();
      auto &parent_exec = registry.emplace<SkillExecution>(parent_exec_entity);
      parent_exec.skill_id = skill_id;
      parent_exec.owner = caster;
      parent_exec.cast_id = static_cast<uint64_t>(9000 + skill_id);
      parent_exec.trigger_depth = 2;

      const auto before = registry.storage<SkillExecution>().size();
      sknm::DispatchSkillHit(registry, caster, target, skill_id,
                             parent_exec.cast_id);
      const auto after = registry.storage<SkillExecution>().size();
      CHECK(after == before);
    }
  }

  SUBCASE("Transmuter mutex selects contract-preferred node for skills 1..9") {
    for (uint32_t skill_id = 1; skill_id <= 9; ++skill_id) {
      CAPTURE(skill_id);
      REQUIRE(compact.transmuter_order_by_skill.contains(skill_id));
      const auto &order = compact.transmuter_order_by_skill.at(skill_id);
      REQUIRE(order.size() >= 2);

      entt::registry registry;
      const auto caster = sknm::CreateCaster(registry, 2000.0f);
      sknm::ConfigureSkillSlot(registry, caster, skill_id, 0, 1);
      sknm::ConfigureSpecialization(registry, caster, skill_id,
                                    {{order[0], 1}, {order[1], 1}});

      CHECK(SkillSystem::TryCast(registry, caster, 0, {32.0f, 0.0f}));

      const auto *runtime =
          registry.try_get<SkillContractRuntimeComponent>(caster);
      REQUIRE(runtime != nullptr);
      REQUIRE(runtime->active_transmuter_node_by_skill.contains(skill_id));
      CHECK(runtime->active_transmuter_node_by_skill.at(skill_id) == order[0]);
      CHECK(SkillSystem::GetActiveTransmuterNode(registry, caster, skill_id) ==
            order[0]);
    }
  }
}

} // namespace NoMoreDay
