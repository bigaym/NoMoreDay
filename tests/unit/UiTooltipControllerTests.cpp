#include "doctest.h"

#include "game/application/ui/UiTooltipController.hpp"

namespace NoMoreDay::ui {

TEST_CASE("[Unit] UI tooltip waits before its first fade in") {
  UiTooltipController tooltip;

  tooltip.Update(42, 0.119f);
  CHECK(tooltip.State().activeTarget == 42);
  CHECK(tooltip.State().delayRemaining == doctest::Approx(0.001f));
  CHECK(tooltip.State().alpha == doctest::Approx(0.0f));

  tooltip.Update(42, 0.001f);
  CHECK(tooltip.State().delayRemaining == doctest::Approx(0.0f));
  CHECK(tooltip.State().alpha == doctest::Approx(0.0f));

  tooltip.Update(42, 0.05f);
  CHECK(tooltip.State().alpha == doctest::Approx(0.5f));
}

TEST_CASE("[Unit] UI tooltip preserves visible alpha across target changes") {
  UiTooltipController tooltip;

  tooltip.Update(10, 0.12f);
  tooltip.Update(10, 0.1f);
  CHECK(tooltip.State().alpha == doctest::Approx(1.0f));
  tooltip.MarkInitialized();

  tooltip.Update(11, 0.0f);
  CHECK(tooltip.State().activeTarget == 11);
  CHECK(tooltip.State().delayRemaining == doctest::Approx(0.05f));
  CHECK(tooltip.State().alpha == doctest::Approx(1.0f));
  CHECK_FALSE(tooltip.State().initialized);

  tooltip.Update(11, 0.05f);
  CHECK(tooltip.State().alpha == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] UI tooltip delays then fades out after hover exit") {
  UiTooltipController tooltip;

  tooltip.Update(10, 0.12f);
  tooltip.Update(10, 0.1f);
  CHECK(tooltip.State().alpha == doctest::Approx(1.0f));

  tooltip.Update(kInvalidUiId, 0.0f);
  CHECK(tooltip.State().delayRemaining == doctest::Approx(0.08f));
  tooltip.Update(kInvalidUiId, 0.08f);
  CHECK(tooltip.State().alpha == doctest::Approx(1.0f));
  tooltip.Update(kInvalidUiId, 0.05f);
  CHECK(tooltip.State().alpha == doctest::Approx(0.6f));
  tooltip.Update(kInvalidUiId, 0.075f);
  CHECK(tooltip.State().alpha == doctest::Approx(0.0f));
  CHECK(tooltip.State().activeTarget == kInvalidUiId);
}

} // namespace NoMoreDay::ui
