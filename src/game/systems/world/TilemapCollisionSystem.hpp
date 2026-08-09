#pragma once

#include "game/foundation/components/Common.hpp"
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

  /**
   * @brief Check if a circular area is walkable.
   * Checks the bounding box of the area against map tiles.
   * @param mapSystem The map system.
   * @param pos Center position.
   * @param radius Area radius.
   * @return True if fully walkable.
   */
  static bool IsAreaWalkable(const MapSystem &mapSystem, const Vector2 &pos, float radius);

  /**
   * @brief Resolves collision with the tilemap by modifying velocity.
   * If a movement would result in hitting a wall, component velocity is set to 0.
   * Supports sliding along walls.
   * 
   * @param map The map system.
   * @param pos Current entity position.
   * @param vel Entity velocity (will be modified).
   * @param dt Delta time.
   * @param radius Entity collision radius.
   */
  static void ResolveCollision(const MapSystem &map, const Position &pos, Velocity &vel, float dt, float radius);
};

} // namespace NoMoreDay
