#include "TestCommon.hpp"
#include "game/data/SkillContract.hpp"
#include "game/data/SkillRegistry.hpp"
#include <array>
#include <filesystem>
#include <fstream>

namespace NoMoreDay {

TEST_CASE("[Integration] SkillContract - Registry loading and validation") {
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  for (uint32_t skill_id = 1; skill_id <= 9; ++skill_id) {
    CAPTURE(skill_id);
    const auto *contract = registry.GetSkillContract(skill_id);
    REQUIRE(contract != nullptr);
    std::string error;
    const bool valid = registry.ValidateSkillContract(skill_id, &error);
    if (valid) {
      CHECK(error.empty());
    } else {
      CHECK_FALSE(error.empty());
    }
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

  SUBCASE("Anti-meta node contract fields are materialized") {
    const auto *skill2NodeA = registry.GetNodeContract(2, 213);
    const auto *skill2NodeB = registry.GetNodeContract(2, 214);
    REQUIRE(skill2NodeA != nullptr);
    REQUIRE(skill2NodeB != nullptr);
    CHECK(skill2NodeA->keystone_exclusion_group == 1);
    CHECK(skill2NodeB->keystone_exclusion_group == 1);
    CHECK(skill2NodeA->cost_affix == CostAffixPreset::HeavyMomentum);

    const auto *skill9NodeA = registry.GetNodeContract(9, 971);
    const auto *skill9NodeB = registry.GetNodeContract(9, 972);
    REQUIRE(skill9NodeA != nullptr);
    REQUIRE(skill9NodeB != nullptr);
    CHECK(skill9NodeA->keystone_exclusion_group == 2);
    CHECK(skill9NodeB->keystone_exclusion_group == 2);
    CHECK(skill9NodeA->cost_affix == CostAffixPreset::GlassCannonCrit);
  }

  SUBCASE("Skill 10 signature contract is materialized") {
    const auto *contract = registry.GetSkillContract(10);
    REQUIRE(contract != nullptr);
    const auto *node = registry.GetNodeContract(10, 1003);
    REQUIRE(node != nullptr);
    CHECK(node->role == SpecNodeRole::Trigger);
  }
}

TEST_CASE("[Integration] SkillContract - Structural alignment matrix (skills 1..9)") {
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  const std::array<uint32_t, 9> expected_trigger_nodes = {
      114, 233, 373, 451, 533, 633, 713, 831, 951};

  for (uint32_t skill_id = 1; skill_id <= 9; ++skill_id) {
    CAPTURE(skill_id);

    const auto *tree = registry.GetSkillTree(skill_id);
    REQUIRE(tree != nullptr);

    const auto *contract = registry.GetSkillContract(skill_id);
    REQUIRE(contract != nullptr);
    CHECK(tree->nodes.size() >= contract->min_nodes);
    CHECK(tree->nodes.size() <= contract->max_nodes);
    CHECK(contract->min_nodes <= contract->max_nodes);
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

TEST_CASE("[Integration] SkillContract - Node 213 does not get implicit cost_affix") {
  namespace fs = std::filesystem;

  const fs::path tempFile =
      fs::temp_directory_path() / "nmd_skill_contract_no_affix_213.json";
  {
    std::ofstream out(tempFile, std::ios::binary | std::ios::trunc);
    REQUIRE(out.is_open());
    out << R"({
  "skills": [
    {
      "id": 42,
      "name_key": "TestSkill",
      "mana_cost": 0,
      "cooldown": 0,
      "talent_tree": [
        {
          "id": 213,
          "name_key": "Node213",
          "desc_key": "Node213Desc",
          "icon_id": 0,
          "x": 0,
          "y": 0,
          "max_points": 1,
          "current_points": 0,
          "is_key_node": true,
          "prerequisites": [],
          "stat_modifiers": [],
          "damage_modifiers": [],
          "tags_to_add": []
        }
      ],
      "skill_contract": {
        "skill_id": 42,
        "min_nodes": 1,
        "max_nodes": 1,
        "nodes": [
          {
            "node_id": 213,
            "role": "Keystone"
          }
        ]
      }
    }
  ]
})";
  }

  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson(tempFile.string());
  const auto *node = registry.GetNodeContract(42, 213);
  REQUIRE(node != nullptr);
  CHECK(node->cost_affix == CostAffixPreset::None);

  std::error_code ec;
  fs::remove(tempFile, ec);
}

} // namespace NoMoreDay
