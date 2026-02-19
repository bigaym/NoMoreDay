#include "engine/render/lighting/LightManager.hpp"

#include "engine/render/RenderConstants.hpp"
#include "engine/render/GPUUtils.hpp"
#include "game/components/Common.hpp"
#include "game/components/LightComponent.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace NoMoreDay::render::lighting {
namespace {

constexpr float kPi = 3.14159265358979323846f;

struct LightCandidate {
  components::GPULight gpuLight = {};
  uint8_t priority = 0;
  float distanceSquared = std::numeric_limits<float>::max();
};

bool IntersectsView(const components::GPULight &light, float minX, float minY,
                    float maxX, float maxY) {
  const float lightMinX = light.posX - light.radius;
  const float lightMaxX = light.posX + light.radius;
  const float lightMinY = light.posY - light.radius;
  const float lightMaxY = light.posY + light.radius;

  return lightMaxX >= minX && lightMinX <= maxX && lightMaxY >= minY &&
         lightMinY <= maxY;
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

Vector2 NormalizeDirection(Vector2 direction) {
  const float lenSq = direction.x * direction.x + direction.y * direction.y;
  if (lenSq <= 1e-8f) {
    return {1.0f, 0.0f};
  }
  const float invLen = 1.0f / std::sqrt(lenSq);
  return {direction.x * invLen, direction.y * invLen};
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

components::GPULight SanitizeRuntimeLight(const components::GPULight &source) {
  components::GPULight light = source;
  light.radius = std::max(0.0f, light.radius);
  light.intensity = std::max(0.0f, light.intensity);

  const uint32_t point = static_cast<uint32_t>(components::LightType::PointLight);
  const uint32_t spot = static_cast<uint32_t>(components::LightType::SpotLight);
  const uint32_t ambient = static_cast<uint32_t>(components::LightType::AmbientZone);
  const uint32_t area = static_cast<uint32_t>(components::LightType::AreaLight);
  const uint32_t line = static_cast<uint32_t>(components::LightType::LineLight);
  if (light.lightType != point && light.lightType != spot &&
      light.lightType != ambient && light.lightType != area &&
      light.lightType != line) {
    light.lightType = point;
  }

  if (light.lightType == spot) {
    const Vector2 normalized = NormalizeDirection({light.dirX, light.dirY});
    light.dirX = normalized.x;
    light.dirY = normalized.y;
    light.spotCosHalfAngle = std::clamp(light.spotCosHalfAngle, -1.0f, 1.0f);
    light.spotOuterCos = std::clamp(light.spotOuterCos, -1.0f, 1.0f);
  } else {
    light.dirX = 1.0f;
    light.dirY = 0.0f;
    light.spotCosHalfAngle = -1.0f;
    light.spotOuterCos = -1.0f;
  }

  return light;
}

} // namespace

LightManager &LightManager::Get() {
  static LightManager manager;
  return manager;
}

void LightManager::Initialize() {
  if (m_lightBuffer != nullptr) {
    return;
  }

  m_lightBuffer = std::make_unique<::NoMoreDay::core::ComputeBuffer>();
  m_lightBuffer->Create(
      static_cast<size_t>(NoMoreDay::Constants::Lighting::MAX_LIGHTS) *
      sizeof(components::GPULight));
  m_stagingBuffer.reserve(
      static_cast<size_t>(NoMoreDay::Constants::Lighting::MAX_LIGHTS));
  m_activeLightRecords.reserve(
      static_cast<size_t>(NoMoreDay::Constants::Lighting::MAX_LIGHTS));
  m_transientLights.reserve(
      static_cast<size_t>(NoMoreDay::Constants::Lighting::MAX_LIGHTS));
}

void LightManager::Shutdown() {
  m_lightBuffer.reset();
  m_stagingBuffer.clear();
  m_activeLightRecords.clear();
  m_transientLights.clear();
  m_activeLightCount = 0;
  m_debugStats = {};
  m_disableViewCullingForTesting = false;
}

void LightManager::Update(entt::registry &registry, const Camera2D &camera,
                          int maxLights, float gameTime) {
  const int allowedLights = std::max(
      0, std::min(maxLights, NoMoreDay::Constants::Lighting::MAX_LIGHTS));
  m_debugStats = {};
  m_debugStats.allowedLights = allowedLights;
  m_stagingBuffer.clear();
  m_activeLightRecords.clear();
  m_activeLightCount = 0;

  if (allowedLights == 0) {
    m_debugStats.transientLights = static_cast<int>(m_transientLights.size());
    m_transientLights.clear();
    return;
  }

  const float zoom = std::max(camera.zoom, 0.0001f);
  const float rawScreenW = static_cast<float>(GetScreenWidth());
  const float rawScreenH = static_cast<float>(GetScreenHeight());
  const float screenW = rawScreenW > 0.0f ? rawScreenW : 1920.0f;
  const float screenH = rawScreenH > 0.0f ? rawScreenH : 1080.0f;
  const float viewMinX = camera.target.x - (camera.offset.x / zoom);
  const float viewMinY = camera.target.y - (camera.offset.y / zoom);
  const float viewMaxX = viewMinX + (screenW / zoom);
  const float viewMaxY = viewMinY + (screenH / zoom);

  std::vector<LightCandidate> candidates;
  candidates.reserve(static_cast<size_t>(allowedLights) + m_transientLights.size());

  auto view = registry.view<Position, LightComponent>();
  m_debugStats.ecsLights = static_cast<int>(view.size_hint());
  for (const entt::entity entity : view) {
    const auto &[position, light] = view.get<Position, LightComponent>(entity);
    if (!light.enabled) {
      continue;
    }

    components::GPULight gpuLight = BuildGpuLight(position, light, entity, gameTime);

    if (gpuLight.radius <= 0.0f || gpuLight.intensity <= 0.0f) {
      continue;
    }
    if (!m_disableViewCullingForTesting &&
        !IntersectsView(gpuLight, viewMinX, viewMinY, viewMaxX, viewMaxY)) {
      continue;
    }

    const float dx = gpuLight.posX - camera.target.x;
    const float dy = gpuLight.posY - camera.target.y;
    candidates.push_back({gpuLight, light.priority, dx * dx + dy * dy});
  }

  m_debugStats.transientLights = static_cast<int>(m_transientLights.size());
  for (const components::GPULight &transientRaw : m_transientLights) {
    const components::GPULight transient = SanitizeRuntimeLight(transientRaw);
    if (transient.radius <= 0.0f || transient.intensity <= 0.0f) {
      continue;
    }
    if (!m_disableViewCullingForTesting &&
        !IntersectsView(transient, viewMinX, viewMinY, viewMaxX, viewMaxY)) {
      continue;
    }

    const float dx = transient.posX - camera.target.x;
    const float dy = transient.posY - camera.target.y;
    candidates.push_back({transient, 255, dx * dx + dy * dy});
  }

  m_debugStats.candidatesAfterCull = static_cast<int>(candidates.size());

  std::sort(candidates.begin(), candidates.end(),
            [](const LightCandidate &lhs, const LightCandidate &rhs) {
              if (lhs.priority != rhs.priority) {
                return lhs.priority > rhs.priority;
              }
              return lhs.distanceSquared < rhs.distanceSquared;
            });

  const size_t finalCount =
      std::min(static_cast<size_t>(allowedLights), candidates.size());
  m_stagingBuffer.reserve(static_cast<size_t>(allowedLights));
  m_activeLightRecords.reserve(static_cast<size_t>(allowedLights));
  for (size_t i = 0; i < finalCount; ++i) {
    m_stagingBuffer.push_back(candidates[i].gpuLight);
    m_activeLightRecords.push_back(
        {.gpuLight = candidates[i].gpuLight, .priority = candidates[i].priority});
  }

  m_activeLightCount = static_cast<int>(m_stagingBuffer.size());
  m_debugStats.selectedLights = m_activeLightCount;
  m_debugStats.droppedByBudget =
      std::max(0, m_debugStats.candidatesAfterCull - m_activeLightCount);
  if (!m_stagingBuffer.empty() && NoMoreDay::utils::GPUUtils::IsInitialized()) {
    if (m_lightBuffer == nullptr) {
      Initialize();
    }
    if (m_lightBuffer != nullptr) {
      m_lightBuffer->OrphanAndUpload(
          m_stagingBuffer.data(),
          static_cast<size_t>(m_activeLightCount) *
              sizeof(components::GPULight));
    }
  }

  m_transientLights.clear();
}

void LightManager::Bind() const {
  if (m_lightBuffer == nullptr) {
    return;
  }
  m_lightBuffer->BindBase(
      static_cast<uint32_t>(NoMoreDay::RenderConstants::Binding::SSBO_LIGHT_DATA));
}

void LightManager::AddTransientLight(const components::GPULight &light) {
  if (m_transientLights.size() >=
      static_cast<size_t>(NoMoreDay::Constants::Lighting::MAX_LIGHTS)) {
    return;
  }
  m_transientLights.push_back(light);
}

void LightManager::ClearShadowMapIndices() {
  if (m_stagingBuffer.empty()) {
    return;
  }

  bool changed = false;
  for (size_t i = 0; i < m_stagingBuffer.size(); ++i) {
    if (m_stagingBuffer[i].shadowMapIndex != 0u ||
        ((m_stagingBuffer[i].flags & 0x1u) != 0u)) {
      m_stagingBuffer[i].shadowMapIndex = 0u;
      m_stagingBuffer[i].flags &= ~0x1u;
      changed = true;
    }
    if (i < m_activeLightRecords.size()) {
      m_activeLightRecords[i].gpuLight.shadowMapIndex = m_stagingBuffer[i].shadowMapIndex;
      m_activeLightRecords[i].gpuLight.flags = m_stagingBuffer[i].flags;
    }
  }

  if (!changed || m_lightBuffer == nullptr || m_activeLightCount <= 0 ||
      !NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return;
  }
  m_lightBuffer->OrphanAndUpload(
      m_stagingBuffer.data(),
      static_cast<size_t>(m_activeLightCount) * sizeof(components::GPULight));
}

void LightManager::ApplyShadowMapAssignments(
    const std::span<const ShadowMapAssignment> assignments) {
  if (assignments.empty() || m_stagingBuffer.empty()) {
    return;
  }

  bool changed = false;
  for (const ShadowMapAssignment &assignment : assignments) {
    if (assignment.lightIndex >= m_stagingBuffer.size()) {
      continue;
    }

    components::GPULight &light = m_stagingBuffer[assignment.lightIndex];
    if (light.shadowMapIndex != assignment.shadowMapIndex) {
      light.shadowMapIndex = assignment.shadowMapIndex;
      changed = true;
    }
    light.flags = (assignment.shadowMapIndex != 0u) ? (light.flags | 0x1u)
                                                     : (light.flags & ~0x1u);
    if (assignment.lightIndex < m_activeLightRecords.size()) {
      m_activeLightRecords[assignment.lightIndex].gpuLight = light;
    }
  }

  if (!changed || m_lightBuffer == nullptr || m_activeLightCount <= 0 ||
      !NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return;
  }
  m_lightBuffer->OrphanAndUpload(
      m_stagingBuffer.data(),
      static_cast<size_t>(m_activeLightCount) * sizeof(components::GPULight));
}

} // namespace NoMoreDay::render::lighting
