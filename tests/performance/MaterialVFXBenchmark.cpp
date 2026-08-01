#include "BenchmarkUtils.hpp"
#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/MaterialDefs.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/passes/DistortionPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/vfx/VFXPlayerComponent.hpp"
#include "engine/vfx/VFXSequenceManager.hpp"
#include "game/vfx/VFXSequencerSystem.hpp"
#include "game/components/Common.hpp"

#include "GLFW/glfw3.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

namespace {
constexpr uint32_t kGLRgba8 = 0x8058;
constexpr uint32_t kMaterialVfxGLTimeElapsed = 0x88BF;
constexpr uint32_t kMaterialVfxGLQueryResult = 0x8866;
constexpr int kVfxPlayers = 100;

struct MaterialVfxGpuTimerQueryApi {
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

std::filesystem::path MakeStressSequencePath() {
  const auto dir = std::filesystem::path("bin") / "tmp_vfx_perf";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / "material_swap_distortion_stress.json";
}

void WriteStressSequenceFile(const std::filesystem::path &path) {
  using nlohmann::json;
  json events = json::array();
  for (int i = 0; i < 14; ++i) {
    events.push_back({
        {"time", 0.0},
        {"type", "MaterialSwap"},
        {"anchor", "Caster"},
        {"params", {{"materialId", 7 + (i % 2)}, {"duration", 0.08}}},
    });
  }

  constexpr int kDistortionCount =
      NoMoreDay::render::passes::DistortionPass::MAX_DISTORTION_SOURCES + 16;
  for (int i = 0; i < kDistortionCount; ++i) {
    events.push_back({
        {"time", 0.0},
        {"type", "Distortion"},
        {"anchor", "Caster"},
        {"params",
         {{"radius", 120.0 + i},
          {"strength", 0.20 + 0.01 * i},
          {"duration", 0.35},
          {"speed", 300.0}}},
    });
  }

  const json sequence = {
      {"vfx_schema_version", 1},
      {"name", "MaterialSwapDistortionStress"},
      {"duration", 0.25},
      {"events", events},
  };

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out << sequence.dump(2);
}
} // namespace

namespace NoMoreDay::tests {

TEST_CASE("[Performance] MaterialVFX - MaterialManager SyncToGPU") {
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    NoMoreDay::utils::GPUUtils::Initialize();
  }

  auto &manager = NoMoreDay::render::MaterialManager::Get();
  manager.Shutdown();
  manager.Initialize();
  manager.LoadFromJson("assets/data/materials_vfx.json");

  constexpr int kWarmupFrames = 30;
  constexpr int kBenchFrames = 180;
  std::vector<double> samples;
  samples.reserve(kBenchFrames);

