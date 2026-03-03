#include "doctest.h"

#include "app/SharedContext.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/BindingRegistry.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/lighting/ClusteredLightingState.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/LightCullingPass.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/UIWorldPass.hpp"
#include "engine/render/passes/VFXPass.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/LightComponent.hpp"

#include <cstdint>
#include <memory>
#include <algorithm>
#include <vector>

namespace {

constexpr uint32_t kHdrRgba16f = 0x881A;

void PopulateDenseLights(entt::registry &registry, int count, float centerX,
                         float centerY) {
  for (int i = 0; i < count; ++i) {
    const entt::entity e = registry.create();
    const float offset = static_cast<float>(i % 8) * 3.0f;
    registry.emplace<Position>(e, centerX + offset, centerY + offset);
    auto &light = registry.emplace<NoMoreDay::LightComponent>(e);
    light.enabled = true;
    light.radius = 140.0f;
    light.intensity = 1.0f + static_cast<float>(i % 5) * 0.1f;
    light.priority = static_cast<uint8_t>(50 + (i % 200));
    light.flicker = false;
  }
}

void EnableClusteredConfig(NoMoreDay::render::core::RenderConfig &cfg) {
  cfg.v3Enabled = true;
  cfg.dynamicLightingEnabled = true;
  cfg.clusteredLightingEnabled = true;
  cfg.clusteredLightingV4Enabled = true;
  cfg.clusterTileSize = 256;
  cfg.clusterZSliceCount = 8;
  cfg.maxLights = 4096;
}

void ExpectLightCullingBindingsAligned() {
  using namespace NoMoreDay::render::core;

  uint32_t binding = 0u;
  REQUIRE(BindingRegistry::TryResolve(BindingDomain::LightCulling, "LIGHT_LIST_IN",
                                      binding));
  CHECK(binding == 0u);
  REQUIRE(BindingRegistry::TryResolve(BindingDomain::LightCulling,
                                      "CLUSTER_HEADER_OUT", binding));
  CHECK(binding == 1u);
  REQUIRE(BindingRegistry::TryResolve(BindingDomain::LightCulling, "CLUSTER_INDEX_OUT",
                                      binding));
  CHECK(binding == 2u);
  REQUIRE(BindingRegistry::TryResolve(BindingDomain::LightCulling, "LIGHT_BOUNDS_IN",
                                      binding));
  CHECK(binding == 3u);
  REQUIRE(BindingRegistry::TryResolve(BindingDomain::LightCulling, "CLUSTER_COUNTER",
                                      binding));
  CHECK(binding == 4u);
  REQUIRE(BindingRegistry::TryResolve(BindingDomain::LightCulling, "CLUSTER_LIGHT_OUT",
                                      binding));
  CHECK(binding == 5u);
}

} // namespace

TEST_CASE("[Integration] Clustered Lighting - Fallback when culling pass missing") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  EnableClusteredConfig(cfg);

  render::passes::LightingPass lightingPass;
  REQUIRE(lightingPass.Initialize());

  auto hdr = render::resources::FramebufferManager::Create(1280, 720, kHdrRgba16f);
  REQUIRE(hdr.IsValid());

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};

  render::graph::RenderContext context = {};
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;

  lightingPass.Execute(context);

  CHECK(lightingPass.UsedClusteredFallbackLastFrame());
  CHECK(!lightingPass.WasClusteredAppliedLastFrame());
  CHECK(!lightingPass.GetLastClusteredFallbackReason().empty());

  lightingPass.Shutdown();
  render::resources::FramebufferManager::Destroy(hdr);
}

