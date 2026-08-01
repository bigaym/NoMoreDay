#include "BenchmarkUtils.hpp"
#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "engine/vfx/VFXTypes.hpp"
#include "game/components/Common.hpp"
#include "game/render/EmissiveStampAdapter.hpp"

#include "GLFW/glfw3.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kHdrRgba16f = 0x881A;
constexpr uint32_t kDistanceR16f = 0x822D;
constexpr uint32_t kRadianceGLTimeElapsed = 0x88BF;
constexpr uint32_t kRadianceGLQueryResult = 0x8866;
constexpr int kBenchmarkWidth = 1920;
constexpr int kBenchmarkHeight = 1080;

struct RadianceGpuTimerQueryApi {
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

enum class GiScenario : uint8_t {
  Cave = 0,
  Town = 1,
  Forest = 2,
};

const char *ScenarioName(const GiScenario scenario) {
  switch (scenario) {
  case GiScenario::Cave:
    return "cave";
  case GiScenario::Town:
    return "town";
  case GiScenario::Forest:
    return "forest";
  }
  return "unknown";
}

void PopulateScenario(entt::registry &registry, const GiScenario scenario,
                      const int materialId) {
  registry.clear();

  int count = 0;
  float spacing = 0.0f;
  float startX = 0.0f;
  float startY = 0.0f;
  switch (scenario) {
  case GiScenario::Cave:
    count = 24;
    spacing = 62.0f;
    startX = -380.0f;
    startY = -260.0f;
    break;
  case GiScenario::Town:
    count = 40;
    spacing = 54.0f;
    startX = -440.0f;
    startY = -320.0f;
    break;
  case GiScenario::Forest:
    count = 56;
    spacing = 48.0f;
    startX = -520.0f;
    startY = -380.0f;
    break;
  }

  const int cols = std::max(1, static_cast<int>(std::sqrt(static_cast<float>(count))));
  for (int i = 0; i < count; ++i) {
    const int row = i / cols;
    const int col = i % cols;
    const float jitter = static_cast<float>((i * 17) % 11) - 5.0f;
    const float x = startX + static_cast<float>(col) * spacing + jitter;
    const float y = startY + static_cast<float>(row) * spacing - jitter;

    const entt::entity entity = registry.create();
    registry.emplace<Position>(entity, x, y);
    registry.emplace<Radius>(entity, 18.0f + static_cast<float>(i % 4) * 4.0f);
    registry.emplace<NoMoreDay::vfx::ActiveMaterialSwap>(
        entity, NoMoreDay::vfx::ActiveMaterialSwap{materialId, 0.2f, 0.2f});
  }
}

uint64_t EstimateRayWork(const int width, const int height, const uint32_t levels,
                         const bool halfResolution) {
  const int baseWidth = halfResolution ? std::max(1, (width + 1) / 2) : width;
  const int baseHeight = halfResolution ? std::max(1, (height + 1) / 2) : height;

  uint64_t total = 0u;
  for (uint32_t level = 0; level < levels; ++level) {
    const int levelWidth = std::max(1, baseWidth >> static_cast<int>(level));
    const int levelHeight = std::max(1, baseHeight >> static_cast<int>(level));

    uint32_t rays = 12u;
    if (levels >= 6u && level == 0u) {
      rays = 8u;
    } else if (level <= 1u) {
      rays = 4u;
    } else if (level <= 3u) {
      rays = 8u;
    }
    total += static_cast<uint64_t>(levelWidth) * static_cast<uint64_t>(levelHeight) *
             static_cast<uint64_t>(rays);
  }
  return total;
}

NoMoreDay::tests::BenchmarkStats MeasureRadianceTier(
    NoMoreDay::render::passes::RadianceCascadesPass &pass,
    NoMoreDay::render::graph::RenderContext &context,
    Camera2D &camera,
    NoMoreDay::render::core::RenderConfig &cfg, const uint32_t cascadeLevels,
    const bool halfResolution, const bool holographicEnabled,
    RadianceGpuTimerQueryApi &queryApi) {
  cfg.giEnabled = true;
  cfg.giCascadeLevels = cascadeLevels;
  cfg.giHalfResolution = halfResolution;
  cfg.giHolographicEnabled = holographicEnabled;
  cfg.giIntensity = 1.0f;
  cfg.giTemporalWeight = 0.9f;
  cfg.giSdfUpdateInterval = halfResolution ? 2u : 1u;

  constexpr int kWarmupFrames = 20;
  constexpr int kBenchFrames = 60;
  std::vector<double> samples;
  samples.reserve(kBenchFrames);

  for (int frame = 0; frame < (kWarmupFrames + kBenchFrames); ++frame) {
    context.giEmissiveTexture = 0u;
    context.giRadianceTexture = 0u;
    camera.target.x = std::sin(static_cast<float>(frame) * 0.07f) * 180.0f;
    camera.target.y = std::cos(static_cast<float>(frame) * 0.05f) * 120.0f;

    const uint64_t previousSnapshotVersion =
        pass.GetVfxEmissionSnapshotVersion();
    REQUIRE(pass.PrepareVfxEmissionSnapshot(context));
    CHECK(pass.GetVfxEmissionSnapshotVersion() == previousSnapshotVersion + 1u);

    const bool measure = frame >= kWarmupFrames;
    uint32_t query = 0u;
    if (measure) {
      queryApi.genQueries(1, &query);
      queryApi.beginQuery(kRadianceGLTimeElapsed, query);
    }

    pass.Execute(context);

    if (measure) {
      queryApi.endQuery(kRadianceGLTimeElapsed);
      uint64_t elapsedNs = 0u;
      queryApi.getQueryObjectUi64v(query, kRadianceGLQueryResult, &elapsedNs);
      queryApi.deleteQueries(1, &query);
      samples.push_back(static_cast<double>(elapsedNs) / 1'000'000.0);
    }

    CHECK(context.giEmissiveTexture != 0u);
    CHECK(context.giRadianceTexture != 0u);
  }

  return NoMoreDay::tests::CalculateStats(samples);
}

double Median(std::vector<double> values) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}

