// src/game/components/HeirloomComponent.hpp
// 传家宝系统 - 标记装备为跨轮回继承的传家宝
#pragma once

#include <chrono>
#include <cstdint>
#include <string>


namespace NoMoreDay {

/// @brief 标记装备为传家宝的 ECS 组件
/// @details 传家宝装备可以跨存档继承，但在低等级时属性会动态压缩
struct HeirloomComponent {
  /// 传家宝等级 (用于分类展示，如 Tier 1, 2, 3)
  uint8_t tier{1};

  /// 原始装备等级要求 (属性压缩参考)
  uint8_t original_level_requirement{1};

  /// 传家宝创建时间戳 (用于 UI 排序)
  int64_t created_timestamp{0};

  /// 是否已在当前轮回激活 (每局只能使用一个传家宝)
  bool is_active_this_run{false};

  /// 传家宝名称 (用于展示，可自定义)
  std::string display_name{};

  /// 原装备稀有度 (用于视觉效果)
  uint8_t original_rarity{
      0}; // 0=Common, 1=Magic, 2=Rare, 3=Epic, 4=Legendary, 5=Mythic
};

} // namespace NoMoreDay
