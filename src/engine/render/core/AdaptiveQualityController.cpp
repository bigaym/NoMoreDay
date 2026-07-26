#include "engine/render/core/AdaptiveQualityController.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace NoMoreDay::render::core {

namespace {

constexpr float kScaleEpsilon = 0.0001f;

} // namespace

AdaptiveQualityController::AdaptiveQualityController(
    AdaptiveQualitySettings settings) {
  Configure(settings);
}

AdaptiveQualitySettings AdaptiveQualityController::Normalize(
    AdaptiveQualitySettings settings) {
  settings.minRenderScale = std::clamp(settings.minRenderScale, 0.1f, 1.0f);
  settings.maxRenderScale = std::clamp(settings.maxRenderScale,
                                       settings.minRenderScale, 1.0f);
  settings.renderScale = std::clamp(settings.renderScale,
                                    settings.minRenderScale,
                                    settings.maxRenderScale);
  settings.renderScaleStep = std::clamp(settings.renderScaleStep, 0.001f,
                                        settings.maxRenderScale -
                                            settings.minRenderScale + 0.001f);
  settings.sustainSeconds = std::max(settings.sustainSeconds, 0.0f);
  settings.cooldownSeconds = std::max(settings.cooldownSeconds, 0.0f);
  settings.exposure = std::max(settings.exposure, 0.001f);
  settings.minExposure = std::clamp(settings.minExposure, 0.001f, 32.0f);
  settings.maxExposure = std::clamp(settings.maxExposure, settings.minExposure,
                                    32.0f);
  settings.exposure = std::clamp(settings.exposure, settings.minExposure,
                                 settings.maxExposure);
  settings.brightenRate = std::max(settings.brightenRate, 0.0f);
  settings.darkenRate = std::max(settings.darkenRate, 0.0f);
  return settings;
}

float AdaptiveQualityController::ClampScale(
    float scale, const AdaptiveQualitySettings &settings) {
  return std::clamp(scale, settings.minRenderScale, settings.maxRenderScale);
}

void AdaptiveQualityController::Configure(AdaptiveQualitySettings settings) {
  m_settings = Normalize(settings);
  m_currentScale = m_settings.renderScale;
  m_overBudgetSince = -1.0;
  m_underBudgetSince = -1.0;
  m_lastTransitionAt = 0.0;
  m_lastSampleFrameIndex = 0;
  m_featureDegradeRequested = false;
}

void AdaptiveQualityController::Reset(double nowSeconds) {
  m_overBudgetSince = -1.0;
  m_underBudgetSince = -1.0;
  m_lastTransitionAt = nowSeconds;
  m_lastSampleFrameIndex = 0;
  m_featureDegradeRequested = false;
}