TEST_CASE("[Integration] Clustered Lighting - Deterministic overflow and index output") {
  using namespace NoMoreDay;

  ExpectLightCullingBindingsAligned();

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  EnableClusteredConfig(cfg);

  entt::registry registry;
  PopulateDenseLights(registry, 220, 20.0f, 20.0f);

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};

  render::lighting::LightManager::Get().Initialize();
  render::lighting::LightManager::Get().Update(registry, camera, cfg.maxLights, 0.0f);

  auto hdr = render::resources::FramebufferManager::Create(1024, 768, kHdrRgba16f);
  REQUIRE(hdr.IsValid());

  ResourceManager resources;
  SharedContext shared = {};
  shared.resources = &resources;

  render::graph::RenderContext context = {};
  context.registry = &registry;
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;
  context.shared = &shared;

  render::passes::LightCullingPass cullingPass;
  cullingPass.Execute(context);
  REQUIRE(cullingPass.SucceededThisFrame());
  REQUIRE(cullingPass.IsClusterDataReadyForCurrentFrame());

  auto &state = render::lighting::ClusteredLightingState::Get();
  REQUIRE(state.ReadBackClusterHeaders());
  REQUIRE(state.ReadBackClusterLightIndices());
  const auto headersA = state.GetClusterHeadersReadback();
  const auto indicesA = state.GetClusterLightIndicesReadback();
  const uint32_t overflowA = state.GetLastOverflowSum();

  cullingPass.Execute(context);
  REQUIRE(cullingPass.SucceededThisFrame());
  REQUIRE(cullingPass.IsClusterDataReadyForCurrentFrame());
  REQUIRE(state.ReadBackClusterHeaders());
  REQUIRE(state.ReadBackClusterLightIndices());
  const auto headersB = state.GetClusterHeadersReadback();
  const auto indicesB = state.GetClusterLightIndicesReadback();
  const uint32_t overflowB = state.GetLastOverflowSum();

  CHECK(overflowA > 0u);
  CHECK(overflowA == overflowB);
  CHECK(headersA.size() == headersB.size());
  CHECK(indicesA.size() == indicesB.size());
  for (size_t i = 0; i < headersA.size(); ++i) {
    CHECK(headersA[i].pointCount == headersB[i].pointCount);
    CHECK(headersA[i].spotCount == headersB[i].spotCount);
    CHECK(headersA[i].areaCount == headersB[i].areaCount);
  }
  std::vector<uint32_t> flatA;
  std::vector<uint32_t> flatB;
  flatA.reserve(indicesA.size());
  flatB.reserve(indicesB.size());
  for (const auto &entry : indicesA) {
    flatA.push_back(entry.lightIndex);
  }
  for (const auto &entry : indicesB) {
    flatB.push_back(entry.lightIndex);
  }
  std::sort(flatA.begin(), flatA.end());
  std::sort(flatB.begin(), flatB.end());
  CHECK(flatA == flatB);

  render::lighting::ClusteredLightingState::Get().Shutdown();
  render::lighting::LightManager::Get().Shutdown();
  render::resources::FramebufferManager::Destroy(hdr);
}

TEST_CASE("[Integration] Clustered Lighting - Legacy V4 gate removed for light culling") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  EnableClusteredConfig(cfg);
  cfg.clusteredLightingV4Enabled = false;

  entt::registry registry;
  PopulateDenseLights(registry, 320, 30.0f, 30.0f);

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};

  render::lighting::LightManager::Get().Initialize();
  render::lighting::LightManager::Get().Update(registry, camera, cfg.maxLights, 0.0f);
  const uint32_t activeLightCount =
      static_cast<uint32_t>(render::lighting::LightManager::Get().GetActiveLightRecordsCpu().size());
  REQUIRE(activeLightCount > 256u);

  auto hdr = render::resources::FramebufferManager::Create(1280, 720, kHdrRgba16f);
  REQUIRE(hdr.IsValid());

  ResourceManager resources;
  SharedContext shared = {};
  shared.resources = &resources;

  render::graph::RenderContext context = {};
  context.registry = &registry;
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;
  context.shared = &shared;

  render::passes::LightCullingPass cullingPass;
  cullingPass.Execute(context);
  REQUIRE(cullingPass.SucceededThisFrame());
  CHECK(cullingPass.IsClusterDataReadyForCurrentFrame());
  CHECK(render::lighting::ClusteredLightingState::Get().GetUploadedLightBoundsCount() ==
        activeLightCount);

  render::lighting::ClusteredLightingState::Get().Shutdown();
  render::lighting::LightManager::Get().Shutdown();
  render::resources::FramebufferManager::Destroy(hdr);
}

