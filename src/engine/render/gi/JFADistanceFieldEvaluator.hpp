#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace NoMoreDay::render::gi {

struct DistanceFieldErrorStats {
  float rms = 0.0f;
  float p95 = 0.0f;
  float max = 0.0f;
  uint32_t sampleCount = 0;
};

class JFADistanceFieldEvaluator final {
public:
  [[nodiscard]] static std::vector<float>
  BuildExactSignedDistanceField(std::span<const uint8_t> occluderMask,
                                int width, int height);

  [[nodiscard]] static std::vector<float>
  BuildApproximateJfaDistanceField(std::span<const uint8_t> occluderMask,
                                   int width, int height,
                                   bool enableCompensation,
                                   bool enableFallbackPlus2);

  [[nodiscard]] static DistanceFieldErrorStats
  ComputeErrorStats(std::span<const float> reference,
                    std::span<const float> candidate);

  [[nodiscard]] static bool NeedsJfaPlus2Fallback(
      const DistanceFieldErrorStats &stats, float p95Threshold = 2.0f,
      float maxThreshold = 4.0f);

  [[nodiscard]] static float ComputeBoundaryJitter(
      std::span<const float> previousField, std::span<const float> currentField,
      float boundaryThreshold = 0.5f);
};

} // namespace NoMoreDay::render::gi
