#include "doctest.h"

#include "engine/render/passes/JFAPass.hpp"

#include <vector>

TEST_CASE("[Unit] JFAPass production policy defaults incremental decisions to full") {
  using namespace NoMoreDay::render;

  gi::JFAUpdateDecision decision{};
  decision.mode = gi::JFAUpdateMode::Incremental;
  decision.dirtyRect = gi::JFARect{10, 10, 20, 20};

  const auto productionDecision =
      passes::JFAPass::ApplyProductionUpdatePolicy(decision, false);
  CHECK(productionDecision.mode == gi::JFAUpdateMode::Full);
  CHECK(productionDecision.fullReason == gi::JFAFullReasons::kProductionDefaultFull);
  CHECK(productionDecision.dirtyRect == decision.dirtyRect);
}

TEST_CASE("[Unit] JFAPass production policy preserves explicit incremental opt-in") {
  using namespace NoMoreDay::render;

  gi::JFAUpdateDecision decision{};
  decision.mode = gi::JFAUpdateMode::Incremental;

  const auto optInDecision =
      passes::JFAPass::ApplyProductionUpdatePolicy(decision, true);
  CHECK(optInDecision.mode == gi::JFAUpdateMode::Incremental);
}

TEST_CASE("[Unit] JFA verification rejects a mismatching incremental artifact") {
  using namespace NoMoreDay::render::gi;

  constexpr int kWidth = 16;
  constexpr int kHeight = 16;
  std::vector<uint8_t> mask(static_cast<size_t>(kWidth * kHeight), 0u);
  mask[static_cast<size_t>(8 * kWidth + 8)] = 1u;
  const auto reference = JFADistanceFieldEvaluator::BuildApproximateJfaDistanceField(
      mask, kWidth, kHeight, true, false);
  auto mismatchingCandidate = reference;
  mismatchingCandidate[0] += 8.0f;

  const auto stats = JFADistanceFieldEvaluator::ComputeErrorStats(
      reference, mismatchingCandidate);
  CHECK(JFADistanceFieldEvaluator::NeedsJfaPlus2Fallback(stats, 0.5f, 2.0f));
}

TEST_CASE("[Unit] JFAPass empty scene skip and occluder count transition contracts") {
  using namespace NoMoreDay::render::gi;

  JFAViewKey viewKey{.cameraVersion = 1, .staticContentVersion = 1, .qualityTier = 1, .width = 1920, .height = 1080, .halfResolution = false};

  // Case 1: Empty scene (bounds empty, count = 0)
  DecideUpdateParams emptyParams;
  emptyParams.previousViewKey = viewKey;
  emptyParams.currentViewKey = viewKey;
  emptyParams.previousOccluderBounds = JFARect{0, 0, 0, 0};
  emptyParams.currentOccluderBounds = JFARect{0, 0, 0, 0};
  emptyParams.occluderCountChanged = false;
  emptyParams.hasValidSeedContext = true;

  JFAUpdateDecision decision = JFADistanceFieldEvaluator::DecideUpdate(emptyParams);
  CHECK(decision.mode == JFAUpdateMode::Skip);
  // Production policy preservation: Skip remains Skip
  auto prodDecision = NoMoreDay::render::passes::JFAPass::ApplyProductionUpdatePolicy(decision, false);
  CHECK(prodDecision.mode == JFAUpdateMode::Skip);

  // Case 2: Transition from empty (0) to non-empty (>0 occluders) triggers occluderCountChanged -> Full fallback
  DecideUpdateParams appearParams;
  appearParams.previousViewKey = viewKey;
  appearParams.currentViewKey = viewKey;
  appearParams.previousOccluderBounds = JFARect{0, 0, 0, 0};
  appearParams.currentOccluderBounds = JFARect{100, 100, 150, 150};
  appearParams.occluderCountChanged = true;
  appearParams.hasValidSeedContext = true;

  JFAUpdateDecision appearDecision = JFADistanceFieldEvaluator::DecideUpdate(appearParams);
  CHECK(appearDecision.mode == JFAUpdateMode::Full);
  CHECK(appearDecision.fullReason == JFAFullReasons::kOccluderDeletedOrUnbounded);

  // Case 3: Transition from non-empty (>0) to empty (0) with occluderCountChanged -> Full fallback
  DecideUpdateParams disappearParams;
  disappearParams.previousViewKey = viewKey;
  disappearParams.currentViewKey = viewKey;
  disappearParams.previousOccluderBounds = JFARect{100, 100, 150, 150};
  disappearParams.currentOccluderBounds = JFARect{0, 0, 0, 0};
  disappearParams.occluderCountChanged = true;
  disappearParams.hasValidSeedContext = true;

  JFAUpdateDecision disappearDecision = JFADistanceFieldEvaluator::DecideUpdate(disappearParams);
  CHECK(disappearDecision.mode == JFAUpdateMode::Full);
  CHECK(disappearDecision.fullReason == JFAFullReasons::kOccluderDeletedOrUnbounded);
}

