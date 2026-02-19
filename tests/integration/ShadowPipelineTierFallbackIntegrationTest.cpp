#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/LightCullingPass.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/ShadowBuildPass.hpp"
#include "engine/render/passes/ShadowPreparePass.hpp"
#include "engine/render/passes/ShadowResolvePass.hpp"
#include "engine/render/passes/UIWorldPass.hpp"
#include "engine/render/passes/VFXPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/shadow/ShadowAtlasAllocator.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <numeric>
#include <vector>

namespace {

constexpr uint32_t kHdrRgba16f = 0x881A;

std::filesystem::path MakeTempSettingsPath(const std::string &name) {
  const std::filesystem::path dir = std::filesystem::path("bin") / "tmp_shadow_pipeline";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / name;
}

void WriteJson(const std::filesystem::path &path, const nlohmann::json &json) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out << json.dump(2);
}

struct AtlasStressTrace {
  std::vector<uint32_t> overflowPerFrame;
  std::vector<uint32_t> allocatedPerFrame;
  std::vector<uint32_t> evictedLightIds;
};

AtlasStressTrace RunAtlasStressTrace() {
  using NoMoreDay::render::shadow::ShadowAtlasAllocator;

  ShadowAtlasAllocator allocator(24u, 2u);
  AtlasStressTrace trace = {};
  trace.overflowPerFrame.reserve(36);
  trace.allocatedPerFrame.reserve(36);

  for (uint32_t frame = 1u; frame <= 36u; ++frame) {
    allocator.BeginFrame(frame);
    uint32_t overflow = 0u;
    uint32_t allocated = 0u;

    for (uint32_t i = 0u; i < 96u; ++i) {
      const uint32_t lightId = ((i * 37u) + (frame * 11u)) % 96u + 1u;
      const float priorityBase = ((i % 16u) < 4u) ? 4.0f : 1.0f;
      const float jitter = static_cast<float>((frame + i) % 7u) * 0.01f;
      const auto allocation = allocator.AcquireTile(
          {.lightId = lightId, .priorityScore = priorityBase + jitter});
      if (allocation.success) {
        ++allocated;
        if (allocation.evicted) {
          trace.evictedLightIds.push_back(allocation.evictedLightId);
        }
      } else {
        ++overflow;
      }
    }

    trace.overflowPerFrame.push_back(overflow);
    trace.allocatedPerFrame.push_back(allocated);
  }

  return trace;
}

} // namespace

TEST_CASE("[Integration] Shadow Pipeline - Tier linkage and fallback behavior") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  const auto settingsPath = MakeTempSettingsPath("tier_fallback_behavior.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"render",
              {{"v3",
                {{"enabled", true},
                 {"shadowEnabled", true},
                 {"shadowMode", "hybrid"},
                 {"maxShadowedLights", 8},
                 {"shadowAtlasSize", 2048}}}}}});

  auto &qm = render::core::QualityTierManager::Get();
  qm.Initialize(settingsPath.string(), true);

  qm.ForceTier(render::core::QualityTier::Low);
  CHECK(qm.GetConfig().shadowMode == render::core::ShadowMode::Off);
  qm.ForceTier(render::core::QualityTier::Medium);
  CHECK(qm.GetConfig().shadowMode == render::core::ShadowMode::Off);
  qm.ForceTier(render::core::QualityTier::High);
  CHECK(qm.GetConfig().shadowMode == render::core::ShadowMode::SDF);
  qm.ForceTier(render::core::QualityTier::Ultra);
  CHECK(qm.GetConfig().shadowMode == render::core::ShadowMode::Hybrid);

  render::passes::LightingPass lightingPass;
  render::passes::ShadowResolvePass resolvePass;
  REQUIRE(lightingPass.Initialize());
  lightingPass.SetShadowResolvePass(&resolvePass);

  auto hdr =
      render::resources::FramebufferManager::Create(1280, 720, kHdrRgba16f, false);
  REQUIRE(hdr.IsValid());

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};

  render::graph::RenderContext context = {};
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;

  qm.ForceTier(render::core::QualityTier::High);
  resolvePass.Execute(context); // no build pass bound -> fallback
  CHECK(resolvePass.HadFailureThisFrame());
  CHECK(!resolvePass.IsShadowReadyForCurrentFrame());
  CHECK(!resolvePass.GetLastFailureReason().empty());

  lightingPass.Execute(context);
  CHECK(lightingPass.UsedV2FallbackLastFrame());
  CHECK(!lightingPass.WasShadowAppliedLastFrame());
  CHECK(!lightingPass.GetLastShadowFallbackReason().empty());

  render::resources::FramebufferManager::Destroy(hdr);
  lightingPass.Shutdown();
  resolvePass.Shutdown();
}

