#pragma once

#include "engine/render/core/RenderConstants.hpp"

#include <cstdint>

namespace NoMoreDay::render::core {

enum class AdaptiveQualityAction : uint8_t {
  Keep = 0,
  DecreaseScale,
  IncreaseScale,
  RequestFeatureDegrade,
};

enum class AdaptiveQualityReason : uint8_t {
  Stable = 0,
  Disabled,
  UserLocked,
  NoValidGpuSample,
  WaitingForDownWindow,
  WaitingForUpWindow,
  Cooldown,
  ScaleAtMinimum,
  ScaleAtMaximum,
  ScaleDecreased,
  ScaleIncreased,
  FeatureDegradeAtMinimum,
};

struct AdaptiveQualityGpuWindow {
  bool valid = false;
  float p95Ms = 0.0f;
  uint64_t frameIndex = 0;
};

struct AdaptiveQualityDecision {
  AdaptiveQualityAction action = AdaptiveQualityAction::Keep;
  AdaptiveQualityReason reason = AdaptiveQualityReason::Stable;
  float previousScale = 1.0f;
  float newScale = 1.0f;
  uint64_t sampleFrameIndex = 0;
};

class AdaptiveQualityController {
public:
  explicit AdaptiveQualityController(
      AdaptiveQualitySettings settings = AdaptiveQualitySettings{});

  void Configure(AdaptiveQualitySettings settings);
  void Reset(double nowSeconds);

  [[nodiscard]] AdaptiveQualityDecision Update(
      const AdaptiveQualityGpuWindow &sample, double nowSeconds);

  [[nodiscard]] float GetCurrentScale() const { return m_currentScale; }
  [[nodiscard]] const AdaptiveQualitySettings &GetSettings() const {
    return m_settings;
  }

private:
  static AdaptiveQualitySettings Normalize(AdaptiveQualitySettings settings);
  static float ClampScale(float scale, const AdaptiveQualitySettings &settings);

  AdaptiveQualitySettings m_settings = {};
  float m_currentScale = 1.0f;
  double m_overBudgetSince = -1.0;
  double m_underBudgetSince = -1.0;
  double m_lastTransitionAt = 0.0;
  uint64_t m_lastSampleFrameIndex = 0;
  bool m_featureDegradeRequested = false;
};

const char *ToString(AdaptiveQualityAction action);
const char *ToString(AdaptiveQualityReason reason);

} // namespace NoMoreDay::render::core
