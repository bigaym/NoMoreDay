#include "doctest.h"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <entt/entt.hpp>
#include <fstream>
#include <queue>
#include <unordered_map>
#include <vector>

// 注意：本文件用例依赖 SkillRegistry 全局单例的注册顺序（990001..990003 自定义树
// + skills.json 加载），全局单例跨用例累积且无 teardown；新增用例勿复用这些 id，
// 也勿假设注册状态会被清理。

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
          {{"label", "持续时间"}, {"per_point", 10.0f}, {"is_percent", true}, {"display_category", "duration"}},
          {{"label", "脉冲频率"}, {"per_point", 8.0f}, {"is_percent", true}, {"display_category", "frequency"}}
      })}
  };

  const TalentNode node = nodeJson.get<TalentNode>();
  REQUIRE(node.display_lines.size() == 2);
  CHECK(node.display_lines[0].label == "持续时间");
  CHECK(node.display_lines[0].per_point == doctest::Approx(10.0f));
  CHECK(node.display_lines[0].is_percent);
  CHECK(node.display_lines[0].displayCategory == DisplayLineCategory::Duration);
  CHECK(node.display_lines[1].displayCategory == DisplayLineCategory::Frequency);
}

// 技能 8 全节点可分配性回归：前置 required_points 必须被前置节点自身 max_points 覆盖，
// 且依赖图无环，避免“前置永远点不满导致后继永久锁定”的加点死锁（审查 C3）。
TEST_CASE("[Unit] SkillSpecialization - skill 8 tree satisfies allocability and acyclicity") {
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  constexpr uint32_t kSkillId = 8;
  const SkillTreeDefinition *tree = SkillRegistry::Get().GetSkillTree(kSkillId);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->nodes.size() == 29);

  std::unordered_map<uint32_t, uint32_t> indegree;
  std::unordered_map<uint32_t, std::vector<uint32_t>> dependents;
  for (const auto &[node_id, node] : tree->nodes) {
    indegree.emplace(node_id, 0);
  }
  for (const auto &[node_id, node] : tree->nodes) {
    for (const auto &pre : node.prerequisites) {
      if (pre.node_id == 0) {
        continue; // 根节点的 0 哨兵前置不属于树内依赖
      }
      CAPTURE(node_id);
      CAPTURE(pre.node_id);
      CAPTURE(pre.required_points);
      const auto pre_it = tree->nodes.find(pre.node_id);
      REQUIRE(pre_it != tree->nodes.end());
      CHECK(pre_it->second.max_points >= pre.required_points);
      dependents[pre.node_id].push_back(node_id);
      ++indegree[node_id];
    }
  }

  std::queue<uint32_t> ready;
  for (const auto &[node_id, degree] : indegree) {
    if (degree == 0) {
      ready.push(node_id);
    }
  }
  size_t processed = 0;
  while (!ready.empty()) {
    const uint32_t current = ready.front();
    ready.pop();
    ++processed;
    for (const uint32_t next : dependents[current]) {
      if (--indegree[next] == 0) {
        ready.push(next);
      }
    }
  }
  CHECK(processed == tree->nodes.size());
}

// M8：前置结构上不可满足（required_points 超过前置节点自身 max_points）时，分配器应拒绝
// 加点，与「点数暂时不足」区分开，避免静默死路。
TEST_CASE("[Unit] SkillSpecialization - unsatisfiable prerequisite is rejected") {
  constexpr uint32_t kSkillId = 990003;
  SkillTreeDefinition tree;
  tree.skill_id = kSkillId;

  TalentNode locked;
  locked.id = 900021;
  locked.name_key = "unit_locked";
  locked.desc_key = "unit_locked_desc";
  locked.max_points = 1; // 上限不足以后继门槛覆盖
  tree.nodes[locked.id] = locked;

  TalentNode target;
  target.id = 900022;
  target.name_key = "unit_target_locked";
  target.desc_key = "unit_target_locked_desc";
  target.max_points = 1;
  target.prerequisites.push_back(TalentPrerequisite{locked.id, 3});
  tree.nodes[target.id] = target;

  SkillRegistry::Get().RegisterSkillTree(tree);

  entt::registry registry;
  const auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.available_talent_points = 10;
  active.specialized_slots[0].skill_id = kSkillId;

  // 前置结构上永不可满足，即便点数充足也不可分配
  CHECK_FALSE(SkillSystem::AddTalentPoint(registry, player, kSkillId, target.id));
  CHECK(active.available_talent_points == 10);
  CHECK_FALSE(active.specialized_slots[0].allocated_points.contains(target.id));
}