  for (int i = 0; i < (kWarmupFrames + kBenchFrames); ++i) {
    NoMoreDay::render::MaterialInstance mat = NoMoreDay::render::MaterialPresets::FireGlow();
    const float t = static_cast<float>(i % 60) / 60.0f;
    mat.baseColorR = 0.4f + 0.5f * t;
    mat.baseColorG = 0.2f + 0.4f * (1.0f - t);
    mat.baseColorB = 0.1f + 0.6f * t;
    mat.emissiveIntensity = 1.0f + 2.0f * t;
    manager.RegisterMaterial(mat, "Benchmark_DynamicMaterial");

    const auto start = std::chrono::high_resolution_clock::now();
    manager.SyncToGPU();
    const auto end = std::chrono::high_resolution_clock::now();

    if (i >= kWarmupFrames) {
      samples.push_back(
          std::chrono::duration<double, std::milli>(end - start).count());
    }
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("MaterialManager::SyncToGPU", stats, "< 0.05ms");
  CHECK(stats.mean_ms < 0.05);

  manager.Shutdown();
}

TEST_CASE("[Performance] MaterialVFX - VFXSequencer 100 Players") {
  auto &qm = NoMoreDay::render::core::QualityTierManager::Get();
  qm.ForceTier(NoMoreDay::render::core::QualityTier::Ultra);

  auto &sequenceManager = NoMoreDay::vfx::VFXSequenceManager::Get();
  sequenceManager.Shutdown();
  sequenceManager.Initialize();
  sequenceManager.LoadFromJson("assets/vfx");

  const int sequenceId = sequenceManager.GetSequenceId("SwordSlash");
  REQUIRE(sequenceId >= 0);
  const auto *sequence = sequenceManager.GetSequence(sequenceId);
  REQUIRE(sequence != nullptr);
  REQUIRE(sequence->duration > 0.0f);

  entt::registry registry;
  for (int i = 0; i < kVfxPlayers; ++i) {
    const auto entity = registry.create();
    registry.emplace<Position>(entity, 100.0f + static_cast<float>(i), 120.0f);

    NoMoreDay::vfx::VFXPlayerComponent player = {};
    player.sequenceId = sequenceId;
    player.elapsed = 0.0f;
    player.nextEventIdx = static_cast<int>(sequence->events.size());
    player.target = entt::null;
    player.loop = true;
    player.active = true;
    registry.emplace<NoMoreDay::vfx::VFXPlayerComponent>(entity, player);
  }

  constexpr int kWarmupFrames = 60;
  constexpr int kBenchFrames = 300;
  constexpr float kBenchmarkDt = 0.0005f;

  for (int i = 0; i < kWarmupFrames; ++i) {
    NoMoreDay::vfx::VFXSequencerSystem::Update(registry, kBenchmarkDt);
  }

  std::vector<double> samples;
  samples.reserve(kBenchFrames);
  for (int i = 0; i < kBenchFrames; ++i) {
    const auto start = std::chrono::high_resolution_clock::now();
    NoMoreDay::vfx::VFXSequencerSystem::Update(registry, kBenchmarkDt);
    const auto end = std::chrono::high_resolution_clock::now();
    samples.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("VFXSequencerSystem::Update (100 players)", stats, "< 0.1ms");
  CHECK(stats.mean_ms < 0.1);

  sequenceManager.Shutdown();
}

TEST_CASE("[Performance] MaterialVFX - DistortionPass 2K@8 Sources") {
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    NoMoreDay::utils::GPUUtils::Initialize();
  }

  MaterialVfxGpuTimerQueryApi queryApi = {};
  REQUIRE(queryApi.Initialize());

  auto &qm = NoMoreDay::render::core::QualityTierManager::Get();
  qm.ForceTier(NoMoreDay::render::core::QualityTier::High);

  NoMoreDay::render::passes::DistortionPass pass;
  REQUIRE(pass.Initialize());

  auto inputBuffer = NoMoreDay::render::resources::FramebufferManager::Create(
      2560, 1440, kGLRgba8, false);
  REQUIRE(inputBuffer.IsValid());
  pass.SetInputBuffer(&inputBuffer);

  Camera2D camera = {};
  camera.target = {1280.0f, 720.0f};
  camera.offset = {1280.0f, 720.0f};
  camera.rotation = 0.0f;
  camera.zoom = 1.0f;

  NoMoreDay::render::graph::RenderContext context = {};
  context.camera = &camera;
  context.qualityManager = &qm;

  constexpr int kWarmupFrames = 60;
  constexpr int kBenchFrames = 180;
  auto submitSources = [&pass]() {
    for (int i = 0; i < 8; ++i) {
      pass.AddDistortionSource(640.0f + static_cast<float>(i * 32), 360.0f, 96.0f,
                               0.35f);
    }
  };

  for (int i = 0; i < kWarmupFrames; ++i) {
    submitSources();
    pass.Execute(context);
  }

  std::vector<double> samples;
  samples.reserve(kBenchFrames);
  for (int i = 0; i < kBenchFrames; ++i) {
    submitSources();
    uint32_t query = 0;
    queryApi.genQueries(1, &query);
    queryApi.beginQuery(kMaterialVfxGLTimeElapsed, query);
    pass.Execute(context);
    queryApi.endQuery(kMaterialVfxGLTimeElapsed);

    uint64_t elapsedNs = 0;
    queryApi.getQueryObjectUi64v(query, kMaterialVfxGLQueryResult, &elapsedNs);
    queryApi.deleteQueries(1, &query);
    samples.push_back(static_cast<double>(elapsedNs) / 1'000'000.0);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("DistortionPass::Execute (2K@8)", stats, "< 0.3ms");
  CHECK(stats.mean_ms < 0.3);

  NoMoreDay::render::resources::FramebufferManager::Destroy(inputBuffer);
  pass.Shutdown();
}

TEST_CASE("[Performance] MaterialVFX - MaterialSwap+Distortion Stress P95") {
  auto &qm = NoMoreDay::render::core::QualityTierManager::Get();
  qm.ForceTier(NoMoreDay::render::core::QualityTier::Ultra);

  const auto path = MakeStressSequencePath();
  WriteStressSequenceFile(path);

  auto &manager = NoMoreDay::vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(path.string()) == 1);
  REQUIRE(manager.GetSequenceId("MaterialSwapDistortionStress") >= 0);

  entt::registry registry;
  const entt::entity entity = registry.create();
  registry.emplace<Position>(entity, 0.0f, 0.0f);

  NoMoreDay::vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();

  constexpr int kWarmupFrames = 90;
  constexpr int kBenchFrames = 360;
  constexpr int kUpdateRepeats = 8;
  constexpr float kDt = 1.0f / 60.0f;

  for (int i = 0; i < kWarmupFrames; ++i) {
    for (int repeat = 0; repeat < kUpdateRepeats; ++repeat) {
      manager.Play(registry, entity, "MaterialSwapDistortionStress");
      NoMoreDay::vfx::VFXSequencerSystem::Update(registry, kDt);
    }
  }

  std::vector<double> samples;
  samples.reserve(kBenchFrames);
  for (int i = 0; i < kBenchFrames; ++i) {
    const auto start = std::chrono::high_resolution_clock::now();
    for (int repeat = 0; repeat < kUpdateRepeats; ++repeat) {
      manager.Play(registry, entity, "MaterialSwapDistortionStress");
      NoMoreDay::vfx::VFXSequencerSystem::Update(registry, kDt);
    }
    const auto end = std::chrono::high_resolution_clock::now();
    samples.push_back(
        std::chrono::duration<double, std::milli>(end - start).count() /
        static_cast<double>(kUpdateRepeats));
  }

  const BenchmarkStats stats = CalculateStats(samples);
  const double p95Ms = ComputePercentileMs(samples, 0.95);
  LOG_BENCHMARK("VFXSequencer MaterialSwap+Distortion Stress", stats, "P95 < 0.3ms");
  CHECK(p95Ms <= 0.3);
  CHECK(NoMoreDay::vfx::VFXSequencerSystem::GetActiveDistortionCountForTesting() <=
        static_cast<size_t>(
            NoMoreDay::render::passes::DistortionPass::MAX_DISTORTION_SOURCES));
  CHECK(NoMoreDay::vfx::VFXSequencerSystem::GetDistortionOverflowDropCountForTesting() +
            NoMoreDay::vfx::VFXSequencerSystem::GetDistortionOverflowEvictCountForTesting() >
        0);

  const double combatFps = 1000.0 / std::max(0.0001, stats.mean_ms);
  std::cout << "RELEASE_GATE_METRIC combat_180_fps=" << combatFps << "\n";

  manager.Shutdown();
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

} // namespace NoMoreDay::tests
