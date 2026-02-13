#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"

#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/particle/ForceFieldManager.hpp"
#include "engine/render/trail/GPUTrailRenderer.hpp"

#include <chrono>
#include <vector>

namespace NoMoreDay::tests {
namespace {

Camera2D MakeBenchmarkCamera() {
  Camera2D camera = {};
  camera.target = {static_cast<float>(GetScreenWidth()) * 0.5f,
                   static_cast<float>(GetScreenHeight()) * 0.5f};
  camera.offset = {0.0f, 0.0f};
  camera.rotation = 0.0f;
  camera.zoom = 1.0f;
  return camera;
}

} // namespace

TEST_CASE("[Performance] ParticleTrail - Scenario 1 Texture Particles 10k") {
  using namespace NoMoreDay::systems;
  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);

  auto &ps = GPUParticleSystem::Get();
  ps.Shutdown();
  ps.Init(120000);

  components::GPUParticle p =
      InkEffectHelper::CreateInkTrail({640.0f, 360.0f}, {0.0f, 0.0f}, 2.0f, 1.0f);
  p.textureIndex = 0;
  p.blendMode = 0;

  for (int i = 0; i < 10000; ++i) {
    ps.Emit(p);
  }
  ps.Update(1.0f / 60.0f);

  Camera2D camera = MakeBenchmarkCamera();
  std::vector<double> samples;
  samples.reserve(120);

  for (int i = 0; i < 120; ++i) {
    ps.Update(1.0f / 60.0f);
    BeginDrawing();
    auto t0 = std::chrono::high_resolution_clock::now();
    ps.Render(camera);
    auto t1 = std::chrono::high_resolution_clock::now();
    EndDrawing();
    samples.push_back(
        std::chrono::duration<double, std::milli>(t1 - t0).count());
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ParticleTrail Scenario1 (Texture 10k)", stats, "< 0.8ms");
  CHECK(stats.mean_ms < 0.8);

  ps.Shutdown();
}

TEST_CASE("[Performance] ParticleTrail - Scenario 2 ForceField 16 + Particle 50k") {
  using namespace NoMoreDay::systems;
  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);

  auto &ps = GPUParticleSystem::Get();
  ps.Shutdown();
  ps.Init(200000);

  auto &ff = render::ForceFieldManager::Get();
  ff.ClearAll();
  for (int i = 0; i < 16; ++i) {
    components::GPUForceField field = {};
    field.posX = 200.0f + 80.0f * static_cast<float>(i % 4);
    field.posY = 200.0f + 80.0f * static_cast<float>(i / 4);
    field.radius = 300.0f;
    field.strength = (i % 2 == 0) ? 120.0f : -90.0f;
    field.falloff = 1.0f;
    field.type = static_cast<uint32_t>(components::ForceFieldType::Radial);
    ff.AddForceField(field);
  }

  components::GPUParticle p =
      InkEffectHelper::CreateInkTrail({640.0f, 360.0f}, {20.0f, 0.0f}, 1.8f, 1.5f);
  for (int i = 0; i < 50000; ++i) {
    p.position.x = 200.0f + static_cast<float>(i % 800);
    p.position.y = 200.0f + static_cast<float>((i / 800) % 450);
    ps.Emit(p);
  }
  ps.Update(1.0f / 60.0f);

  std::vector<double> samples;
  samples.reserve(120);
  for (int i = 0; i < 120; ++i) {
    auto t0 = std::chrono::high_resolution_clock::now();
    ps.Update(1.0f / 60.0f);
    auto t1 = std::chrono::high_resolution_clock::now();
    samples.push_back(
        std::chrono::duration<double, std::milli>(t1 - t0).count());
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ParticleTrail Scenario2 (ForceField + 50k)", stats, "< 0.5ms");
  CHECK(stats.mean_ms < 0.5);

  ps.Shutdown();
}

TEST_CASE("[Performance] ParticleTrail - Scenario 3 GPU Trail 256x48") {
  auto &trail = render::GPUTrailRenderer::Get();
  trail.Shutdown();
  trail.Init(256, 48);

  components::GPUTrailHeader header = {};
  header.maxPoints = 48;
  header.maxLifetime = 0.8f;
  header.widthStart = 12.0f;
  header.widthEnd = 2.0f;
  header.colorStart = 0xFFFFFFFFu;
  header.colorEnd = 0x00FFFFFFu;

  for (int t = 0; t < 256; ++t) {
    const int id = trail.AllocateTrail(header);
    REQUIRE(id >= 0);
    for (int p = 0; p < 48; ++p) {
      const float x = 100.0f + static_cast<float>(p) * 10.0f;
      const float y = 50.0f + static_cast<float>(t) * 2.0f;
      trail.AppendPoint(id, {x, y}, {1.0f, 0.0f}, 8.0f, 0xFFFFFFFFu);
    }
  }

  Camera2D camera = MakeBenchmarkCamera();
  std::vector<double> samples;
  samples.reserve(120);
  for (int i = 0; i < 120; ++i) {
    trail.Update(1.0f / 60.0f);
    BeginDrawing();
    auto t0 = std::chrono::high_resolution_clock::now();
    trail.Render(camera);
    auto t1 = std::chrono::high_resolution_clock::now();
    EndDrawing();
    samples.push_back(
        std::chrono::duration<double, std::milli>(t1 - t0).count());
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ParticleTrail Scenario3 (Trail 256x48)", stats, "< 0.3ms");
  CHECK(stats.mean_ms < 0.3);

  trail.Shutdown();
}

TEST_CASE("[Performance] ParticleTrail - Scenario 4 SubEmitter 1k/frame") {
  using namespace NoMoreDay::systems;
  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);

  auto &ps = GPUParticleSystem::Get();
  ps.Shutdown();
  ps.Init(200000);

  auto runCase = [&](bool enableSubEmitter) {
    ps.Clear();
    std::vector<double> samples;
    samples.reserve(120);

    components::GPUParticle p = InkEffectHelper::CreateInkTrail(
        {640.0f, 360.0f}, {0.0f, 0.0f}, 1.0f, 0.02f);
    p.subEmitterType = enableSubEmitter ? 1 : 0;
    p.subEmitterParam = 1.0f;

    for (int frame = 0; frame < 120; ++frame) {
      for (int i = 0; i < 1000; ++i) {
        p.position = {200.0f + static_cast<float>(i % 400),
                      200.0f + static_cast<float>((i / 400) * 10)};
        ps.Emit(p);
      }

      auto t0 = std::chrono::high_resolution_clock::now();
      ps.Update(1.0f / 30.0f);
      auto t1 = std::chrono::high_resolution_clock::now();
      samples.push_back(
          std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    return CalculateStats(samples);
  };

  const BenchmarkStats baseline = runCase(false);
  const BenchmarkStats withSubEmit = runCase(true);
  const double dispatchOverheadMs =
      std::max(0.0, withSubEmit.mean_ms - baseline.mean_ms);

  LOG_WARN("ParticleTrail Scenario4 (SubEmitter 1k/frame): Baseline={:.3f}ms, "
           "WithSubEmit={:.3f}ms, Overhead={:.3f}ms (Target: < 0.2ms)",
           baseline.mean_ms, withSubEmit.mean_ms, dispatchOverheadMs);
  CHECK(dispatchOverheadMs < 0.2);

  ps.Shutdown();
}

} // namespace NoMoreDay::tests
