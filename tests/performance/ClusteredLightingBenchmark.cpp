#include "BenchmarkUtils.hpp"
#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/lighting/ClusteredLightingState.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/LightCullingPass.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/LightComponent.hpp"

#include "GLFW/glfw3.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

constexpr uint32_t kHdrRgba16f = 0x881A;
constexpr uint32_t kGLTimeElapsed = 0x88BF;
constexpr uint32_t kGLQueryResult = 0x8866;
constexpr uint32_t kClusteredBenchmarkSeed = 0x4E4D4443; // "NMDC"
constexpr int kBenchmarkWidth = 3840;
constexpr int kBenchmarkHeight = 2160;
constexpr float kBenchmarkZoom = 1.0f;
constexpr float kBenchmarkCameraOffsetX = 960.0f;
constexpr float kBenchmarkCameraOffsetY = 540.0f;
constexpr float kClusteredScenarioLightRadius = 420.0f;
constexpr float kClusteredScenarioLightSpacing = 64.0f;
constexpr float kClusteredScenarioStartX = -256.0f;
constexpr float kClusteredScenarioStartY = -192.0f;

struct GpuTimerQueryApi {
  using GenQueriesFn = void(APIENTRY *)(int, uint32_t *);
  using BeginQueryFn = void(APIENTRY *)(uint32_t, uint32_t);
  using EndQueryFn = void(APIENTRY *)(uint32_t);
  using GetQueryObjectUi64vFn = void(APIENTRY *)(uint32_t, uint32_t, uint64_t *);
  using DeleteQueriesFn = void(APIENTRY *)(int, const uint32_t *);

  GenQueriesFn genQueries = nullptr;
  BeginQueryFn beginQuery = nullptr;
  EndQueryFn endQuery = nullptr;
  GetQueryObjectUi64vFn getQueryObjectUi64v = nullptr;
  DeleteQueriesFn deleteQueries = nullptr;

  bool Initialize() {
    genQueries = reinterpret_cast<GenQueriesFn>(glfwGetProcAddress("glGenQueries"));
    beginQuery = reinterpret_cast<BeginQueryFn>(glfwGetProcAddress("glBeginQuery"));
    endQuery = reinterpret_cast<EndQueryFn>(glfwGetProcAddress("glEndQuery"));
    getQueryObjectUi64v = reinterpret_cast<GetQueryObjectUi64vFn>(
        glfwGetProcAddress("glGetQueryObjectui64v"));
    deleteQueries =
        reinterpret_cast<DeleteQueriesFn>(glfwGetProcAddress("glDeleteQueries"));
    return genQueries != nullptr && beginQuery != nullptr && endQuery != nullptr &&
           getQueryObjectUi64v != nullptr && deleteQueries != nullptr;
  }
};

void PopulateLights(entt::registry &registry, int count, float radius = 120.0f,
                    float spacing = 36.0f, float startX = -420.0f,
                    float startY = -280.0f, bool forceSpotLight = false) {
  const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));

  for (int i = 0; i < count; ++i) {
    const int row = i / cols;
    const int col = i % cols;
    const float posX = startX + static_cast<float>(col) * spacing;
    const float posY = startY + static_cast<float>(row) * spacing;
    const entt::entity e = registry.create();
    registry.emplace<Position>(e, posX, posY);

    auto &light = registry.emplace<NoMoreDay::LightComponent>(e);
    light.enabled = true;
    light.radius = radius;
    light.intensity = 1.0f;
    light.colorR = 1.0f;
    light.colorG = 0.9f;
    light.colorB = 0.75f;
    light.priority = static_cast<uint8_t>(80 + (i % 120));
    light.flicker = false;
    if (forceSpotLight) {
      light.type = NoMoreDay::components::LightType::SpotLight;
      light.spotAngle = 90.0f;
      light.spotDirection = static_cast<float>((i * 37) % 360);
    }
  }
}

