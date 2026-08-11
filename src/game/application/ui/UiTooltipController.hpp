#pragma once

#include "game/application/ui/UiRuntimeTypes.hpp"

namespace NoMoreDay::ui {

struct UiTooltipTiming {
  float initialDelay = 0.12f;
  float targetChangeDelay = 0.05f;
  float exitDelay = 0.08f;
  float fadeInPerSecond = 10.0f;
  float fadeOutPerSecond = 8.0f;
};

struct UiTooltipState {
  UiId activeTarget = kInvalidUiId;
  float alpha = 0.0f;
  float delayRemaining = 0.0f;
  bool initialized = false;
  bool hoveredLastFrame = false;
};

class UiTooltipController {
public:
  explicit UiTooltipController(UiTooltipTiming timing = {}) noexcept;

  void Reset() noexcept;
  void Update(UiId hoveredTarget, float deltaSeconds) noexcept;
  void MarkInitialized() noexcept;

  [[nodiscard]] const UiTooltipTiming &Timing() const noexcept;
  [[nodiscard]] const UiTooltipState &State() const noexcept;

private:
  UiTooltipTiming m_timing;
  UiTooltipState m_state{};
};

} // namespace NoMoreDay::ui
