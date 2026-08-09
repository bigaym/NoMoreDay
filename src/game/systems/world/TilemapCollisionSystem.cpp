#include "game/systems/world/TilemapCollisionSystem.hpp"
#include "game/components/Common.hpp"
#include "game/systems/world/WorldConstants.hpp"
#include "game/systems/physics/PhysicsConstants.hpp"
#include <cmath>
#include <algorithm>

namespace NoMoreDay {

bool TilemapCollisionSystem::IsPositionWalkable(const MapSystem &mapSystem,
                                                const Vector2 &pos) {
  using namespace NoMoreDay::Constants::World;
  int tx = static_cast<int>(pos.x / GRID_TILE_SIZE);
  int ty = static_cast<int>(pos.y / GRID_TILE_SIZE);
  return mapSystem.isWalkable(tx, ty);
}

bool TilemapCollisionSystem::IsAreaWalkable(const MapSystem &mapSystem, const Vector2 &pos, float radius) {
    using namespace NoMoreDay::Constants::World;
    
    // Calculate AABB bounds in Tile Coordinates
    float minX = pos.x - radius;
    float maxX = pos.x + radius;
    float minY = pos.y - radius;
    float maxY = pos.y + radius;

    int tx_min = static_cast<int>(minX / GRID_TILE_SIZE);
    int tx_max = static_cast<int>(maxX / GRID_TILE_SIZE);
    int ty_min = static_cast<int>(minY / GRID_TILE_SIZE);
    int ty_max = static_cast<int>(maxY / GRID_TILE_SIZE);

    // Clamp to map bounds (optional, but isWalkable handles bounds check safely usually)
    // MapSystem::isWalkable internal check is safe.

    // Iterate all covered tiles
    for (int y = ty_min; y <= ty_max; ++y) {
        for (int x = tx_min; x <= tx_max; ++x) {
            // Check if this tile is walkable
            if (!mapSystem.isWalkable(x, y)) {
                return false; // Found a blockage
            }
        }
    }
    return true;
}

bool TilemapCollisionSystem::FindSafeTeleportTarget(const MapSystem &mapSystem,
                                                    const Vector2 &startPos,
                                                    const Vector2 &direction,
                                                    float distance,
                                                    Vector2 &outPos) {
  Vector2 targetPos = {startPos.x + direction.x * distance,
                       startPos.y + direction.y * distance};

  if (IsPositionWalkable(mapSystem, targetPos)) {
    outPos = targetPos;
    return true;
  }

  // If blocked, try to find the closest walkable point along the path or nearby
  // Simple search: check smaller steps
  const int steps = 5;
  for (int i = 1; i < steps; ++i) {
    float dashStep = distance * (1.0f - (float)i / steps);
    Vector2 stepPos = {startPos.x + direction.x * dashStep,
                       startPos.y + direction.y * dashStep};
    if (IsPositionWalkable(mapSystem, stepPos)) {
      outPos = stepPos;
      return true;
    }
  }

  // Try slight angles if straight path is blocked
  const float angles[] = {15.0f, -15.0f, 30.0f, -30.0f};
  for (float angleDeg : angles) {
    float angleRad = angleDeg * DEG2RAD;
    float cosA = std::cos(angleRad);
    float sinA = std::sin(angleRad);
    Vector2 rotatedDir = {direction.x * cosA - direction.y * sinA,
                          direction.x * sinA + direction.y * cosA};

    Vector2 altPos = {startPos.x + rotatedDir.x * distance,
                      startPos.y + rotatedDir.y * distance};
    if (IsPositionWalkable(mapSystem, altPos)) {
      outPos = altPos;
      return true;
    }
  }

  return false;
}

void TilemapCollisionSystem::ResolveCollision(const MapSystem &map, const Position &pos, Velocity &vel, float dt, float radius) {
    using namespace NoMoreDay::Constants::World;
    using namespace NoMoreDay::Constants::Physics;
    const float TILE_SIZE = GRID_TILE_SIZE;

    // Horizontal collision
    if (std::abs(vel.vx) > EPSILON_VELOCITY) {
      float nextX = pos.x + vel.vx * dt;
      int tileX = static_cast<int>(
          (vel.vx > 0 ? nextX + radius : nextX - radius) / TILE_SIZE);
      int tileY_top = static_cast<int>((pos.y - radius + 0.5f) / TILE_SIZE);
      int tileY_bottom =
          static_cast<int>((pos.y + radius - 0.5f) / TILE_SIZE);

      if (!map.isWalkable(tileX, tileY_top) ||
          !map.isWalkable(tileX, tileY_bottom)) {
        vel.vx = 0;
      }
    }

    // Vertical collision
    if (std::abs(vel.vy) > EPSILON_VELOCITY) {
      float nextY = pos.y + vel.vy * dt;
      int tileY = static_cast<int>(
          (vel.vy > 0 ? nextY + radius : nextY - radius) / TILE_SIZE);
      int tileX_left =
          static_cast<int>((pos.x - radius + 0.5f) / TILE_SIZE);
      int tileX_right =
          static_cast<int>((pos.x + radius - 0.5f) / TILE_SIZE);

      if (!map.isWalkable(tileX_left, tileY) ||
          !map.isWalkable(tileX_right, tileY)) {
        vel.vy = 0;
      }
    }
}

} // namespace NoMoreDay