TEST_CASE("[Integration] Clustered Lighting - Forced shader load failure keeps deterministic fallback") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  EnableClusteredConfig(cfg);

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};

  auto hdr = render::resources::FramebufferManager::Create(1024, 768, kHdrRgba16f);
  REQUIRE(hdr.IsValid());

  ResourceManager resources;
  SharedContext shared = {};
  shared.resources = &resources;

  entt::registry registry;
  render::graph::RenderContext context = {};
  context.registry = &registry;
  context.qualityManager = &qm;
  context.camera = &camera;
  context.shared = &shared;
  context.hdrSceneBuffer = hdr;

  render::passes::LightCullingPass cullingPass;
  cullingPass.SetComputeShaderPathForTesting(
      "assets/shaders/lighting/light_culling_missing_for_test.comp");
  cullingPass.Execute(context);

  CHECK(cullingPass.HadFailureThisFrame());
  CHECK_FALSE(cullingPass.SucceededThisFrame());
  CHECK_FALSE(cullingPass.IsClusterDataReadyForCurrentFrame());
  CHECK(cullingPass.GetLastFailureReason() == "failed to initialize light culling shader");

  render::resources::FramebufferManager::Destroy(hdr);
}

TEST_CASE("[Integration] Clustered Lighting - Culling to lighting consumption path") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  EnableClusteredConfig(cfg);

  entt::registry registry;
  PopulateDenseLights(registry, 128, 40.0f, 40.0f);

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};

  render::lighting::LightManager::Get().Initialize();
  render::lighting::LightManager::Get().Update(registry, camera, cfg.maxLights, 0.0f);

  auto hdr = render::resources::FramebufferManager::Create(1280, 720, kHdrRgba16f);
  REQUIRE(hdr.IsValid());

  ResourceManager resources;
  SharedContext shared = {};
  shared.resources = &resources;

  render::graph::RenderContext context = {};
  context.registry = &registry;
  context.shared = &shared;
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;

  render::passes::LightCullingPass cullingPass;
  cullingPass.Execute(context);
  REQUIRE(cullingPass.SucceededThisFrame());
  REQUIRE(cullingPass.IsClusterDataReadyForCurrentFrame());

  render::passes::LightingPass lightingPass;
  REQUIRE(lightingPass.Initialize());
  lightingPass.SetLightCullingPass(&cullingPass);
  lightingPass.Execute(context);

  CHECK(lightingPass.WasClusteredAppliedLastFrame());
  CHECK(!lightingPass.UsedClusteredFallbackLastFrame());

  lightingPass.Shutdown();
  render::lighting::ClusteredLightingState::Get().Shutdown();
  render::lighting::LightManager::Get().Shutdown();
  render::resources::FramebufferManager::Destroy(hdr);
}

