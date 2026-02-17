#include "BenchmarkUtils.hpp"
#include "doctest.h"

#include "engine/render/core/RenderConstants.hpp"
#include "engine/render/shadow/ShadowAtlasAllocator.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <numeric>
#include <vector>

namespace {

struct ShadowPerfStats {
  double meanMs = 0.0;
  double p95Ms = 0.0;
  double p99Ms = 0.0;
};

ShadowPerfStats ComputeShadowPerfStats(std::vector<double> samples) {
  if (samples.empty()) {
    return {};
  }
  std::sort(samples.begin(), samples.end());
  const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);

  size_t idx95 = static_cast<size_t>(samples.size() * 0.95);
  if (idx95 >= samples.size()) {
    idx95 = samples.size() - 1;
  }
  size_t idx99 = static_cast<size_t>(samples.size() * 0.99);
  if (idx99 >= samples.size()) {
    idx99 = samples.size() - 1;
  }

  ShadowPerfStats stats = {};
  stats.meanMs = sum / static_cast<double>(samples.size());
  stats.p95Ms = samples[idx95];
  stats.p99Ms = samples[idx99];
  return stats;
}

ShadowPerfStats RunShadowSyntheticProfile(bool ultraTier) {
  using NoMoreDay::render::shadow::ShadowAtlasAllocator;

  constexpr int kFrames = 720;
  constexpr int kPixels = 128;
  constexpr int kCasters = 28;
  ShadowAtlasAllocator allocator(32u, 2u);
  std::vector<double> samples;
  samples.reserve(kFrames);

  volatile float sink = 0.0f;
  for (int frame = 0; frame < kFrames; ++frame) {
    const auto start = std::chrono::high_resolution_clock::now();

    for (int p = 0; p < kPixels; ++p) {
      const float px = static_cast<float>((p * 7 + frame) % 256);
      const float py = static_cast<float>((p * 11 + frame * 3) % 256);
      float minDist = 10000.0f;
      for (int c = 0; c < kCasters; ++c) {
        const float cx = static_cast<float>((c * 13 + frame * 5) % 256);
        const float cy = static_cast<float>((c * 17 + frame * 2) % 256);
        const float dx = px - cx;
        const float dy = py - cy;
        const float d = std::sqrt((dx * dx) + (dy * dy));
        minDist = std::min(minDist, d - 12.0f);
      }
      sink += minDist;
    }

    if (ultraTier) {
      allocator.BeginFrame(static_cast<uint32_t>(frame + 1));
      for (uint32_t light = 0; light < 96u; ++light) {
        const uint32_t lightId = ((light * 31u) + static_cast<uint32_t>(frame * 7)) % 96u + 1u;
        const float priority = ((light % 8u) == 0u) ? 4.0f : 1.0f;
        const auto allocation =
            allocator.AcquireTile({.lightId = lightId, .priorityScore = priority});
        if (allocation.success) {
          sink += 0.0001f;
        }
      }
    }

    const auto end = std::chrono::high_resolution_clock::now();
    samples.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  CHECK(sink != 0.0f);
  return ComputeShadowPerfStats(std::move(samples));
}

} // namespace

TEST_CASE("[Performance] Shadow Pipeline - Tier budgets (mean/p95/p99)") {
  const ShadowPerfStats high = RunShadowSyntheticProfile(false);
  const ShadowPerfStats ultra = RunShadowSyntheticProfile(true);

  LOG_WARN(
      "ShadowPipelineBenchmark High(mean={:.4f},p95={:.4f},p99={:.4f}) "
      "Ultra(mean={:.4f},p95={:.4f},p99={:.4f})",
      high.meanMs, high.p95Ms, high.p99Ms, ultra.meanMs, ultra.p95Ms,
      ultra.p99Ms);

  CHECK(high.meanMs <= NoMoreDay::render::core::kBudgetShadow_High);
  CHECK(high.p95Ms <= 0.8);
  CHECK(high.p99Ms <= 1.0);

  CHECK(ultra.meanMs <= NoMoreDay::render::core::kBudgetShadow_Extreme);
  CHECK(ultra.p95Ms <= 1.3);
  CHECK(ultra.p99Ms <= 1.6);
}