TEST_CASE("[Integration] Shadow Pipeline - Resize/context restore and ownership stability") {
  using namespace NoMoreDay::render;

  graph::RenderGraph defaultGraph;
  defaultGraph.AddPass(std::make_shared<passes::ScenePass>());
  defaultGraph.AddPass(std::make_shared<passes::ShadowPreparePass>());
  defaultGraph.AddPass(std::make_shared<passes::ShadowBuildPass>());
  defaultGraph.AddPass(std::make_shared<passes::ShadowResolvePass>());
  defaultGraph.AddPass(std::make_shared<passes::LightCullingPass>());
  defaultGraph.AddPass(std::make_shared<passes::LightingPass>());
  defaultGraph.AddPass(std::make_shared<passes::VFXPass>());
  defaultGraph.AddPass(std::make_shared<passes::UIWorldPass>());
  defaultGraph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::SceneHdrColor, graph::RenderOwnerTag::UIWorld));
  CHECK_NOTHROW(defaultGraph.Build());
  CHECK(!defaultGraph.HasValidationErrors());

  graph::RenderGraph offscreenGraph;
  offscreenGraph.AddPass(std::make_shared<passes::ScenePass>());
  offscreenGraph.AddPass(std::make_shared<passes::VFXPass>());
  offscreenGraph.AddPass(std::make_shared<passes::UIWorldPass>());
  offscreenGraph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::SceneHdrColor, graph::RenderOwnerTag::UIWorld));
  CHECK_NOTHROW(offscreenGraph.Build());
  CHECK(!offscreenGraph.HasValidationErrors());

  passes::ShadowResolvePass resolvePass;
  passes::LightingPass lightingPass;
  REQUIRE(lightingPass.Initialize());

  constexpr int kWidths[4] = {1280, 1600, 1024, 1920};
  constexpr int kHeights[4] = {720, 900, 768, 1080};
  for (int i = 0; i < 8; ++i) {
    const int idx = i % 4;
    resolvePass.OnResize(kWidths[idx], kHeights[idx]);
    CHECK(resolvePass.HasShadowMask());
    lightingPass.OnResize(kWidths[idx], kHeights[idx]);

    if ((i % 3) == 2) {
      resolvePass.Shutdown();
      lightingPass.Shutdown();
      REQUIRE(lightingPass.Initialize());
    }
  }

  lightingPass.Shutdown();
  resolvePass.Shutdown();
}

TEST_CASE("[Integration] Shadow Pipeline - Atlas overflow stress is deterministic") {
  const AtlasStressTrace first = RunAtlasStressTrace();
  const AtlasStressTrace second = RunAtlasStressTrace();

  CHECK(first.overflowPerFrame == second.overflowPerFrame);
  CHECK(first.allocatedPerFrame == second.allocatedPerFrame);
  CHECK(first.evictedLightIds == second.evictedLightIds);

  const uint32_t overflowSum = std::accumulate(first.overflowPerFrame.begin(),
                                               first.overflowPerFrame.end(), 0u);
  CHECK(overflowSum > 0u);
  CHECK(!first.evictedLightIds.empty());
}
