#include "engine/render/gi/JFADistanceFieldEvaluator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace NoMoreDay::render::gi {
namespace {

constexpr uint16_t kInvalidSeedCoord = 0xFFFFu;

struct SeedCoord {
  uint16_t x = kInvalidSeedCoord;
  uint16_t y = kInvalidSeedCoord;
};

[[nodiscard]] bool IsSeedValid(const SeedCoord &seed) noexcept {
  return seed.x != kInvalidSeedCoord && seed.y != kInvalidSeedCoord;
}

[[nodiscard]] size_t ToIndex(const int x, const int y,
                             const int width) noexcept {
  return static_cast<size_t>(y) * static_cast<size_t>(width) +
         static_cast<size_t>(x);
}

[[nodiscard]] float DistanceSquared(const int x, const int y,
                                    const SeedCoord &seed) noexcept {
  const float dx = static_cast<float>(x) - static_cast<float>(seed.x);
  const float dy = static_cast<float>(y) - static_cast<float>(seed.y);
  return (dx * dx) + (dy * dy);
}

void RunJfaStep(std::span<const SeedCoord> inputSeeds,
                std::span<SeedCoord> outputSeeds, int width, int height,
                int stepSize) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t index = ToIndex(x, y, width);
      SeedCoord bestSeed = inputSeeds[index];
      float bestDistSq =
          IsSeedValid(bestSeed)
              ? DistanceSquared(x, y, bestSeed)
              : std::numeric_limits<float>::max();

      for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
          const int sampleX = x + (offsetX * stepSize);
          const int sampleY = y + (offsetY * stepSize);
          if (sampleX < 0 || sampleX >= width || sampleY < 0 ||
              sampleY >= height) {
            continue;
          }
          const SeedCoord candidateSeed =
              inputSeeds[ToIndex(sampleX, sampleY, width)];
          if (!IsSeedValid(candidateSeed)) {
            continue;
          }
          const float candidateDistSq = DistanceSquared(x, y, candidateSeed);
          if (candidateDistSq < bestDistSq) {
            bestDistSq = candidateDistSq;
            bestSeed = candidateSeed;
          }
        }
      }

      outputSeeds[index] = bestSeed;
    }
  }
}

[[nodiscard]] std::vector<SeedCoord>
InitializeSeedBuffer(std::span<const uint8_t> occluderMask, int width,
                     int height) {
  std::vector<SeedCoord> seeds(static_cast<size_t>(width) *
                               static_cast<size_t>(height));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t index = ToIndex(x, y, width);
      if (occluderMask[index] != 0u) {
        seeds[index] = {
            .x = static_cast<uint16_t>(x),
            .y = static_cast<uint16_t>(y),
        };
      } else {
        seeds[index] = {};
      }
    }
  }
  return seeds;
}

[[nodiscard]] std::vector<float>
ResolveSignedDistanceFromSeeds(std::span<const SeedCoord> seeds,
                               std::span<const uint8_t> occluderMask, int width,
                               int height) {
  const float fallbackDistance =
      static_cast<float>(std::max(width, height) * 2);
  std::vector<float> distanceField(static_cast<size_t>(width) *
                                   static_cast<size_t>(height));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t index = ToIndex(x, y, width);
      const SeedCoord seed = seeds[index];
      float distance = fallbackDistance;
      if (IsSeedValid(seed)) {
        distance = std::sqrt(DistanceSquared(x, y, seed));
      }
      if (occluderMask[index] != 0u) {
        distance = -distance;
      }
      distanceField[index] = distance;
    }
  }
  return distanceField;
}

[[nodiscard]] int HighestPowerOfTwoLessEqual(const int value) noexcept {
  if (value <= 1) {
    return 1;
  }
  int result = 1;
  while ((result << 1) <= value) {
    result <<= 1;
  }
  return result;
}

} // namespace

