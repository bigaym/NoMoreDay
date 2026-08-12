#pragma once

#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include <entt/entt.hpp>

#include "raylib.h"

class LevelManager;
namespace NoMoreDay {
namespace systems {
class SpatialHashGrid;
}
} // namespace NoMoreDay

namespace NoMoreDay::ui {

// Instance controller for the gameplay minimap.
//
// R5 migration: paint is snapshot-only. Update(const GameUiSnapshot&) keeps
// the fog-texture maintenance (a retained GPU resource driven by the world's
// FogOfWarSystem/MapSystem — the LevelManager is a world system, not the ECS
// registry) and resolves the overlay markers (enemies, player, portal arrow,
// zone/kill/affix texts) from the snapshot into fixed controller-owned caches.
// Paint emits draw-list commands only (Hud layer).
//
// Owned by GameUiHost; the host drives EnterGameplay/LeaveGameplay around
// gameplay sessions, feeds Update once per frame with the frame snapshot, calls
// Paint during PrepareRender, forwards the F1 debug toggle to ToggleDebugReveal
// and calls Shutdown before GL resource teardown.
class MinimapController {
public:
  explicit MinimapController(UiRuntime& runtime);
  ~MinimapController();

  MinimapController(const MinimapController&) = delete;
  MinimapController& operator=(const MinimapController&) = delete;

  // Releases GPU resources (minimap texture). Must run while the GL context
  // is still alive (GameUiHost::Shutdown runs before resource unload).
  void Shutdown();

  // Resets session-scoped state when a gameplay session begins. Idempotent.
  void EnterGameplay();

  // Clears session-scoped state when a gameplay session ends. Idempotent.
  void LeaveGameplay();

  // Per-frame update: maintains the fog texture (LevelManager world data) and
  // caches the overlay markers from the snapshot. deltaSeconds drives the
  // refresh timer of the partial fog-texture upload.
  void Update(const GameUiSnapshot& snapshot, const LevelManager& levelManager,
              const NoMoreDay::systems::SpatialHashGrid* grid = nullptr,
              float deltaSeconds = 0.0f);

  // Paints the minimap (texture, enemies, player marker, zone/kill info) into
  // the draw list (Hud layer). Snapshot-only: reads the caches filled by
  // Update, zero allocation.
  void Paint(UiDrawList& drawList, const UiViewport& viewport) const;

  // The GPU minimap texture (owned by the controller). The host registers it
  // under kMinimapTextureResourceId so the paint path can reference it by id.
  [[nodiscard]] const Texture2D& Texture() const noexcept {
    return m_minimapTexture;
  }

  // Flips the debug reveal flag (F1), same semantics as the legacy toggle.
  void ToggleDebugReveal();
  [[nodiscard]] bool DebugRevealEnabled() const noexcept {
    return m_debugRevealMap;
  }

  // Runtime node id of the minimap root (kInvalidUiId if the node could not
  // be created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept { return m_rootNodeId; }

private:
  // Unloads the minimap texture and clears CPU-side caches.
  void UnloadResources();

  // Fixed-capacity enemy marker cache (logic-space positions on the minimap).
  struct EnemyDot {
    float x = 0.0f;
    float y = 0.0f;
  };
  static constexpr std::size_t kMaxEnemyDots = 128;
  std::array<EnemyDot, kMaxEnemyDots> m_enemyDots{};
  std::size_t m_enemyDotCount = 0;

  // Cached overlay texts (fixed buffers, rebuilt when the snapshot revision
  // or the zone state changes).
  char m_zoneText[128] = {0};
  bool m_hasZoneText = false;
  char m_killText[64] = {0};
  bool m_hasKillText = false;
  bool m_killGoalReached = false;
  char m_affixTexts[4][64] = {};
  bool m_affixPositive[4] = {};
  std::uint8_t m_affixTextCount = 0;
  // Portal arrow direction (radians) when the next-level portal is reachable.
  bool m_hasPortalArrow = false;
  float m_portalAngle = 0.0f;

  // Player tile + view state captured by Update for the texture crop.
  int m_playerGx = 0;
  int m_playerGy = 0;
  bool m_hasPlayerPosition = false;
  bool m_hasFog = false;
  bool m_isTown = false;
  // Set once Update has run at least once; Paint early-outs before that so a
  // never-updated controller emits zero commands (the host paints the minimap
  // in PrepareRender before the first Draw-time Update of a session).
  bool m_hasRunUpdate = false;

  UiRuntime& m_runtime;
  UiId m_rootNodeId = kInvalidUiId;
  Texture2D m_minimapTexture = {0};
  int m_minimapW = 0;
  int m_minimapH = 0;
  std::vector<Color> m_minimapPixels;
  bool m_debugRevealMap = false;
  bool m_minimapDirty = true;
  std::vector<Color> m_partialBuffer;
  float m_refreshTimer = 0.0f;
  bool m_inGameplay = false;
  std::uint64_t m_lastRevision = 0;
};

} // namespace NoMoreDay::ui
