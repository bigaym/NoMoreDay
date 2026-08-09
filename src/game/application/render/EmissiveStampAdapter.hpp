#pragma once

#include "engine/render/GPUData.hpp"

#include <entt/entt.hpp>
#include <cstdint>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Pure emissive stamp projection result handed to the Engine.
 *
 * Zero engine/game rendering dependency beyond the DTO type. `stamps` holds one
 * `components::EmissiveStampInput` per registered
 * `Position + ActiveMaterialSwap` entity (excluding `KilledTag`) whose material
 * resolves to a valid mask layer and a positive emissive contribution, in
 * registry view order. The projection is intentionally uncapped so the Engine
 * may drive per-stamp compute dispatch from the real demand without re-reading
 * game components.
 */
struct EmissiveProjection {
  std::vector<NoMoreDay::components::EmissiveStampInput> stamps;
};

/**
 * @brief Projects game ECS emissive materials into a pure DTO array.
 *
 * Modeled on the LightAdapter/HeightFieldAdapter/GPULootAdapter precedents: the
 * Game layer owns the registry projection (material resolution, mask layer,
 * emissive color/intensity, world half-extent) and hands the Engine a
 * plain-data span; the Engine's RadianceCascadesPass keeps the world->pixel
 * conversion, parameter upload and compute dispatch.
 */
class EmissiveStampAdapter {
public:
  static EmissiveProjection BuildEmissiveStamps(entt::registry &registry);
};

} // namespace NoMoreDay
