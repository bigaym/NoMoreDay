#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/SharedContext.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/PopupRenderer.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/RenderContext.hpp"
#include "engine/render/debug/RenderProfiler.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/render/GPUEntityAdapter.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

namespace NoMoreDay::tests {

double ComputePercentileMs(std::vector<double> samples, double percentile) {
  if (samples.empty()) {
    return 0.0;
  }
  std::sort(samples.begin(), samples.end());
  const double clamped = std::clamp(percentile, 0.0, 1.0);
  size_t index = static_cast<size_t>(
      std::floor(clamped * static_cast<double>(samples.size() - 1)));
  if (index >= samples.size()) {
    index = samples.size() - 1;
  }
  return samples[index];
}

TEST_CASE("[Performance] ParticleSystem - Scenario A Particle Stress Test") {
  using namespace NoMoreDay::systems;

  GPUParticleSystem::Get().Init(100000); // Max particles

  const int FPS = 60;
  const float DT = 1.0f / FPS;
  const int TOTAL_FRAMES = 600;               // 10 seconds
  const int EMISSION_PER_FRAME = 10000 / FPS; // ~166

  std::vector<double> updateTimes;
  updateTimes.reserve(TOTAL_FRAMES);

  NoMoreDay::components::GPUParticle pTemplate =
      InkEffectHelper::CreateInkTrail({0, 0}, {0, 0}, 1.0f, 1.0f);

  for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
    // 1. Emit
    for (int i = 0; i < EMISSION_PER_FRAME; ++i) {
      GPUParticleSystem::Get().Emit(pTemplate);
    }

    // 2. Measure Update
    auto start = std::chrono::high_resolution_clock::now();
    GPUParticleSystem::Get().Update(DT);
    auto end = std::chrono::high_resolution_clock::now();

    updateTimes.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  BenchmarkStats stats = CalculateStats(updateTimes);
  LOG_BENCHMARK("Scenario A (Particles)", stats, "< 0.5ms");

  CHECK(stats.mean_ms < 0.5);

  GPUParticleSystem::Get().Shutdown();
}

TEST_CASE("[Performance] PopupRenderer - Scenario B Popup Spam Test") {
  using namespace NoMoreDay::render;

  PopupRenderer::Get().Init();

  const int FPS = 60;
  const float DT = 1.0f / FPS;
  const int TOTAL_FRAMES = 300; // 5 seconds
  const int NEW_POPUPS_PER_FRAME = 50;

  std::vector<double> renderTimes;
  renderTimes.reserve(TOTAL_FRAMES);

  Matrix identity = MatrixIdentity(); // Dummy view proj

  for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
    // 1. Add Popups
    for (int i = 0; i < NEW_POPUPS_PER_FRAME; ++i) {
      PopupRenderer::Get().Emit({0, 0}, 100 + i, (i % 10 == 0));
    }

    PopupRenderer::Get().Update(DT);

    // 2. Measure Render (Submission)
    auto start = std::chrono::high_resolution_clock::now();
    PopupRenderer::Get().Render(identity);
    auto end = std::chrono::high_resolution_clock::now();

    renderTimes.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  BenchmarkStats stats = CalculateStats(renderTimes);
  LOG_BENCHMARK("Scenario B (Popups)", stats, "< 0.3ms");

  CHECK(stats.mean_ms < 0.3);

  PopupRenderer::Get().Shutdown();
}

TEST_CASE("[Performance] GPUEntitySystem - Scenario C Entity Horde Test") {
  using namespace NoMoreDay::systems;

  ResourceManager resources;
  const int TEST_ENTITIES = 20000;

  systems::GPUEntitySystem gpuEntitySystem;
  render::MDIRenderer mdiRenderer;
  RenderContext renderContext;
  renderContext.gpuEntitySystem = &gpuEntitySystem;
  renderContext.mdiRenderer = &mdiRenderer;
  renderContext.gpuFlowFieldSystem = &systems::GPUFlowFieldSystem::Get();
  renderContext.resources = &resources;

  NoMoreDay::SharedContext context;
  context.resources = &resources;
  context.renderContext = &renderContext;

  gpuEntitySystem.Init(resources, TEST_ENTITIES);
  mdiRenderer.Init(resources, TEST_ENTITIES);

  entt::registry registry;
  context.registry = &registry;
  NoMoreDay::GPUEntityAdapter gpuEntityAdapter;
  gpuEntityAdapter.Init(TEST_ENTITIES, &registry, gpuEntitySystem);
  for (int i = 0; i < TEST_ENTITIES; ++i) {
    auto e = registry.create();
    registry.emplace<::Position>(e, (float)(rand() % 4000),
                                 (float)(rand() % 4000));
    registry.emplace<::Radius>(e, 2.0f);
    registry.emplace<::GPUIndex>(e, -1);

    if (rand() % 2 == 0) { // 50% moving
      registry.emplace<::Velocity>(e, 10.0f, 10.0f);
    } else {
      registry.emplace<::Velocity>(e, 0.0f, 0.0f);
    }
  }

  const int FPS = 60;
  const float DT = 1.0f / FPS;
  const int TOTAL_FRAMES = 100;

  std::vector<double> updateTimes;
  updateTimes.reserve(TOTAL_FRAMES);

  for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
    // Simulate movement for 50% entities to trigger "Dirty" updates if system
    // tracks it Or essentially GPUEntitySystem::Update iterates all and syncs.

    // Measure Update (Sync + Upload)
    auto start = std::chrono::high_resolution_clock::now();
    gpuEntityAdapter.Update(registry, gpuEntitySystem, DT, (float)frame);
    gpuEntitySystem.UploadGPU({&resources, &mdiRenderer, context.renderAlpha});
    auto end = std::chrono::high_resolution_clock::now();

    updateTimes.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  BenchmarkStats stats = CalculateStats(updateTimes);
  LOG_BENCHMARK("Scenario C (Entities)", stats, "< 3.0ms");

  CHECK(stats.mean_ms < 3.0);

  gpuEntitySystem.Shutdown();
}

TEST_CASE("[Performance] PostProcess - Scenario D ColorGrading Sampling Cost") {
  constexpr int kFps = 60;
  constexpr int kTotalFrames = 240; // 4 seconds
  constexpr int kPixels = 512;      // lightweight synthetic batch
  constexpr float kLutSize = 16.0f;
  constexpr float kIntensity = 0.85f;
  constexpr float kInvLut = 1.0f / kLutSize;

  std::vector<double> frameTimes;
  frameTimes.reserve(kTotalFrames);

  volatile float sink = 0.0f;
  for (int frame = 0; frame < kTotalFrames; ++frame) {
    const auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < kPixels; ++i) {
      const float t = static_cast<float>(frame * kPixels + i) * 0.0137f;
      const float r = std::fmod(std::sin(t) * 0.5f + 0.5f, 1.0f);
      const float g = std::fmod(std::sin(t * 1.7f) * 0.5f + 0.5f, 1.0f);
      const float b = std::fmod(std::sin(t * 2.3f) * 0.5f + 0.5f, 1.0f);

      const float blue = b * (kLutSize - 1.0f);
      const float z0 = std::floor(blue);
      const float z1 = std::min(z0 + 1.0f, kLutSize - 1.0f);
      const float zf = blue - z0;

      const float lut0r = (r * (kLutSize - 1.0f) + z0) * kInvLut;
      const float lut0g = (g * (kLutSize - 1.0f)) * kInvLut;
      const float lut1r = (r * (kLutSize - 1.0f) + z1) * kInvLut;
      const float lut1g = (g * (kLutSize - 1.0f)) * kInvLut;

      const float gradedR = lut0r * (1.0f - zf) + lut1r * zf;
      const float gradedG = lut0g * (1.0f - zf) + lut1g * zf;
      const float gradedB = b * 0.9f + 0.1f * r;

      sink += (r * (1.0f - kIntensity) + gradedR * kIntensity) +
              (g * (1.0f - kIntensity) + gradedG * kIntensity) +
              (b * (1.0f - kIntensity) + gradedB * kIntensity);
    }

    const auto end = std::chrono::high_resolution_clock::now();
    frameTimes.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  BenchmarkStats stats = CalculateStats(frameTimes);
  LOG_BENCHMARK("Scenario D (ColorGrading)", stats, "< 0.25ms");
  CHECK(stats.mean_ms < 0.25);
  CHECK(sink >= 0.0f);
}

TEST_CASE("[Performance] Lighting - Scenario E Volumetric Integration Cost") {
  constexpr int kTotalFrames = 240;
  constexpr int kScreenSamples = 64;
  constexpr int kRaySteps = 48;
  constexpr float kScattering = 0.16f;
  constexpr float kDecay = 0.95f;

  std::vector<double> frameTimes;
  frameTimes.reserve(kTotalFrames);

  volatile float sink = 0.0f;
  for (int frame = 0; frame < kTotalFrames; ++frame) {
    const auto start = std::chrono::high_resolution_clock::now();

    for (int p = 0; p < kScreenSamples; ++p) {
      float illum = 0.0f;
      float transmittance = 1.0f;
      const float phase = static_cast<float>(frame + p) * 0.021f;

      for (int s = 0; s < kRaySteps; ++s) {
        const float noise = std::sin(phase + static_cast<float>(s) * 0.11f) *
                                0.5f +
                            0.5f;
        illum += noise * transmittance * kScattering;
        transmittance *= kDecay;
      }

      sink += illum;
    }

    const auto end = std::chrono::high_resolution_clock::now();
    frameTimes.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  BenchmarkStats stats = CalculateStats(frameTimes);
  LOG_BENCHMARK("Scenario E (Volumetric)", stats, "< 0.80ms");
  CHECK(stats.mean_ms < 0.80);
  CHECK(sink >= 0.0f);
}

TEST_CASE("[Performance] Debug - Scenario F Profiler HUD Overhead") {
  using namespace NoMoreDay::render::debug;

  constexpr int kTotalFrames = 300;
  constexpr std::array<RenderPassId, 12> kPasses = {
      RenderPassId::Scene,      RenderPassId::Lighting,
      RenderPassId::HeightShadow, RenderPassId::FluidSimulation,
      RenderPassId::Volumetric, RenderPassId::VFX,
      RenderPassId::GPUText,    RenderPassId::GPULoot,
      RenderPassId::UIWorld,    RenderPassId::PostProcess,
      RenderPassId::Distortion, RenderPassId::Composite};

  RenderProfiler profiler;
  // Seed one sample for each pass so HUD read path has stable input.
  profiler.BeginFrame();
  for (RenderPassId pass : kPasses) {
    profiler.BeginPass(pass);
    profiler.EndPass(pass);
  }
  profiler.EndFrame();

  std::vector<double> frameTimes;
  frameTimes.reserve(kTotalFrames);
  volatile float sink = 0.0f;

  for (int frame = 0; frame < kTotalFrames; ++frame) {
    const auto start = std::chrono::high_resolution_clock::now();

    // HUD refresh at 12Hz (every 5 frames @60FPS) to match practical debug usage.
    if ((frame % 5) == 0) {
      for (RenderPassId pass : kPasses) {
        const auto stat = profiler.GetStats(pass);
        sink += stat.cpuMeanMs + stat.cpuP95Ms + stat.budgetMs;
      }
    }

    const auto end = std::chrono::high_resolution_clock::now();
    frameTimes.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  BenchmarkStats stats = CalculateStats(frameTimes);
  LOG_BENCHMARK("Scenario F (ProfilerHUD)", stats, "< 0.15ms");
  CHECK(stats.mean_ms < 0.15);
  CHECK(sink >= 0.0f);
}

TEST_CASE("[Performance] Text - Scenario H CPU vs GPU Route Preparation (100+ onscreen)") {
  constexpr int kFrames = 240;
  constexpr int kPopupsPerFrame = 128;

  std::vector<double> cpuRouteMs;
  std::vector<double> gpuRouteMs;
  cpuRouteMs.reserve(kFrames);
  gpuRouteMs.reserve(kFrames);

  volatile uint32_t sink = 0;
  for (int frame = 0; frame < kFrames; ++frame) {
    {
      const auto start = std::chrono::high_resolution_clock::now();
      for (int i = 0; i < kPopupsPerFrame; ++i) {
        const std::string text = std::to_string(1000 + ((frame + i) % 9000));
        for (char ch : text) {
          sink += static_cast<uint32_t>(ch);
        }
      }
      const auto end = std::chrono::high_resolution_clock::now();
      cpuRouteMs.push_back(
          std::chrono::duration<double, std::milli>(end - start).count());
    }

    {
      const auto start = std::chrono::high_resolution_clock::now();
      for (int i = 0; i < kPopupsPerFrame; ++i) {
        const uint32_t packedColor = 0x00FFFFFFu | ((i % 2 == 0) ? (4u << 24u) : 0u);
        sink += packedColor;
      }
      const auto end = std::chrono::high_resolution_clock::now();
      gpuRouteMs.push_back(
          std::chrono::duration<double, std::milli>(end - start).count());
    }
  }

  const BenchmarkStats cpuStats = CalculateStats(cpuRouteMs);
  const BenchmarkStats gpuStats = CalculateStats(gpuRouteMs);
  LOG_BENCHMARK("Scenario H (Text CPU Route)", cpuStats, "baseline");
  LOG_BENCHMARK("Scenario H (Text GPU Route)", gpuStats, "<= 0.10x CPU route");

  CHECK(gpuStats.mean_ms <= cpuStats.mean_ms * 0.10);
  CHECK(sink > 0u);
}

TEST_CASE("[Performance] Rendering - Scenario G Tier AutoDegrade Profiles") {
  using Tier = NoMoreDay::render::core::QualityTier;
  struct Profile {
    Tier tier = Tier::Medium;
    int degradeCycles = 0;
    int recoverCycles = 0;
  };

  constexpr std::array<Profile, 4> kProfiles = {
      Profile{Tier::Low, 2, 2},
      Profile{Tier::Medium, 3, 3},
      Profile{Tier::High, 4, 4},
      Profile{Tier::Ultra, 5, 5},
  };

  auto &manager = NoMoreDay::render::core::QualityTierManager::Get();
  std::vector<double> samples;
  samples.reserve(256);

  for (const auto &profile : kProfiles) {
    manager.ForceTier(profile.tier);
    manager.ResetAutoDegrade("bench_profile_start");

    const auto thresholds =
        NoMoreDay::render::core::QualityTierManager::GetAutoDegradeBudgetThresholds(
            profile.tier);
    const float pressureMs = thresholds.degradeTriggerMs + 1.0f;
    const float recoverMs = std::max(0.0f, thresholds.recoverTriggerMs - 1.0f);

    for (int i = 0; i < profile.degradeCycles; ++i) {
      const auto start = std::chrono::high_resolution_clock::now();
      manager.IncreaseAutoDegradeLevel("bench_overflow", pressureMs,
                                       thresholds.degradeTriggerMs);
      const auto end = std::chrono::high_resolution_clock::now();
      samples.push_back(
          std::chrono::duration<double, std::milli>(end - start).count());
    }

    for (int i = 0; i < profile.recoverCycles; ++i) {
      const auto start = std::chrono::high_resolution_clock::now();
      manager.DecreaseAutoDegradeLevel("bench_recover", recoverMs,
                                       thresholds.recoverTriggerMs);
      const auto end = std::chrono::high_resolution_clock::now();
      samples.push_back(
          std::chrono::duration<double, std::milli>(end - start).count());
    }
  }

  REQUIRE(!samples.empty());
  const BenchmarkStats stats = CalculateStats(samples);
  const double p95Ms = ComputePercentileMs(samples, 0.95);
  LOG_BENCHMARK("Scenario G (TierAutoDegrade)", stats, "< 0.05ms");
  CHECK(stats.mean_ms < 0.05);
  CHECK(p95Ms < 0.08);
}

} // namespace NoMoreDay::tests
