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
#include "engine/resource/ResourceManager.hpp"

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
// from both ring and profiler -> GL_INVALID_OPERATION (0x0502). After S1a/S1b the
// profiler never touches GL queries: it records CPU timing via BeginCpuPass and
// backfills the ring's GPU results through FlushRingToProfiler (four-state model).
TEST_CASE("[Integration] S1b - RenderProfiler single timer owner + four-state backfill") {
  using namespace NoMoreDay;
  using namespace NoMoreDay::render;
  using namespace NoMoreDay::render::graph;
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::debug;

  if (!S1aEnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping S1b single-owner backfill test");
  }

  (void)S1aDrainGlErrors();

  // Reset the shared ring singleton so the frame counter starts fresh and no
  // leftover per-pass results from earlier tests leak into the backfill.
  auto &ring = debug::GPUTimerQueryRing::Get();
  ring.Shutdown();
  ring.Initialize();

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
  ResourceManager resources;
  Camera2D camera{};
  camera.zoom = 1.0f;

  context.registry = &registry;
  context.resources = &resources;
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

  // Backfill path: poll + accept ready results (delayed ready). Loop until the
  // four executed passes surface as Valid (results arrive a few frames later).
  const std::array<RenderPassId, 4> kExecutedPasses = {
      RenderPassId::Scene, RenderPassId::VFX, RenderPassId::UIWorld,
      RenderPassId::Composite};
  bool allValid = false;
  for (int attempt = 0; attempt < 60; ++attempt) {
    profiler.FlushRingToProfiler();
    profiler.UpdateStats();
    const auto &stats = profiler.GetAllStats();
    allValid = true;
    for (RenderPassId passId : kExecutedPasses) {
      allValid = allValid &&
                 stats[static_cast<size_t>(passId)].gpuState == QueryState::Valid;
    }
    if (allValid) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  CHECK(allValid);

  const std::vector<GLenum> errors = S1aDrainGlErrors();
  for (GLenum err : errors) {
    CAPTURE(err);
  }
  const bool hasInvalidOperation =
      std::find(errors.begin(), errors.end(), kS1aGlInvalidOperation) != errors.end();
  CHECK_FALSE(hasInvalidOperation);

  // Real GL context present -> capability is available; four-state backfill
  // must have transitioned executed passes to Valid with a source frame.
  CHECK(profiler.IsGpuTimingAvailable());
  profiler.UpdateStats();
  const auto &allStats = profiler.GetAllStats();
  for (RenderPassId passId : kExecutedPasses) {
    const auto &stats = allStats[static_cast<size_t>(passId)];
    CAPTURE(static_cast<uint32_t>(passId));
    CHECK(stats.gpuState == QueryState::Valid);
    CHECK(stats.frameIndex > 0);
    // A ready result is legitimate even if the driver measures 0.0ms (software
    // GL/WARP can report zero-duration queries); state+frame prove backfill.
    CHECK(stats.gpuMeanMs >= 0.0f);
    CHECK(stats.gpuP95Ms >= 0.0f);
    CHECK(stats.cpuMeanMs > 0.0f);
  }
  // Passes never executed by the graph produce no GPU samples -> Unavailable.
  for (size_t i = 0; i < allStats.size(); ++i) {
    const auto passId = static_cast<RenderPassId>(i);
    if (std::find(kExecutedPasses.begin(), kExecutedPasses.end(), passId) !=
        kExecutedPasses.end()) {
      continue;
    }
    const auto &stats = allStats[i];
    CHECK(stats.gpuState == QueryState::Unavailable);
    CHECK(stats.gpuMeanMs == 0.0f);
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
  render::RenderFrameInput input;
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
    RenderSystem::render(registry, input, camera);
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
