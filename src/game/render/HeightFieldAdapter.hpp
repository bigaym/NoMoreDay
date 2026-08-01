#pragma once

#include "engine/render/lighting/GlobalHeightField.hpp"

#include <entt/entt.hpp>
#include <cstdint>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Pure height-field projection result handed to the Engine.
 *
 * Zero engine/game rendering dependency beyond the stamp DTO type. `stamps`
 * holds one `GlobalHeightField::HeightStamp` per registered terrain tile, static
 * occluder, static collider, dynamic occluder and blue-masked sprite; `dynamic`
 * selects the base/static layer vs. the dynamic layer in the Engine. `worldWidth/
 * worldHeight/tileWorldSize` mirror the game Constants::World values the Engine
 * previously read directly.
 */
struct HeightFieldProjection {
  std::vector<render::lighting::GlobalHeightField::HeightStamp> stamps;
  float worldWidth = 0.0f;
  float worldHeight = 0.0f;
  float tileWorldSize = 0.0f;
};

/**
 * @brief Projects game ECS terrain/casters/sprites into a pure stamp array.
 *
 * Modeled on the LightAdapter/OccluderProjector precedents: the Game layer owns
 * the registry projection (including the per-texture blue-mask estimate cache)
 * and hands the Engine a plain-data span; the Engine's GlobalHeightField keeps
 * the chunk compose/upload/texel rasterization. The projection is intentionally
 * uncapped so the Engine may rasterize stamps without re-reading game components.
 */
class HeightFieldAdapter {
public:
  static HeightFieldProjection BuildStamps(entt::registry &registry);
};

} // namespace NoMoreDay
