#pragma once

#include "engine/render/GPUData.hpp"

#include <entt/entt.hpp>
#include <cstdint>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Pure occluder projection result handed to the Engine.
 *
 * Zero engine/game rendering dependency beyond the DTO types. `casters` holds
 * one `GPUShadowCaster` per registered `Position + ShadowCasterComponent` (the
 * radius falls back to 24.0f unless `VisionComponent::radius > 0`), and the FNV
 * signatures/counts mirror the values OccluderExtractPass previously derived
 * inside its UploadOccluders (now computed once here, shared by both passes).
 */
struct OccluderProjection {
  std::vector<NoMoreDay::components::GPUShadowCaster> casters;
  uint32_t staticCount = 0u;
  uint32_t dynamicCount = 0u;
  uint64_t staticSignature = 0u;
  uint64_t dynamicSignature = 0u;
};

/**
 * @brief Projects game ECS occluders into a pure DTO array + FNV signatures.
 *
 * Modeled on the GPUEntityAdapter precedent: the Game layer owns the registry
 * projection and hands Engine-owned CPU/GPU buffers the resulting plain data.
 * The projection is intentionally uncapped so ShadowBuildPass may truncate to
 * its kMaxShadowCasters budget without re-reading game components.
 */
class OccluderProjector {
public:
  static OccluderProjection Project(entt::registry &registry);
};

} // namespace NoMoreDay
