#include "BenchmarkUtils.hpp"
#include "doctest.h"

#include "engine/render/core/QualityTierManager.hpp"
#include "engine/vfx/VFXSequenceManager.hpp"
#include "engine/vfx/VFXSequencerSystem.hpp"
#include "game/components/Common.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <vector>

namespace NoMoreDay::tests {

TEST_CASE("[Performance] VFXLightingIntegration - 10 Concurrent Sequences Frame Variance") {
  constexpr std::array<const char *, 10> kTemplateNames = {
      "V3Template_MeleeSlashFlash",     "V3Template_MeleeHeavyQuake",
      "V3Template_MeleeComboRhythm",    "V3Template_SpellFireballBurst",
      "V3Template_SpellFrostSpread",    "V3Template_SpellChainLightning",
      "V3Template_AoEPoisonMist",       "V3Template_AoEHolyColumn",
      "V3Template_SummonShadowTeleport","V3Template_SummonElementalFocus",
  };

  auto &qualityManager = render::core::QualityTierManager::Get();
  qualityManager.ForceTier(render::core::QualityTier::High);

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson("assets/vfx/templates/v3") >=
          static_cast<int>(kTemplateNames.size()));

  entt::registry registry;
  for (size_t i = 0; i < kTemplateNames.size(); ++i) {
    const char *name = kTemplateNames[i];
    REQUIRE(manager.GetSequenceId(name) >= 0);

    const entt::entity entity = registry.create();
    registry.emplace<Position>(entity, 16.0f + static_cast<float>(i) * 12.0f,
                               24.0f + static_cast<float>(i) * 6.0f);
    manager.Play(registry, entity, name, entt::null, true);
  }

  constexpr float kDt = 1.0f / 60.0f;
  constexpr int kWarmupFrames = 120;
  constexpr int kBenchFrames = 480;

  vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();
  for (int frame = 0; frame < kWarmupFrames; ++frame) {
    vfx::VFXSequencerSystem::Update(registry, kDt);
  }

  std::vector<double> samples;
  samples.reserve(kBenchFrames);
  for (int frame = 0; frame < kBenchFrames; ++frame) {
    const auto begin = std::chrono::high_resolution_clock::now();
    vfx::VFXSequencerSystem::Update(registry, kDt);
    const auto end = std::chrono::high_resolution_clock::now();
    samples.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
  }

  const auto [minIt, maxIt] = std::minmax_element(samples.begin(), samples.end());
  REQUIRE(minIt != samples.end());
  REQUIRE(maxIt != samples.end());
  const double fluctuationMs = *maxIt - *minIt;

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("VFXLightingIntegration::10ConcurrentFrameVariance", stats,
                "fluctuation <= 1.0ms");
  CHECK(fluctuationMs <= 1.0);

  manager.Shutdown();
}

} // namespace NoMoreDay::tests
