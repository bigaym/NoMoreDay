#include "game/systems/world/TilemapCollisionSystem.hpp"
#include "game/components/Common.hpp"

namespace NoMoreDay {

bool TilemapCollisionSystem::IsPositionWalkable(const MapSystem &mapSystem,
                                                const Vector2 &pos) {
  using namespace NoMoreDay::Constants::World;
  int tx = static_cast<int>(pos.x / GRID_TILE_SIZE);
  int ty = static_cast<int>(pos.y / GRID_TILE_SIZE);
  return mapSystem.isWalkable(tx, ty);
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

} // namespace NoMoreDay
