#include "game/foundation/data/SkillMechanicsRegistry.hpp"

#include "core/logging/Logger.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

namespace NoMoreDay::data {

SkillMechanicsRegistry &SkillMechanicsRegistry::Get() {
  static SkillMechanicsRegistry s_instance;
  return s_instance;
}

bool SkillMechanicsRegistry::EnsureLoaded() {
  if (m_loaded) {
    return true;
  }
  return LoadFromFile();
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

    // 顶层键 = 技能 id（"1"、"2"...），跳过元字段（version/comment）
    for (auto it = root.begin(); it != root.end(); ++it) {
      const std::string &skillKey = it.key();
      if (skillKey == "version" || skillKey == "comment") {
        continue;
      }
      if (!it.value().is_object()) {
        continue;
      }
      const uint32_t skill_id = static_cast<uint32_t>(std::stoul(skillKey));
      auto &skillMap = m_nodes[skill_id];
      for (auto nodeIt = it.value().begin(); nodeIt != it.value().end();
           ++nodeIt) {
        if (!nodeIt.value().is_object()) {
          continue;
        }
        const uint32_t node_id =
            static_cast<uint32_t>(std::stoul(nodeIt.key()));
        NodeTable table;
        for (auto valIt = nodeIt.value().begin(); valIt != nodeIt.value().end();
             ++valIt) {
          if (valIt.value().is_number()) {
            table.values[valIt.key()] =
                valIt.value().get<float>();
          }
        }
        skillMap[node_id] = std::move(table);
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