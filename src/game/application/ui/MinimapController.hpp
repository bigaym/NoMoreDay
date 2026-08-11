#pragma once

#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"

#include <vector>

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
// Ports the legacy static panel UIMinimap into a hostable instance: the
// controller owns a UiRuntime root node (created in the ctor, declared layout
// for the top-right minimap region) and renders with the exact same visual
// output as the original UIMinimap::Draw. It holds no static mutable UI
// state; all texture/cache/debug-reveal state is an instance member.
//
// Owned by GameUiHost; the host drives EnterGameplay/LeaveGameplay around
// gameplay sessions, calls Draw during the UI render pass (before the legacy
// panel renderer, matching the legacy draw order), forwards the F1 debug
// toggle to ToggleDebugReveal, and calls Shutdown before GL resource teardown.
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

  // Draws the minimap (texture, enemies, player marker, zone/kill info).
  // Visual output is equivalent to the legacy UIMinimap::Draw.
  void Draw(entt::registry& registry, const LevelManager& levelManager,
            const NoMoreDay::systems::SpatialHashGrid* grid = nullptr);

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
};

} // namespace NoMoreDay::ui