std::vector<float> JFADistanceFieldEvaluator::BuildExactSignedDistanceField(
    const std::span<const uint8_t> occluderMask, const int width,
    const int height) {
  if (width <= 0 || height <= 0 ||
      occluderMask.size() != static_cast<size_t>(width) *
                                 static_cast<size_t>(height)) {
    return {};
  }

  std::vector<SeedCoord> seeds;
  seeds.reserve(occluderMask.size());
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (occluderMask[ToIndex(x, y, width)] != 0u) {
        seeds.push_back({
            .x = static_cast<uint16_t>(x),
            .y = static_cast<uint16_t>(y),
        });
      }
    }
  }

  const float fallbackDistance =
      static_cast<float>(std::max(width, height) * 2);
  std::vector<float> distanceField(static_cast<size_t>(width) *
                                   static_cast<size_t>(height));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t index = ToIndex(x, y, width);
      float distance = fallbackDistance;
      if (!seeds.empty()) {
        float minDistSq = std::numeric_limits<float>::max();
        for (const SeedCoord &seed : seeds) {
          minDistSq = std::min(minDistSq, DistanceSquared(x, y, seed));
        }
        distance = std::sqrt(minDistSq);
      }
      if (occluderMask[index] != 0u) {
        distance = -distance;
      }
      distanceField[index] = distance;
    }
  }

  return distanceField;
}

std::vector<float> JFADistanceFieldEvaluator::BuildApproximateJfaDistanceField(
    const std::span<const uint8_t> occluderMask, const int width,
    const int height, const bool enableCompensation,
    const bool enableFallbackPlus2) {
  if (width <= 0 || height <= 0 ||
      occluderMask.size() != static_cast<size_t>(width) *
                                 static_cast<size_t>(height)) {
    return {};
  }

  std::vector<SeedCoord> ping = InitializeSeedBuffer(occluderMask, width, height);
  std::vector<SeedCoord> pong = ping;

  int stepSize = HighestPowerOfTwoLessEqual(std::max(width, height));
  stepSize = std::max(1, stepSize / 2);
  while (stepSize >= 1) {
    RunJfaStep(ping, pong, width, height, stepSize);
    ping.swap(pong);
    stepSize /= 2;
  }

  if (enableCompensation) {
    if (std::max(width, height) >= 2) {
      RunJfaStep(ping, pong, width, height, 2);
      ping.swap(pong);
    }
    RunJfaStep(ping, pong, width, height, 1);
    ping.swap(pong);
  }

  if (enableFallbackPlus2) {
    if (std::max(width, height) >= 4) {
      RunJfaStep(ping, pong, width, height, 4);
      ping.swap(pong);
    }
    if (std::max(width, height) >= 2) {
      RunJfaStep(ping, pong, width, height, 2);
      ping.swap(pong);
    }
    RunJfaStep(ping, pong, width, height, 1);
    ping.swap(pong);
  }

  return ResolveSignedDistanceFromSeeds(ping, occluderMask, width, height);
}

DistanceFieldErrorStats JFADistanceFieldEvaluator::ComputeErrorStats(
    const std::span<const float> reference, const std::span<const float> candidate) {
  DistanceFieldErrorStats stats = {};
  if (reference.empty() || reference.size() != candidate.size()) {
    return stats;
  }

  std::vector<float> errors;
  errors.reserve(reference.size());

  double sumSquares = 0.0;
  float maxError = 0.0f;
  for (size_t i = 0; i < reference.size(); ++i) {
    const float error = std::fabs(reference[i] - candidate[i]);
    errors.push_back(error);
    sumSquares += static_cast<double>(error) * static_cast<double>(error);
    maxError = std::max(maxError, error);
  }

  std::sort(errors.begin(), errors.end());
  size_t p95Index = (errors.size() * 95) / 100;
  if (p95Index >= errors.size()) {
    p95Index = errors.size() - 1;
  }

  stats.sampleCount = static_cast<uint32_t>(errors.size());
  stats.rms =
      static_cast<float>(std::sqrt(sumSquares / static_cast<double>(errors.size())));
  stats.p95 = errors[p95Index];
  stats.max = maxError;
  return stats;
}

