#pragma once

#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"

#include <cstdint>

#include <entt/entt.hpp>

// Forward declarations. The controller only passes these by reference, so
// declarations suffice in this header.
class LevelManager;
namespace NoMoreDay {
enum class MaterialCategory : std::uint8_t;
} // namespace NoMoreDay

namespace NoMoreDay::ui {

// Instance controller for the inventory panel.
//
// Ports the legacy static panel UIInventory into a hostable instance: the
// controller owns a UiRuntime root node (created in the ctor) and performs the
// same per-frame update as the original UIInventory::Update (alpha animation
// of the panel visibility). It holds no static mutable UI state; the legacy
// static members were migrated into instance members and are reset by
// EnterGameplay/LeaveGameplay so no session state leaks into the next run.
//
// The controller is meant to be owned by GameUiHost; the host drives
// EnterGameplay/LeaveGameplay around gameplay sessions and feeds Update once
// per frame. The legacy UIInventory::Draw stays in the static panel for now
// and is migrated in a later U7 step.
class UIInventoryController {
public:
  explicit UIInventoryController(UiRuntime& runtime);
  ~UIInventoryController() = default;

  UIInventoryController(const UIInventoryController&) = delete;
  UIInventoryController& operator=(const UIInventoryController&) = delete;

  // Resets session-scoped state (page/tab/scroll/search state) and reveals the
  // panel root node when a gameplay session begins. Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the panel root node. Idempotent.
  void LeaveGameplay();

  // Per-frame update: mirrors the legacy UIInventory::Update (alpha animation
  // of the panel). Does not draw anything.
  void Update(entt::registry& registry, const LevelManager& levelManager);

  // Runtime node id of the panel root (kInvalidUiId if the node could not be
  // created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept;

  [[nodiscard]] bool IsInGameplay() const noexcept;

private:
  // Restores the migrated session state to its defaults.
  void ResetSessionState() noexcept;
  void SetNodeVisible(bool visible);

  UiRuntime& m_runtime;
  UiId m_rootNodeId = kInvalidUiId;
  bool m_inGameplay = false; // Session state set by Enter/LeaveGameplay.

  // Session-scoped panel state migrated from the legacy static members of
  // UIInventory (U7 cleanup: static mutable state -> instance members).
  int m_inventoryPage = 0;
  int m_activeTab = 0; // 0: Items, 1: Materials
  float m_materialScrollOffset = 0.0f;
  char m_searchBuffer[64] = {0};
  // Defaults to MaterialCategory::Count ("All") once the ctor runs.
  NoMoreDay::MaterialCategory m_selectedCategory;
  bool m_isSearchFocused = false;
};

} // namespace NoMoreDay::ui
