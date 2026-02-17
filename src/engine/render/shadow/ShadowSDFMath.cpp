#include "engine/render/shadow/ShadowSDFMath.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace NoMoreDay::render::shadow {

float SignedDistanceToCaster(const components::GPUShadowCaster &caster,
                             const float worldX, const float worldY) noexcept {
  const float dx = worldX - caster.posX;
  const float dy = worldY - caster.posY;
  const float radius = std::max(caster.radius, 0.001f);
  return std::sqrt((dx * dx) + (dy * dy)) - radius;
}

float ComputeSceneSDF(const std::span<const components::GPUShadowCaster> casters,
                      const float worldX, const float worldY) noexcept {
  if (casters.empty()) {
    return std::numeric_limits<float>::max();
  }

  float minSdf = std::numeric_limits<float>::max();
  for (const auto &caster : casters) {
    minSdf = std::min(minSdf, SignedDistanceToCaster(caster, worldX, worldY));
  }
  return minSdf;
}

float ResolveShadowFactor(const float sdf, const float softness) noexcept {
  if (!std::isfinite(sdf)) {
    return 1.0f;
  }
  return std::clamp(sdf / std::max(softness, 0.0001f), 0.0f, 1.0f);
}

float ComputeShadowFactor(const std::span<const components::GPUShadowCaster> casters,
                          const float worldX, const float worldY,
                          const float softness) noexcept {
  return ResolveShadowFactor(ComputeSceneSDF(casters, worldX, worldY), softness);
}

} // namespace NoMoreDay::render::shadow
