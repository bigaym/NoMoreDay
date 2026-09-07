#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <entt/entity/entity.hpp>

namespace NoMoreDay {

/**
 * @brief 定容紧凑实体集合 (Small Buffer Optimization, 零堆分配)
 *
 * 满足 Standard Layout 与 平凡可析构，专为命中去重与目标追踪设计。
 * 容量满时采用忽略策略 (insert 返回 false)。
 */
template <size_t Capacity>
class CompactEntitySet {
public:
  static constexpr size_t kCapacity = Capacity;

  constexpr CompactEntitySet() noexcept = default;

  /**
   * @brief 尝试插入实体。
   * 若集合已包含该实体，返回 false。
   * 若集合已满，返回 false（满槽忽略）。
   * 插入成功返回 true。
   */
  bool insert(entt::entity e) noexcept {
    if (contains(e)) {
      return false;
    }
    if (count_ >= Capacity) {
      return false;
    }
    data_[count_++] = e;
    return true;
  }

  [[nodiscard]] bool contains(entt::entity e) const noexcept {
    for (size_t i = 0; i < count_; ++i) {
      if (data_[i] == e) {
        return true;
      }
    }
    return false;
  }

  void clear() noexcept {
    count_ = 0;
  }

  [[nodiscard]] size_t size() const noexcept { return count_; }
  [[nodiscard]] constexpr size_t capacity() const noexcept { return Capacity; }
  [[nodiscard]] bool empty() const noexcept { return count_ == 0; }
  [[nodiscard]] bool full() const noexcept { return count_ >= Capacity; }

  [[nodiscard]] const entt::entity *begin() const noexcept { return data_.data(); }
  [[nodiscard]] const entt::entity *end() const noexcept { return data_.data() + count_; }
  [[nodiscard]] entt::entity *begin() noexcept { return data_.data(); }
  [[nodiscard]] entt::entity *end() noexcept { return data_.data() + count_; }

  [[nodiscard]] entt::entity operator[](size_t index) const noexcept {
    return data_[index];
  }

  bool operator==(const CompactEntitySet &other) const noexcept {
    if (count_ != other.count_) {
      return false;
    }
    for (size_t i = 0; i < count_; ++i) {
      if (data_[i] != other.data_[i]) {
        return false;
      }
    }
    return true;
  }

private:
  std::array<entt::entity, Capacity> data_{};
  size_t count_ = 0;
};

static_assert(std::is_standard_layout_v<CompactEntitySet<8>>);
static_assert(std::is_trivially_destructible_v<CompactEntitySet<8>>);
static_assert(std::is_standard_layout_v<CompactEntitySet<32>>);
static_assert(std::is_trivially_destructible_v<CompactEntitySet<32>>);

} // namespace NoMoreDay
