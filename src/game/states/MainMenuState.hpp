#pragma once

#include "engine/scene/State.hpp"
#include <raylib.h>
#include <string>

namespace NoMoreDay {

class MainMenuState : public IState {
public:
  MainMenuState(StateManager &manager, SharedContext &context);
  virtual ~MainMenuState() = default;

  void OnEnter() override;
  void OnExit() override;

  bool OnUpdate(float dt) override;
  void OnRender() override;

private:
  struct Button {
    Rectangle bounds;
    std::string text;
    bool hovered;
  };

  void DrawButton(const Button &btn, bool enabled = true);
  bool IsButtonClicked(const Button &btn);

  Button m_startButton;
  Button m_continueButton;
  Button m_exitButton;

  bool m_hasSave = false;

  Font m_titleFont;
  float m_titleOpacity = 0.0f;
};

} // namespace NoMoreDay
