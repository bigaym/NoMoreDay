#pragma once

#include "engine/render/GPUData.hpp"

#include <entt/entt.hpp>
#include <cstdint>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Pure loot projection result handed to the Engine.
 *
 * Zero engine/game rendering dependency beyond the DTO type. `instances`
 * holds one `components::GPULootInstance` per registered `Position + LootTag`
 * entity that also carries `ItemComponent` or `GoldComponent`, in registry
 * view order. The projection is intentionally uncapped so the Engine may size
 * its instance SSBO to the real demand without re-reading game components.
 */
struct LootProjection {
  std::vector<NoMoreDay::components::GPULootInstance> instances;
};

/**
 * @brief Projects game ECS dropped loot into a pure DTO array.
 *
 * Modeled on the LightAdapter/HeightFieldAdapter precedents: the Game layer
 * owns the registry projection (including rarity color packing and glow
 * intensity) and hands the Engine a plain-data span; the Engine's
 * GPULootSystem keeps GPU upload, compute dispatch and instanced drawing.
 */
class GPULootAdapter {
public:
  static LootProjection BuildLoot(entt::registry &registry);
};

} // namespace NoMoreDay
