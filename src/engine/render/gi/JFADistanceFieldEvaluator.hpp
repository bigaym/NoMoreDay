#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace NoMoreDay::render::gi {

struct DistanceFieldErrorStats {
  float rms = 0.0f;
  float p95 = 0.0f;
  float max = 0.0f;
  uint32_t sampleCount = 0;
};

struct JFARect {
  int minX = 0;
  int minY = 0;
  int maxX = 0;
  int maxY = 0;

  [[nodiscard]] constexpr bool IsEmpty() const noexcept {
    return minX >= maxX || minY >= maxY;
  }

  [[nodiscard]] constexpr int Width() const noexcept {
    return IsEmpty() ? 0 : (maxX - minX);
  }

  [[nodiscard]] constexpr int Height() const noexcept {
    return IsEmpty() ? 0 : (maxY - minY);
  }

  [[nodiscard]] constexpr int Area() const noexcept {
    return Width() * Height();
  }

  [[nodiscard]] constexpr JFARect Union(const JFARect &other) const noexcept {
    if (IsEmpty()) return other;
    if (other.IsEmpty()) return *this;
    return JFARect{
        std::min(minX, other.minX),
        std::min(minY, other.minY),
        std::max(maxX, other.maxX),
        std::max(maxY, other.maxY)};
  }

  [[nodiscard]] constexpr JFARect Expand(int margin, int clampW, int clampH) const noexcept {
    if (IsEmpty()) return *this;
    return JFARect{
        std::max(0, minX - margin),
        std::max(0, minY - margin),
        std::min(clampW, maxX + margin),
        std::min(clampH, maxY + margin)};
  }

  [[nodiscard]] constexpr bool TouchesBoundary(int clampW, int clampH) const noexcept {
    if (IsEmpty()) return false;
    return minX <= 0 || minY <= 0 || maxX >= clampW || maxY >= clampH;
  }

  constexpr bool operator==(const JFARect &rhs) const noexcept = default;
};

struct JFAViewKey {
  uint32_t cameraVersion = 0;
  uint32_t staticContentVersion = 0;
  uint32_t qualityTier = 0;
  int width = 0;
  int height = 0;
  bool halfResolution = false;

  constexpr bool operator==(const JFAViewKey &rhs) const noexcept = default;
  constexpr bool operator!=(const JFAViewKey &rhs) const noexcept = default;
};

enum class JFAUpdateMode {
  Full,
  Incremental,
  Skip,
  Revert
};



namespace JFAFullReasons {
constexpr const char *kViewOrStaticChanged = "view-or-static-change";
constexpr const char *kUnsafeRegion = "unsafe-region";
constexpr const char *kMissingBoundaryContext = "missing-boundary-context";
constexpr const char *kVerificationFull = "verification-full";
constexpr const char *kResizeOrScaleChanged = "resize-or-scale-changed";
constexpr const char *kOccluderDeletedOrUnbounded = "occluder-deleted-or-unbounded";
constexpr const char *kAreaExceedsThreshold = "area-exceeds-threshold";
} // namespace JFAFullReasons

struct JFAUpdateDecision {
  JFAUpdateMode mode = JFAUpdateMode::Full;
  JFARect dirtyRect = {};
  JFARect expandedRect = {};
  std::string fullReason;
};


struct DecideUpdateParams {
  JFAViewKey previousViewKey = {};
  JFAViewKey currentViewKey = {};
  JFARect previousOccluderBounds = {};
  JFARect currentOccluderBounds = {};
  bool occluderCountChanged = false;
  bool hasValidSeedContext = true;
  float maxAreaFractionThreshold = 0.5f;
};

struct IncrementalJfaParams {
  std::span<const uint8_t> previousMask;
  std::span<const uint8_t> currentMask;
  int width = 0;
  int height = 0;
  JFARect previousBounds = {};
  JFARect currentBounds = {};
  bool occluderCountChanged = false;
  bool enableCompensation = true;
  bool enableFallbackPlus2 = false;
  float p95Threshold = 2.0f;
  float maxThreshold = 4.0f;
};

struct IncrementalJfaResult {
  std::vector<float> field;
  JFAUpdateDecision decision;
  DistanceFieldErrorStats stats;
  bool verificationFailed = false;
};

class JFADistanceFieldEvaluator final {
public:
  [[nodiscard]] static int MaxGiSdfInfluencePixels(bool halfResolution) noexcept;

  [[nodiscard]] static JFAUpdateDecision DecideUpdate(const DecideUpdateParams &params);

  [[nodiscard]] static std::vector<float>
  BuildExactSignedDistanceField(std::span<const uint8_t> occluderMask,
                                int width, int height);

  [[nodiscard]] static std::vector<float>
  BuildApproximateJfaDistanceField(std::span<const uint8_t> occluderMask,
                                   int width, int height,
                                   bool enableCompensation,
                                   bool enableFallbackPlus2);

  [[nodiscard]] static IncrementalJfaResult
  BuildIncrementalJfaDistanceField(const IncrementalJfaParams &params);

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

