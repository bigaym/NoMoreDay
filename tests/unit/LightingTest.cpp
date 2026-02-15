#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "game/components/Common.hpp"
#include "game/components/LightComponent.hpp"

#include <cstddef>

using namespace NoMoreDay;

TEST_CASE("[Unit] Lighting - GPULight ABI Layout") {
  CHECK(sizeof(components::GPULight) == 48);
  CHECK(offsetof(components::GPULight, posX) == 0);
  CHECK(offsetof(components::GPULight, posY) == 4);
  CHECK(offsetof(components::GPULight, radius) == 8);
  CHECK(offsetof(components::GPULight, intensity) == 12);
  CHECK(offsetof(components::GPULight, colorR) == 16);
  CHECK(offsetof(components::GPULight, colorG) == 20);
  CHECK(offsetof(components::GPULight, colorB) == 24);
  CHECK(offsetof(components::GPULight, colorA) == 28);
  CHECK(offsetof(components::GPULight, dirX) == 32);
  CHECK(offsetof(components::GPULight, dirY) == 36);
  CHECK(offsetof(components::GPULight, spotCosHalfAngle) == 40);
  CHECK(offsetof(components::GPULight, lightType) == 44);
}

TEST_CASE("[Unit] Lighting - QualityTier Config") {
  auto &qm = render::core::QualityTierManager::Get();

  qm.ForceTier(render::core::QualityTier::Low);
  CHECK(qm.GetConfig().dynamicLightingEnabled == false);
  CHECK(qm.GetConfig().maxLights == 0);
  CHECK(qm.GetConfig().ambientIntensity == doctest::Approx(0.5f));

  qm.ForceTier(render::core::QualityTier::Medium);
  CHECK(qm.GetConfig().dynamicLightingEnabled == true);
  CHECK(qm.GetConfig().maxLights == 32);
  CHECK(qm.GetConfig().ambientIntensity == doctest::Approx(0.3f));

  qm.ForceTier(render::core::QualityTier::High);
  CHECK(qm.GetConfig().maxLights == 128);
  CHECK(qm.GetConfig().ambientIntensity == doctest::Approx(0.25f));

  qm.ForceTier(render::core::QualityTier::Ultra);
  CHECK(qm.GetConfig().maxLights == 256);
  CHECK(qm.GetConfig().ambientIntensity == doctest::Approx(0.2f));
}

TEST_CASE("[Unit] Lighting - LightManager Low Tier Skip") {
  entt::registry registry;
  const entt::entity e = registry.create();
  registry.emplace<Position>(e, 100.0f, 100.0f);
  registry.emplace<LightComponent>(e);

  Camera2D camera = {};
  camera.target = {100.0f, 100.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  auto &manager = render::lighting::LightManager::Get();
  manager.Shutdown();
  manager.Update(registry, camera, 0, 0.0f);

  CHECK(manager.GetActiveLightCount() == 0);
  CHECK(manager.GetActiveLightsCpu().empty());
}

TEST_CASE("[Unit] Lighting - LightManager Collect Cull Sort") {
  entt::registry registry;

  const entt::entity lowPrio = registry.create();
  registry.emplace<Position>(lowPrio, 120.0f, 120.0f);
  auto &low = registry.emplace<LightComponent>(lowPrio);
  low.priority = 10;
  low.radius = 80.0f;
  low.intensity = 1.0f;

  const entt::entity highNear = registry.create();
  registry.emplace<Position>(highNear, 80.0f, 80.0f);
  auto &hn = registry.emplace<LightComponent>(highNear);
  hn.priority = 200;
  hn.radius = 80.0f;
  hn.intensity = 1.0f;

  const entt::entity highFar = registry.create();
  registry.emplace<Position>(highFar, 90.0f, 90.0f);
  auto &hf = registry.emplace<LightComponent>(highFar);
  hf.priority = 200;
  hf.radius = 120.0f;
  hf.intensity = 1.0f;

  const entt::entity offscreen = registry.create();
  registry.emplace<Position>(offscreen, -3000.0f, -3000.0f);
  auto &off = registry.emplace<LightComponent>(offscreen);
  off.priority = 255;
  off.radius = 40.0f;
  off.intensity = 1.0f;

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  auto &manager = render::lighting::LightManager::Get();
  manager.Shutdown();
  manager.Update(registry, camera, 2, 0.0f);

  REQUIRE(manager.GetActiveLightCount() == 2);
  const auto &lights = manager.GetActiveLightsCpu();
  REQUIRE(lights.size() == 2);

  CHECK(lights[0].posX == doctest::Approx(80.0f));
  CHECK(lights[0].posY == doctest::Approx(80.0f));
  CHECK(lights[1].posX == doctest::Approx(90.0f));
  CHECK(lights[1].posY == doctest::Approx(90.0f));
}

TEST_CASE("[Unit] Lighting - Transient Light One Frame") {
  entt::registry registry;
  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  components::GPULight flash = {};
  flash.posX = 0.0f;
  flash.posY = 0.0f;
  flash.radius = 100.0f;
  flash.intensity = 2.0f;
  flash.colorR = 1.0f;
  flash.colorG = 1.0f;
  flash.colorB = 1.0f;
  flash.colorA = 1.0f;

  auto &manager = render::lighting::LightManager::Get();
  manager.Shutdown();
  manager.AddTransientLight(flash);
  manager.Update(registry, camera, 4, 0.0f);
  CHECK(manager.GetActiveLightCount() == 1);

  manager.Update(registry, camera, 4, 0.0f);
  CHECK(manager.GetActiveLightCount() == 0);
}

TEST_CASE("[Unit] Lighting - Low Tier Bypasses LightingPass") {
  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Low);

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  render::passes::LightingPass pass;
  CHECK(pass.IsInitialized() == false);

  render::graph::RenderContext context = {};
  context.qualityManager = &qm;
  context.camera = &camera;
  pass.Execute(context);

  CHECK(pass.IsInitialized() == false);
}

