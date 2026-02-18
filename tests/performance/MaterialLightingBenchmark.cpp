#include "BenchmarkUtils.hpp"
#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/core/QualityTierManager.hpp"

#include "GLFW/glfw3.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

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

NoMoreDay::tests::BenchmarkStats MeasureParticleRenderTier(
    NoMoreDay::render::core::QualityTier tier, GpuTimerQueryApi &queryApi,
    NoMoreDay::systems::GPUParticleSystem &particleSystem, const Camera2D &camera,
    NoMoreDay::render::core::QualityTierManager &qualityManager) {
  qualityManager.ForceTier(tier);

  constexpr int kWarmupFrames = 40;
  constexpr int kBenchFrames = 160;
  constexpr float kDt = 1.0f / 60.0f;

  std::vector<double> samples;
  samples.reserve(kBenchFrames);

  for (int i = 0; i < kWarmupFrames + kBenchFrames; ++i) {
    particleSystem.Update(kDt);

    const bool benchFrame = i >= kWarmupFrames;
    uint32_t query = 0;
    if (benchFrame) {
      queryApi.genQueries(1, &query);
      queryApi.beginQuery(kGLTimeElapsed, query);
    }
    particleSystem.Render(camera);
    if (benchFrame) {
      queryApi.endQuery(kGLTimeElapsed);
      uint64_t elapsedNs = 0;
      queryApi.getQueryObjectUi64v(query, kGLQueryResult, &elapsedNs);
      queryApi.deleteQueries(1, &query);
      samples.push_back(static_cast<double>(elapsedNs) / 1'000'000.0);
    }
  }

  return NoMoreDay::tests::CalculateStats(samples);
}

void SeedBenchmarkParticles(NoMoreDay::systems::GPUParticleSystem &particleSystem,
                            int materialId) {
  std::vector<NoMoreDay::components::GPUParticle> particles;
  particles.reserve(12000);
  for (int i = 0; i < 12000; ++i) {
    NoMoreDay::components::GPUParticle p = {};
    p.position = {static_cast<float>((i % 200) * 6), static_cast<float>((i / 200) * 6)};
    p.velocity = {0.0f, 0.0f};
    p.color = {255, 255, 255, 180};
    p.lifetime = 8.0f;
    p.maxLifetime = 8.0f;
    p.scale = 1.5f;
    p.flags = 0;
    particles.push_back(p);
  }
  particleSystem.EmitBatch(particles, materialId);
  particleSystem.Update(1.0f / 60.0f);
}

} // namespace

TEST_CASE("[Performance] Material Lighting - Tier overhead budgets") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  GpuTimerQueryApi queryApi = {};
  REQUIRE(queryApi.Initialize());

  auto &qualityManager = render::core::QualityTierManager::Get();
  qualityManager.Initialize("settings.json");
  const auto originalTier = qualityManager.GetTier();

  auto &materialManager = render::MaterialManager::Get();
  materialManager.Shutdown();
  materialManager.Initialize();
  materialManager.LoadFromJson("assets/data/materials_vfx.json");
  int materialId = materialManager.GetMaterialId("FireExplosion");
  if (materialId < 0) {
    materialId = materialManager.GetMaterialId("FireGlow");
  }
  REQUIRE(materialId >= 0);

  auto &particleSystem = systems::GPUParticleSystem::Get();
  particleSystem.Shutdown();
  particleSystem.Init(200000);
  REQUIRE(particleSystem.IsInitialized());

  SeedBenchmarkParticles(particleSystem, materialId);

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {600.0f, 360.0f};
  camera.offset = {640.0f, 360.0f};

  const auto lowStats = MeasureParticleRenderTier(render::core::QualityTier::Low,
                                                  queryApi, particleSystem,
                                                  camera, qualityManager);
  const auto mediumStats = MeasureParticleRenderTier(
      render::core::QualityTier::Medium, queryApi, particleSystem, camera,
      qualityManager);
  const auto highStats = MeasureParticleRenderTier(render::core::QualityTier::High,
                                                   queryApi, particleSystem,
                                                   camera, qualityManager);

  const double highOverheadMs = highStats.mean_ms - mediumStats.mean_ms;
  const double mediumDeltaMs = std::abs(mediumStats.mean_ms - lowStats.mean_ms);
  const double mediumBudgetMs = std::max(0.15, lowStats.mean_ms * 0.25);

  DOCTEST_MESSAGE("MaterialLighting low(ms)=", lowStats.mean_ms,
                  ", medium(ms)=", mediumStats.mean_ms,
                  ", high(ms)=", highStats.mean_ms,
                  ", highOverhead(ms)=", highOverheadMs,
                  ", mediumDelta(ms)=", mediumDeltaMs);

  CHECK(highOverheadMs <= 0.6);
  CHECK(mediumDeltaMs <= mediumBudgetMs);

  particleSystem.Shutdown();
  materialManager.Shutdown();
  qualityManager.ForceTier(originalTier);
}
