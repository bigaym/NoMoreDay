#include "game/application/ui/UiTooltipController.hpp"

#include <algorithm>

namespace NoMoreDay::ui {

UiTooltipController::UiTooltipController(UiTooltipTiming timing) noexcept
    : m_timing(timing) {}

void UiTooltipController::Reset() noexcept { m_state = {}; }

void UiTooltipController::Update(UiId hoveredTarget,
                                 float deltaSeconds) noexcept {
  const float delta = std::max(0.0f, deltaSeconds);
  const bool isHovered = hoveredTarget != kInvalidUiId;
  const bool targetChanged = isHovered && hoveredTarget != m_state.activeTarget;

  if (targetChanged) {
    m_state.activeTarget = hoveredTarget;
    m_state.delayRemaining = m_state.alpha > 0.01f
                                 ? m_timing.targetChangeDelay
                                 : m_timing.initialDelay;
    m_state.initialized = false;
  }

  if (isHovered) {
    if (m_state.delayRemaining > 0.0f) {
      m_state.delayRemaining = std::max(0.0f, m_state.delayRemaining - delta);
    } else {
      m_state.alpha =
          std::min(1.0f, m_state.alpha + delta * m_timing.fadeInPerSecond);
    }
  } else {
    if (m_state.hoveredLastFrame) {
      m_state.delayRemaining = m_timing.exitDelay;
    }

    if (m_state.delayRemaining > 0.0f) {
      m_state.delayRemaining = std::max(0.0f, m_state.delayRemaining - delta);
    } else if (m_state.alpha > 0.0f) {
      m_state.alpha =
          std::max(0.0f, m_state.alpha - delta * m_timing.fadeOutPerSecond);
    }

    if (m_state.alpha <= 0.0f && m_state.delayRemaining <= 0.0f) {
      m_state.activeTarget = kInvalidUiId;
      m_state.initialized = false;
    }
  }

  m_state.hoveredLastFrame = isHovered;
}

void UiTooltipController::MarkInitialized() noexcept {
  m_state.initialized = true;
}

const UiTooltipTiming &UiTooltipController::Timing() const noexcept {
  return m_timing;
}

const UiTooltipState &UiTooltipController::State() const noexcept {
  return m_state;
}

} // namespace NoMoreDay::ui
