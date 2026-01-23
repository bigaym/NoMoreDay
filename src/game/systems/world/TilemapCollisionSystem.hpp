#pragma once

#include "game/components/Common.hpp"
#include "game/systems/world/MapSystem.hpp"
#include "raylib.h"

namespace NoMoreDay {

class TilemapCollisionSystem {
public:
  /**
   * @brief Check if a world position is walkable.
   * @param mapSystem The map system containing tile data.
   * @param pos The world position to check.
   * @return True if the position is walkable, false otherwise.
   */
  static bool IsPositionWalkable(const MapSystem &mapSystem,
                                 const Vector2 &pos);

  /**
   * @brief Find a safe (walkable) position near the target.
   * @param mapSystem The map system containing tile data.
   * @param startPos The starting position (usually the target's position).
   * @param direction The direction to look for a safe spot.
   * @param distance The distance from the start position.
   * @param outPos The found safe position.
   * @return True if a safe position was found, false otherwise.
   */
  static bool FindSafeTeleportTarget(const MapSystem &mapSystem,
                                     const Vector2 &startPos,
                                     const Vector2 &direction, float distance,
                                     Vector2 &outPos);
};

} // namespace NoMoreDay