bool Is4070Class(std::string renderer) {
  std::transform(renderer.begin(), renderer.end(), renderer.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return renderer.find("4070") != std::string::npos;
}

} // namespace

TEST_CASE("[Performance] RadianceCascades - Tier and Holographic Matrix") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  RadianceGpuTimerQueryApi queryApi = {};
  REQUIRE(queryApi.Initialize());

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());

  auto &materialManager = render::MaterialManager::Get();
  materialManager.Shutdown();
  materialManager.Initialize();
  materialManager.LoadFromJson("assets/data/materials_vfx.json");
  int materialId = materialManager.GetMaterialId("FireGlow");
  if (materialId <= 0) {
    materialId = 2;
  }
  REQUIRE(materialId > 0);

  entt::registry registry;
  ResourceManager resources;

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {static_cast<float>(kBenchmarkWidth) * 0.5f,
                   static_cast<float>(kBenchmarkHeight) * 0.5f};

  auto hdr = render::resources::FramebufferManager::Create(kBenchmarkWidth,
                                                           kBenchmarkHeight,
                                                           kHdrRgba16f, false);
  auto distanceField = render::resources::FramebufferManager::Create(
      kBenchmarkWidth, kBenchmarkHeight, kDistanceR16f, false);
  REQUIRE(hdr.IsValid());
  REQUIRE(distanceField.IsValid());

  render::graph::RenderContext context = {};
  context.registry = &registry;
  context.resources = &resources;
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;
  context.giDistanceFieldTexture = distanceField.colorTexture;
  context.giDistanceFieldWidth = distanceField.width;
  context.giDistanceFieldHeight = distanceField.height;

  render::passes::RadianceCascadesPass pass;

  std::vector<NoMoreDay::components::EmissiveStampInput> emissiveStamps;

  std::vector<double> highStandardMeans;
  std::vector<double> ultraStandardMeans;
  std::vector<double> highHolographicMeans;
  std::vector<double> ultraHolographicMeans;
  std::vector<double> ultraToHighWorkRatios;
  highStandardMeans.reserve(3);
  ultraStandardMeans.reserve(3);
  highHolographicMeans.reserve(3);
  ultraHolographicMeans.reserve(3);
  ultraToHighWorkRatios.reserve(3);

  constexpr std::array<GiScenario, 3> kScenarios = {
      GiScenario::Cave, GiScenario::Town, GiScenario::Forest};
  for (const auto scenario : kScenarios) {
    PopulateScenario(registry, scenario, materialId);

    NoMoreDay::EmissiveProjection emissiveProjection =
        NoMoreDay::EmissiveStampAdapter::BuildEmissiveStamps(registry);
    emissiveStamps = std::move(emissiveProjection.stamps);
    context.emissiveStamps =
        emissiveStamps.empty() ? nullptr : emissiveStamps.data();
    context.emissiveStampCount = static_cast<uint32_t>(emissiveStamps.size());

    const auto highStandard =
        MeasureRadianceTier(pass, context, camera, cfg, 4u, true, false, queryApi);
    const auto ultraStandard =
        MeasureRadianceTier(pass, context, camera, cfg, 6u, false, false, queryApi);
    const auto highHolographic =
        MeasureRadianceTier(pass, context, camera, cfg, 4u, true, true, queryApi);
    const auto ultraHolographic =
        MeasureRadianceTier(pass, context, camera, cfg, 6u, false, true, queryApi);

    const uint64_t highWork = EstimateRayWork(kBenchmarkWidth, kBenchmarkHeight, 4u, true);
    const uint64_t ultraWork =
        EstimateRayWork(kBenchmarkWidth, kBenchmarkHeight, 6u, false);
    const double workRatio =
        static_cast<double>(ultraWork) / std::max<double>(1.0, static_cast<double>(highWork));

    DOCTEST_MESSAGE("Radiance scenario=", ScenarioName(scenario),
                    " highStd=", highStandard.mean_ms, "ms",
                    " ultraStd=", ultraStandard.mean_ms, "ms",
                    " highHolo=", highHolographic.mean_ms, "ms",
                    " ultraHolo=", ultraHolographic.mean_ms, "ms",
                    " workRatio=", workRatio);

    CHECK(highStandard.mean_ms > 0.0);
    CHECK(ultraStandard.mean_ms > 0.0);
    CHECK(highHolographic.mean_ms > 0.0);
    CHECK(ultraHolographic.mean_ms > 0.0);
    CHECK(workRatio >= 2.0);
    CHECK(ultraStandard.mean_ms <= highStandard.mean_ms * 4.0);
    CHECK(ultraHolographic.mean_ms <= ultraStandard.mean_ms * 1.6);

    highStandardMeans.push_back(highStandard.mean_ms);
    ultraStandardMeans.push_back(ultraStandard.mean_ms);
    highHolographicMeans.push_back(highHolographic.mean_ms);
    ultraHolographicMeans.push_back(ultraHolographic.mean_ms);
    ultraToHighWorkRatios.push_back(workRatio);
  }

  const double highStandardMedian = Median(highStandardMeans);
  const double ultraStandardMedian = Median(ultraStandardMeans);
  const double highHolographicMedian = Median(highHolographicMeans);
  const double ultraHolographicMedian = Median(ultraHolographicMeans);
  const double workRatioMedian = Median(ultraToHighWorkRatios);

  const double strictUltraBudgetMs = 2.5;
  const double relaxedUltraBudgetMs = 6.0;
  const bool strictBudget = Is4070Class(qm.GetRendererString());
  const double ultraBudgetMs = strictBudget ? strictUltraBudgetMs : relaxedUltraBudgetMs;

  DOCTEST_MESSAGE("Radiance median highStd=", highStandardMedian, "ms",
                  " ultraStd=", ultraStandardMedian, "ms",
                  " highHolo=", highHolographicMedian, "ms",
                  " ultraHolo=", ultraHolographicMedian, "ms",
                  " workRatio=", workRatioMedian, " strictBudget=", strictBudget);

  CHECK(workRatioMedian >= 2.0);
  CHECK(ultraStandardMedian <= ultraBudgetMs);
  CHECK(ultraHolographicMedian <= ultraStandardMedian * 1.6);
  CHECK(highHolographicMedian <= highStandardMedian * 1.6);

  std::cout << "RELEASE_GATE_METRIC gi_high_standard_mean_ms=" << highStandardMedian
            << "\n";
  std::cout << "RELEASE_GATE_METRIC gi_ultra_standard_mean_ms=" << ultraStandardMedian
            << "\n";
  std::cout << "RELEASE_GATE_METRIC gi_high_holographic_mean_ms="
            << highHolographicMedian << "\n";
  std::cout << "RELEASE_GATE_METRIC gi_ultra_holographic_mean_ms="
            << ultraHolographicMedian << "\n";
  std::cout << "RELEASE_GATE_METRIC gi_ultra_high_work_ratio=" << workRatioMedian
            << "\n";

  pass.Shutdown();
  render::resources::FramebufferManager::Destroy(distanceField);
  render::resources::FramebufferManager::Destroy(hdr);
  materialManager.Shutdown();
}
