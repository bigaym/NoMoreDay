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
