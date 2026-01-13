#pragma once

#include <entt/entt.hpp>

namespace NoMoreDay {

/**
 * @brief 精英词缀系统 - 管理 Link 和 Avenger 词缀的逻辑
 *
 * Link (灵魂链接):
 *   - 维护链接组的连接状态
 *   - 当组内成员受伤时平均分配伤害
 *
 * Avenger (复仇者):
 *   - 监听 OnKill 事件
 *   - 当周围友军死亡时增加堆叠
 */
class EliteModifierSystem {
public:
  /**
   * @brief 初始化系统，注册事件处理器
   */
  static void Init();

  /**
   * @brief 关闭系统，注销事件处理器
   */
  static void Shutdown();

  /**
   * @brief 每帧更新：维护 Link 连接状态
   */
  static void Update(entt::registry &registry, float dt);

  /**
   * @brief 分配伤害到 Link 组
   * @param target 受击目标
   * @param damage 原始伤害
   * @return 是否成功分配（如果不在 Link 组则返回 false）
   */
  static bool DistributeDamageToLinkGroup(entt::registry &registry,
                                          entt::entity target, float damage);

private:
  /**
   * @brief 更新所有 SoulLink 组件的链接列表
   */
  static void UpdateSoulLinks(entt::registry &registry);

  /**
   * @brief 渲染 Link 连接线（调试用）
   */
  static void RenderLinkLines(entt::registry &registry);

  /**
   * @brief OnKill 事件处理器 - 处理 Avenger 堆叠
   */
  static void OnEnemyKilled(entt::registry &registry,
                            const struct CombatEvent &evt);

  static uint32_t s_killHandlerId;
  static bool s_initialized;
};

} // namespace NoMoreDay