AdaptiveQualityDecision AdaptiveQualityController::Update(
    const AdaptiveQualityGpuWindow &sample, double nowSeconds) {
  AdaptiveQualityDecision decision = {};
  decision.previousScale = m_currentScale;
  decision.newScale = m_currentScale;
  decision.sampleFrameIndex = sample.frameIndex;

  if (!m_settings.dynamicResolutionEnabled) {
    decision.reason = AdaptiveQualityReason::Disabled;
    return decision;
  }
  if (m_settings.renderScaleLocked) {
    decision.reason = AdaptiveQualityReason::UserLocked;
    return decision;
  }
  if (!sample.valid || !std::isfinite(sample.p95Ms) || sample.p95Ms <= 0.0f ||
      (sample.frameIndex != 0 && sample.frameIndex == m_lastSampleFrameIndex)) {
    decision.reason = AdaptiveQualityReason::NoValidGpuSample;
    return decision;
  }

  if (sample.frameIndex != 0) {
    m_lastSampleFrameIndex = sample.frameIndex;
  }

  const bool overBudget = m_settings.downThresholdMs > 0.0f &&
                          sample.p95Ms > m_settings.downThresholdMs;
  const bool underBudget = m_settings.upThresholdMs > 0.0f &&
                           sample.p95Ms < m_settings.upThresholdMs;
  if (overBudget) {
    if (m_overBudgetSince < 0.0) {
      m_overBudgetSince = nowSeconds;
    }
    m_underBudgetSince = -1.0;
    if ((nowSeconds - m_overBudgetSince) <
        static_cast<double>(m_settings.sustainSeconds)) {
      decision.reason = AdaptiveQualityReason::WaitingForDownWindow;
      return decision;
    }
    if ((nowSeconds - m_lastTransitionAt) <
        static_cast<double>(m_settings.cooldownSeconds)) {
      decision.reason = AdaptiveQualityReason::Cooldown;
      return decision;
    }

    if (m_currentScale > m_settings.minRenderScale + kScaleEpsilon) {
      m_currentScale = ClampScale(m_currentScale - m_settings.renderScaleStep,
                                  m_settings);
      m_lastTransitionAt = nowSeconds;
      m_featureDegradeRequested = false;
      decision.action = AdaptiveQualityAction::DecreaseScale;
      decision.reason = AdaptiveQualityReason::ScaleDecreased;
      decision.newScale = m_currentScale;
      m_overBudgetSince = nowSeconds;
      return decision;
    }

    decision.reason = AdaptiveQualityReason::ScaleAtMinimum;
    if (!m_featureDegradeRequested) {
      m_featureDegradeRequested = true;
      m_lastTransitionAt = nowSeconds;
      decision.action = AdaptiveQualityAction::RequestFeatureDegrade;
      decision.reason = AdaptiveQualityReason::FeatureDegradeAtMinimum;
    }
    return decision;
  }

  if (underBudget) {
    if (m_underBudgetSince < 0.0) {
      m_underBudgetSince = nowSeconds;
    }
    m_overBudgetSince = -1.0;
    if ((nowSeconds - m_underBudgetSince) <
        static_cast<double>(m_settings.sustainSeconds)) {
      decision.reason = AdaptiveQualityReason::WaitingForUpWindow;
      return decision;
    }
    if ((nowSeconds - m_lastTransitionAt) <
        static_cast<double>(m_settings.cooldownSeconds)) {
      decision.reason = AdaptiveQualityReason::Cooldown;
      return decision;
    }

    if (m_currentScale + kScaleEpsilon < m_settings.maxRenderScale) {
      m_currentScale = ClampScale(m_currentScale + m_settings.renderScaleStep,
                                  m_settings);
      m_lastTransitionAt = nowSeconds;
      m_featureDegradeRequested = false;
      decision.action = AdaptiveQualityAction::IncreaseScale;
      decision.reason = AdaptiveQualityReason::ScaleIncreased;
      decision.newScale = m_currentScale;
      m_underBudgetSince = nowSeconds;
      return decision;
    }

    decision.reason = AdaptiveQualityReason::ScaleAtMaximum;
    return decision;
  }

  m_overBudgetSince = -1.0;
  m_underBudgetSince = -1.0;
  decision.reason = AdaptiveQualityReason::Stable;
  return decision;
}

const char *ToString(AdaptiveQualityAction action) {
  switch (action) {
  case AdaptiveQualityAction::Keep:
    return "keep";
  case AdaptiveQualityAction::DecreaseScale:
    return "decrease_scale";
  case AdaptiveQualityAction::IncreaseScale:
    return "increase_scale";
  case AdaptiveQualityAction::RequestFeatureDegrade:
    return "request_feature_degrade";
  }
  return "unknown";
}

const char *ToString(AdaptiveQualityReason reason) {
  switch (reason) {
  case AdaptiveQualityReason::Stable:
    return "stable";
  case AdaptiveQualityReason::Disabled:
    return "disabled";
  case AdaptiveQualityReason::UserLocked:
    return "user_locked";
  case AdaptiveQualityReason::NoValidGpuSample:
    return "no_valid_gpu_sample";
  case AdaptiveQualityReason::WaitingForDownWindow:
    return "waiting_for_down_window";
  case AdaptiveQualityReason::WaitingForUpWindow:
    return "waiting_for_up_window";
  case AdaptiveQualityReason::Cooldown:
    return "cooldown";
  case AdaptiveQualityReason::ScaleAtMinimum:
    return "scale_at_minimum";
  case AdaptiveQualityReason::ScaleAtMaximum:
    return "scale_at_maximum";
  case AdaptiveQualityReason::ScaleDecreased:
    return "scale_decreased";
  case AdaptiveQualityReason::ScaleIncreased:
    return "scale_increased";
  case AdaptiveQualityReason::FeatureDegradeAtMinimum:
    return "feature_degrade_at_minimum";
  }
  return "unknown";
}

} // namespace NoMoreDay::render::core
