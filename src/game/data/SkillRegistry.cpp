#include "game/data/SkillRegistry.hpp"
#include "core/logging/Logger.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>


namespace NoMoreDay {

using json = nlohmann::json;

namespace {

template <typename EnumT>
std::optional<EnumT> ParseEnumFromJson(const json &value,
                                       const std::unordered_map<std::string, EnumT> &map) {
  if (value.is_number_integer()) {
    const int raw = value.get<int>();
    if (raw >= 0 && raw <= std::numeric_limits<uint8_t>::max()) {
      return static_cast<EnumT>(raw);
    }
    return std::nullopt;
  }
  if (!value.is_string()) {
    return std::nullopt;
  }
  auto it = map.find(value.get<std::string>());
  if (it == map.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<SpecNodeRole> ParseSpecNodeRole(const json &value) {
  static const std::unordered_map<std::string, SpecNodeRole> kMap = {
      {"Passive", SpecNodeRole::Passive},
      {"Keystone", SpecNodeRole::Keystone},
      {"Trigger", SpecNodeRole::Trigger},
      {"Synergy", SpecNodeRole::Synergy},
      {"Transmuter", SpecNodeRole::Transmuter},
  };
  return ParseEnumFromJson(value, kMap);
}

std::optional<ResistModel> ParseResistModel(const json &value) {
  static const std::unordered_map<std::string, ResistModel> kMap = {
      {"None", ResistModel::None},
      {"TypeA_Penetration", ResistModel::TypeA_Penetration},
      {"TypeB_Shred", ResistModel::TypeB_Shred},
      {"TypeC_Exposure", ResistModel::TypeC_Exposure},
      {"TypeD_StatToPenetration", ResistModel::TypeD_StatToPenetration},
      {"TypeE_CapSuppression", ResistModel::TypeE_CapSuppression},
  };
  return ParseEnumFromJson(value, kMap);
}

std::optional<ScopePolicy> ParseScopePolicy(const json &value) {
  static const std::unordered_map<std::string, ScopePolicy> kMap = {
      {"SkillOnly", ScopePolicy::SkillOnly},
      {"GlobalWhileBuffActive", ScopePolicy::GlobalWhileBuffActive},
      {"GlobalAlways", ScopePolicy::GlobalAlways},
  };
  return ParseEnumFromJson(value, kMap);
}

uint8_t ClampToU8(int value, int fallback) {
  if (value < 0 || value > std::numeric_limits<uint8_t>::max()) {
    return static_cast<uint8_t>(fallback);
  }
  return static_cast<uint8_t>(value);
}

void ApplyPrerequisiteAnchoredLayout(SkillTreeDefinition &tree) {
  if (tree.nodes.empty()) {
    return;
  }

  std::unordered_map<uint32_t, std::pair<float, float>> local_offset;
  std::unordered_map<uint32_t, std::pair<float, float>> resolved_pos;
  std::unordered_map<uint32_t, uint8_t> state;

  for (const auto &[id, node] : tree.nodes) {
    local_offset[id] = {node.x, node.y};
    state[id] = 0;
  }

  std::function<std::pair<float, float>(uint32_t)> resolve_node =
      [&](uint32_t node_id) -> std::pair<float, float> {
    auto node_it = tree.nodes.find(node_id);
    if (node_it == tree.nodes.end()) {
      return {0.0f, 0.0f};
    }

    if (state[node_id] == 2) {
      return resolved_pos[node_id];
    }
    if (state[node_id] == 1) {
      // Cycle fallback: keep local offset to avoid deadlock.
      return local_offset[node_id];
    }

    state[node_id] = 1;
    const auto &node = node_it->second;
    const auto offset = local_offset[node_id];

    std::pair<float, float> anchor = {0.0f, 0.0f};
    if (!node.prerequisites.empty()) {
      const uint32_t first_pre = node.prerequisites.front().node_id;
      if (first_pre != 0 && tree.nodes.contains(first_pre)) {
        anchor = resolve_node(first_pre);
      }
    }

    const std::pair<float, float> absolute = {anchor.first + offset.first,
                                              anchor.second + offset.second};
    resolved_pos[node_id] = absolute;
    state[node_id] = 2;
    return absolute;
  };

  for (const auto &[id, node] : tree.nodes) {
    (void)node;
    resolve_node(id);
  }

  for (auto &[id, node] : tree.nodes) {
    const auto it = resolved_pos.find(id);
    if (it == resolved_pos.end()) {
      continue;
    }
    node.x = it->second.first;
    node.y = it->second.second;
  }
}

SkillContractDefinition BuildDefaultContract(
    const SkillData &data, const SkillTreeDefinition *tree) {
  SkillContractDefinition def;
  def.contract.skill_id = data.id;

  const size_t count = tree ? tree->nodes.size() : 0;
  const auto safe_count =
      static_cast<uint8_t>(std::min<size_t>(count, std::numeric_limits<uint8_t>::max()));
  def.contract.min_nodes = safe_count;
  def.contract.max_nodes = safe_count;
  def.contract.max_transmuters = 2;
  def.contract.max_triggers = 1;
  def.contract.has_sword_intent_node = false;
  def.contract.has_synergy_node = false;
  def.contract.transmuter_node_ids = {0u, 0u};

  if (!tree) {
    return def;
  }
  for (const auto &[node_id, node] : tree->nodes) {
    NodeContractData node_contract;
    node_contract.node_id = node_id;
    node_contract.role =
        (node.max_points == 1) ? SpecNodeRole::Keystone : SpecNodeRole::Passive;
    def.nodes[node_id] = node_contract;
  }
  return def;
}

void ParseContractNode(const json &node_json, SkillContractDefinition &def) {
  if (!node_json.contains("node_id")) {
    return;
  }
  const uint32_t node_id = node_json.at("node_id").get<uint32_t>();

  NodeContractData node = {};
  if (auto it = def.nodes.find(node_id); it != def.nodes.end()) {
    node = it->second;
  }
  node.node_id = node_id;

  if (node_json.contains("role")) {
    if (auto role = ParseSpecNodeRole(node_json.at("role"))) {
      node.role = *role;
    }
  }
  if (node_json.contains("resist_model")) {
    if (auto model = ParseResistModel(node_json.at("resist_model"))) {
      node.resist_model = *model;
    }
  }
  if (node_json.contains("scope_policy")) {
    if (auto scope = ParseScopePolicy(node_json.at("scope_policy"))) {
      node.scope_policy = *scope;
    }
  }
  node.affects_sword_intent =
      node_json.value("affects_sword_intent", node.affects_sword_intent);
  node.affects_sword_step =
      node_json.value("affects_sword_step", node.affects_sword_step);

  if (node_json.contains("trigger")) {
    const auto &trigger = node_json.at("trigger");
    node.trigger.trigger_skill_id =
        trigger.value("trigger_skill_id", node.trigger.trigger_skill_id);
    node.trigger.effectiveness =
        trigger.value("effectiveness", node.trigger.effectiveness);
    node.trigger.internal_cooldown =
        trigger.value("internal_cooldown", node.trigger.internal_cooldown);
    node.trigger.consumes_mana =
        trigger.value("consumes_mana", node.trigger.consumes_mana);
  }

  def.nodes[node_id] = node;
}

void ParseSkillContract(const json &contract_json, SkillContractDefinition &def) {
  def.contract.skill_id = contract_json.value("skill_id", def.contract.skill_id);
  def.contract.min_nodes = ClampToU8(
      contract_json.value("min_nodes", static_cast<int>(def.contract.min_nodes)),
      def.contract.min_nodes);
  def.contract.max_nodes = ClampToU8(
      contract_json.value("max_nodes", static_cast<int>(def.contract.max_nodes)),
      def.contract.max_nodes);
  def.contract.max_transmuters = ClampToU8(
      contract_json.value("max_transmuters",
                          static_cast<int>(def.contract.max_transmuters)),
      def.contract.max_transmuters);
  def.contract.max_triggers = ClampToU8(
      contract_json.value("max_triggers",
                          static_cast<int>(def.contract.max_triggers)),
      def.contract.max_triggers);
  def.contract.has_sword_intent_node = contract_json.value(
      "has_sword_intent_node", def.contract.has_sword_intent_node);
  def.contract.has_synergy_node =
      contract_json.value("has_synergy_node", def.contract.has_synergy_node);

  if (contract_json.contains("transmuter_node_ids") &&
      contract_json.at("transmuter_node_ids").is_array()) {
    std::array<uint32_t, 2> ids = {0u, 0u};
    size_t idx = 0;
    for (const auto &id_json : contract_json.at("transmuter_node_ids")) {
      if (idx >= ids.size()) {
        break;
      }
      ids[idx++] = id_json.get<uint32_t>();
    }
    def.contract.transmuter_node_ids = ids;
  }

  if (contract_json.contains("nodes") && contract_json.at("nodes").is_array()) {
    for (const auto &node_json : contract_json.at("nodes")) {
      if (!node_json.is_object()) {
        continue;
      }
      ParseContractNode(node_json, def);
    }
  }
}

bool ValidateSkillContractInternal(const SkillContractDefinition &def,
                                   const SkillTreeDefinition *tree,
                                   std::string *error) {
  auto set_error = [&](const std::string &msg) {
    if (error) {
      *error = msg;
    }
  };

  if (!tree) {
    set_error("missing SkillTreeDefinition");
    return false;
  }

  const size_t node_count = tree->nodes.size();
  if (node_count < def.contract.min_nodes || node_count > def.contract.max_nodes) {
    std::ostringstream oss;
    oss << "node count out of range, got=" << node_count
        << " expected=[" << static_cast<int>(def.contract.min_nodes) << ","
        << static_cast<int>(def.contract.max_nodes) << "]";
    set_error(oss.str());
    return false;
  }

  uint32_t transmuter_count = 0;
  uint32_t trigger_count = 0;
  uint32_t synergy_count = 0;
  uint32_t sword_intent_count = 0;

  for (const auto &[node_id, node] : def.nodes) {
    if (!tree->nodes.contains(node_id)) {
      std::ostringstream oss;
      oss << "contract node_id not found in tree: " << node_id;
      set_error(oss.str());
      return false;
    }

    if (node.affects_sword_intent) {
      ++sword_intent_count;
    }

    switch (node.role) {
    case SpecNodeRole::Transmuter:
      ++transmuter_count;
      break;
    case SpecNodeRole::Trigger:
      ++trigger_count;
      break;
    case SpecNodeRole::Synergy:
      ++synergy_count;
      break;
    default:
      break;
    }
  }

  if (transmuter_count > def.contract.max_transmuters) {
    std::ostringstream oss;
    oss << "transmuter overflow, got=" << transmuter_count
        << " max=" << static_cast<int>(def.contract.max_transmuters);
    set_error(oss.str());
    return false;
  }
  if (trigger_count > def.contract.max_triggers) {
    std::ostringstream oss;
    oss << "trigger overflow, got=" << trigger_count
        << " max=" << static_cast<int>(def.contract.max_triggers);
    set_error(oss.str());
    return false;
  }
  if (def.contract.has_synergy_node && synergy_count == 0) {
    set_error("missing required synergy node");
    return false;
  }
  if (def.contract.has_sword_intent_node && sword_intent_count == 0) {
    set_error("missing required sword intent node");
    return false;
  }

  for (const uint32_t transmuter_id : def.contract.transmuter_node_ids) {
    if (transmuter_id == 0) {
      continue;
    }
    auto it = def.nodes.find(transmuter_id);
    if (it == def.nodes.end() || it->second.role != SpecNodeRole::Transmuter) {
      std::ostringstream oss;
      oss << "transmuter_node_ids contains non-transmuter node: " << transmuter_id;
      set_error(oss.str());
      return false;
    }
  }

  return true;
}

bool ValidateBladeAscendantResistCoverage(
    const std::unordered_map<uint32_t, SkillContractDefinition> &contracts,
    std::string *error) {
  std::array<bool, 5> present = {false, false, false, false, false};
  for (uint32_t skill_id = 1; skill_id <= 9; ++skill_id) {
    auto it = contracts.find(skill_id);
    if (it == contracts.end()) {
      continue;
    }
    for (const auto &[_, node] : it->second.nodes) {
      switch (node.resist_model) {
      case ResistModel::TypeA_Penetration:
        present[0] = true;
        break;
      case ResistModel::TypeB_Shred:
        present[1] = true;
        break;
      case ResistModel::TypeC_Exposure:
        present[2] = true;
        break;
      case ResistModel::TypeD_StatToPenetration:
        present[3] = true;
        break;
      case ResistModel::TypeE_CapSuppression:
        present[4] = true;
        break;
      default:
        break;
      }
    }
  }
  for (size_t i = 0; i < present.size(); ++i) {
    if (!present[i]) {
      if (error) {
        *error = "missing at least one resist model bucket in skills 1-9";
      }
      return false;
    }
  }
  return true;
}

} // namespace

static Tag StringToTag(const std::string &str) {
  // First try exact match with lowercase IDs
  if (auto tag = TagFromString(str)) {
    return *tag;
  }

  // Fallback: Try converting to lowercase for case-insensitive match
  std::string lower = str;
  for (char &c : lower) {
    if (c >= 'A' && c <= 'Z')
      c += 32;
  }
  if (auto tag = TagFromString(lower)) {
    return *tag;
  }

  // Legacy capitalized names not in kTagInfoTable
  static const std::unordered_map<std::string, Tag> kLegacyTags = {
      {"DamageOverTime", Tag::DamageOverTime},
      {"SwordRiding", Tag::SwordRiding},
      {"sword_skill", Tag::SwordSkill}};

  auto it = kLegacyTags.find(str);
  if (it != kLegacyTags.end())
    return it->second;

  LOG_WARN("Unknown tag string: {}", str);
  return Tag::None;
}

SkillRegistry &SkillRegistry::Get() {
  static SkillRegistry instance;
  return instance;
}

void SkillRegistry::LoadFromJson(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open skills JSON: {}", path);
    return;
  }

  json j;
  try {
    file >> j;
  } catch (const std::exception &e) {
    LOG_ERROR("Failed to parse skills JSON: {}", e.what());
    return;
  }

  skills_.clear();
  skill_trees_.clear();
  skill_contracts_.clear();
  for (const auto &item : j["skills"]) {
    SkillData data;
    data.id = item["id"];
    data.name_key = item.value("name_key", "");
    data.desc_key = item.value("desc_key", "");
    data.mana_cost = item.value("mana_cost", 0.0f);
    data.cooldown = item.value("cooldown", 0.0f);

    data.tags = Tag::None;
    if (item.contains("tags")) {
      for (const auto &tagStr : item["tags"]) {
        data.tags = data.tags | StringToTag(tagStr.get<std::string>());
      }
    }

    data.base_damage = item.value("base_damage", 0.0f);
    data.weapon_damage_mult = item.value("weapon_damage_mult", 1.0f);
    data.added_damage_effectiveness =
        item.value("added_damage_effectiveness", 1.0f);
    data.max_charges = item.value("charge_count", 1);
    data.icon_id = item.value("icon_id", 0);

    if (item.contains("params")) {
      for (auto &[key, val] : item["params"].items()) {
        if (val.is_number()) {
          data.params[key] = val.get<float>();
        }
      }
    }

    skills_[data.id] = data;

    // Parse Talent Tree
    if (item.contains("talent_tree")) {
      SkillTreeDefinition tree;
      tree.skill_id = data.id;
      for (const auto &nodeItem : item["talent_tree"]) {
        TalentNode node = nodeItem.get<TalentNode>();
        tree.nodes[node.id] = node;
      }
      ApplyPrerequisiteAnchoredLayout(tree);
      skill_trees_[data.id] = tree;
    }

    const SkillTreeDefinition *tree_ptr = GetSkillTree(data.id);
    SkillContractDefinition contract_def = BuildDefaultContract(data, tree_ptr);
    if (item.contains("skill_contract") && item["skill_contract"].is_object()) {
      ParseSkillContract(item["skill_contract"], contract_def);
    }
    skill_contracts_[data.id] = std::move(contract_def);
    std::string validation_error;
    if (!ValidateSkillContract(data.id, &validation_error)) {
      LOG_ERROR("Skill {} contract validation failed: {}", data.id,
                validation_error);
    }
  }
  LOG_INFO("Loaded {} skills, {} trees and {} contracts from {}", skills_.size(),
           skill_trees_.size(), skill_contracts_.size(), path);
  std::string resist_error;
  if (!ValidateBladeAscendantResistCoverage(skill_contracts_, &resist_error)) {
    LOG_ERROR("Blade Ascendant resist contract validation failed: {}",
              resist_error);
  }
}

const SkillData *SkillRegistry::GetSkill(uint32_t id) const {
  auto it = skills_.find(id);
  if (it != skills_.end()) {
    return &it->second;
  }

  // Fallback: If ID 0 (Basic Attack) is requested but not mapped, return a
  // default
  if (id == 0) {
    static const SkillData kDefaultBasicAttack = []() {
      SkillData d;
      d.id = 0;
      d.name_key = "Basic Attack";
      d.weapon_damage_mult = 1.0f;
      d.base_damage = 0.0f;
      d.tags = Tag::Physical | Tag::Attack | Tag::Melee; // Reasonable defaults
      return d;
    }();
    return &kDefaultBasicAttack;
  }

  return nullptr;
}

const SkillTreeDefinition *
SkillRegistry::GetSkillTree(uint32_t skill_id) const {
  auto it = skill_trees_.find(skill_id);
  if (it != skill_trees_.end()) {
    return &it->second;
  }
  return nullptr;
}

const SkillContractDefinition *
SkillRegistry::GetSkillContractDefinition(uint32_t skill_id) const {
  auto it = skill_contracts_.find(skill_id);
  if (it == skill_contracts_.end()) {
    return nullptr;
  }
  return &it->second;
}

const SkillContract *SkillRegistry::GetSkillContract(uint32_t skill_id) const {
  const auto *def = GetSkillContractDefinition(skill_id);
  return def ? &def->contract : nullptr;
}

const NodeContractData *SkillRegistry::GetNodeContract(uint32_t skill_id,
                                                       uint32_t node_id) const {
  const auto *def = GetSkillContractDefinition(skill_id);
  if (!def) {
    return nullptr;
  }
  auto it = def->nodes.find(node_id);
  if (it == def->nodes.end()) {
    return nullptr;
  }
  return &it->second;
}

bool SkillRegistry::ValidateSkillContract(uint32_t skill_id,
                                          std::string *error) const {
  const auto *def = GetSkillContractDefinition(skill_id);
  const auto *tree = GetSkillTree(skill_id);
  if (!def) {
    if (error) {
      *error = "missing SkillContractDefinition";
    }
    return false;
  }
  return ValidateSkillContractInternal(*def, tree, error);
}

} // namespace NoMoreDay
