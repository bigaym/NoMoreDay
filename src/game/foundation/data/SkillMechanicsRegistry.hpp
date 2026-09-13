#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NoMoreDay::data {

/**
 * @brief 技能节点机制数值注册表。
 *
 * 读取 assets/data/skill_mechanics.json，键 = 技能 id -> 节点 id -> 数值字段名。
 * 技能行为在运行时按 (skill_id, node_id, key) 读取数值，禁止在代码中硬编码机制数值。
 * 加载方式：启动期显式 LoadFromFile，失败由调用方阻断启动（无惰性加载接口）。
 */
class SkillMechanicsRegistry {
public:
  static SkillMechanicsRegistry &Get();

  /**
   * @brief 加载机制表，并在可选 schema 存在时校验键名漂移。
   *
   * schemaPath 为空时按机制表同目录推导 skill_mechanics_schema.json；schema 缺失
   * 或损坏时静默跳过校验，加载行为与历史版本完全一致。键名漂移只记录警告，不影响
   * 返回值与已加载数据。
   */
  [[nodiscard]] bool
  LoadFromFile(const std::string &path = "assets/data/skill_mechanics.json",
               const std::string &schemaPath = "");

  /** 最近一次加载产生的键名 schema 诊断（测试用；加载成功/失败均可读取）。 */
  [[nodiscard]] const std::vector<std::string> &GetLastLoadWarnings() const {
    return m_lastLoadWarnings;
  }

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
  /** string_view 重载：供 skills::GetMech 等热路径零分配查表。 */
  [[nodiscard]] float GetFloat(uint32_t skill_id, uint32_t node_id,
                               std::string_view key,
                               float default_value = 0.0f) const {
    return GetFloatImpl(skill_id, node_id, key, default_value);
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

  // 三元组统一编码为 "skill:node:key"，用于集合比较；键名不含 ':'，编码无歧义。
  [[nodiscard]] static std::string EncodeTuple(uint32_t skill_id,
                                               uint32_t node_id,
                                               std::string_view key);

  // 可选键名 schema：登记代码允许读取的键名与三元组。
  struct SchemaInfo {
    std::unordered_set<std::string> knownKeys;   // 允许出现的键名全集
    std::unordered_set<std::string> readTuples;  // 代码静态读取的三元组
    std::unordered_set<std::string> dynamicKeys; // 只能确定键名的动态读取
    bool loaded = false;
  };

  void LoadSchema(const std::string &schemaPath);
  void ResetSchema();
  void ValidateAgainstSchema();

  struct NodeTable {
    std::unordered_map<std::string, float, TransparentStringHash,
                       TransparentStringEqual>
        values;
  };
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, NodeTable>>
      m_nodes; // skill_id -> node_id -> 数值表
  bool m_loaded = false;
  SchemaInfo m_schema;                       // 可选键名 schema 状态
  std::vector<std::string> m_lastLoadWarnings; // 最近一次加载的键名诊断
};

} // namespace NoMoreDay::data
