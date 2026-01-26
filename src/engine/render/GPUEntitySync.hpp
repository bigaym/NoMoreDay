#pragma once
#include "engine/render/GPUData.hpp"
#include <entt/entt.hpp>
#include <functional>
#include <vector>

namespace NoMoreDay::render {

/**
 * @brief GPU 物理数据同步 Job。
 *
 * 职责: 将 Position, Radius, Velocity 同步到 GPUEntity Shadow Buffer。
 * 执行频率: 每帧
 * 复杂度: O(N) 线性，缓存友好
 */
class GPUPhysicsSync {
public:
  struct Config {
    int maxEntities = 200000;
    float teleportThreshold = 100.0f; // 超过此距离视为传送，不插值
  };

  void Init(const Config &config);

  /**
   * @brief 执行物理同步。
   * @param registry EnTT Registry
   * @param shadowBuffer 输出: GPUEntity 影子缓冲区
   * @param frameCounter 当前帧号
   * @return 使用的最高槽位索引 (用于优化 memcpy 范围)
   */
  int Execute(entt::registry &registry,
              std::vector<NoMoreDay::components::GPUEntity> &shadowBuffer,
              uint64_t frameCounter);

private:
  Config m_config;
};

/**
 * @brief GPU 视觉数据同步 Job。
 *
 * 职责: 将 CombatStats, ActiveEffects 同步到 GPUVisualStats Shadow Buffer。
 * 执行频率: 脏标记触发 或 每 N 帧
 */
class GPUVisualSync {
public:
  struct Config {
    int maxEntities = 200000;
    int refreshInterval = 5; // 无脏标记时，每 N 帧刷新一次
  };

  void Init(const Config &config);

  /**
   * @brief 执行视觉同步。
   * @param registry EnTT Registry
   * @param visualBuffer 输出: GPUVisualStats 影子缓冲区
   * @param frameCounter 当前帧号
   * @param currentTime 当前游戏时间 (用于状态计时器)
   */
  void Execute(entt::registry &registry,
               std::vector<NoMoreDay::components::GPUVisualStats> &visualBuffer,
               uint64_t frameCounter, float currentTime);

private:
  Config m_config;
};

/**
 * @brief GPU 槽位管理器。
 *
 * 职责: 管理 GPUIndex.index 的分配与回收。
 */
class GPUSlotManager {
public:
  using SlotRecycleCallback = std::function<void(int)>;

  void Init(int maxEntities, entt::registry *registry,
            SlotRecycleCallback onRecycle = nullptr);

  /**
   * @brief 为新实体分配槽位，回收已死亡实体槽位。
   */
  void Process(entt::registry &registry);

  /**
   * @brief EnTT 销毁回调。
   */
  void OnEntityDestroyed(entt::registry &registry, entt::entity entity);

  // 访问器
  int GetMaxEntities() const { return m_maxEntities; }
  const std::vector<entt::entity> &GetSlotToEntity() const {
    return m_slotToEntity;
  }

private:
  int m_maxEntities = 0;
  std::vector<int> m_freeSlots;
  std::vector<entt::entity> m_slotToEntity;
  SlotRecycleCallback m_onRecycle;
};

} // namespace NoMoreDay::render
