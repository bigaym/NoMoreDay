#pragma once

#include "game/systems/item/storage/ItemStorageTypes.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace NoMoreDay {

/**
 * @brief High-performance handle pool managing fixed-size POD ItemInstances.
 * Zero heap allocation on hot paths, generation-checked handle safety, memcpy-friendly.
 */
class ItemStore {
public:
  ItemStore();
  ~ItemStore() = default;

  ItemStore(const ItemStore &other);
  ItemStore &operator=(const ItemStore &other);
  ItemStore(ItemStore &&other) noexcept;
  ItemStore &operator=(ItemStore &&other) noexcept;

  /**
   * @brief Create an item instance in the pool from a prototype.
   * @return Generation-validated handle to the allocated instance.
   */
  ItemHandle create(const ItemInstance &proto) noexcept;

  /**
   * @brief Retrieve immutable view of an item instance by handle.
   * @return Pointer to instance, or nullptr if handle is stale or invalid.
   */
  [[nodiscard]] const ItemInstance *get(ItemHandle h) const noexcept;

  /**
   * @brief Retrieve mutable pointer to an item instance by handle.
   * @return Pointer to instance, or nullptr if handle is stale or invalid.
   */
  [[nodiscard]] ItemInstance *getMutable(ItemHandle h) noexcept;

  /**
   * @brief Check whether a handle is currently valid (non-null, correct generation, active).
   */
  [[nodiscard]] bool isValid(ItemHandle h) const noexcept;

  /**
   * @brief Mutate an item in-place via callable and bump version.
   * @note The callable `fn` must be non-throwing (noexcept). If `fn` throws an exception,
   *       `std::terminate` will be called due to noexcept specification.
   * @return true if handle was valid and mutated, false otherwise.
   */
  template <typename Fn>
  requires (!std::is_same_v<std::decay_t<Fn>, std::function<void(ItemInstance &)>>)
  bool mutate(ItemHandle h, Fn &&fn) noexcept {
    if (!isValid(h)) {
      return false;
    }
    fn(m_instances[h.index]);
    m_version.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  /**
   * @brief Mutate overload for std::function.
   */
  bool mutate(ItemHandle h, const std::function<void(ItemInstance &)> &fn) noexcept;

  /**
   * @brief Destroy an item instance, returning its slot to free list and incrementing generation.
   */
  void destroy(ItemHandle h) noexcept;

  /**
   * @brief Current global mutation version.
   */
  [[nodiscard]] uint64_t version() const noexcept {
    return m_version.load(std::memory_order_relaxed);
  }

  /**
   * @brief Attach sparse side-table data for an item handle.
   */
  void setSideTable(ItemHandle h, ItemSideTableData data);

  /**
   * @brief Get immutable sparse side-table data for an item handle.
   */
  [[nodiscard]] const ItemSideTableData *getSideTable(ItemHandle h) const noexcept;

  /**
   * @brief Get mutable sparse side-table data for an item handle.
   */
  [[nodiscard]] ItemSideTableData *getSideTableMutable(ItemHandle h) noexcept;

  /**
   * @brief Linear zero-allocation visitor over all currently active items.
   */
  template <typename Fn>
  void visit(Fn &&fn) const {
    for (size_t i = 1; i < m_instances.size(); ++i) {
      if (m_occupied[i]) {
        if constexpr (std::is_invocable_v<Fn, ItemHandle, const ItemInstance &>) {
          fn(ItemHandle{static_cast<uint32_t>(i), m_generations[i]}, m_instances[i]);
        } else if constexpr (std::is_invocable_v<Fn, const ItemInstance &>) {
          fn(m_instances[i]);
        } else if constexpr (std::is_invocable_v<Fn, ItemHandle>) {
          fn(ItemHandle{static_cast<uint32_t>(i), m_generations[i]});
        }
      }
    }
  }

  /**
   * @brief Active item count.
   */
  [[nodiscard]] size_t activeCount() const noexcept {
    return m_activeCount;
  }

  /**
   * @brief Whether the store has no active items.
   */
  [[nodiscard]] bool empty() const noexcept {
    return m_activeCount == 0;
  }

  /**
   * @brief Total allocated slot capacity.
   */
  [[nodiscard]] size_t capacity() const noexcept {
    return m_instances.capacity();
  }

  /**
   * @brief Total slot count (excluding index 0 dummy).
   */
  [[nodiscard]] size_t slotCount() const noexcept {
    return m_instances.empty() ? 0 : m_instances.size() - 1;
  }

  /**
   * @brief 用于底层批量还原的原始实例条目。
   */
  struct RawInstanceEntry {
    uint32_t index = 0;
    uint32_t gen = 0;
    ItemInstance instance{};
  };

  /**
   * @brief 批量恢复/灌入底层实例池状态，自动重构空闲链表与活跃计数。
   */
  void restoreRawEntries(
      const std::vector<RawInstanceEntry> &entries,
      const std::unordered_map<uint32_t, ItemSideTableData> &sideTables = {});

  /**
   * @brief 获取所有稀疏旁表数据的不可变引用。
   */
  [[nodiscard]] const std::unordered_map<uint32_t, ItemSideTableData> &
  getAllSideTables() const noexcept {
    return m_sideTables;
  }

  /**
   * @brief Clear all instances and side tables, resetting active count and incrementing version.
   */
  void clear() noexcept;

  /**
   * @brief Reserve capacity in internal vectors.
   */
  void reserve(size_t cap);

private:
  std::vector<ItemInstance> m_instances;
  std::vector<uint32_t> m_generations;
  std::vector<uint32_t> m_freeList;
  std::unordered_map<uint32_t, ItemSideTableData> m_sideTables;
  std::vector<uint8_t> m_occupied;
  std::atomic<uint64_t> m_version{1};
  size_t m_activeCount = 0;
};

} // namespace NoMoreDay
