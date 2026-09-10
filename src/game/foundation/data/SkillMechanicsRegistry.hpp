#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
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
  /**
   * @brief 字符串字面量专用重载：以 string_view 直接查表，不构造临时 std::string。
   *
   * 字面量到 const char* 属精确匹配，优于到 const std::string& 的用户定义转换，
   * 因此热路径调用点会直接命中本重载。
   */
  [[nodiscard]] float GetFloat(uint32_t skill_id, uint32_t node_id,
                               const char *key,
                               float default_value = 0.0f) const {
    return GetFloatImpl(skill_id, node_id, std::string_view(key), default_value);
  }
  /** 节点表是否存在（已分配但未配数值的节点可据此回退到默认值）。 */
  [[nodiscard]] bool HasNode(uint32_t skill_id, uint32_t node_id) const;

  void ResetForTests();

private:
  // 异质查找：允许以 string_view 直接查询 string 键，避免热路径构造临时串
  struct TransparentStringHash {
    using is_transparent = void;
    [[nodiscard]] size_t operator()(std::string_view key) const noexcept {
      return std::hash<std::string_view>{}(key);
    }
  };
  struct TransparentStringEqual {
    using is_transparent = void;
    [[nodiscard]] bool operator()(std::string_view lhs,
                                  std::string_view rhs) const noexcept {
      return lhs == rhs;
    }
  };

  [[nodiscard]] float GetFloatImpl(uint32_t skill_id, uint32_t node_id,
                                   std::string_view key,
                                   float default_value) const;

  struct NodeTable {
    std::unordered_map<std::string, float, TransparentStringHash,
                       TransparentStringEqual>
        values;
  };
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, NodeTable>>
      m_nodes; // skill_id -> node_id -> 数值表
  bool m_loaded = false;
};

} // namespace NoMoreDay::data