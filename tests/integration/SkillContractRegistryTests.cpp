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

  for (uint32_t skill_id = 1; skill_id <= 10; ++skill_id) {
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
    const auto *tree = registry.GetSkillTree(10);
    REQUIRE(tree != nullptr);
    CHECK(tree->nodes.size() == 26);

    const auto *contract = registry.GetSkillContract(10);
    REQUIRE(contract != nullptr);
    CHECK(contract->min_nodes == 26);
    CHECK(contract->max_nodes == 26);
    CHECK(contract->has_sword_intent_node);
    CHECK(contract->has_synergy_node);
    CHECK(contract->transmuter_node_ids[0] == 1021);
    CHECK(contract->transmuter_node_ids[1] == 1022);

    const auto *trigger = registry.GetNodeContract(10, 1011);
    REQUIRE(trigger != nullptr);
    CHECK(trigger->role == SpecNodeRole::Trigger);
    CHECK(trigger->trigger.trigger_skill_id == 10);
    CHECK(trigger->trigger.effectiveness == doctest::Approx(0.35f));

    const auto *synergy = registry.GetNodeContract(10, 1017);
    REQUIRE(synergy != nullptr);
    CHECK(synergy->role == SpecNodeRole::Synergy);
    CHECK(synergy->affects_sword_step);

    const auto *loop = registry.GetNodeContract(10, 1013);
    REQUIRE(loop != nullptr);
    CHECK(loop->role == SpecNodeRole::Keystone);
    CHECK(loop->affects_sword_intent);

    const auto *orbit = registry.GetNodeContract(10, 1021);
    const auto *starfall = registry.GetNodeContract(10, 1022);
    REQUIRE(orbit != nullptr);
    REQUIRE(starfall != nullptr);
    CHECK(orbit->role == SpecNodeRole::Transmuter);
    CHECK(starfall->role == SpecNodeRole::Transmuter);
    CHECK(orbit->keystone_exclusion_group == 3);
    CHECK(starfall->keystone_exclusion_group == 3);
  }

  SUBCASE("Skill 11 Heavenly Sword mastery contract is materialized") {
    const auto *skill = registry.GetSkill(11);
    REQUIRE(skill != nullptr);
    CHECK(skill->name_key == "天剑降临");

    const auto *tree = registry.GetSkillTree(11);
    REQUIRE(tree != nullptr);
    CHECK(tree->nodes.size() == 25);
    CHECK(tree->nodes.contains(1124));

    const auto *contract = registry.GetSkillContract(11);
    REQUIRE(contract != nullptr);
    CHECK(contract->min_nodes == 25);
    CHECK(contract->max_nodes == 25);
    CHECK(contract->has_sword_intent_node);
    CHECK(contract->has_synergy_node);
    CHECK(contract->transmuter_node_ids[0] == 0);
    CHECK(contract->transmuter_node_ids[1] == 0);

    const auto *trigger = registry.GetNodeContract(11, 1111);
    REQUIRE(trigger != nullptr);
    CHECK(trigger->role == SpecNodeRole::Trigger);
    CHECK(trigger->trigger.trigger_skill_id == 11);
    CHECK(trigger->trigger.effectiveness == doctest::Approx(0.25f));

    const auto *synergy = registry.GetNodeContract(11, 1117);
    REQUIRE(synergy != nullptr);
    CHECK(synergy->role == SpecNodeRole::Synergy);

    const auto *impact = registry.GetNodeContract(11, 1107);
    const auto *loop = registry.GetNodeContract(11, 1113);
    const auto *attune = registry.GetNodeContract(11, 1120);
    REQUIRE(impact != nullptr);
    REQUIRE(loop != nullptr);
    REQUIRE(attune != nullptr);
    CHECK(impact->role == SpecNodeRole::Keystone);
    CHECK(loop->role == SpecNodeRole::Keystone);
    CHECK(attune->role == SpecNodeRole::Keystone);
  }

  SUBCASE("Skill 12 Blood Sea mastery contract is materialized") {
    const auto *skill = registry.GetSkill(12);
    REQUIRE(skill != nullptr);
    CHECK(skill->name_key == "血海");

    const auto *tree = registry.GetSkillTree(12);
    REQUIRE(tree != nullptr);
    CHECK(tree->nodes.size() == 25);
    CHECK(tree->nodes.contains(1224));

    const auto *contract = registry.GetSkillContract(12);
    REQUIRE(contract != nullptr);
    CHECK(contract->min_nodes == 25);
    CHECK(contract->max_nodes == 25);
    CHECK(contract->has_sword_intent_node);
    CHECK(contract->has_synergy_node);
    CHECK(contract->transmuter_node_ids[0] == 1221);
    CHECK(contract->transmuter_node_ids[1] == 1222);

    const auto *trigger = registry.GetNodeContract(12, 1211);
    REQUIRE(trigger != nullptr);
    CHECK(trigger->role == SpecNodeRole::Trigger);
    CHECK(trigger->trigger.trigger_skill_id == 12);
    CHECK(trigger->trigger.effectiveness == doctest::Approx(0.3f));

    const auto *synergy = registry.GetNodeContract(12, 1217);
    REQUIRE(synergy != nullptr);
    CHECK(synergy->role == SpecNodeRole::Synergy);

    const auto *keystoneA = registry.GetNodeContract(12, 1207);
    const auto *keystoneB = registry.GetNodeContract(12, 1213);
    const auto *keystoneC = registry.GetNodeContract(12, 1220);
    const auto *transmuterA = registry.GetNodeContract(12, 1221);
    const auto *transmuterB = registry.GetNodeContract(12, 1222);
    REQUIRE(keystoneA != nullptr);
    REQUIRE(keystoneB != nullptr);
    REQUIRE(keystoneC != nullptr);
    REQUIRE(transmuterA != nullptr);
    REQUIRE(transmuterB != nullptr);
    CHECK(keystoneA->role == SpecNodeRole::Keystone);
    CHECK(keystoneB->role == SpecNodeRole::Keystone);
    CHECK(keystoneC->role == SpecNodeRole::Keystone);
    CHECK(transmuterA->role == SpecNodeRole::Transmuter);
    CHECK(transmuterB->role == SpecNodeRole::Transmuter);
    CHECK(transmuterA->keystone_exclusion_group == 4);
    CHECK(transmuterB->keystone_exclusion_group == 4);
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

TEST_CASE("[Integration] SkillContract - SaveSkillTreeLayout persists relative coordinates") {
  namespace fs = std::filesystem;

  const fs::path tempDir =
      fs::temp_directory_path() / "nmd_skill_tree_layout_save_test";
  const fs::path skillsFile = tempDir / "skills.json";
  const fs::path masteryFile = tempDir / "mastery_skill_trees.json";
  std::error_code ec;
  fs::remove_all(tempDir, ec);
  fs::create_directories(tempDir, ec);
  REQUIRE(!ec);

  {
    std::ofstream out(skillsFile, std::ios::binary | std::ios::trunc);
    REQUIRE(out.is_open());
    out << R"({
  "skills": [
    {
      "id": 10,
      "name_key": "SevenStarSlash",
      "desc_key": "Test",
      "mana_cost": 40,
      "cooldown": 12,
      "icon_id": 10,
      "tags": ["Physical"],
      "params": {}
    }
  ]
})";
  }

  {
    std::ofstream out(masteryFile, std::ios::binary | std::ios::trunc);
    REQUIRE(out.is_open());
    out << R"({
  "skills": [
    {
      "skill_id": 10,
      "talent_tree": [
        {
          "id": 1000,
          "name_key": "RootA",
          "desc_key": "",
          "icon_id": 1000,
          "max_points": 1,
          "x": 1.0,
          "y": 2.0,
          "prerequisites": [],
          "stat_modifiers": []
        },
        {
          "id": 1001,
          "name_key": "Child",
          "desc_key": "",
          "icon_id": 1001,
          "max_points": 1,
          "x": 3.0,
          "y": 4.0,
          "prerequisites": [{"node_id": 1000, "required_points": 1}],
          "stat_modifiers": []
        },
        {
          "id": 1002,
          "name_key": "Fork",
          "desc_key": "",
          "icon_id": 1002,
          "max_points": 1,
          "x": 1.0,
          "y": -1.0,
          "prerequisites": [
            {"node_id": 1001, "required_points": 1},
            {"node_id": 1000, "required_points": 1}
          ],
          "stat_modifiers": []
        }
      ]
    }
  ]
})";
  }

  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson(skillsFile.string());

  auto *tree = registry.GetMutableSkillTree(10);
  REQUIRE(tree != nullptr);
  tree->nodes[1000].x = 4.0f;
  tree->nodes[1000].y = 5.0f;
  tree->nodes[1001].x = 7.0f;
  tree->nodes[1001].y = 11.0f;
  tree->nodes[1002].x = 8.5f;
  tree->nodes[1002].y = 9.5f;

  REQUIRE(registry.SaveSkillTreeLayout(10));

  std::ifstream in(masteryFile, std::ios::binary);
  REQUIRE(in.is_open());
  nlohmann::json saved;
  in >> saved;
  const auto &nodes = saved.at("skills").at(0).at("talent_tree");
  REQUIRE(nodes.size() == 3);

  CHECK(nodes.at(0).at("x").get<float>() == doctest::Approx(4.0f));
  CHECK(nodes.at(0).at("y").get<float>() == doctest::Approx(5.0f));
  CHECK(nodes.at(1).at("x").get<float>() == doctest::Approx(3.0f));
  CHECK(nodes.at(1).at("y").get<float>() == doctest::Approx(6.0f));
  CHECK(nodes.at(2).at("x").get<float>() == doctest::Approx(1.5f));
  CHECK(nodes.at(2).at("y").get<float>() == doctest::Approx(-1.5f));

  fs::remove_all(tempDir, ec);
}

} // namespace NoMoreDay