TEST_CASE("[Unit] Lighting - LightType Mapping Spot Ambient Point") {
  entt::registry registry;
  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};
  camera.zoom = 1.0f;

  const entt::entity spotEntity = registry.create();
  registry.emplace<Position>(spotEntity, 40.0f, 0.0f);
  auto &spot = registry.emplace<LightComponent>(spotEntity);
  spot.type = components::LightType::SpotLight;
  spot.spotDirection = 90.0f;
  spot.spotAngle = 60.0f;
  spot.priority = 240;

  const entt::entity ambientEntity = registry.create();
  registry.emplace<Position>(ambientEntity, 20.0f, 0.0f);
  auto &ambient = registry.emplace<LightComponent>(ambientEntity);
  ambient.type = components::LightType::AmbientZone;
  ambient.priority = 200;

  const entt::entity pointEntity = registry.create();
  registry.emplace<Position>(pointEntity, 0.0f, 0.0f);
  auto &point = registry.emplace<LightComponent>(pointEntity);
  point.type = components::LightType::PointLight;
  point.priority = 160;

  auto &manager = render::lighting::LightManager::Get();
  manager.Shutdown();
  manager.Update(registry, camera, 8, 0.0f);

  const auto &lights = manager.GetActiveLightsCpu();
  REQUIRE(lights.size() == 3);

  const auto &spotGpu = lights[0];
  CHECK(spotGpu.lightType ==
        static_cast<uint32_t>(components::LightType::SpotLight));
  CHECK(spotGpu.dirX == doctest::Approx(0.0f).epsilon(0.001f));
  CHECK(spotGpu.dirY == doctest::Approx(1.0f).epsilon(0.001f));
  CHECK(spotGpu.spotCosHalfAngle == doctest::Approx(0.8660254f).epsilon(0.001f));

  const auto &ambientGpu = lights[1];
  CHECK(ambientGpu.lightType ==
        static_cast<uint32_t>(components::LightType::AmbientZone));
  CHECK(ambientGpu.spotCosHalfAngle == doctest::Approx(-1.0f));

  const auto &pointGpu = lights[2];
  CHECK(pointGpu.lightType ==
        static_cast<uint32_t>(components::LightType::PointLight));
  CHECK(pointGpu.dirX == doctest::Approx(1.0f));
  CHECK(pointGpu.dirY == doctest::Approx(0.0f));
}