TEST_CASE("[Integration] Clustered Lighting - Resize/context restore and offscreen graph stability") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  EnableClusteredConfig(cfg);

  entt::registry registry;
  PopulateDenseLights(registry, 96, 30.0f, 30.0f);

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};

  ResourceManager resources;
  SharedContext shared = {};
  shared.resources = &resources;

  render::lighting::LightManager::Get().Initialize();
  render::passes::LightCullingPass cullingPass;
  render::passes::LightingPass lightingPass;
  REQUIRE(lightingPass.Initialize());
  lightingPass.SetLightCullingPass(&cullingPass);

  constexpr int kWidths[4] = {1280, 1600, 1024, 1920};
  constexpr int kHeights[4] = {720, 900, 768, 1080};
  for (int i = 0; i < 8; ++i) {
    const int idx = i % 4;
    auto hdr = render::resources::FramebufferManager::Create(
        kWidths[idx], kHeights[idx], kHdrRgba16f);
    REQUIRE(hdr.IsValid());

    render::lighting::LightManager::Get().Update(registry, camera, cfg.maxLights, 0.0f);

    render::graph::RenderContext context = {};
    context.registry = &registry;
    context.shared = &shared;
    context.qualityManager = &qm;
    context.camera = &camera;
    context.hdrSceneBuffer = hdr;

    cullingPass.Execute(context);
    CHECK(cullingPass.SucceededThisFrame());
    lightingPass.Execute(context);
    const bool lightingPathValid = lightingPass.WasClusteredAppliedLastFrame() ||
                                   lightingPass.UsedClusteredFallbackLastFrame();
    CHECK(lightingPathValid);

    render::resources::FramebufferManager::Destroy(hdr);
    if ((i % 3) == 2) {
      lightingPass.Shutdown();
      REQUIRE(lightingPass.Initialize());
      lightingPass.SetLightCullingPass(&cullingPass);
    }
  }

  render::graph::RenderGraph offscreenGraph;
  offscreenGraph.AddPass(std::make_shared<render::passes::ScenePass>());
  offscreenGraph.AddPass(std::make_shared<render::passes::VFXPass>());
  offscreenGraph.AddPass(std::make_shared<render::passes::UIWorldPass>());
  offscreenGraph.AddPass(std::make_shared<render::passes::CompositePass>(
      render::graph::RenderResourceTag::SceneHdrColor,
      render::graph::RenderOwnerTag::UIWorld));
  CHECK_NOTHROW(offscreenGraph.Build());
  CHECK(!offscreenGraph.HasValidationErrors());

  lightingPass.Shutdown();
  render::lighting::ClusteredLightingState::Get().Shutdown();
  render::lighting::LightManager::Get().Shutdown();
}

TEST_CASE("[Integration] Clustered Lighting - Boundary conditions") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  EnableClusteredConfig(cfg);

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};

  ResourceManager resources;
  SharedContext shared = {};
  shared.resources = &resources;

  auto hdr = render::resources::FramebufferManager::Create(1024, 768, kHdrRgba16f);
  REQUIRE(hdr.IsValid());

  render::passes::LightCullingPass cullingPass;
  render::lighting::LightManager::Get().Initialize();
  entt::registry registry;

  render::graph::RenderContext context = {};
  context.registry = &registry;
  context.shared = &shared;
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;

  // 0 lights
  render::lighting::LightManager::Get().Update(registry, camera, cfg.maxLights, 0.0f);
  cullingPass.Execute(context);
  CHECK(cullingPass.SucceededThisFrame());
  CHECK(cullingPass.GetLastOverflowCount() == 0u);

  // 1 light
  PopulateDenseLights(registry, 1, 10.0f, 10.0f);
  render::lighting::LightManager::Get().Update(registry, camera, cfg.maxLights, 0.0f);
  cullingPass.Execute(context);
  CHECK(cullingPass.SucceededThisFrame());
  CHECK(cullingPass.GetLastOverflowCount() == 0u);

  // Overfull + uneven distribution
  registry.clear();
  PopulateDenseLights(registry, 220, 0.0f, 0.0f);
  PopulateDenseLights(registry, 8, 1200.0f, 1200.0f);
  render::lighting::LightManager::Get().Update(registry, camera, cfg.maxLights, 0.0f);
  cullingPass.Execute(context);
  CHECK(cullingPass.SucceededThisFrame());
  CHECK(cullingPass.GetLastOverflowCount() > 0u);

  render::lighting::ClusteredLightingState::Get().Shutdown();
  render::lighting::LightManager::Get().Shutdown();
  render::resources::FramebufferManager::Destroy(hdr);
}
