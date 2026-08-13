#pragma once

#include <cmath>
#include <vector>

#include <raylib.h>
#include <raymath.h>

#include "engine/render/GPUParticleSystem.hpp"

namespace NoMoreDay::ui {

// Kind of crafting success burst. Mirrors the pre-R7 UIRenderer effects:
// fuse = gold sparks + ancient-red ink trails, salvage = red sparks.
enum class UiCraftBurstKind { Fuse, Salvage };

// Builds the success-burst particle set for a completed craft action.
//
// R10 (收尾): restores the fuse / salvage particle bursts that were removed
// with the R7 UIRenderer draw path. The burst is anchored at `anchor` (world
// space, normally the player's position). This is a pure builder: it never
// touches the GPUParticleSystem singleton, so it is directly unit-testable;
// only the per-particle velocity direction/speed is randomized.
inline std::vector<components::GPUParticle> BuildCraftSuccessBurst(
    UiCraftBurstKind kind, Vector2 anchor) {
  using components::GPUParticle;
  std::vector<GPUParticle> particles;
  if (kind == UiCraftBurstKind::Fuse) {
    // Legacy fuse burst (UICraftingController, pre-R7): 20 gold sparks plus
    // 20 ancient-red ink trails, speed 100..300 in a random direction.
    particles.reserve(40);
    for (int i = 0; i < 20; ++i) {
      const float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
      const float speed = 100.0f + (float)GetRandomValue(0, 200);
      const Vector2 velocity = {cosf(angle) * speed, sinf(angle) * speed};
      particles.push_back(
          systems::InkEffectHelper::CreateSpark(anchor, velocity, GOLD, 2.5f));
    }
    for (int i = 0; i < 20; ++i) {
      const float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
      const float speed = 100.0f + (float)GetRandomValue(0, 200);
      const Vector2 velocity = {cosf(angle) * speed, sinf(angle) * speed};
      GPUParticle p = systems::InkEffectHelper::CreateInkTrail(
          anchor, velocity, 2.0f, 0.8f);
      p.color = {230, 0, 0, 200}; // ancient-blood red (legacy).
      particles.push_back(p);
    }
    return particles;
  }
  // Legacy salvage burst: 30 red sparks, speed 150..400.
  particles.reserve(30);
  for (int i = 0; i < 30; ++i) {
    const float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
    const float speed = 150.0f + (float)GetRandomValue(0, 250);
    const Vector2 velocity = {cosf(angle) * speed, sinf(angle) * speed};
    particles.push_back(
        systems::InkEffectHelper::CreateSpark(anchor, velocity, RED, 2.0f));
  }
  return particles;
}

} // namespace NoMoreDay::ui
