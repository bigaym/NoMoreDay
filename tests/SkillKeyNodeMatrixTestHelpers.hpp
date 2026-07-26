#pragma once

#include "TestCommon.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/data/SkillContract.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace NoMoreDay::test::skill_keynode_matrix {

enum class KeyNodeCheckLayer : uint8_t {
  CastState,
  RuntimeTick,
  CombatEvent,
  TagConversion,
  GuardPolicy,
  VisualSignalGuard
};

struct KeyNodeExpectation {
  uint32_t skill_id = 0;
  uint32_t node_id = 0;
  const char *label = "";
  KeyNodeCheckLayer layer = KeyNodeCheckLayer::CastState;
  bool requires_runtime_transmuter_selection = false;
};

struct CompactContractBuckets {
  std::map<uint32_t, std::set<uint32_t>> key_nodes_by_skill;
  std::map<uint32_t, std::vector<uint32_t>> transmuter_order_by_skill;
  std::map<uint32_t, uint32_t> trigger_node_by_skill;
  std::unordered_map<uint32_t, uint32_t> trigger_skill_by_node;
  std::unordered_set<uint32_t> all_key_nodes;
  std::unordered_set<uint32_t> transmuter_nodes;
  std::unordered_set<uint32_t> trigger_nodes;
  std::unordered_set<uint32_t> synergy_nodes;
  std::unordered_set<uint32_t> sword_intent_nodes;
  std::unordered_set<uint32_t> sword_step_nodes;
};

inline const std::array<uint32_t, 12> &MatrixSkillIds() {
  static const std::array<uint32_t, 12> skill_ids = {1,  2,  3, 4, 5, 6,
                                                      7,  8,  9, 10, 11, 12};
  return skill_ids;
}

inline std::filesystem::path ResolveProjectPath(const char *relative_path) {
  const std::array<std::filesystem::path, 4> roots = {
      std::filesystem::current_path(),
      std::filesystem::current_path() / "..",
      std::filesystem::current_path() / "../..",
      std::filesystem::current_path() / "../../.."};

  for (const auto &root : roots) {
    const auto candidate = root / relative_path;
    if (std::filesystem::exists(candidate)) {
      return std::filesystem::weakly_canonical(candidate);
    }
  }

  throw std::runtime_error(std::string("Unable to locate file: ") +
                           relative_path);
}

inline nlohmann::json LoadJsonFile(const char *relative_path) {
  const auto path = ResolveProjectPath(relative_path);
  std::ifstream in(path);
  if (!in.good()) {
    throw std::runtime_error(std::string("Failed to open file: ") +
                             path.string());
  }
  nlohmann::json document = nlohmann::json::object();
  in >> document;
  return document;
}

inline std::map<uint32_t, std::vector<uint32_t>> ExpectedKeyNodesBySkill() {
  return {
      {1, {113, 114, 130, 152, 170, 171}},
      {2, {213, 214, 230, 233, 250, 252, 270}},
      {3, {330, 352, 370, 371, 373}},
      {4, {430, 451, 452, 470, 471}},
      {5, {530, 533, 552, 570, 571}},
      {6, {630, 633, 652, 670, 671}},
      {7, {713, 730, 750, 752, 770}},
      {8, {813, 830, 831, 852, 870, 871}},
      {9, {913, 930, 950, 951, 952, 970, 971, 972}},
      {10, {1002, 1004, 1005, 1007, 1008, 1009, 1011, 1013, 1017, 1021, 1022, 1025}},
      {11, {1101, 1102, 1107, 1109, 1111, 1113, 1115, 1117, 1120}},
      {12, {1202, 1207, 1209, 1211, 1213, 1217, 1220, 1221, 1222, 1224}},
  };
}

inline size_t ExpectedTotalKeyNodeCount() {
  size_t total = 0;
  for (const auto &[skill_id, nodes] : ExpectedKeyNodesBySkill()) {
    (void)skill_id;
    total += nodes.size();
  }
  return total;
}

inline std::map<uint32_t, std::vector<uint32_t>> LoadFixtureKeyNodes() {
  const auto doc =
      LoadJsonFile("tests/fixtures/skill_specialization_keynodes.json");

  std::map<uint32_t, std::vector<uint32_t>> result;
  if (!doc.contains("skills") || !doc.at("skills").is_array()) {
    return result;
  }

  for (const auto &skill_json : doc.at("skills")) {
    const uint32_t skill_id = skill_json.value("skill_id", 0u);
    std::vector<uint32_t> nodes;
    if (skill_json.contains("key_nodes") && skill_json.at("key_nodes").is_array()) {
      for (const auto &node_json : skill_json.at("key_nodes")) {
        nodes.push_back(node_json.get<uint32_t>());
      }
    }
    std::sort(nodes.begin(), nodes.end());
    result[skill_id] = std::move(nodes);
  }
  return result;
}

