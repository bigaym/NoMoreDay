#include "game/application/render/LightAdapter.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/LightComponent.hpp"
#include "game/systems/item/LootFilter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr float kPi = 3.14159265358979323846f;

Vector2 NormalizeDirection(Vector2 direction) {
  const float lenSq = direction.x * direction.x + direction.y * direction.y;
  if (lenSq <= 1e-8f) {
    return {1.0f, 0.0f};
  }
  const float invLen = 1.0f / std::sqrt(lenSq);
  return {direction.x * invLen, direction.y * invLen};
}

float ComputeFlickerIntensity(const LightComponent &light, entt::entity entity,
                              float gameTime) {
  if (!light.flicker) {
    return light.intensity;
  }

  const float entityPhase =
      static_cast<float>(entt::to_integral(entity) & 0xFF) * 0.024543693f;
  const float wave =
      std::sin(gameTime * std::max(0.0f, light.flickerSpeed) + entityPhase);
  const float scale = 1.0f + light.flickerAmplitude * wave;
  return std::max(0.0f, light.intensity * scale);
}

uint32_t ToRawLightType(components::LightType type) {
  switch (type) {
  case components::LightType::PointLight:
    return static_cast<uint32_t>(components::LightType::PointLight);
  case components::LightType::SpotLight:
    return static_cast<uint32_t>(components::LightType::SpotLight);
  case components::LightType::AmbientZone:
    return static_cast<uint32_t>(components::LightType::AmbientZone);
  case components::LightType::AreaLight:
    return static_cast<uint32_t>(components::LightType::AreaLight);
  case components::LightType::LineLight:
    return static_cast<uint32_t>(components::LightType::LineLight);
  }
  return static_cast<uint32_t>(components::LightType::PointLight);
}

components::GPULight BuildGpuLight(const Position &position,
                                   const LightComponent &light,
                                   entt::entity entity, float gameTime) {
  components::GPULight gpuLight = {};
  gpuLight.posX = position.x;
  gpuLight.posY = position.y;
  gpuLight.radius = std::max(0.0f, light.radius);
  gpuLight.intensity = ComputeFlickerIntensity(light, entity, gameTime);
  gpuLight.colorR = light.colorR;
  gpuLight.colorG = light.colorG;
  gpuLight.colorB = light.colorB;
  gpuLight.colorA = 1.0f;
  gpuLight.lightType = ToRawLightType(light.type);

  if (light.type == components::LightType::SpotLight) {
    const float directionRadians = light.spotDirection * (kPi / 180.0f);
    const Vector2 direction =
        NormalizeDirection({std::cos(directionRadians), std::sin(directionRadians)});
    gpuLight.dirX = direction.x;
    gpuLight.dirY = direction.y;

    const float clampedAngle = std::clamp(light.spotAngle, 0.0f, 360.0f);
    const float halfAngleRadians = (clampedAngle * 0.5f) * (kPi / 180.0f);
    gpuLight.spotCosHalfAngle =
        (clampedAngle >= 360.0f) ? -1.0f : std::cos(halfAngleRadians);
    gpuLight.spotOuterCos = gpuLight.spotCosHalfAngle;
  } else {
    gpuLight.dirX = 1.0f;
    gpuLight.dirY = 0.0f;
    gpuLight.spotCosHalfAngle = -1.0f;
    gpuLight.spotOuterCos = -1.0f;
  }
  gpuLight.shadowMapIndex = 0u;
  gpuLight.priority = static_cast<uint32_t>(light.priority);
  gpuLight.flags = 0u;

  return gpuLight;
}

} // namespace

LightProjection LightAdapter::BuildLightCandidates(entt::registry &registry,
                                                   float gameTime) {
  LightProjection projection;

  auto view = registry.view<Position, LightComponent>();
  projection.ecsLights = static_cast<int>(view.size_hint());
  projection.lights.reserve(static_cast<size_t>(view.size_hint()));

  for (const entt::entity entity : view) {
    const auto &[position, light] = view.get<Position, LightComponent>(entity);
    if (!light.enabled) {
      continue;
    }

    if (const auto *lootResult =
            registry.try_get<LootFilterResultComponent>(entity);
        lootResult != nullptr && !lootResult->visible) {
      continue;
    }

    components::GPULight gpuLight = BuildGpuLight(position, light, entity, gameTime);

    if (gpuLight.radius <= 0.0f || gpuLight.intensity <= 0.0f) {
      continue;
    }

    projection.lights.push_back(gpuLight);
  }

  return projection;
}

} // namespace NoMoreDay
