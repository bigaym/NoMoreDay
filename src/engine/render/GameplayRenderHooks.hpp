#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "engine/render/GPUData.hpp"
#include "engine/render/lighting/GlobalHeightField.hpp"
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

  // Engine-owned occluder staging buffer (filled by the adapter via the shared
  // OccluderProjector, consumed by OccluderExtractPass/ShadowBuildPass through
  // graph::RenderContext).
  std::vector<NoMoreDay::components::GPUShadowCaster> *occluderBuffer = nullptr;

  // Engine-owned light candidate staging buffer (filled by the adapter via the
  // shared LightAdapter projection, consumed by LightManager::UpdateCandidates).
  std::vector<NoMoreDay::components::GPULight> *lightBuffer = nullptr;

  // Engine-owned height-field stamp staging buffer (filled by the adapter via
  // the shared HeightFieldAdapter projection, consumed by HeightShadowPass
  // through graph::RenderContext).
  std::vector<lighting::GlobalHeightField::HeightStamp> *heightFieldBuffer =
      nullptr;

  // Engine-owned loot instance staging buffer (filled by the adapter via the
  // shared GPULootAdapter projection, consumed by
  // GPULootSystem::UploadInstances).
  std::vector<NoMoreDay::components::GPULootInstance> *lootBuffer = nullptr;

  // Engine-owned emissive stamp staging buffer (filled by the adapter via the
  // shared EmissiveStampAdapter projection, consumed by
  // RadianceCascadesPass::RunMaterialEmissive through graph::RenderContext).
  std::vector<NoMoreDay::components::EmissiveStampInput> *emissiveStampBuffer =
      nullptr;

  // Engine-computed render flags (quality config + runtime readiness).
  bool gpuTextEnabled = false;
  bool gpuLootEnabled = false;
  bool gpuLootGlowEnabled = false;

  // Out-field: glyph atlas font the Engine binds for the glyph instanced draw.
  Font font = {};

  // Out-fields: occluder projection stats filled by the adapter (shared FNV
  // signature contract consumed by OccluderExtractPass).
  uint32_t occluderStaticCount = 0u;
  uint32_t occluderDynamicCount = 0u;
  uint64_t occluderStaticSignature = 0u;
  uint64_t occluderDynamicSignature = 0u;

  // Out-field: number of registered ECS lights seen by the adapter (lighting
  // diagnostic stat consumed by LightManager::UpdateCandidates).
  int ecsLights = 0;

  // Out-fields: game world semantics injected by the adapter (previously read
  // from game Constants::World by HeightShadowPass).
  float worldWidth = 0.0f;
  float worldHeight = 0.0f;
  float tileWorldSize = 0.0f;
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
  virtual void onOccluders(GameplayRenderFrame &frame) = 0;
  virtual void onLights(GameplayRenderFrame &frame) = 0;
  virtual void onHeightField(GameplayRenderFrame &frame) = 0;
  virtual void onLoot(GameplayRenderFrame &frame) = 0;
  virtual void onEmissive(GameplayRenderFrame &frame) = 0;
};

} // namespace NoMoreDay::render