inline std::map<uint32_t, std::vector<std::string>> LoadFixtureLayers() {
  const auto doc =
      LoadJsonFile("tests/fixtures/skill_specialization_keynodes.json");

  std::map<uint32_t, std::vector<std::string>> result;
  if (!doc.contains("skills") || !doc.at("skills").is_array()) {
    return result;
  }

  for (const auto &skill_json : doc.at("skills")) {
    const uint32_t skill_id = skill_json.value("skill_id", 0u);
    std::vector<std::string> layers;
    if (skill_json.contains("expected_layers") &&
        skill_json.at("expected_layers").is_array()) {
      for (const auto &layer_json : skill_json.at("expected_layers")) {
        layers.push_back(layer_json.get<std::string>());
      }
    }
    result[skill_id] = std::move(layers);
  }
  return result;
}

inline CompactContractBuckets LoadCompactContractBuckets() {
  const auto doc = LoadJsonFile("assets/data/skill_contracts_compact.json");
  CompactContractBuckets buckets;
  if (!doc.contains("skills") || !doc.at("skills").is_array()) {
    return buckets;
  }

  for (const auto &skill_json : doc.at("skills")) {
    const uint32_t skill_id = skill_json.value("skill_id", 0u);
    auto &skill_nodes = buckets.key_nodes_by_skill[skill_id];

    if (skill_json.contains("transmuter_node_ids") &&
        skill_json.at("transmuter_node_ids").is_array()) {
      for (const auto &node_json : skill_json.at("transmuter_node_ids")) {
        const uint32_t node_id = node_json.get<uint32_t>();
        if (node_id == 0u) {
          continue;
        }
        buckets.transmuter_order_by_skill[skill_id].push_back(node_id);
        buckets.transmuter_nodes.insert(node_id);
        buckets.all_key_nodes.insert(node_id);
        skill_nodes.insert(node_id);
      }
    }

    if (skill_json.contains("synergy_node_ids") &&
        skill_json.at("synergy_node_ids").is_array()) {
      for (const auto &node_json : skill_json.at("synergy_node_ids")) {
        const uint32_t node_id = node_json.get<uint32_t>();
        buckets.synergy_nodes.insert(node_id);
        buckets.all_key_nodes.insert(node_id);
        skill_nodes.insert(node_id);
      }
    }

    if (skill_json.contains("sword_intent_node_ids") &&
        skill_json.at("sword_intent_node_ids").is_array()) {
      for (const auto &node_json : skill_json.at("sword_intent_node_ids")) {
        const uint32_t node_id = node_json.get<uint32_t>();
        buckets.sword_intent_nodes.insert(node_id);
        buckets.all_key_nodes.insert(node_id);
        skill_nodes.insert(node_id);
      }
    }

    if (skill_json.contains("sword_step_node_ids") &&
        skill_json.at("sword_step_node_ids").is_array()) {
      for (const auto &node_json : skill_json.at("sword_step_node_ids")) {
        const uint32_t node_id = node_json.get<uint32_t>();
        buckets.sword_step_nodes.insert(node_id);
        buckets.all_key_nodes.insert(node_id);
        skill_nodes.insert(node_id);
      }
    }

    if (skill_json.contains("trigger_nodes") &&
        skill_json.at("trigger_nodes").is_array()) {
      for (const auto &trigger_json : skill_json.at("trigger_nodes")) {
        const uint32_t node_id = trigger_json.value("node_id", 0u);
        const uint32_t trigger_skill_id =
            trigger_json.value("trigger_skill_id", 0u);
        buckets.trigger_nodes.insert(node_id);
        buckets.all_key_nodes.insert(node_id);
        buckets.trigger_skill_by_node[node_id] = trigger_skill_id;
        if (trigger_skill_id > 0u) {
          buckets.trigger_node_by_skill[skill_id] = node_id;
        }
        skill_nodes.insert(node_id);
      }
    }

    if (skill_json.contains("keystone_node_ids") &&
        skill_json.at("keystone_node_ids").is_array()) {
      for (const auto &node_json : skill_json.at("keystone_node_ids")) {
        const uint32_t node_id = node_json.get<uint32_t>();
        buckets.all_key_nodes.insert(node_id);
        skill_nodes.insert(node_id);
      }
    }

    if (skill_json.contains("passive_node_ids") &&
        skill_json.at("passive_node_ids").is_array()) {
      for (const auto &node_json : skill_json.at("passive_node_ids")) {
        const uint32_t node_id = node_json.get<uint32_t>();
        buckets.all_key_nodes.insert(node_id);
        skill_nodes.insert(node_id);
      }
    }

    if (skill_json.contains("keystone_exclusion_groups") &&
        skill_json.at("keystone_exclusion_groups").is_object()) {
      for (auto it = skill_json.at("keystone_exclusion_groups").begin();
           it != skill_json.at("keystone_exclusion_groups").end(); ++it) {
        const uint32_t node_id = static_cast<uint32_t>(std::stoul(it.key()));
        buckets.all_key_nodes.insert(node_id);
        skill_nodes.insert(node_id);
      }
    }
  }

  return buckets;
}

