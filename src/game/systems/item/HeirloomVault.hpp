// src/game/systems/item/HeirloomVault.hpp
// 传家宝宝库管理系统 - 处理跨存档的传家宝存储与加载
#pragma once

#include "core/logging/Logger.hpp"
#include "game/foundation/components/HeirloomComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/systems/item/HeirloomScaling.hpp"
#include <concepts>
#include <string>
#include <string_view>
#include <vector>
#include <span>

namespace NoMoreDay {

/// @brief 传家宝数据结构
/// @details 包含 ItemComponent 的核心数据和 HeirloomComponent 的元数据
struct HeirloomData {
  ItemComponent item;
  HeirloomComponent heirloom;

  // 用于 UI 显示的预计算字段
  float effective_power_at_level_1{0.0f};
  float effective_power_at_level_50{0.0f};
};

/// @brief 传家宝宝库管理器
/// @details 管理传家宝内存数据（持久化由 ItemStorageService 二进制单轨统一接管）
class HeirloomVault {
public:
  HeirloomVault() = default;

  /// 最大传家宝数量
  static constexpr size_t kMaxHeirlooms = 20;

  /// 添加传家宝
  /// @param item 物品数据
  /// @param level_requirement 等级要求
  /// @param rarity 稀有度
  /// @return 是否添加成功 (可能因数量限制失败)
  bool addHeirloom(const ItemComponent &item, uint8_t level_requirement,
                   Rarity rarity) {
    if (m_heirlooms.size() >= kMaxHeirlooms) {
      LOG_WARN("[HeirloomVault] Vault is full ({} items), cannot add more.",
               kMaxHeirlooms);
      return false;
    }

    HeirloomData data;
    data.item = item;
    data.heirloom.tier = calculateTier(rarity);
    data.heirloom.original_level_requirement = level_requirement;
    data.heirloom.created_timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    data.heirloom.display_name = item.name;
    data.heirloom.original_rarity = static_cast<uint8_t>(rarity);

    // 预计算有效战力
    data.effective_power_at_level_1 =
        HeirloomScaling::calculateEffectivePowerPercent(1, level_requirement);
    data.effective_power_at_level_50 =
        HeirloomScaling::calculateEffectivePowerPercent(50, level_requirement);

    m_heirlooms.push_back(std::move(data));
    LOG_INFO("[HeirloomVault] Added heirloom: {} (Tier {})", item.name,
             data.heirloom.tier);

    return true;
  }

  /// 移除传家宝
  /// @param index 宝库中的索引
  /// @return 是否移除成功
  bool removeHeirloom(size_t index) {
    if (index >= m_heirlooms.size()) {
      return false;
    }

    LOG_INFO("[HeirloomVault] Removed heirloom: {}",
             m_heirlooms[index].item.name);
    m_heirlooms.erase(m_heirlooms.begin() + static_cast<ptrdiff_t>(index));
    return true;
  }

  /// 获取所有传家宝 (只读)
  [[nodiscard]] std::span<const HeirloomData> getHeirlooms() const noexcept {
    return m_heirlooms;
  }

  /// 获取传家宝数量
  [[nodiscard]] size_t size() const noexcept { return m_heirlooms.size(); }

  /// 检查宝库是否已满
  [[nodiscard]] bool isFull() const noexcept {
    return m_heirlooms.size() >= kMaxHeirlooms;
  }

  /// 获取指定传家宝
  [[nodiscard]] const HeirloomData *getHeirloom(size_t index) const noexcept {
    if (index >= m_heirlooms.size()) {
      return nullptr;
    }
    return &m_heirlooms[index];
  }

private:
  /// 根据稀有度计算传家宝等级
  [[nodiscard]] static uint8_t calculateTier(Rarity rarity) noexcept {
    switch (rarity) {
    case Rarity::Mythic:
      return 3;
    case Rarity::Legendary:
      return 2;
    default:
      return 1;
    }
  }

  std::vector<HeirloomData> m_heirlooms;
};

} // namespace NoMoreDay
