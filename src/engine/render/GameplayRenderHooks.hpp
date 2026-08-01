#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "engine/render/GPUData.hpp"
#include <vector>

namespace NoMoreDay::render {

// Instanced beam element for loot rarity beams. Pure data, owned by the Engine
// SSBO; the Game adapter only fills the CPU-side vector via the hooks contract.
struct GPUBeamInstance {
  Vector2 position;
  Vector2 size;
  Vector4 color;
  float time;
  float padding[3];
};

/**
 * @brief Engine-safe per-frame DTO handed to the Game gameplay render adapter.
 *
 * Zero game/app dependencies: registry/camera come from the caller, the
 * label/glyph/beam vectors are Engine-owned instance buffers the adapter fills
 * (Engine keeps the SSBO/shaders and performs the instanced draws), and `font`
 * is an out-field the adapter fills so the Engine glyph pass can bind the atlas.
 */
struct GameplayRenderFrame {
  entt::registry &registry;
  const Camera2D &camera;

  // Engine-owned instance buffers (filled by adapter, drawn by Engine).
  std::vector<NoMoreDay::components::GPULabelInstance> *labelBuffer = nullptr;
  std::vector<NoMoreDay::components::GPUGlyphInstance> *glyphBuffer = nullptr;
  std::vector<GPUBeamInstance> *beamBuffer = nullptr;

  // Engine-computed render flags (quality config + runtime readiness).
  bool gpuTextEnabled = false;
  bool gpuLootEnabled = false;
  bool gpuLootGlowEnabled = false;

  // Out-field: glyph atlas font the Engine binds for the glyph instanced draw.
  Font font = {};
};

/**
 * @brief Game-side gameplay rendering contract.
 *
 * The Engine calls these hooks from the scene/VFX/UIWorld passes. nullptr/empty
 * hooks mean the gameplay draw segment is skipped entirely (gate/harness paths
 * must render nothing and must not crash).
 */
class GameplayRenderHooks {
public:
  virtual ~GameplayRenderHooks() = default;

  virtual void onFrameData(GameplayRenderFrame &frame) = 0;
  virtual void onScene(GameplayRenderFrame &frame) = 0;
  virtual void onVFX(GameplayRenderFrame &frame) = 0;
  virtual void onUIWorld(GameplayRenderFrame &frame) = 0;
};

} // namespace NoMoreDay::render
