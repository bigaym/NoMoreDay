#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace NoMoreDay::data {

/**
 * @brief 技能节点机制数值注册表。
 *
 * 读取 assets/data/skill_mechanics.json，键 = 技能 id -> 节点 id -> 数值字段名。
 * 技能行为在运行时按 (skill_id, node_id, key) 读取数值，禁止在代码中硬编码机制数值。
 * 加载方式与 SkillRegistry/AilmentRegistry 一致：惰性 EnsureLoaded + 显式 LoadFromFile。
 */
class SkillMechanicsRegistry {
public:
  static SkillMechanicsRegistry &Get();

  [[nodiscard]] bool EnsureLoaded();
  [[nodiscard]] bool
  LoadFromFile(const std::string &path = "assets/data/skill_mechanics.json");

  /** 读取 (skill_id, node_id) 下 key 对应的浮点数值，缺失时返回默认值。 */
  [[nodiscard]] float GetFloat(uint32_t skill_id, uint32_t node_id,
                               const std::string &key,
                               float default_value = 0.0f) const;
  /** 节点表是否存在（已分配但未配数值的节点可据此回退到默认值）。 */
  [[nodiscard]] bool HasNode(uint32_t skill_id, uint32_t node_id) const;

  void ResetForTests();

private:
  struct NodeTable {
    std::unordered_map<std::string, float> values;
  };
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, NodeTable>>
      m_nodes; // skill_id -> node_id -> 数值表
  bool m_loaded = false;
};

} // namespace NoMoreDay::data