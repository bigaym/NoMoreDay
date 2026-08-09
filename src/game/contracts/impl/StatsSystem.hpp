#pragma once
#include "game/components/Stats.hpp"
#include "game/data/TagRegistry.hpp"
#include <entt/entity/registry.hpp>
#include <unordered_map>
#include <shared_mutex>

namespace NoMoreDay {

class StatsSystem {
public:
  // @brief 根据主要属性和修饰符重新计算特定实体的所有战斗属性。
  //
  static void Recalculate(entt::registry &registry, entt::entity entity);

  // @brief 获取考虑了特定标签后的最终属性值。
  // 会自动结合 CombatStats 中的基础值与动态标签修饰符。
  static float GetStatWithTags(entt::registry &registry, entt::entity entity,
                               StatType type, Tag tags, uint32_t skill_id = 0,
                               entt::entity source_entity = entt::null);

  // @brief 系统更新：为所有带有 StatsDirty 标签的实体重新计算属性。
  //
  static void update(entt::registry &registry);

  // @brief 更新所有实体的活跃 Buff 生命周期。
  static void UpdateBuffs(entt::registry &registry, float dt);

  // @brief 清除特定实体的属性缓存（在属性重新计算时调用）
  static void ClearCache(entt::registry &registry, entt::entity entity);

  // @brief 初始化系统，注册监听器（如销毁监听）
  static void Initialize(entt::registry &registry);

  // @brief 关闭系统，清理监听器
  static void Shutdown(entt::registry &registry);

  // @brief 完全重置内部状态 (主要是缓存)，仅用于测试或硬重置
  static void Reset();

private:
  // Per-entity stat cache, moved out of CombatStats to allow alignas(32) for
  // SIMD Key: FNV-1a hash of (StatType, Tag, SkillID, SourceEntity) Outer Key:
  // Entity ID Flattened cache for better performance Outer Key: Entity ID
  // (uint32_t) Inner Key: Hash of (StatType, Tag, SkillID, SourceEntity)
  static inline std::unordered_map<uint32_t,
                                   std::unordered_map<uint64_t, float>>
      s_tagStatCache;
  static inline std::shared_mutex s_cacheMutex;
};

} // namespace NoMoreDay