NoMoreDay::tests::BenchmarkStats MeasureLightingPath(
    bool clusteredEnabled, int lightCount, int frames, int lightingRepeats,
    GpuTimerQueryApi &queryApi,
    NoMoreDay::render::passes::LightCullingPass &cullingPass,
    NoMoreDay::render::passes::LightingPass &lightingPass,
    NoMoreDay::render::graph::RenderContext &context, entt::registry &registry,
    const Camera2D &camera, NoMoreDay::render::core::RenderConfig &cfg,
    int *firstActiveLightCount = nullptr) {
  std::vector<double> samples;
  samples.reserve(static_cast<size_t>(frames));

  cfg.clusteredLightingEnabled = clusteredEnabled;
  cfg.v3Enabled = true;
  cfg.dynamicLightingEnabled = true;
  cfg.clusterTileSize = 32;
  cfg.clusterZSliceCount = 4;
  cfg.maxLights = lightCount;
  cfg.shadowEnabled = false;

  auto &lightManager = NoMoreDay::render::lighting::LightManager::Get();
  const int warmupFrames = std::max(8, frames / 6);

  for (int i = 0; i < warmupFrames + frames; ++i) {
    lightManager.Update(registry, camera, cfg.maxLights, static_cast<float>(i) * 0.016f);
    if (firstActiveLightCount != nullptr && i == warmupFrames) {
      *firstActiveLightCount = lightManager.GetActiveLightCount();
    }

    if (clusteredEnabled) {
      cullingPass.Execute(context);
      CHECK(cullingPass.SucceededThisFrame());
      CHECK(cullingPass.IsClusterDataReadyForCurrentFrame());
    }

    const bool measureThisFrame = i >= warmupFrames;
    uint32_t query = 0;
    if (measureThisFrame) {
      queryApi.genQueries(1, &query);
      queryApi.beginQuery(kGLTimeElapsed, query);
    }
    for (int pass = 0; pass < lightingRepeats; ++pass) {
      lightingPass.Execute(context);
      if (clusteredEnabled) {
        CHECK(lightingPass.WasClusteredAppliedLastFrame());
        CHECK_FALSE(lightingPass.UsedClusteredFallbackLastFrame());
      }
    }
    if (measureThisFrame) {
      queryApi.endQuery(kGLTimeElapsed);
    }

    if (measureThisFrame) {
      uint64_t elapsedNs = 0;
      queryApi.getQueryObjectUi64v(query, kGLQueryResult, &elapsedNs);
      queryApi.deleteQueries(1, &query);
      samples.push_back(static_cast<double>(elapsedNs) / 1'000'000.0);
    }
  }

  return NoMoreDay::tests::CalculateStats(samples);
}

} // namespace

TEST_CASE("[Performance] Clustered Lighting - 128 lights A/B no regression") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  GpuTimerQueryApi queryApi;
  REQUIRE(queryApi.Initialize());

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());

  entt::registry registry;
  PopulateLights(registry, 128, kClusteredScenarioLightRadius,
                 kClusteredScenarioLightSpacing, kClusteredScenarioStartX,
                 kClusteredScenarioStartY, true);

  ResourceManager resources;

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {kBenchmarkCameraOffsetX, kBenchmarkCameraOffsetY};
  camera.zoom = kBenchmarkZoom;

  auto hdr = render::resources::FramebufferManager::Create(
      kBenchmarkWidth, kBenchmarkHeight, kHdrRgba16f);
  REQUIRE(hdr.IsValid());

  render::graph::RenderContext context = {};
  context.registry = &registry;
  context.resources = &resources;
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;

  render::lighting::LightManager::Get().Initialize();
  render::lighting::LightManager::Get().SetDisableViewCullingForTesting(true);
  render::passes::LightCullingPass cullingPass;
  cullingPass.SetReadbackEnabledForTesting(false);
  render::passes::LightingPass lightingPass;
  REQUIRE(lightingPass.Initialize());
  lightingPass.SetLightCullingPass(&cullingPass);
  lightingPass.SetSkipResolveForTesting(true);

  constexpr int kFrames = 120;
  constexpr int kTrials = 3;
  constexpr int kLightingRepeats = 32;
  CHECK(kFrames > 0);
  CHECK(kTrials >= 3);
  CHECK(std::max(8, kFrames / 6) < kFrames);
  CHECK(kLightingRepeats >= 1);
  std::cout << "RELEASE_GATE_CONTEXT clustered_seed=" << kClusteredBenchmarkSeed
            << " lights=128"
            << " resolution=" << kBenchmarkWidth << "x" << kBenchmarkHeight
            << " zoom=" << kBenchmarkZoom
            << " camera_offset=" << kBenchmarkCameraOffsetX << ","
            << kBenchmarkCameraOffsetY
            << " light_radius=" << kClusteredScenarioLightRadius
            << " light_spacing=" << kClusteredScenarioLightSpacing
            << " light_origin=" << kClusteredScenarioStartX << ","
            << kClusteredScenarioStartY
            << " warmup_frames=" << std::max(8, kFrames / 6)
            << " measure_frames=" << kFrames << " trials=" << kTrials
            << " lighting_repeats=" << kLightingRepeats << "\n";

  std::vector<double> v2Means;
  std::vector<double> clusteredMeans;
  std::vector<double> improvementPctSamples;
  v2Means.reserve(kTrials);
  clusteredMeans.reserve(kTrials);
  improvementPctSamples.reserve(kTrials);

  for (int trial = 0; trial < kTrials; ++trial) {
    int v2ActiveLights = 0;
    int clusteredActiveLights = 0;
    const auto v2Stats =
        MeasureLightingPath(false, 128, kFrames, kLightingRepeats, queryApi,
                            cullingPass, lightingPass, context, registry, camera,
                            cfg, &v2ActiveLights);
    const auto clusteredStats = MeasureLightingPath(
        true, 128, kFrames, kLightingRepeats, queryApi, cullingPass, lightingPass,
        context, registry, camera, cfg, &clusteredActiveLights);

    const double denom = std::max(v2Stats.mean_ms, 0.0001);
    const double improvementPct =
        ((v2Stats.mean_ms - clusteredStats.mean_ms) / denom) * 100.0;
    v2Means.push_back(v2Stats.mean_ms);
    clusteredMeans.push_back(clusteredStats.mean_ms);
    improvementPctSamples.push_back(improvementPct);

    CHECK(v2Stats.mean_ms > 0.0);
    CHECK(v2ActiveLights == 128);
    CHECK(clusteredActiveLights == 128);
  }

  auto medianOf = [](std::vector<double> values) -> double {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
  };

  const double v2MeanMedian = medianOf(v2Means);
  const double clusteredMeanMedian = medianOf(clusteredMeans);
  const double improvementPctMedianSamples = medianOf(improvementPctSamples);
  const double improvementPctFromMedianMeans =
      ((v2MeanMedian - clusteredMeanMedian) / std::max(v2MeanMedian, 0.0001)) *
      100.0;
  const double maxAllowedMedian = v2MeanMedian * 1.05;

  DOCTEST_MESSAGE("Clustered(128) median mean(ms)=", clusteredMeanMedian,
                  ", V2 median mean(ms)=", v2MeanMedian,
                  ", median trial improvement=", improvementPctMedianSamples, "%",
                  ", median-mean improvement=", improvementPctFromMedianMeans,
                  "%");
  std::cout << "RELEASE_GATE_METRIC clustered_128_v2_mean_ms=" << v2MeanMedian
            << "\n";
  std::cout << "RELEASE_GATE_METRIC clustered_128_mean_ms="
            << clusteredMeanMedian << "\n";
  std::cout << "RELEASE_GATE_METRIC clustered_128_improvement_pct="
            << improvementPctFromMedianMeans << "\n";
  CHECK(clusteredMeanMedian <= maxAllowedMedian);

  lightingPass.Shutdown();
  render::lighting::LightManager::Get().SetDisableViewCullingForTesting(false);
  render::lighting::ClusteredLightingState::Get().Shutdown();
  render::lighting::LightManager::Get().Shutdown();
  render::resources::FramebufferManager::Destroy(hdr);
}

