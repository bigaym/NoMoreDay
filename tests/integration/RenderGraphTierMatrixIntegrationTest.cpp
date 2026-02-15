#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/DistortionPass.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/passes/PostProcessPass.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/UIWorldPass.hpp"
#include "engine/render/passes/VFXPass.hpp"
#include "engine/render/passes/VolumetricLightPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/TransientResourcePool.hpp"

#include <cstdint>
#include <memory>

namespace {

constexpr uint32_t kTierMatrixRgba16f = 0x881A;

bool IsHdrPostProcessRequested(const NoMoreDay::render::core::RenderConfig &cfg) {
  return cfg.bloomEnabled || cfg.fxaaEnabled || cfg.vignetteEnabled ||
         (cfg.colorGradingEnabled && cfg.colorGradingLutSize > 0);
}

bool IsHdrPipelineRequested(const NoMoreDay::render::core::RenderConfig &cfg) {
  return cfg.dynamicLightingEnabled || cfg.volumetricLightEnabled ||
         IsHdrPostProcessRequested(cfg);
}

void BuildDefaultPathGraph(
    const NoMoreDay::render::core::RenderConfig &cfg,
    NoMoreDay::render::graph::RenderGraph &graph) {
  using namespace NoMoreDay::render;
  using graph::RenderOwnerTag;
  using graph::RenderResourceTag;

  const bool useHdrPath = IsHdrPipelineRequested(cfg);
  const bool useDistortion = useHdrPath && cfg.distortionEnabled;

  graph.AddPass(std::make_shared<passes::ScenePass>());
  if (useHdrPath && cfg.dynamicLightingEnabled) {
    graph.AddPass(std::make_shared<passes::LightingPass>());
  }
  if (useHdrPath && cfg.volumetricLightEnabled) {
    graph.AddPass(std::make_shared<passes::VolumetricLightPass>());
  }
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::UIWorldPass>());

  RenderResourceTag compositeInputResource = RenderResourceTag::SceneHdrColor;
  RenderOwnerTag compositeInputOwner = RenderOwnerTag::UIWorld;
  if (useHdrPath) {
    graph.AddPass(std::make_shared<passes::PostProcessPass>());
    compositeInputResource = RenderResourceTag::PostProcessLdrColor;
    compositeInputOwner = RenderOwnerTag::PostProcess;
  }
  if (useDistortion) {
    graph.AddPass(std::make_shared<passes::DistortionPass>());
    compositeInputResource = RenderResourceTag::DistortionLdrColor;
    compositeInputOwner = RenderOwnerTag::Distortion;
  }

  graph.AddPass(std::make_shared<passes::CompositePass>(compositeInputResource,
                                                        compositeInputOwner));
}

void BuildOffscreenPathGraph(NoMoreDay::render::graph::RenderGraph &graph) {
  using namespace NoMoreDay::render;
  graph.AddPass(std::make_shared<passes::ScenePass>());
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::UIWorldPass>());
  graph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::SceneHdrColor, graph::RenderOwnerTag::UIWorld));
}

} // namespace

TEST_CASE("[Integration] RenderGraph - Tier Matrix Stability and Ownership Paths") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  auto &qm = render::core::QualityTierManager::Get();
  const auto originalTier = qm.GetTier();

  render::passes::PostProcessPass postProcessPass;
  render::passes::DistortionPass distortionPass;
  REQUIRE(postProcessPass.Initialize());
  REQUIRE(distortionPass.Initialize());

  render::resources::TransientResourcePool transientPool;
  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};

  constexpr render::core::QualityTier kTiers[4] = {
      render::core::QualityTier::Low, render::core::QualityTier::Medium,
      render::core::QualityTier::High, render::core::QualityTier::Ultra};
  constexpr int kWidths[3] = {1280, 1600, 1024};
  constexpr int kHeights[3] = {720, 900, 768};

  int executedResizeFrames = 0;
  int ownershipGraphBuilds = 0;

  for (render::core::QualityTier tier : kTiers) {
    qm.ForceTier(tier);
    const auto cfg = qm.GetConfig();
    const bool useHdrPath = IsHdrPipelineRequested(cfg);

    for (int i = 0; i < 3; ++i) {
      auto hdr = render::resources::FramebufferManager::Create(
          kWidths[i], kHeights[i], kTierMatrixRgba16f);
      REQUIRE(hdr.IsValid());

      render::graph::RenderContext context = {};
      context.qualityManager = &qm;
      context.camera = &camera;
      context.hdrSceneBuffer = hdr;
      context.transientPool = &transientPool;

      transientPool.BeginFrame();
      if (useHdrPath) {
        postProcessPass.Execute(context);
        CHECK(postProcessPass.GetOutputBuffer().IsValid());

        if (cfg.distortionEnabled) {
          distortionPass.SetInputBuffer(&postProcessPass.GetOutputBuffer());
          distortionPass.AddDistortionSource(0.0f, 0.0f, 128.0f, 0.35f);
          distortionPass.Execute(context);
          CHECK(distortionPass.GetOutputBuffer().IsValid());
        }
      }
      transientPool.EndFrame();

      render::resources::FramebufferManager::Destroy(hdr);
      ++executedResizeFrames;
    }

    // Simulate context restore style lifecycle for startup/Alt+Tab robustness.
    postProcessPass.Shutdown();
    distortionPass.Shutdown();
    CHECK(postProcessPass.Initialize());
    CHECK(distortionPass.Initialize());

    {
      render::graph::RenderGraph defaultPathGraph;
      BuildDefaultPathGraph(cfg, defaultPathGraph);
      CHECK_NOTHROW(defaultPathGraph.Build());
      CHECK(!defaultPathGraph.HasValidationErrors());
      ++ownershipGraphBuilds;
    }
    {
      render::graph::RenderGraph offscreenPathGraph;
      BuildOffscreenPathGraph(offscreenPathGraph);
      CHECK_NOTHROW(offscreenPathGraph.Build());
      CHECK(!offscreenPathGraph.HasValidationErrors());
      ++ownershipGraphBuilds;
    }
  }

  CHECK(executedResizeFrames == 12);
  CHECK(ownershipGraphBuilds == 8);

  postProcessPass.Shutdown();
  distortionPass.Shutdown();
  qm.ForceTier(originalTier);
}
