// src/game/states/HeirloomVaultState.hpp
// 传家宝宝库界面 - 展示和选择跨轮回继承的装备
#pragma once

#include "game/scene/State.hpp"
#include "game/systems/item/HeirloomVault.hpp"
#include <optional>

namespace NoMoreDay {

/// @brief 传家宝宝库选择界面
/// @details 在游戏开始前展示可用的传家宝，允许玩家选择一件带入新游戏
class HeirloomVaultState : public IState {
public:
  using IState::IState;

  void OnEnter() override;
  void OnExit() override;
  bool OnUpdate(float dt) override;
  void OnRender() override;

  /// 宝库界面是半透明覆盖层
  [[nodiscard]] bool IsTransparent() const override { return true; }

  /// 获取玩家选择的传家宝索引 (如果有)
  [[nodiscard]] std::optional<size_t> getSelectedIndex() const noexcept {
    return m_confirmedSelection;
  }

private:
  int m_hoveredIndex{-1};
  int m_selectedIndex{-1};
  std::optional<size_t> m_confirmedSelection{std::nullopt};

  float m_scrollOffset{0.0f};
  float m_fadeAlpha{0.0f};

  // UI 常量
  static constexpr float kSlotWidth = 300.0f;
  static constexpr float kSlotHeight = 100.0f;
  static constexpr float kSlotPadding = 10.0f;
  static constexpr float kPanelWidth = 400.0f;
  static constexpr float kPanelMargin = 50.0f;

  void renderSlot(const HeirloomData &data, int index, float x, float y);
  void renderTooltip(const HeirloomData &data);
  void handleInput();
};

} // namespace NoMoreDay
