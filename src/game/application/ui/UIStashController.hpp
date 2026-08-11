#pragma once

#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"

#include <cstdint>
#include <utility>
#include <vector>

#include <entt/entt.hpp>

// Forward declaration. The controller exposes the active stash type by value
// and stores it as an instance member, so a declaration suffices here.
namespace NoMoreDay {
enum class StashType : std::uint8_t;
} // namespace NoMoreDay

namespace NoMoreDay::ui {

// Instance controller for the stash panel.
//
// Ports the legacy static panel UIStash into a hostable instance: the
// controller owns a UiRuntime root node (created in the ctor) and performs the
// same per-frame work as the original UIStash::Update (alpha animation) and
// UIStash::Draw (panel rendering, drag, tabs, search, stash interaction). It
// holds no static mutable UI state; the legacy static members were migrated
// into instance members and are reset by EnterGameplay/LeaveGameplay so no
// session state leaks into the next run.
//
// The controller is meant to be owned by GameUiHost; the host drives
// EnterGameplay/LeaveGameplay around gameplay sessions and feeds Update once
// per frame. The legacy static UIStash stays as the fallback path for now and
// is migrated in a later U7 step.
class UIStashController {
public:
  explicit UIStashController(UiRuntime& runtime);
  ~UIStashController() = default;

  UIStashController(const UIStashController&) = delete;
  UIStashController& operator=(const UIStashController&) = delete;

  // Resets session-scoped state (active type/tab, search buffers and cache)
  // and reveals the panel root node when a gameplay session begins.
  // Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the panel root node. Idempotent.
  void LeaveGameplay();

  // Per-frame update: mirrors the legacy UIStash::Update (alpha animation of
  // the panel visibility). Does not draw anything.
  void Update(entt::registry& registry);

  // Opens/closes/toggles the stash panel. Mirrors the legacy UIStash API:
  // Open also reveals the inventory panel (drag destination) and resets the
  // active tab to the first one, Toggle flips the current visibility and
  // Close hides the stash only.
  void Open(NoMoreDay::StashType type);
  void Close();
  void Toggle();

  // Mirrors the legacy UIStash read accessors.
  [[nodiscard]] bool IsVisible() const noexcept;
  [[nodiscard]] NoMoreDay::StashType GetActiveType() const noexcept;
  [[nodiscard]] int GetActiveTabIndex() const noexcept;

  // Draws the stash panel with the same visual output as the legacy
  // UIStash::Draw. Immediate-mode raylib; reads the shared UISystem::State.
  void Draw(entt::registry& registry);

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
  // UIStash (U7 cleanup: static mutable state -> instance members).
  // Defaults to StashType::Personal once the ctor runs.
  NoMoreDay::StashType m_activeType;
  int m_activeTabIndex = 0;
  char m_searchBuffer[64] = {0};
  char m_lastSearchBuffer[64] = {0}; // Cache key of the search results below.
  std::vector<std::pair<int, int>> m_cachedSearchResults; // Cache value.
  bool m_isSearchFocused = false;
};

} // namespace NoMoreDay::ui
