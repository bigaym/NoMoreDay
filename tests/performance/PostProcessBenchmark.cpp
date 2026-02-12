#include "BenchmarkUtils.hpp"
#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/passes/PostProcessPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "GLFW/glfw3.h"
#include <cstdint>

namespace {
constexpr uint32_t kGLRgba16f = 0x881A;
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
}

namespace NoMoreDay::tests {

static BenchmarkStats MeasureGpuTimeMs(
    NoMoreDay::render::passes::PostProcessPass &pass,
    NoMoreDay::render::graph::RenderContext &context, int frames,
    GpuTimerQueryApi &api) {
  std::vector<double> samples;
  samples.reserve(static_cast<size_t>(frames));

  for (int i = 0; i < frames; ++i) {
    uint32_t query = 0;
    api.genQueries(1, &query);
    api.beginQuery(kGLTimeElapsed, query);
    pass.Execute(context);
    api.endQuery(kGLTimeElapsed);

    uint64_t elapsedNs = 0;
    api.getQueryObjectUi64v(query, kGLQueryResult, &elapsedNs);
    api.deleteQueries(1, &query);
    samples.push_back(static_cast<double>(elapsedNs) / 1'000'000.0);
  }

  return CalculateStats(samples);
}

TEST_CASE("[Performance] PostProcess - GPU Timer Query") {
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    NoMoreDay::utils::GPUUtils::Initialize();
  }

  GpuTimerQueryApi queryApi;
  REQUIRE(queryApi.Initialize());

  auto &qm = NoMoreDay::render::core::QualityTierManager::Get();
  NoMoreDay::render::passes::PostProcessPass pass;
  REQUIRE(pass.Initialize());

  auto hdr = NoMoreDay::render::resources::FramebufferManager::Create(1280, 720,
                                                                       kGLRgba16f);
  REQUIRE(hdr.IsValid());

  NoMoreDay::render::graph::RenderContext context = {};
  context.qualityManager = &qm;
  context.hdrSceneBuffer = hdr;

  constexpr int kFrames = 120;
  qm.ForceTier(NoMoreDay::render::core::QualityTier::Ultra);

  auto &cfg = const_cast<NoMoreDay::render::core::RenderConfig &>(qm.GetConfig());
  const auto originalConfig = cfg;

  NoMoreDay::render::core::RenderConfig tonemapOnly = originalConfig;
  tonemapOnly.bloomEnabled = false;
  tonemapOnly.bloomMipLevels = 0;
  tonemapOnly.fxaaEnabled = false;
  tonemapOnly.vignetteEnabled = false;

  NoMoreDay::render::core::RenderConfig bloomAndTonemap = tonemapOnly;
  bloomAndTonemap.bloomEnabled = true;
  bloomAndTonemap.bloomMipLevels = 7;
  bloomAndTonemap.bloomThreshold = originalConfig.bloomThreshold;
  bloomAndTonemap.bloomIntensity = originalConfig.bloomIntensity;
  bloomAndTonemap.bloomKnee = originalConfig.bloomKnee;

  NoMoreDay::render::core::RenderConfig fullPipeline = bloomAndTonemap;
  fullPipeline.fxaaEnabled = true;
  fullPipeline.vignetteEnabled = true;
  fullPipeline.vignetteIntensity = originalConfig.vignetteIntensity;
  fullPipeline.vignetteRadius = originalConfig.vignetteRadius;

  cfg = tonemapOnly;
  const auto tonemapStats = MeasureGpuTimeMs(pass, context, kFrames, queryApi);

  cfg = bloomAndTonemap;
  const auto bloomTonemapStats = MeasureGpuTimeMs(pass, context, kFrames, queryApi);

  cfg = fullPipeline;
  const auto fullStats = MeasureGpuTimeMs(pass, context, kFrames, queryApi);

  const double bloomMean = bloomTonemapStats.mean_ms - tonemapStats.mean_ms;
  const double bloomP99 = bloomTonemapStats.p99_ms - tonemapStats.p99_ms;
  const double postMean = fullStats.mean_ms - bloomTonemapStats.mean_ms;
  const double postP99 = fullStats.p99_ms - bloomTonemapStats.p99_ms;

  DOCTEST_MESSAGE("TonemapOnly mean(ms)=", tonemapStats.mean_ms,
                  ", p99(ms)=", tonemapStats.p99_ms);
  DOCTEST_MESSAGE("Bloom+Tonemap mean(ms)=", bloomTonemapStats.mean_ms,
                  ", p99(ms)=", bloomTonemapStats.p99_ms);
  DOCTEST_MESSAGE("FullPipeline mean(ms)=", fullStats.mean_ms,
                  ", p99(ms)=", fullStats.p99_ms);
  DOCTEST_MESSAGE("Bloom estimated mean(ms)=", bloomMean,
                  ", p99(ms)=", bloomP99);
  DOCTEST_MESSAGE("Tonemap+FXAA+Vignette estimated delta mean(ms)=", postMean,
                  ", p99(ms)=", postP99);

  CHECK(tonemapStats.mean_ms >= 0.0);
  CHECK(bloomTonemapStats.mean_ms >= tonemapStats.mean_ms);
  CHECK(fullStats.mean_ms >= bloomTonemapStats.mean_ms);
  CHECK(bloomMean >= 0.0);
  CHECK(postMean >= 0.0);

  cfg = originalConfig;

  pass.Shutdown();
  NoMoreDay::render::resources::FramebufferManager::Destroy(hdr);
}

} // namespace NoMoreDay::tests
