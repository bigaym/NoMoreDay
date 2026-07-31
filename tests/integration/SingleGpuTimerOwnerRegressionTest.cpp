#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/debug/RenderProfiler.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/UIWorldPass.hpp"
#include "engine/render/passes/VFXPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/TransientResourcePool.hpp"

#include "app/SharedContext.hpp"
#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {
constexpr uint32_t kS1aGlFramebuffer = 0x8D40;
constexpr uint32_t kS1aGlRgba16f = 0x881A;
constexpr GLenum kS1aGlInvalidOperation = 0x0502;

bool S1aEnsureGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "S1a Single Timer Owner Regression Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  return NoMoreDay::utils::GPUUtils::IsInitialized();
}

std::vector<GLenum> S1aDrainGlErrors() {
  std::vector<GLenum> errors;
  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    errors.push_back(err);
  }
  return errors;
}
} // namespace

// Regression: RenderProfiler must not open a second GL_TIME_ELAPSED query while
// GPUTimerQueryRing::BeginPass owns the timer. The old code issued glBeginQuery
// from both ring and profiler -> GL_INVALID_OPERATION (0x0502). After S1a the
// profiler is CPU-only and reports GPU stats as Unavailable.
TEST_CASE("[Integration] S1a - RenderProfiler CPU-only: no second GL_TIME_ELAPSED owner") {
  using namespace NoMoreDay;
  using namespace NoMoreDay::render;
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::debug;

  if (!S1aEnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping S1a CPU-only profiler test");
  }

  (void)S1aDrainGlErrors();

  RenderGraph graph;
  graph.AddPass(std::make_shared<passes::ScenePass>());
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::UIWorldPass>());
  graph.AddPass(std::make_shared<passes::CompositePass>(
      RenderResourceTag::SceneHdrColor, RenderOwnerTag::UIWorld));
  CHECK_NOTHROW(graph.Build());
  CHECK(graph.GetPassCount() == 4);

  auto hdr = FramebufferManager::Create(256, 256, kS1aGlRgba16f, true);
  REQUIRE(hdr.IsValid());

  RenderProfiler profiler;
  TransientResourcePool transientPool;
  render::graph::RenderContext context = {};
  entt::registry registry;
  SharedContext shared;
  Camera2D camera{};
  camera.zoom = 1.0f;

  context.registry = &registry;
  context.shared = &shared;
  context.camera = &camera;
  context.transientPool = &transientPool;
  context.qualityManager = &render::core::QualityTierManager::Get();
  context.hdrSceneBuffer = hdr;
  context.renderProfiler = &profiler;
  REQUIRE(context.IsValid());

  utils::GPUUtils::BindFramebuffer(kS1aGlFramebuffer, hdr.fbo);
  profiler.BeginFrame();
  constexpr int kFrames = 8;
  for (int f = 0; f < kFrames; ++f) {
    transientPool.BeginFrame();
    graph.Execute(context);
    transientPool.EndFrame();
  }
  profiler.EndFrame();
  utils::GPUUtils::BindFramebuffer(kS1aGlFramebuffer, 0);

  const std::vector<GLenum> errors = S1aDrainGlErrors();
  for (GLenum err : errors) {
    CAPTURE(err);
  }
  const bool hasInvalidOperation =
      std::find(errors.begin(), errors.end(), kS1aGlInvalidOperation) != errors.end();
  CHECK_FALSE(hasInvalidOperation);

  CHECK_FALSE(profiler.IsGpuTimingAvailable());
  profiler.UpdateStats();
  const auto &allStats = profiler.GetAllStats();
  const std::array<RenderPassId, 4> kExecutedPasses = {
      RenderPassId::Scene, RenderPassId::VFX, RenderPassId::UIWorld,
      RenderPassId::Composite};
  for (RenderPassId passId : kExecutedPasses) {
    const auto &stats = allStats[static_cast<size_t>(passId)];
    CAPTURE(static_cast<uint32_t>(passId));
    CHECK(stats.gpuState == QueryState::Unavailable);
    CHECK(stats.gpuMeanMs == 0.0f);
    CHECK(stats.gpuP95Ms == 0.0f);
    CHECK(stats.cpuMeanMs > 0.0f);
  }

  FramebufferManager::Destroy(hdr);
}

// Regression: the gate's sample loop must not wrap the timer ring in its own
// BeginFrame/EndFrame. RenderGraph::Execute is the single frame owner, so the
// ring advances exactly once per RenderSystem::render (no slot overwrite).
TEST_CASE("[Integration] S1a - Gate loop single ring frame owner (no slot overwrite)") {
  using namespace NoMoreDay;
  using namespace NoMoreDay::render;

  if (!S1aEnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping S1a gate frame-owner test");
  }

  (void)S1aDrainGlErrors();

  auto hdr = resources::FramebufferManager::Create(256, 256, kS1aGlRgba16f, true);
  REQUIRE(hdr.IsValid());

  entt::registry registry;
  SharedContext shared;
  Camera2D camera{};
  camera.zoom = 1.0f;

  auto &ring = debug::GPUTimerQueryRing::Get();
  // Reset the shared singleton so the frame counter starts fresh and the
  // advance-per-render is measurable (aggregate results lag the counter).
  ring.Shutdown();
  ring.Initialize();
  const auto before = ring.GetFrameResult();

  constexpr int kFrames = 40;
  for (int f = 0; f < kFrames; ++f) {
    utils::GPUUtils::BindFramebuffer(kS1aGlFramebuffer, hdr.fbo);
    RenderSystem::render(registry, shared, camera);
    utils::GPUUtils::BindFramebuffer(kS1aGlFramebuffer, 0);
    ring.PollReadyQueries();
  }

  auto frameResult = ring.GetFrameResult();
  for (int attempt = 0; attempt < 60 && frameResult.state != debug::QueryState::Valid;
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ring.PollReadyQueries();
    frameResult = ring.GetFrameResult();
  }

  const std::vector<GLenum> errors = S1aDrainGlErrors();
  for (GLenum err : errors) {
    CAPTURE(err);
  }
  const bool hasInvalidOperation =
      std::find(errors.begin(), errors.end(), kS1aGlInvalidOperation) != errors.end();
  CHECK_FALSE(hasInvalidOperation);

  CHECK(frameResult.state == debug::QueryState::Valid);
  CHECK(frameResult.frameIndex > before.frameIndex);
  CHECK(frameResult.frameIndex <= static_cast<uint64_t>(kFrames));
  CHECK(ring.GetValidFrameP95Ms() >= 0.0);

  resources::FramebufferManager::Destroy(hdr);
}