TEST_CASE("[Performance] Clustered Lighting - Low-light no regression") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  GpuTimerQueryApi queryApi;
  REQUIRE(queryApi.Initialize());

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());

  entt::registry registry;
  PopulateLights(registry, 8);

  ResourceManager resources;

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {kBenchmarkCameraOffsetX, kBenchmarkCameraOffsetY};
  camera.zoom = kBenchmarkZoom;

  auto hdr = render::resources::FramebufferManager::Create(
      kBenchmarkWidth, kBenchmarkHeight, kHdrRgba16f);
  REQUIRE(hdr.IsValid());

  render::graph::RenderContext context = {};
  context.registry = &registry;
  context.resources = &resources;
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;

  render::lighting::LightManager::Get().Initialize();
  render::lighting::LightManager::Get().SetDisableViewCullingForTesting(true);
  render::passes::LightCullingPass cullingPass;
  render::passes::LightingPass lightingPass;
  REQUIRE(lightingPass.Initialize());
  lightingPass.SetLightCullingPass(&cullingPass);

  constexpr int kFrames = 120;
  constexpr int kLightingRepeats = 1;
  const auto v2Stats =
      MeasureLightingPath(false, 8, kFrames, kLightingRepeats, queryApi,
                          cullingPass, lightingPass, context, registry, camera,
                          cfg);
  const auto clusteredStats =
      MeasureLightingPath(true, 8, kFrames, kLightingRepeats, queryApi,
                          cullingPass, lightingPass, context, registry, camera,
                          cfg);

  const double maxAllowed = v2Stats.mean_ms * 1.05;
  DOCTEST_MESSAGE("Clustered(8) mean(ms)=", clusteredStats.mean_ms,
                  ", V2 mean(ms)=", v2Stats.mean_ms,
                  ", maxAllowed(ms)=", maxAllowed);

  CHECK(v2Stats.mean_ms > 0.0);
  CHECK(clusteredStats.mean_ms <= maxAllowed);

  lightingPass.Shutdown();
  render::lighting::LightManager::Get().SetDisableViewCullingForTesting(false);
  render::lighting::ClusteredLightingState::Get().Shutdown();
  render::lighting::LightManager::Get().Shutdown();
  render::resources::FramebufferManager::Destroy(hdr);
}
