#include "BenchmarkUtils.hpp"
#include "doctest.h"

#include "app/SharedContext.hpp"
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
#include <vector>

namespace {

constexpr uint32_t kHdrRgba16f = 0x881A;
constexpr uint32_t kGLTimeElapsed = 0x88BF;
constexpr uint32_t kGLQueryResult = 0x8866;

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
                    float startY = -280.0f) {
  const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));

  for (int i = 0; i < count; ++i) {
    const int row = i / cols;
    const int col = i % cols;
    const entt::entity e = registry.create();
    registry.emplace<Position>(e, startX + static_cast<float>(col) * spacing,
                               startY + static_cast<float>(row) * spacing);

    auto &light = registry.emplace<NoMoreDay::LightComponent>(e);
    light.enabled = true;
    light.radius = radius;
    light.intensity = 1.0f;
    light.colorR = 1.0f;
    light.colorG = 0.9f;
    light.colorB = 0.75f;
    light.priority = static_cast<uint8_t>(80 + (i % 120));
    light.flicker = false;
  }
}

NoMoreDay::tests::BenchmarkStats MeasureLightingPath(
    bool clusteredEnabled, int lightCount, int frames, GpuTimerQueryApi &queryApi,
    NoMoreDay::render::passes::LightCullingPass &cullingPass,
    NoMoreDay::render::passes::LightingPass &lightingPass,
    NoMoreDay::render::graph::RenderContext &context, entt::registry &registry,
    const Camera2D &camera, NoMoreDay::render::core::RenderConfig &cfg) {
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
    lightingPass.Execute(context);
    if (clusteredEnabled) {
      CHECK(lightingPass.WasClusteredAppliedLastFrame());
      CHECK_FALSE(lightingPass.UsedClusteredFallbackLastFrame());
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
  PopulateLights(registry, 128);

  ResourceManager resources;
  SharedContext shared = {};
  shared.resources = &resources;

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  auto hdr = render::resources::FramebufferManager::Create(1280, 720, kHdrRgba16f);
  REQUIRE(hdr.IsValid());

  render::graph::RenderContext context = {};
  context.registry = &registry;
  context.shared = &shared;
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;

  render::lighting::LightManager::Get().Initialize();
  render::passes::LightCullingPass cullingPass;
  render::passes::LightingPass lightingPass;
  REQUIRE(lightingPass.Initialize());
  lightingPass.SetLightCullingPass(&cullingPass);

  constexpr int kFrames = 120;
  const auto v2Stats = MeasureLightingPath(false, 128, kFrames, queryApi, cullingPass,
                                           lightingPass, context, registry, camera, cfg);
  const auto clusteredStats = MeasureLightingPath(
      true, 128, kFrames, queryApi, cullingPass, lightingPass, context, registry,
      camera, cfg);

  const double denom = std::max(v2Stats.mean_ms, 0.0001);
  const double improvement = (v2Stats.mean_ms - clusteredStats.mean_ms) / denom;
  const double maxAllowed = v2Stats.mean_ms * 1.05;
  DOCTEST_MESSAGE("Clustered(128) mean(ms)=", clusteredStats.mean_ms,
                  ", V2 mean(ms)=", v2Stats.mean_ms,
                  ", improvement=", improvement * 100.0, "%");

  CHECK(v2Stats.mean_ms > 0.0);
  CHECK(clusteredStats.mean_ms <= maxAllowed);

  lightingPass.Shutdown();
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
  SharedContext shared = {};
  shared.resources = &resources;

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  auto hdr = render::resources::FramebufferManager::Create(1280, 720, kHdrRgba16f);
  REQUIRE(hdr.IsValid());

  render::graph::RenderContext context = {};
  context.registry = &registry;
  context.shared = &shared;
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;

  render::lighting::LightManager::Get().Initialize();
  render::passes::LightCullingPass cullingPass;
  render::passes::LightingPass lightingPass;
  REQUIRE(lightingPass.Initialize());
  lightingPass.SetLightCullingPass(&cullingPass);

  constexpr int kFrames = 120;
  const auto v2Stats =
      MeasureLightingPath(false, 8, kFrames, queryApi, cullingPass, lightingPass,
                          context, registry, camera, cfg);
  const auto clusteredStats =
      MeasureLightingPath(true, 8, kFrames, queryApi, cullingPass, lightingPass,
                          context, registry, camera, cfg);

  const double maxAllowed = v2Stats.mean_ms * 1.05;
  DOCTEST_MESSAGE("Clustered(8) mean(ms)=", clusteredStats.mean_ms,
                  ", V2 mean(ms)=", v2Stats.mean_ms,
                  ", maxAllowed(ms)=", maxAllowed);

  CHECK(v2Stats.mean_ms > 0.0);
  CHECK(clusteredStats.mean_ms <= maxAllowed);

  lightingPass.Shutdown();
  render::lighting::ClusteredLightingState::Get().Shutdown();
  render::lighting::LightManager::Get().Shutdown();
  render::resources::FramebufferManager::Destroy(hdr);
}