inline std::vector<std::pair<uint32_t, int>>
AsAllocatedPoints(const std::vector<uint32_t> &nodes, int points = 1) {
  std::vector<std::pair<uint32_t, int>> allocated;
  allocated.reserve(nodes.size());
  for (const uint32_t node_id : nodes) {
    allocated.emplace_back(node_id, points);
  }
  return allocated;
}

inline entt::entity CreateCaster(entt::registry &registry, float mana = 500.0f) {
  const entt::entity caster = registry.create();
  registry.emplace<PlayerTag>(caster);
  registry.emplace<Position>(caster, 0.0f, 0.0f);
  registry.emplace<Velocity>(caster, 0.0f, 0.0f);
  registry.emplace<CombatStats>(caster).mana = mana;
  registry.emplace<HealthComponent>(caster, 1000.0f, 1000.0f);
  return caster;
}

inline entt::entity CreateTarget(entt::registry &registry,
                                 Vector2 pos = Vector2{10.0f, 0.0f}) {
  const entt::entity target = registry.create();
  registry.emplace<EnemyTag>(target);
  registry.emplace<Position>(target, pos.x, pos.y);
  registry.emplace<Velocity>(target, 0.0f, 0.0f);
  registry.emplace<CombatStats>(target);
  registry.emplace<HealthComponent>(target, 1000.0f, 1000.0f);
  return target;
}

inline void ConfigureSkillSlot(entt::registry &registry, entt::entity caster,
                               uint32_t skill_id, int slot_index = 0,
                               int charges = 1, float cooldown = 0.0f) {
  auto &active = registry.get_or_emplace<ActiveSkillsComponent>(caster);
  active.slots[slot_index].id = skill_id;
  active.slots[slot_index].current_charges = charges;
  active.slots[slot_index].cooldown = cooldown;
}

inline void ConfigureSpecialization(
    entt::registry &registry, entt::entity caster, uint32_t skill_id,
    const std::vector<std::pair<uint32_t, int>> &allocated_points,
    int specialized_slot = 0) {
  auto &active = registry.get_or_emplace<ActiveSkillsComponent>(caster);
  auto &spec = active.specialized_slots[specialized_slot];
  spec.skill_id = skill_id;
  spec.allocated_points.clear();
  for (const auto &[node_id, points] : allocated_points) {
    spec.allocated_points[node_id] = points;
  }
}

inline void SeedExecutionNodes(SkillExecution &exec,
                               const std::vector<uint32_t> &node_ids) {
  for (const uint32_t node_id : node_ids) {
    exec.active_nodes.set(node_id % 100);
  }
}

inline SkillExecution BuildExecution(uint32_t skill_id, entt::entity owner,
                                     Vector2 target_pos,
                                     const std::vector<uint32_t> &node_ids,
                                     uint64_t cast_id = 1) {
  SkillExecution exec;
  exec.skill_id = skill_id;
  exec.owner = owner;
  exec.target_pos = target_pos;
  exec.cast_id = cast_id;
  SeedExecutionNodes(exec, node_ids);
  return exec;
}

inline void DispatchSkillHit(entt::registry &registry, entt::entity caster,
                             entt::entity target, uint32_t skill_id,
                             uint64_t cast_id,
                             Tag tags = Tag::Hit | Tag::Melee) {
  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateSkillHit(caster, target, skill_id, tags,
                                                   false, cast_id));
}

inline bool HasEffectById(const entt::registry &registry, entt::entity entity,
                          const char *effect_id) {
  const auto *effects = registry.try_get<ActiveEffectsComponent>(entity);
  if (!effects) {
    return false;
  }
  for (const auto &effect : effects->effects) {
    if (effect.id == effect_id && effect.remaining > 0.0f) {
      return true;
    }
  }
  return false;
}

} // namespace NoMoreDay::test::skill_keynode_matrix