bool JFADistanceFieldEvaluator::NeedsJfaPlus2Fallback(
    const DistanceFieldErrorStats &stats, const float p95Threshold,
    const float maxThreshold) {
  if (stats.sampleCount == 0u) {
    return false;
  }
  return stats.p95 > p95Threshold || stats.max > maxThreshold;
}

float JFADistanceFieldEvaluator::ComputeBoundaryJitter(
    const std::span<const float> previousField,
    const std::span<const float> currentField, const float boundaryThreshold) {
  if (previousField.empty() || previousField.size() != currentField.size()) {
    return 0.0f;
  }

  float maxDelta = 0.0f;
  for (size_t i = 0; i < previousField.size(); ++i) {
    const bool isBoundarySample =
        std::fabs(previousField[i]) <= boundaryThreshold ||
        std::fabs(currentField[i]) <= boundaryThreshold;
    if (!isBoundarySample) {
      continue;
    }
    maxDelta = std::max(maxDelta, std::fabs(currentField[i] - previousField[i]));
  }
  return maxDelta;
}

int JFADistanceFieldEvaluator::MaxGiSdfInfluencePixels(const bool halfResolution) noexcept {
  return halfResolution ? 32 : 64;
}

JFAUpdateDecision JFADistanceFieldEvaluator::DecideUpdate(const DecideUpdateParams &params) {
  JFAUpdateDecision decision;

  if (params.previousViewKey != params.currentViewKey) {
    if (params.previousViewKey.width != params.currentViewKey.width ||
        params.previousViewKey.height != params.currentViewKey.height ||
        params.previousViewKey.halfResolution != params.currentViewKey.halfResolution ||
        params.previousViewKey.qualityTier != params.currentViewKey.qualityTier) {
      decision.mode = JFAUpdateMode::Full;
      decision.fullReason = JFAFullReasons::kResizeOrScaleChanged;
      return decision;
    }
    decision.mode = JFAUpdateMode::Full;
    decision.fullReason = JFAFullReasons::kViewOrStaticChanged;
    return decision;
  }

  if (params.occluderCountChanged) {
    decision.mode = JFAUpdateMode::Full;
    decision.fullReason = JFAFullReasons::kOccluderDeletedOrUnbounded;
    return decision;
  }

  const int workWidth = params.currentViewKey.width;
  const int workHeight = params.currentViewKey.height;

  const JFARect dirty = params.previousOccluderBounds.Union(params.currentOccluderBounds);
  if (dirty.IsEmpty()) {
    decision.mode = JFAUpdateMode::Skip;
    return decision;
  }

  decision.dirtyRect = dirty;

  const int margin = MaxGiSdfInfluencePixels(params.currentViewKey.halfResolution);
  const JFARect expanded = dirty.Expand(margin, workWidth, workHeight);
  decision.expandedRect = expanded;

  if (expanded.TouchesBoundary(workWidth, workHeight)) {
    decision.mode = JFAUpdateMode::Full;
    decision.fullReason = JFAFullReasons::kUnsafeRegion;
    return decision;
  }

  const int maxArea = static_cast<int>(static_cast<float>(workWidth * workHeight) * params.maxAreaFractionThreshold);
  if (expanded.Area() > maxArea) {
    decision.mode = JFAUpdateMode::Full;
    decision.fullReason = JFAFullReasons::kAreaExceedsThreshold;
    return decision;
  }

  if (!params.hasValidSeedContext) {
    decision.mode = JFAUpdateMode::Full;
    decision.fullReason = JFAFullReasons::kMissingBoundaryContext;
    return decision;
  }

  decision.mode = JFAUpdateMode::Incremental;
  return decision;
}

