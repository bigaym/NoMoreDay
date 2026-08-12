#pragma once

#include "game/application/ui/UIPanelDragService.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"

#include <entt/entt.hpp>

namespace NoMoreDay::ui {

// Instance controller for the character attribute panel.
//
// Ports the legacy static panel NoMoreDay::UICharacter into a hostable
// instance: the controller owns a UiRuntime root node (created in the ctor)
// and renders the panel with the exact same visual output as the original
// UICharacter::Draw. The state that used to live in file-scope statics in
// UICharacter.cpp (active tab, panel scroll offset, last content height) is
// now instance state, reset by EnterGameplay/LeaveGameplay, so the controller
// holds no static mutable UI state.
//
// The controller is meant to be owned by GameUiHost; the host drives
// EnterGameplay/LeaveGameplay around gameplay sessions, feeds SetVisible /
// SetAlpha from its panel visibility logic, and calls Draw during the UI
// render pass with the current player entity.
class UICharacterController {
public:
  explicit UICharacterController(UiRuntime& runtime);
  ~UICharacterController() = default;

  UICharacterController(const UICharacterController&) = delete;
  UICharacterController& operator=(const UICharacterController&) = delete;

  // Resets session-scoped state (tab, scroll) and hides the panel root node
  // (a gameplay session starts with the panel closed; KEY_C opens it).
  // Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the panel root node. Idempotent.
  void LeaveGameplay();

  // U8: per-frame instance alpha update towards the authoritative visibility
  // flag (m_visible; host KEY_C/ESC route through SetVisible, so no legacy
  // State re-adopt remains). Does not draw anything. Called by the host right
  // after the legacy update.
  void Update(float dt);

  // Draws the character attribute panel for the given player entity. When
  // player is entt::null (or no longer valid), the legacy PlayerTag lookup is
  // used as a fallback, preserving the original UICharacter::Draw behavior.
  void Draw(entt::registry& registry, entt::entity player);

  // Toggles the panel root node visibility in the runtime.
  void SetVisible(bool visible);
  [[nodiscard]] bool IsVisible() const noexcept;

  // Sets the panel opacity used by Draw (1.0 = fully opaque).
  void SetAlpha(float alpha);
  [[nodiscard]] float Alpha() const noexcept;

  [[nodiscard]] bool IsInGameplay() const noexcept;

  // Runtime node id of the panel root (kInvalidUiId if the node could not be
  // created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept;

private:
  // Stat row helper (ported verbatim from UICharacter; stateless).
  static void DrawStatRow(const char* label, const char* value, float x,
                          float& y, float width, float fontSize = 16.0f,
                          float alpha = 1.0f);

  // Restores the panel session state to its defaults.
  void ResetSessionState();

  UiRuntime& m_runtime;
  UiId m_rootNodeId = kInvalidUiId;
  // U8: instance visibility (authoritative; host KEY_C/ESC route through
  // SetVisible). Defaults to false so the panel does not show at startup.
  bool m_visible = false;
  bool m_inGameplay = false;  // Session state set by Enter/LeaveGameplay.
  // U8: panel opacity, animated by Update; starts at 0.0 (matching the legacy
  // State.characterPanelAlpha) so the render gate never opens at startup.
  float m_alpha = 0.0f;

  // U8: instance panel-drag state (was UISystem::State.panelStates + the
  // UISystem::UpdatePanelDrag helper; same pattern as UIStashController).
  NoMoreDay::PanelState m_panelState;
  UIPanelID m_activeDragPanel = UIPanelID::None;

  // Panel session state (ported from the file-scope statics of the legacy
  // UICharacter.cpp; reset by Enter/LeaveGameplay).
  int m_activeCharTab = 0;  // 0: attack, 1: defense, 2: summon, 3: other.
  float m_charPanelScroll = 0.0f;
  float m_lastContentHeight = 0.0f;
};

} // namespace NoMoreDay::ui
