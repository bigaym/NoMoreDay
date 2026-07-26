#include "doctest.h"

#include "engine/render/core/AdaptiveQualityController.hpp"

using namespace NoMoreDay::render::core;

namespace {

AdaptiveQualitySettings MakeEnabledSettings() {
  AdaptiveQualitySettings settings = {};
  settings.dynamicResolutionEnabled = true;
  settings.renderScaleLocked = false;
  settings.renderScale = 1.0f;
  settings.minRenderScale = 0.7f;
  settings.maxRenderScale = 1.0f;
  settings.renderScaleStep = 0.1f;
  settings.downThresholdMs = 10.0f;
  settings.upThresholdMs = 8.0f;
  settings.sustainSeconds = 1.0f;
  settings.cooldownSeconds = 0.0f;
  return settings;
}

AdaptiveQualityDecision Update(AdaptiveQualityController &controller,
                               bool valid, float p95Ms, uint64_t frameIndex,
                               double nowSeconds) {
  return controller.Update({valid, p95Ms, frameIndex}, nowSeconds);
}

} // namespace

TEST_CASE("[Unit] AdaptiveQualityController - invalid GPU data never changes scale") {
  AdaptiveQualityController controller(MakeEnabledSettings());

  const auto pending = Update(controller, false, 50.0f, 1, 10.0);
  CHECK(pending.action == AdaptiveQualityAction::Keep);
  CHECK(pending.reason == AdaptiveQualityReason::NoValidGpuSample);
  CHECK(controller.GetCurrentScale() == doctest::Approx(1.0f));

  const auto invalidValue = Update(controller, true, 0.0f, 2, 11.0);
  CHECK(invalidValue.reason == AdaptiveQualityReason::NoValidGpuSample);
  CHECK(controller.GetCurrentScale() == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] AdaptiveQualityController - sustained pressure and cooldown") {
  auto settings = MakeEnabledSettings();
  settings.cooldownSeconds = 5.0f;
  AdaptiveQualityController controller(settings);
  controller.Reset(0.0);

  CHECK(Update(controller, true, 12.0f, 1, 0.0).reason ==
        AdaptiveQualityReason::WaitingForDownWindow);
  CHECK(Update(controller, true, 12.0f, 2, 6.0).action ==
        AdaptiveQualityAction::DecreaseScale);
  CHECK(controller.GetCurrentScale() == doctest::Approx(0.9f));

  const auto cooldown = Update(controller, true, 12.0f, 3, 7.0);
  CHECK(cooldown.action == AdaptiveQualityAction::Keep);
  CHECK(cooldown.reason == AdaptiveQualityReason::Cooldown);

  CHECK(Update(controller, true, 12.0f, 4, 12.0).action ==
        AdaptiveQualityAction::DecreaseScale);
  CHECK(controller.GetCurrentScale() == doctest::Approx(0.8f));
}

TEST_CASE("[Unit] AdaptiveQualityController - floor requests feature degrade") {
  AdaptiveQualityController controller(MakeEnabledSettings());

  CHECK(Update(controller, true, 12.0f, 1, 0.0).reason ==
        AdaptiveQualityReason::WaitingForDownWindow);
  CHECK(Update(controller, true, 12.0f, 2, 1.0).action ==
        AdaptiveQualityAction::DecreaseScale);
  CHECK(Update(controller, true, 12.0f, 3, 2.0).action ==
        AdaptiveQualityAction::DecreaseScale);
  CHECK(Update(controller, true, 12.0f, 4, 3.0).action ==
        AdaptiveQualityAction::DecreaseScale);

  const auto floor = Update(controller, true, 12.0f, 5, 4.0);
  CHECK(floor.action == AdaptiveQualityAction::RequestFeatureDegrade);
  CHECK(floor.reason == AdaptiveQualityReason::FeatureDegradeAtMinimum);
  CHECK(controller.GetCurrentScale() == doctest::Approx(0.7f));

  const auto duplicate = Update(controller, true, 12.0f, 5, 20.0);
  CHECK(duplicate.reason == AdaptiveQualityReason::NoValidGpuSample);
}

TEST_CASE("[Unit] AdaptiveQualityController - recovery is hysteretic") {
  AdaptiveQualityController controller(MakeEnabledSettings());
  controller.Reset(0.0);

  CHECK(Update(controller, true, 12.0f, 1, 0.0).reason ==
        AdaptiveQualityReason::WaitingForDownWindow);
  CHECK(Update(controller, true, 12.0f, 2, 1.0).action ==
        AdaptiveQualityAction::DecreaseScale);

  CHECK(Update(controller, true, 7.0f, 3, 1.0).reason ==
        AdaptiveQualityReason::WaitingForUpWindow);
  CHECK(Update(controller, true, 7.0f, 4, 2.0).action ==
        AdaptiveQualityAction::IncreaseScale);
  CHECK(controller.GetCurrentScale() == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] AdaptiveQualityController - locked and disabled remain fixed") {
  auto settings = MakeEnabledSettings();
  settings.renderScaleLocked = true;
  AdaptiveQualityController locked(settings);
  CHECK(Update(locked, true, 50.0f, 1, 100.0).reason ==
        AdaptiveQualityReason::UserLocked);
  CHECK(locked.GetCurrentScale() == doctest::Approx(1.0f));

  settings.dynamicResolutionEnabled = false;
  settings.renderScaleLocked = false;
  AdaptiveQualityController disabled(settings);
  CHECK(Update(disabled, true, 50.0f, 1, 100.0).reason ==
        AdaptiveQualityReason::Disabled);
  CHECK(disabled.GetCurrentScale() == doctest::Approx(1.0f));
}
