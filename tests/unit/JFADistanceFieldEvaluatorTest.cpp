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
