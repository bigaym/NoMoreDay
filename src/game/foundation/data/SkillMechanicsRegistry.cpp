#include "game/foundation/data/SkillMechanicsRegistry.hpp"

#include "core/logging/Logger.hpp"
#include <array>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <string_view>

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

} // namespace

SkillMechanicsRegistry &SkillMechanicsRegistry::Get() {
  static SkillMechanicsRegistry s_instance;
  return s_instance;
}

bool SkillMechanicsRegistry::LoadFromFile(const std::string &path) {
  m_nodes.clear();
  m_loaded = false;

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

void SkillMechanicsRegistry::ResetForTests() {
  m_nodes.clear();
  m_loaded = false;
}

} // namespace NoMoreDay::data