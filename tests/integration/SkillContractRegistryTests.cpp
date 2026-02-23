#include "TestCommon.hpp"
#include "game/data/SkillContract.hpp"
#include "game/data/SkillRegistry.hpp"
#include <array>

namespace NoMoreDay {

TEST_CASE("[Integration] SkillContract - Registry loading and validation") {
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  for (uint32_t skill_id = 1; skill_id <= 9; ++skill_id) {
    CAPTURE(skill_id);
    const auto *contract = registry.GetSkillContract(skill_id);
    REQUIRE(contract != nullptr);
    std::string error;
    CHECK(registry.ValidateSkillContract(skill_id, &error));
    CHECK(error.empty());
  }
}

TEST_CASE("[Integration] SkillContract - Compact mapping materialized") {
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  SUBCASE("Skill 1 trigger contract") {
    const auto *node = registry.GetNodeContract(1, 114);
    REQUIRE(node != nullptr);
    CHECK(node->role == SpecNodeRole::Trigger);
    CHECK(node->trigger.trigger_skill_id == 2);
    CHECK(node->scope_policy == ScopePolicy::SkillOnly);
  }

  SUBCASE("Skill 8 transmuter role") {
    const auto *node = registry.GetNodeContract(8, 870);
    REQUIRE(node != nullptr);
    CHECK(node->role == SpecNodeRole::Transmuter);
  }

  SUBCASE("Skill 9 global scope policy") {
    const auto *node = registry.GetNodeContract(9, 971);
    REQUIRE(node != nullptr);
    CHECK(node->scope_policy == ScopePolicy::GlobalWhileBuffActive);
    CHECK(node->resist_model == ResistModel::TypeD_StatToPenetration);
  }
}

TEST_CASE("[Integration] SkillContract - Structural alignment matrix (skills 1..9)") {
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  constexpr size_t kExpectedNodeCount = 25;
  const std::array<uint32_t, 9> expected_trigger_nodes = {
      114, 233, 373, 451, 533, 633, 713, 831, 951};

  for (uint32_t skill_id = 1; skill_id <= 9; ++skill_id) {
    CAPTURE(skill_id);

    const auto *tree = registry.GetSkillTree(skill_id);
    REQUIRE(tree != nullptr);
    CHECK(tree->nodes.size() == kExpectedNodeCount);

    const auto *contract = registry.GetSkillContract(skill_id);
    REQUIRE(contract != nullptr);
    CHECK(contract->min_nodes == kExpectedNodeCount);
    CHECK(contract->max_nodes == kExpectedNodeCount);
    CHECK(contract->max_triggers == 1);
    CHECK(contract->max_transmuters == 2);

    int trigger_count = 0;
    int synergy_count = 0;
    int transmuter_count = 0;
    int keystone_count = 0;
    uint32_t trigger_node_id = 0;

    for (const auto &[node_id, _] : tree->nodes) {
      const auto *node = registry.GetNodeContract(skill_id, node_id);
      if (!node) {
        continue;
      }
      switch (node->role) {
      case SpecNodeRole::Trigger:
        ++trigger_count;
        trigger_node_id = node->node_id;
        break;
      case SpecNodeRole::Synergy:
        ++synergy_count;
        break;
      case SpecNodeRole::Transmuter:
        ++transmuter_count;
        break;
      case SpecNodeRole::Keystone:
        ++keystone_count;
        break;
      default:
        break;
      }
    }

    CHECK(trigger_count == 1);
    CHECK(trigger_node_id == expected_trigger_nodes[skill_id - 1]);
    CHECK(synergy_count >= 1);
    CHECK(transmuter_count == 2);
    CHECK(keystone_count >= 2);
  }
}

} // namespace NoMoreDay
