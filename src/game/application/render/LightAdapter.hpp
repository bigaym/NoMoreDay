#pragma once

#include "engine/render/GPUData.hpp"

#include <entt/entt.hpp>
#include <cstdint>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Pure light projection result handed to the Engine.
 *
 * Zero engine/game rendering dependency beyond the DTO type. `lights` holds one
 * `components::GPULight` per registered `Position + LightComponent` that is
 * enabled and has positive radius/intensity (flicker applied), and `ecsLights`
 * mirrors the count the Engine previously derived from the registry view. The
 * projection is intentionally uncapped so LightManager::UpdateCandidates may
 * view-cull, sort and truncate to its budget without re-reading game components.
 */
struct LightProjection {
  std::vector<NoMoreDay::components::GPULight> lights;
  int ecsLights = 0;
};

/**
 * @brief Projects game ECS lights into a pure DTO array.
 *
 * Modeled on the GPUEntityAdapter/OccluderProjector precedents: the Game layer
 * owns the registry projection and hands the Engine a plain-data span; the
 * Engine's LightManager keeps GPU upload, view culling, sorting, transient
 * handling and budget truncation.
 */
class LightAdapter {
public:
  static LightProjection BuildLightCandidates(entt::registry &registry,
                                              float gameTime);
};

} // namespace NoMoreDay
