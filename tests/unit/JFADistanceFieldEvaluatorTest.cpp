#include "doctest.h"

#include "engine/render/gi/JFADistanceFieldEvaluator.hpp"

#include <vector>

namespace {

size_t ToIndex(int x, int y, int width) {
  return static_cast<size_t>(y) * static_cast<size_t>(width) +
         static_cast<size_t>(x);
}

} // namespace

TEST_CASE("[Unit] JFADistanceFieldEvaluator - Full-res JFA accuracy envelope") {
  using NoMoreDay::render::gi::JFADistanceFieldEvaluator;

  constexpr int kWidth = 32;
  constexpr int kHeight = 32;
  std::vector<uint8_t> occluderMask(static_cast<size_t>(kWidth) *
                                    static_cast<size_t>(kHeight), 0u);

  for (int y = 10; y < 22; ++y) {
    for (int x = 10; x < 22; ++x) {
      occluderMask[ToIndex(x, y, kWidth)] = 1u;
    }
  }

  const auto exactField = JFADistanceFieldEvaluator::BuildExactSignedDistanceField(
      occluderMask, kWidth, kHeight);
  const auto jfaField =
      JFADistanceFieldEvaluator::BuildApproximateJfaDistanceField(
          occluderMask, kWidth, kHeight, true, false);

  REQUIRE(exactField.size() == occluderMask.size());
  REQUIRE(jfaField.size() == occluderMask.size());

  const auto stats =
      JFADistanceFieldEvaluator::ComputeErrorStats(exactField, jfaField);
  CHECK(stats.sampleCount == static_cast<uint32_t>(occluderMask.size()));
  CHECK(stats.p95 <= 2.0f);
  CHECK(stats.max <= 4.0f);

  CHECK(exactField[ToIndex(16, 16, kWidth)] <= 0.0f);
  CHECK(exactField[ToIndex(0, 0, kWidth)] >= 0.0f);
}

TEST_CASE("[Unit] JFADistanceFieldEvaluator - Fallback and boundary jitter helpers") {
  using NoMoreDay::render::gi::DistanceFieldErrorStats;
  using NoMoreDay::render::gi::JFADistanceFieldEvaluator;

  DistanceFieldErrorStats inRange = {};
  inRange.sampleCount = 64u;
  inRange.p95 = 1.5f;
  inRange.max = 3.0f;
  CHECK(!JFADistanceFieldEvaluator::NeedsJfaPlus2Fallback(inRange));

  DistanceFieldErrorStats outOfRange = {};
  outOfRange.sampleCount = 64u;
  outOfRange.p95 = 2.5f;
  outOfRange.max = 3.5f;
  CHECK(JFADistanceFieldEvaluator::NeedsJfaPlus2Fallback(outOfRange));

  const std::vector<float> previous = {-2.0f, -0.4f, 0.2f, 2.0f};
  const std::vector<float> current = {-2.0f, 0.3f, 0.1f, 2.0f};
  const float jitter = JFADistanceFieldEvaluator::ComputeBoundaryJitter(
      previous, current, 0.5f);
  CHECK(jitter == doctest::Approx(0.7f));
}

