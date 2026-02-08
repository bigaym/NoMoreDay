#pragma once

#include "game/components/MapComponent.hpp"
#include <string>

struct DestructibleTileComponent {
  float maxHP = 100.0f;
  float currentHP = 100.0f;
  std::string debrisType = "stone";
  bool isDestroyed = false;
  Tile::Type destroyedType = Tile::Type::FLOOR;
};
