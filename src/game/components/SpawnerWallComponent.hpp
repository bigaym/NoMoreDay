#pragma once

#include <string>
#include <vector>

struct SpawnerWallComponent {
  int gridX = 0;
  int gridY = 0;
  float spawnInterval = 5.0f;
  float spawnTimer = 0.0f;
  int maxSpawns = 10;
  int currentSpawns = 0;
  std::vector<std::string> spawnPool;
  bool isActive = true;
};
