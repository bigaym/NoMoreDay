#include "doctest.h"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay {

namespace {

SkillTreeDefinition MakeTreeSingleThreshold(uint32_t skillId) {
  SkillTreeDefinition tree;
  tree.skill_id = skillId;

  TalentNode root;
  root.id = 900001;
  root.name_key = "unit_root";
  root.desc_key = "unit_root_desc";
  root.max_points = 3;
  tree.nodes[root.id] = root;

  TalentNode target;
  target.id = 900002;
  target.name_key = "unit_target";
  target.desc_key = "unit_target_desc";
  target.max_points = 1;
  target.prerequisites.push_back(TalentPrerequisite{root.id, 2});
  tree.nodes[target.id] = target;

  return tree;
}

SkillTreeDefinition MakeTreeOrThreshold(uint32_t skillId) {
  SkillTreeDefinition tree;
  tree.skill_id = skillId;

  TalentNode preA;
  preA.id = 900011;
  preA.name_key = "unit_pre_a";
  preA.desc_key = "unit_pre_a_desc";
  preA.max_points = 3;
  tree.nodes[preA.id] = preA;

  TalentNode preB;
  preB.id = 900012;
  preB.name_key = "unit_pre_b";
  preB.desc_key = "unit_pre_b_desc";
  preB.max_points = 1;
  tree.nodes[preB.id] = preB;

  TalentNode target;
  target.id = 900013;
  target.name_key = "unit_target_or";
  target.desc_key = "unit_target_or_desc";
  target.max_points = 1;
  target.prerequisites.push_back(TalentPrerequisite{preA.id, 2});
  target.prerequisites.push_back(TalentPrerequisite{preB.id, 1});
  tree.nodes[target.id] = target;

  return tree;
}

} // namespace

TEST_CASE("[Unit] SkillSpecialization - required_points blocks until threshold") {
  constexpr uint32_t kSkillId = 990001;
  SkillRegistry::Get().RegisterSkillTree(MakeTreeSingleThreshold(kSkillId));

  entt::registry registry;
  const auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.available_talent_points = 6;
  active.specialized_slots[0].skill_id = kSkillId;

  CHECK_FALSE(SkillSystem::AddTalentPoint(registry, player, kSkillId, 900002));
  CHECK(active.available_talent_points == 6);

  CHECK(SkillSystem::AddTalentPoint(registry, player, kSkillId, 900001));
  CHECK_FALSE(SkillSystem::AddTalentPoint(registry, player, kSkillId, 900002));
  CHECK(active.available_talent_points == 5);

  CHECK(SkillSystem::AddTalentPoint(registry, player, kSkillId, 900001));
  CHECK(SkillSystem::AddTalentPoint(registry, player, kSkillId, 900002));
  CHECK(active.available_talent_points == 3);
}

TEST_CASE("[Unit] SkillSpecialization - required_points works with OR prerequisites") {
  constexpr uint32_t kSkillId = 990002;
  SkillRegistry::Get().RegisterSkillTree(MakeTreeOrThreshold(kSkillId));

  entt::registry registry;
  const auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.available_talent_points = 6;
  active.specialized_slots[0].skill_id = kSkillId;

  CHECK(SkillSystem::AddTalentPoint(registry, player, kSkillId, 900011));
  CHECK_FALSE(SkillSystem::AddTalentPoint(registry, player, kSkillId, 900013));

  CHECK(SkillSystem::AddTalentPoint(registry, player, kSkillId, 900012));
  CHECK(SkillSystem::AddTalentPoint(registry, player, kSkillId, 900013));
  CHECK(active.available_talent_points == 3);
}

TEST_CASE("[Unit] TalentNode JSON - prerequisites parse required_points and legacy format") {
  const nlohmann::json nodeJson = {
      {"id", 900101},
      {"name_key", "json_node"},
      {"desc_key", "json_node_desc"},
      {"max_points", 1},
      {"prerequisites",
       nlohmann::json::array({nlohmann::json{{"node_id", 900001},
                                              {"required_points", 3}},
                              900002})},
  };

  const TalentNode node = nodeJson.get<TalentNode>();
  REQUIRE(node.prerequisites.size() == 2);

  CHECK(node.prerequisites[0].node_id == 900001);
  CHECK(node.prerequisites[0].required_points == 3);

  CHECK(node.prerequisites[1].node_id == 900002);
  CHECK(node.prerequisites[1].required_points == 1);
}

TEST_CASE("[Unit] TalentNode JSON - display_lines parse quantitative tooltip metadata") {
  const nlohmann::json nodeJson = {
      {"id", 900201},
      {"name_key", "display_node"},
      {"desc_key", "display_node_desc"},
      {"max_points", 3},
      {"display_lines", nlohmann::json::array({
          {{"label", "持续时间"}, {"per_point", 10.0f}, {"is_percent", true}},
          {{"label", "脉冲频率"}, {"per_point", 8.0f}, {"is_percent", true}}
      })}
  };

  const TalentNode node = nodeJson.get<TalentNode>();
  REQUIRE(node.display_lines.size() == 2);
  CHECK(node.display_lines[0].label == "持续时间");
  CHECK(node.display_lines[0].per_point == doctest::Approx(10.0f));
  CHECK(node.display_lines[0].is_percent);
}

} // namespace NoMoreDay