// 技能 4 全节点可分配性回归：前置 required_points 必须被前置节点自身 max_points 覆盖，
// 且依赖图无环。476 元素曝光为双前置 [473×1, 474×1]，分配按 OR 语义；但布局锚点只取
// prerequisites.front()（473），走 474 线时父子连线锚点有视觉偏差，功能无损（M7 已知项）。
TEST_CASE("[Unit] SkillSpecialization - skill 4 tree satisfies allocability and acyclicity") {
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  constexpr uint32_t kSkillId = 4;
  const SkillTreeDefinition *tree = SkillRegistry::Get().GetSkillTree(kSkillId);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->nodes.size() == 29);

  std::unordered_map<uint32_t, uint32_t> indegree;
  std::unordered_map<uint32_t, std::vector<uint32_t>> dependents;
  for (const auto &[node_id, node] : tree->nodes) {
    indegree.emplace(node_id, 0);
  }
  for (const auto &[node_id, node] : tree->nodes) {
    for (const auto &pre : node.prerequisites) {
      if (pre.node_id == 0) {
        continue;
      }
      CAPTURE(node_id);
      CAPTURE(pre.node_id);
      CAPTURE(pre.required_points);
      const auto pre_it = tree->nodes.find(pre.node_id);
      REQUIRE(pre_it != tree->nodes.end());
      CHECK(pre_it->second.max_points >= pre.required_points);
      dependents[pre.node_id].push_back(node_id);
      ++indegree[node_id];
    }
  }

  std::queue<uint32_t> ready;
  for (const auto &[node_id, degree] : indegree) {
    if (degree == 0) {
      ready.push(node_id);
    }
  }
  size_t processed = 0;
  while (!ready.empty()) {
    const uint32_t current = ready.front();
    ready.pop();
    ++processed;
    for (const uint32_t next : dependents[current]) {
      if (--indegree[next] == 0) {
        ready.push(next);
      }
    }
  }
  CHECK(processed == tree->nodes.size());

  // 476 双前置为 OR 可选线，记录以免被误当作冗余前置合并
  const auto exposure = tree->nodes.find(476);
  REQUIRE(exposure != tree->nodes.end());
  CHECK(exposure->second.prerequisites.size() == 2);
}

// M9：skill_4_tree.json 布局源必须携带与 skills.json 运行时树一致的 max_points，
// 二者作为可校验镜像同步，防止双源漂移。
TEST_CASE("[Unit] SkillSpecialization - skill 4 layout tree max_points matches skills.json") {
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  const SkillTreeDefinition *tree = SkillRegistry::Get().GetSkillTree(4);
  REQUIRE(tree != nullptr);

  std::ifstream layoutFile("assets/data/skill_4_tree.json");
  REQUIRE(layoutFile.is_open());
  nlohmann::json layoutDoc;
  layoutFile >> layoutDoc;

  const auto &nodes = layoutDoc.at("skills").at(0).at("talent_tree");
  REQUIRE(nodes.size() == 29);
  for (const auto &nodeJson : nodes) {
    const uint32_t nodeId = nodeJson.at("id").get<uint32_t>();
    CAPTURE(nodeId);
    const auto it = tree->nodes.find(nodeId);
    REQUIRE(it != tree->nodes.end());
    CHECK(nodeJson.at("max_points").get<int>() == it->second.max_points);
  }
}

} // namespace NoMoreDay
