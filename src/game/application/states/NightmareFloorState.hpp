// src/game/application/states/NightmareFloorState.hpp
// 无尽梦魇模式 - 层间过渡界面
#pragma once

#include "game/application/scene/State.hpp"
#include "game/application/states/LeaderboardSystem.hpp"
#include "game/systems/world/CorruptionSystem.hpp"


namespace NoMoreDay {

/// @brief 无尽梦魇层间过渡界面
/// @details 显示当前层数、腐化值、下一层难度预览
class NightmareFloorState : public IState {
public:
  using IState::IState;

  void OnEnter() override;
  void OnExit() override;
  bool OnUpdate(float dt) override;
  void OnRender() override;

  /// 半透明覆盖
  [[nodiscard]] bool IsTransparent() const override { return true; }

private:
  float m_fadeAlpha{0.0f};
  float m_displayTimer{0.0f};
  bool m_isAdvancing{false};

  // UI 常量
  static constexpr float kDisplayDuration = 3.0f; // 自动推进前的展示时间
  static constexpr float kPanelWidth = 500.0f;
  static constexpr float kPanelHeight = 350.0f;

  void renderFloorInfo();
  void renderCorruptionInfo();
  void renderDifficultyPreview();
  void renderButtons();
  void handleInput();
};

} // namespace NoMoreDay
