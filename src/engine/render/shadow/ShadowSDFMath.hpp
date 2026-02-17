#pragma once

#include "engine/render/GPUData.hpp"

#include <span>

namespace NoMoreDay::render::shadow {

[[nodiscard]] float SignedDistanceToCaster(
    const components::GPUShadowCaster &caster, float worldX,
    float worldY) noexcept;

[[nodiscard]] float ComputeSceneSDF(std::span<const components::GPUShadowCaster> casters,
                                    float worldX, float worldY) noexcept;

[[nodiscard]] float ResolveShadowFactor(float sdf, float softness) noexcept;

[[nodiscard]] float ComputeShadowFactor(
    std::span<const components::GPUShadowCaster> casters, float worldX,
    float worldY, float softness) noexcept;

} // namespace NoMoreDay::render::shadow
