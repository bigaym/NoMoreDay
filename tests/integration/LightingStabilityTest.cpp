#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/LightComponent.hpp"
#include "game/application/render/LightAdapter.hpp"

#include <cmath>
#include <cstdint>

namespace {
constexpr uint32_t kLightingStabilityRgba16f = 0x881A;

void PopulateStabilityLights(entt::registry &registry, int count) {
  const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
  constexpr float kStartX = -320.0f;
  constexpr float kStartY = -320.0f;
  constexpr float kSpacing = 48.0f;

  for (int i = 0; i < count; ++i) {
    const int row = i / cols;
    const int col = i % cols;

    const entt::entity e = registry.create();
    registry.emplace<Position>(e, kStartX + static_cast<float>(col) * kSpacing,
                               kStartY + static_cast<float>(row) * kSpacing);

    auto &light = registry.emplace<NoMoreDay::LightComponent>(e);
    light.enabled = true;
    light.radius = 120.0f;
    light.intensity = 1.0f;
    light.colorR = 0.95f;
    light.colorG = 0.9f;
    light.colorB = 1.0f;
    light.priority = 128;
    light.flicker = (i % 7 == 0);
    light.flickerSpeed = 4.0f;
    light.flickerAmplitude = 0.1f;
  }
}

void AddStressTransientLight(float timeSeconds) {
  NoMoreDay::components::GPULight transient = {};
  transient.posX = std::sin(timeSeconds * 2.0f) * 280.0f;
  transient.posY = std::cos(timeSeconds * 1.6f) * 220.0f;
  transient.radius = 180.0f;
  transient.intensity = 2.0f;
  transient.colorR = 1.0f;
  transient.colorG = 0.7f;
  transient.colorB = 0.4f;
  transient.colorA = 1.0f;
  NoMoreDay::render::lighting::LightManager::Get().AddTransientLight(transient);
}

} // namespace

TEST_CASE("[Integration] Lighting - Stability (Resize + Tier Switch Stress)") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  auto &qm = render::core::QualityTierManager::Get();
  const auto originalTier = qm.GetTier();

  render::lighting::LightManager::Get().Initialize();
  render::passes::LightingPass pass;
  REQUIRE(pass.Initialize());

  entt::registry registry;
  PopulateStabilityLights(registry, 256);

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  auto hdr = render::resources::FramebufferManager::Create(
      1280, 720, kLightingStabilityRgba16f);
  REQUIRE(hdr.IsValid());

  render::graph::RenderContext context = {};
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;

  constexpr int kWidths[4] = {1280, 1600, 1920, 1024};
  constexpr int kHeights[4] = {720, 900, 1080, 768};
  constexpr render::core::QualityTier kTiers[4] = {
      render::core::QualityTier::Low, render::core::QualityTier::Medium,
      render::core::QualityTier::High, render::core::QualityTier::Ultra};

  constexpr int kStressFrames = 2400;
  int resizeCount = 0;
  int tierSwitchCount = 0;

  for (int frame = 0; frame < kStressFrames; ++frame) {
    const float timeSeconds = static_cast<float>(frame) * 0.016f;

    if (frame % 60 == 0) {
      const auto nextTier = kTiers[(frame / 60) % 4];
      qm.ForceTier(nextTier);
      ++tierSwitchCount;
    }

    if (frame % 100 == 0) {
      const int index = (frame / 100) % 4;
      render::resources::FramebufferManager::Destroy(hdr);
      hdr = render::resources::FramebufferManager::Create(
          kWidths[index], kHeights[index], kLightingStabilityRgba16f);
      REQUIRE(hdr.IsValid());
      context.hdrSceneBuffer = hdr;
      pass.OnResize(kWidths[index], kHeights[index]);
      ++resizeCount;
    }

    if (frame % 30 == 0) {
      AddStressTransientLight(timeSeconds);
    }

    camera.target.x = std::sin(timeSeconds * 0.35f) * 280.0f;
    camera.target.y = std::cos(timeSeconds * 0.42f) * 220.0f;

    const auto &cfg = qm.GetConfig();
    const auto projection = LightAdapter::BuildLightCandidates(registry, timeSeconds);
    render::lighting::LightManager::Get().UpdateCandidates(
        projection.lights, camera, cfg.maxLights, projection.ecsLights);
    pass.Execute(context);
  }

  CHECK(resizeCount >= 20);
  CHECK(tierSwitchCount >= 20);
  CHECK(render::lighting::LightManager::Get().GetActiveLightCount() >= 0);

  render::resources::FramebufferManager::Destroy(hdr);
  pass.Shutdown();
  render::lighting::LightManager::Get().Shutdown();
  qm.ForceTier(originalTier);
}