TEST_CASE("[Unit] JFADistanceFieldEvaluator - Phase 1 Dirty Decision Contracts") {
  using namespace NoMoreDay::render::gi;

  // Task 1.2: Check MaxGiSdfInfluencePixels
  CHECK(JFADistanceFieldEvaluator::MaxGiSdfInfluencePixels(false) == 64);
  CHECK(JFADistanceFieldEvaluator::MaxGiSdfInfluencePixels(true) == 32);

  JFAViewKey viewKeyA{.cameraVersion = 1, .staticContentVersion = 1, .qualityTier = 1, .width = 1920, .height = 1080, .halfResolution = false};
  JFAViewKey viewKeyB = viewKeyA;

  // Task 1.1 & 1.3: Safe incremental update decision
  DecideUpdateParams params;
  params.previousViewKey = viewKeyA;
  params.currentViewKey = viewKeyB;
  params.previousOccluderBounds = JFARect{100, 100, 150, 150};
  params.currentOccluderBounds = JFARect{105, 105, 155, 155};
  params.occluderCountChanged = false;
  params.hasValidSeedContext = true;

  JFAUpdateDecision decision = JFADistanceFieldEvaluator::DecideUpdate(params);
  CHECK(decision.mode == JFAUpdateMode::Incremental);
  CHECK(decision.dirtyRect == JFARect{100, 100, 155, 155});
  CHECK(decision.expandedRect == JFARect{36, 36, 219, 219});
  CHECK(decision.fullReason.empty());

  // Task 1.4: View/Camera version change -> Full fallback
  params.currentViewKey.cameraVersion = 2;
  decision = JFADistanceFieldEvaluator::DecideUpdate(params);
  CHECK(decision.mode == JFAUpdateMode::Full);
  CHECK(decision.fullReason == JFAFullReasons::kViewOrStaticChanged);

  // Resize / Tier change -> Full fallback
  params.currentViewKey = viewKeyA;
  params.currentViewKey.width = 1280;
  decision = JFADistanceFieldEvaluator::DecideUpdate(params);
  CHECK(decision.mode == JFAUpdateMode::Full);
  CHECK(decision.fullReason == JFAFullReasons::kResizeOrScaleChanged);

  // Boundary touch -> Full fallback
  params.currentViewKey = viewKeyA;
  params.previousOccluderBounds = JFARect{10, 10, 50, 50};
  params.currentOccluderBounds = JFARect{10, 10, 50, 50};
  decision = JFADistanceFieldEvaluator::DecideUpdate(params);
  CHECK(decision.mode == JFAUpdateMode::Full);
  CHECK(decision.fullReason == JFAFullReasons::kUnsafeRegion);

  // Missing seed context -> Full fallback
  params.previousOccluderBounds = JFARect{100, 100, 150, 150};
  params.currentOccluderBounds = JFARect{105, 105, 155, 155};
  params.hasValidSeedContext = false;
  decision = JFADistanceFieldEvaluator::DecideUpdate(params);
  CHECK(decision.mode == JFAUpdateMode::Full);
  CHECK(decision.fullReason == JFAFullReasons::kMissingBoundaryContext);

  // Occluder count changed / unbounded -> Full fallback
  params.hasValidSeedContext = true;
  params.occluderCountChanged = true;
  decision = JFADistanceFieldEvaluator::DecideUpdate(params);
  CHECK(decision.mode == JFAUpdateMode::Full);
  CHECK(decision.fullReason == JFAFullReasons::kOccluderDeletedOrUnbounded);
}

TEST_CASE("[Unit] JFADistanceFieldEvaluator - Phase 3 100-Step Dynamic Property Test") {
  using namespace NoMoreDay::render::gi;

  constexpr int kWidth = 64;
  constexpr int kHeight = 64;
  std::vector<uint8_t> prevMask(static_cast<size_t>(kWidth * kHeight), 0u);
  std::vector<uint8_t> currMask = prevMask;

  int occluderX = 20;
  int occluderY = 20;
  constexpr int kSize = 8;

  auto drawBox = [&](std::vector<uint8_t> &mask, int bx, int by, uint8_t val) {
    for (int y = by; y < by + kSize; ++y) {
      for (int x = bx; x < bx + kSize; ++x) {
        if (x >= 0 && x < kWidth && y >= 0 && y < kHeight) {
          mask[static_cast<size_t>(y * kWidth + x)] = val;
        }
      }
    }
  };

  drawBox(prevMask, occluderX, occluderY, 1u);

  for (int step = 0; step < 100; ++step) {
    currMask.assign(kWidth * kHeight, 0u);
    int prevX = occluderX;
    int prevY = occluderY;

    if (step % 3 == 0) {
      // Move right/down
      occluderX = (occluderX + 1) % (kWidth - kSize - 10);
      occluderY = (occluderY + 1) % (kHeight - kSize - 10);
    } else if (step % 3 == 1) {
      // Add extra box
      drawBox(currMask, 40, 40, 1u);
    }

    drawBox(currMask, occluderX, occluderY, 1u);

    IncrementalJfaParams incParams{
        .previousMask = prevMask,
        .currentMask = currMask,
        .width = kWidth,
        .height = kHeight,
        .previousBounds = JFARect{prevX, prevY, prevX + kSize, prevY + kSize},
        .currentBounds = JFARect{occluderX, occluderY, occluderX + kSize, occluderY + kSize},
        .occluderCountChanged = (step % 3 == 1),
        .enableCompensation = true,
        .enableFallbackPlus2 = false,
        .p95Threshold = 2.0f,
        .maxThreshold = 4.0f,
    };

    IncrementalJfaResult res = JFADistanceFieldEvaluator::BuildIncrementalJfaDistanceField(incParams);
    CHECK(res.stats.p95 <= 2.0f);
    CHECK(res.stats.max <= 4.0f);
    CHECK(!res.verificationFailed);

    prevMask = currMask;
  }
}