IncrementalJfaResult JFADistanceFieldEvaluator::BuildIncrementalJfaDistanceField(
    const IncrementalJfaParams &params) {
  IncrementalJfaResult result;

  if (params.width <= 0 || params.height <= 0 ||
      params.currentMask.size() != static_cast<size_t>(params.width) * static_cast<size_t>(params.height)) {
    result.decision.mode = JFAUpdateMode::Full;
    result.decision.fullReason = JFAFullReasons::kVerificationFull;
    return result;
  }

  JFAViewKey viewKey{.cameraVersion = 1, .staticContentVersion = 1, .qualityTier = 1,
                    .width = params.width, .height = params.height, .halfResolution = false};

  DecideUpdateParams decideParams{
      .previousViewKey = viewKey,
      .currentViewKey = viewKey,
      .previousOccluderBounds = params.previousBounds,
      .currentOccluderBounds = params.currentBounds,
      .occluderCountChanged = params.occluderCountChanged,
      .hasValidSeedContext = true,
  };

  result.decision = DecideUpdate(decideParams);

  if (result.decision.mode == JFAUpdateMode::Full) {
    result.field = BuildApproximateJfaDistanceField(
        params.currentMask, params.width, params.height,
        params.enableCompensation, params.enableFallbackPlus2);
    auto exact = BuildExactSignedDistanceField(params.currentMask, params.width, params.height);
    result.stats = ComputeErrorStats(exact, result.field);
    return result;
  }

  if (result.decision.mode == JFAUpdateMode::Skip) {
    result.field = BuildApproximateJfaDistanceField(
        params.previousMask.empty() ? params.currentMask : params.previousMask,
        params.width, params.height, params.enableCompensation, params.enableFallbackPlus2);
    auto exact = BuildExactSignedDistanceField(params.currentMask, params.width, params.height);
    result.stats = ComputeErrorStats(exact, result.field);
    return result;
  }

  std::vector<SeedCoord> ping = InitializeSeedBuffer(
      params.previousMask.empty() ? params.currentMask : params.previousMask,
      params.width, params.height);

  const JFARect expanded = result.decision.expandedRect;
  for (int y = expanded.minY; y < expanded.maxY; ++y) {
    for (int x = expanded.minX; x < expanded.maxX; ++x) {
      const size_t idx = ToIndex(x, y, params.width);
      if (params.currentMask[idx] != 0u) {
        ping[idx] = {static_cast<uint16_t>(x), static_cast<uint16_t>(y)};
      } else {
        ping[idx] = {};
      }
    }
  }

  std::vector<SeedCoord> pong = ping;

  int stepSize = HighestPowerOfTwoLessEqual(std::max(params.width, params.height));
  stepSize = std::max(1, stepSize / 2);
  while (stepSize >= 1) {
    RunJfaStep(ping, pong, params.width, params.height, stepSize);
    ping.swap(pong);
    stepSize /= 2;
  }

  if (params.enableCompensation) {
    if (std::max(params.width, params.height) >= 2) {
      RunJfaStep(ping, pong, params.width, params.height, 2);
      ping.swap(pong);
    }
    RunJfaStep(ping, pong, params.width, params.height, 1);
    ping.swap(pong);
  }

  result.field = ResolveSignedDistanceFromSeeds(ping, params.currentMask, params.width, params.height);
  auto exact = BuildExactSignedDistanceField(params.currentMask, params.width, params.height);
  result.stats = ComputeErrorStats(exact, result.field);

  if (NeedsJfaPlus2Fallback(result.stats, params.p95Threshold, params.maxThreshold)) {
    result.verificationFailed = true;
    result.decision.mode = JFAUpdateMode::Revert;
    result.decision.fullReason = JFAFullReasons::kVerificationFull;
    result.field = BuildApproximateJfaDistanceField(
        params.currentMask, params.width, params.height,
        params.enableCompensation, true);
    result.stats = ComputeErrorStats(exact, result.field);
  }



  return result;
}

} // namespace NoMoreDay::render::gi
