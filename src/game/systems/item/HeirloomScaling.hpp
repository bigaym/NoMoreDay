// src/game/systems/item/HeirloomScaling.hpp
// 传家宝属性缩放系统 - 基于角色等级动态压缩传家宝属性
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>


namespace NoMoreDay {

/// @brief 传家宝属性缩放计算器
/// @details 在低等级时压缩传家宝属性，随着角色等级提升逐渐恢复全属性
///          公式: scaling = min_scale + (1 - min_scale) * (player_level /
///          item_level)^exponent
struct HeirloomScaling {
  /// 最低属性缩放比例 (防止传家宝完全无用)
  static constexpr float kMinScale = 0.15f;

  /// 缩放曲线指数 (越高则前期越弱，后期恢复越快)
  static constexpr float kScaleExponent = 0.8f;

  /// 计算属性缩放系数
  /// @param player_level 当前角色等级
  /// @param item_original_level 装备原始等级要求
  /// @return 属性倍率 [kMinScale, 1.0]
  [[nodiscard]] static constexpr float
  calculateScalingFactor(uint8_t player_level,
                         uint8_t item_original_level) noexcept {
    if (item_original_level == 0) {
      return 1.0f; // 无等级要求的物品不受影响
    }

    if (player_level >= item_original_level) {
      return 1.0f; // 等级达到，恢复全属性
    }

    // 非线性压缩曲线: min + (1 - min) * (level / req)^exp
    const float ratio = static_cast<float>(player_level) /
                        static_cast<float>(item_original_level);

    // 使用 power curve 实现平滑过渡
    const float scaled_ratio = std::pow(ratio, kScaleExponent);

    return kMinScale + (1.0f - kMinScale) * scaled_ratio;
  }

  /// 应用缩放到整数属性值
  /// @tparam T 整数类型 (int, uint32_t, etc.)
  /// @param original_value 原始属性值
  /// @param scaling_factor 缩放系数 (来自 calculateScalingFactor)
  /// @return 压缩后的属性值，至少为 1
  template <typename T>
  [[nodiscard]] static constexpr T
  applyScalingInt(T original_value, float scaling_factor) noexcept {
    const T scaled = static_cast<T>(original_value * scaling_factor);
    return std::max(scaled, static_cast<T>(1)); // 至少为 1
  }

  /// 应用缩放到浮点属性值
  /// @param original_value 原始属性值
  /// @param scaling_factor 缩放系数
  /// @return 压缩后的属性值
  [[nodiscard]] static constexpr float
  applyScalingFloat(float original_value, float scaling_factor) noexcept {
    return original_value * scaling_factor;
  }

  /// 计算当前等级下的有效 DPS 倍率
  /// 用于快速预览传家宝在不同等级下的战力
  /// @param player_level 角色等级
  /// @param item_level 物品等级
  /// @return 有效 DPS 相对于满级的百分比 [15%, 100%]
  [[nodiscard]] static constexpr float
  calculateEffectivePowerPercent(uint8_t player_level,
                                 uint8_t item_level) noexcept {
    return calculateScalingFactor(player_level, item_level) * 100.0f;
  }
};

} // namespace NoMoreDay
