#include "game/foundation/data/SkillMechanicsRegistry.hpp"

#include "core/logging/Logger.hpp"
#include <array>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <unordered_set>

namespace NoMoreDay::data {

namespace {

// 必需技能 id：任一缺席都视为配置损坏，避免运行期用默认值掩盖配置缺失。
// 技能 10/11/12 已完成机制外置（A2-1/A2-2/A2-3），故当前要求 1..12。
static constexpr std::array<uint32_t, 12> kRequiredSkillIds{
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

// 解析十进制无符号 id：仅接受纯数字，拒绝前导符号/空白与超出 uint32 的输入。
// std::stoul 的 out_of_range 在此吞掉，保证校验不会把异常抛给调用方。
[[nodiscard]] bool TryParseUint32Id(const std::string &text, uint32_t &outId) {
  if (text.empty()) {
    return false;
  }
  for (const char ch : text) {
    if (ch < '0' || ch > '9') {
      return false;
    }
  }
  try {
    const unsigned long value = std::stoul(text);
    if (value > std::numeric_limits<uint32_t>::max()) {
      return false;
    }
    outId = static_cast<uint32_t>(value);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

// 机制表同目录推导出的 schema 文件名；schema 为可选产物，缺失即跳过校验。
static constexpr std::string_view kSchemaFileName = "skill_mechanics_schema.json";

[[nodiscard]] std::string DeriveSchemaPath(const std::string &mechanicsPath) {
  const size_t slash = mechanicsPath.find_last_of("/\\");
  const std::string dir =
      (slash == std::string::npos) ? std::string() : mechanicsPath.substr(0, slash + 1);
  return dir + std::string(kSchemaFileName);
}

} // namespace

SkillMechanicsRegistry &SkillMechanicsRegistry::Get() {
  static SkillMechanicsRegistry s_instance;
  return s_instance;
}

bool SkillMechanicsRegistry::LoadFromFile(const std::string &path,
                                           const std::string &schemaPath) {
  m_nodes.clear();
  m_loaded = false;
  m_lastLoadWarnings.clear();

  // schema 为可选产物：显式路径优先，否则按机制表同目录推导；缺失时静默跳过校验。
  LoadSchema(schemaPath.empty() ? DeriveSchemaPath(path) : schemaPath);

  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("SkillMechanicsRegistry: failed to open {}", path);
    return false;
  }

  try {
    nlohmann::json root;
    file >> root;

    // 顶层键 = 技能 id（"1"、"2"...），跳过元字段（version/comment）。
    // 结构损坏一律显式失败并清空状态，禁止静默部分加载。
    for (auto it = root.begin(); it != root.end(); ++it) {
      const std::string &skillKey = it.key();
      if (skillKey == "version" || skillKey == "comment") {
        continue;
      }
      uint32_t skill_id = 0;
      if (!TryParseUint32Id(skillKey, skill_id) || skill_id == 0) {
        LOG_ERROR("SkillMechanicsRegistry: top-level key '{}' is not a positive "
                  "integer skill id in {}",
                  skillKey, path);
        m_nodes.clear();
        return false;
      }
      if (!it.value().is_object()) {
        LOG_ERROR(
            "SkillMechanicsRegistry: skill section '{}' is not an object in {}",
            skillKey, path);
        m_nodes.clear();
        return false;
      }
      auto &skillMap = m_nodes[skill_id];
      for (auto nodeIt = it.value().begin(); nodeIt != it.value().end();
           ++nodeIt) {
        // 技能级说明字段（comment）不参与节点解析，允许各技能自带注释。
        if (nodeIt.key() == "comment") {
          continue;
        }
        // 节点 id 允许为 0：技能 5/7/8/9 的 0 号节点是基础投递机制，运行时确有读取。
        uint32_t node_id = 0;
        if (!TryParseUint32Id(nodeIt.key(), node_id)) {
          LOG_ERROR(
              "SkillMechanicsRegistry: node key '{}' of skill {} is not an "
              "unsigned integer in {}",
              nodeIt.key(), skill_id, path);
          m_nodes.clear();
          return false;
        }
        if (!nodeIt.value().is_object()) {
          LOG_ERROR("SkillMechanicsRegistry: node '{}' of skill {} is not an "
                    "object in {}",
                    nodeIt.key(), skill_id, path);
          m_nodes.clear();
          return false;
        }
        NodeTable table;
        for (auto valIt = nodeIt.value().begin(); valIt != nodeIt.value().end();
             ++valIt) {
          if (!valIt.value().is_number()) {
            LOG_ERROR(
                "SkillMechanicsRegistry: leaf '{}.{}.{}' is not a number in {}",
                skill_id, node_id, valIt.key(), path);
            m_nodes.clear();
            return false;
          }
          table.values[valIt.key()] = valIt.value().get<float>();
        }
        skillMap[node_id] = std::move(table);
      }
    }

    // 必需技能缺失即判定配置不完整，避免运行期静默回退到默认值。
    for (const uint32_t requiredId : kRequiredSkillIds) {
      if (!m_nodes.contains(requiredId)) {
        LOG_ERROR("SkillMechanicsRegistry: required skill id {} missing from {}",
                  requiredId, path);
        m_nodes.clear();
        return false;
      }
    }
  } catch (const std::exception &e) {
    LOG_ERROR("SkillMechanicsRegistry: failed to parse {}: {}", path, e.what());
    m_nodes.clear();
    return false;
  }

  m_loaded = true;
  LOG_INFO("SkillMechanicsRegistry: loaded {} skills from {}", m_nodes.size(),
           path);
  // 结构合法后再做键名漂移校验：只告警，绝不改变加载结果。
  ValidateAgainstSchema();
  return true;
}

float SkillMechanicsRegistry::GetFloat(uint32_t skill_id, uint32_t node_id,
                                       const std::string &key,
                                       float default_value) const {
  return GetFloatImpl(skill_id, node_id, key, default_value);
}

float SkillMechanicsRegistry::GetFloatImpl(uint32_t skill_id, uint32_t node_id,
                                           std::string_view key,
                                           float default_value) const {
  const auto skillIt = m_nodes.find(skill_id);
  if (skillIt == m_nodes.end()) {
    return default_value;
  }
  const auto nodeIt = skillIt->second.find(node_id);
  if (nodeIt == skillIt->second.end()) {
    return default_value;
  }
  const auto valIt = nodeIt->second.values.find(key);
  if (valIt == nodeIt->second.values.end()) {
    return default_value;
  }
  return valIt->second;
}

bool SkillMechanicsRegistry::HasNode(uint32_t skill_id,
                                     uint32_t node_id) const {
  const auto skillIt = m_nodes.find(skill_id);
  if (skillIt == m_nodes.end()) {
    return false;
  }
  return skillIt->second.contains(node_id);
}

std::string SkillMechanicsRegistry::EncodeTuple(uint32_t skill_id,
                                                uint32_t node_id,
                                                std::string_view key) {
  std::string encoded = std::to_string(skill_id);
  encoded.push_back(':');
  encoded += std::to_string(node_id);
  encoded.push_back(':');
  encoded.append(key);
  return encoded;
}

void SkillMechanicsRegistry::ResetSchema() {
  m_schema.knownKeys.clear();
  m_schema.readTuples.clear();
  m_schema.dynamicKeys.clear();
  m_schema.loaded = false;
}

void SkillMechanicsRegistry::LoadSchema(const std::string &schemaPath) {
  ResetSchema();
  std::ifstream file(schemaPath);
  if (!file.is_open()) {
    // schema 为可选产物：缺失即静默跳过键名校验，保持历史加载行为。
    return;
  }
  try {
    nlohmann::json root;
    file >> root;
    if (!root.is_object()) {
      LOG_WARN("SkillMechanicsRegistry: key schema '{}' is not an object; "
               "skipping key validation",
               schemaPath);
      return;
    }
    // entries：代码静态读取的 [skill, node, key] 三元组（键名 + 精确位置）。
    if (const auto entriesIt = root.find("entries");
        entriesIt != root.end() && entriesIt->is_array()) {
      for (const auto &entry : *entriesIt) {
        if (!entry.is_array() || entry.size() != 3 || !entry[0].is_number() ||
            !entry[1].is_number() || !entry[2].is_string()) {
          continue;
        }
        const uint32_t skill_id = entry[0].get<uint32_t>();
        const uint32_t node_id = entry[1].get<uint32_t>();
        const std::string key = entry[2].get<std::string>();
        m_schema.readTuples.insert(EncodeTuple(skill_id, node_id, key));
        m_schema.knownKeys.insert(key);
      }
    }
    // dynamic_keys：skill/node 为运行时变量、只能确定键名的读取点。
    if (const auto dynamicIt = root.find("dynamic_keys");
        dynamicIt != root.end() && dynamicIt->is_array()) {
      for (const auto &key : *dynamicIt) {
        if (!key.is_string()) {
          continue;
        }
        const std::string name = key.get<std::string>();
        m_schema.dynamicKeys.insert(name);
        m_schema.knownKeys.insert(name);
      }
    }
    // unreferenced：机制表已有但代码未精确读取的三元组，只登记键名以避免误报。
    if (const auto unrefIt = root.find("unreferenced");
        unrefIt != root.end() && unrefIt->is_array()) {
      for (const auto &entry : *unrefIt) {
        if (entry.is_array() && entry.size() == 3 && entry[2].is_string()) {
          m_schema.knownKeys.insert(entry[2].get<std::string>());
        }
      }
    }
    m_schema.loaded = true;
    LOG_INFO("SkillMechanicsRegistry: key schema loaded from {} ({} read tuples, "
             "{} known keys)",
             schemaPath, m_schema.readTuples.size(), m_schema.knownKeys.size());
  } catch (const std::exception &e) {
    LOG_WARN("SkillMechanicsRegistry: failed to parse key schema '{}'; skipping "
             "key validation: {}",
             schemaPath, e.what());
    ResetSchema();
  }
}

void SkillMechanicsRegistry::ValidateAgainstSchema() {
  if (!m_schema.loaded) {
    return; // 无 schema：保持历史行为，不做任何键名校验
  }
  const auto warn = [this](const std::string &message) {
    m_lastLoadWarnings.push_back(message);
    LOG_WARN("SkillMechanicsRegistry: {}", message);
  };

  // 汇总机制表实际存在的三元组与键名，供双向比对。
  std::unordered_set<std::string> jsonTuples;
  std::unordered_set<std::string> jsonKeys;
  for (const auto &[skill_id, nodes] : m_nodes) {
    for (const auto &[node_id, table] : nodes) {
      for (const auto &[key, value] : table.values) {
        (void)value;
        jsonTuples.insert(EncodeTuple(skill_id, node_id, key));
        jsonKeys.insert(key);
      }
    }
  }

  // 方向一：机制表键名未登记进 schema（JSON 新增/改名而代码未同步）。
  for (const auto &[skill_id, nodes] : m_nodes) {
    for (const auto &[node_id, table] : nodes) {
      for (const auto &[key, value] : table.values) {
        (void)value;
        if (!m_schema.knownKeys.contains(key)) {
          warn("mechanics key '" + EncodeTuple(skill_id, node_id, key) +
               "' is present in the mechanics table but absent from the key schema");
        }
      }
    }
  }
  // 方向二：schema 登记为代码读取、但机制表没有（代码改名而 JSON 未同步）。
  for (const auto &tuple : m_schema.readTuples) {
    if (!jsonTuples.contains(tuple)) {
      warn("mechanics key '" + tuple +
           "' is read by code but missing from the mechanics table");
    }
  }
  // 方向三：动态读取的键名整体缺席（无法定位节点，退化为按键名判断）。
  for (const auto &key : m_schema.dynamicKeys) {
    if (!jsonKeys.contains(key)) {
      warn("mechanics key '" + key +
           "' is dynamically read by code but missing from the mechanics table");
    }
  }
}

void SkillMechanicsRegistry::ResetForTests() {
  m_nodes.clear();
  m_loaded = false;
  ResetSchema();
  m_lastLoadWarnings.clear();
}

} // namespace NoMoreDay::data
