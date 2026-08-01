#include "BenchmarkUtils.hpp"
#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/LightComponent.hpp"
#include "game/render/LightAdapter.hpp"

#include "GLFW/glfw3.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace {
constexpr uint32_t kLightingGLRgba16f = 0x881A;
constexpr uint32_t kLightingGLTimeElapsed = 0x88BF;
constexpr uint32_t kLightingGLQueryResult = 0x8866;

struct LightingGpuTimerQueryApi {
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

void PopulateLights(entt::registry &registry, int count) {
  const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
  constexpr float kStartX = 80.0f;
  constexpr float kStartY = 80.0f;
  constexpr float kSpacing = 64.0f;

  for (int i = 0; i < count; ++i) {
    const int row = i / cols;
    const int col = i % cols;

    const entt::entity e = registry.create();
    registry.emplace<Position>(e, kStartX + static_cast<float>(col) * kSpacing,
                               kStartY + static_cast<float>(row) * kSpacing);

    auto &light = registry.emplace<NoMoreDay::LightComponent>(e);
    light.enabled = true;
    light.radius = 120.0f;
    light.intensity = 1.0f;
    light.colorR = 1.0f;
    light.colorG = 0.92f;
    light.colorB = 0.78f;
    light.priority = 128;
    light.flicker = false;
  }
}

NoMoreDay::tests::BenchmarkStats
MeasureLightingGpuMs(NoMoreDay::render::passes::LightingPass &pass,
                     NoMoreDay::render::graph::RenderContext &context,
                     entt::registry &registry, const Camera2D &camera, int maxLights,
                     int frames, LightingGpuTimerQueryApi &api) {
  std::vector<double> samples;
  samples.reserve(static_cast<size_t>(frames));

  auto &lightManager = NoMoreDay::render::lighting::LightManager::Get();

  for (int i = 0; i < frames; ++i) {
    const auto projection = NoMoreDay::LightAdapter::BuildLightCandidates(
        registry, static_cast<float>(i) * 0.016f);
    lightManager.UpdateCandidates(projection.lights, camera, maxLights,
                                  projection.ecsLights);

    uint32_t query = 0;
    api.genQueries(1, &query);
    api.beginQuery(kLightingGLTimeElapsed, query);
    pass.Execute(context);
    api.endQuery(kLightingGLTimeElapsed);

    uint64_t elapsedNs = 0;
    api.getQueryObjectUi64v(query, kLightingGLQueryResult, &elapsedNs);
    api.deleteQueries(1, &query);
    samples.push_back(static_cast<double>(elapsedNs) / 1'000'000.0);
  }

  return NoMoreDay::tests::CalculateStats(samples);
}

} // namespace

namespace NoMoreDay::tests {

TEST_CASE("[Performance] LightingPass - GPU Timer Query") {
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    NoMoreDay::utils::GPUUtils::Initialize();
  }

  LightingGpuTimerQueryApi queryApi;
  REQUIRE(queryApi.Initialize());

  auto &qm = NoMoreDay::render::core::QualityTierManager::Get();
  qm.ForceTier(NoMoreDay::render::core::QualityTier::Ultra);

  auto &cfg = const_cast<NoMoreDay::render::core::RenderConfig &>(qm.GetConfig());
  const auto originalConfig = cfg;
  cfg.dynamicLightingEnabled = true;
  cfg.ambientIntensity = 0.2f;

  NoMoreDay::render::lighting::LightManager::Get().Initialize();

  NoMoreDay::render::passes::LightingPass pass;
  REQUIRE(pass.Initialize());

  auto hdr = NoMoreDay::render::resources::FramebufferManager::Create(1280, 720,
                                                                       kLightingGLRgba16f);
  REQUIRE(hdr.IsValid());

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  NoMoreDay::render::graph::RenderContext context = {};
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;

  constexpr int kFrames = 120;

  entt::registry lights64;
  PopulateLights(lights64, 64);
  cfg.maxLights = 64;
  const auto stats64 = MeasureLightingGpuMs(pass, context, lights64, camera, 64,
                                            kFrames, queryApi);

  entt::registry lights128;
  PopulateLights(lights128, 128);
  cfg.maxLights = 128;
  const auto stats128 = MeasureLightingGpuMs(pass, context, lights128, camera, 128,
                                             kFrames, queryApi);

  entt::registry lights256;
  PopulateLights(lights256, 256);
  cfg.maxLights = 256;
  const auto stats256 = MeasureLightingGpuMs(pass, context, lights256, camera, 256,
                                             kFrames, queryApi);

  DOCTEST_MESSAGE("Lighting(64) mean(ms)=", stats64.mean_ms,
                  ", p99(ms)=", stats64.p99_ms, ", target<=0.5ms");
  DOCTEST_MESSAGE("Lighting(128) mean(ms)=", stats128.mean_ms,
                  ", p99(ms)=", stats128.p99_ms, ", target<=0.8ms");
  DOCTEST_MESSAGE("Lighting(256) mean(ms)=", stats256.mean_ms,
                  ", p99(ms)=", stats256.p99_ms, ", target<=1.0ms");

  CHECK(stats64.mean_ms >= 0.0);
  CHECK(stats128.mean_ms >= 0.0);
  CHECK(stats256.mean_ms >= 0.0);

  CHECK(stats64.mean_ms <= 0.5);
  CHECK(stats128.mean_ms <= 0.8);
  CHECK(stats256.mean_ms <= 1.0);

  cfg = originalConfig;

  pass.Shutdown();
  NoMoreDay::render::lighting::LightManager::Get().Shutdown();
  NoMoreDay::render::resources::FramebufferManager::Destroy(hdr);
}

} // namespace NoMoreDay::tests
