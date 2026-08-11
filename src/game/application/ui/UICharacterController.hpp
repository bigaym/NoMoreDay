#pragma once

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

  // Resets session-scoped state (tab, scroll) and shows the panel root node.
  // Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the panel root node. Idempotent.
  void LeaveGameplay();

  // Draws the character attribute panel for the given player entity. When
  // player is entt::null (or no longer valid), the legacy PlayerTag lookup is
  // used as a fallback, preserving the original UICharacter::Draw behavior.
  void Draw(entt::registry& registry, entt::entity player);

  // Toggles the panel root node visibility in the runtime.
  void SetVisible(bool visible);
  [[nodiscard]] bool IsVisible() const noexcept;

  // Sets the panel opacity used by Draw (1.0 = fully opaque). Also mirrors
  // the value into UISystem::State.characterPanelAlpha so legacy consumers of
  // the static UI context (e.g. gameplay input gating) stay coherent.
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
  bool m_visible = true;      // Mirrors the runtime node visibility.
  bool m_inGameplay = false;  // Session state set by Enter/LeaveGameplay.
  float m_alpha = 1.0f;       // Panel opacity, driven by the host.

  // Panel session state (ported from the file-scope statics of the legacy
  // UICharacter.cpp; reset by Enter/LeaveGameplay).
  int m_activeCharTab = 0;  // 0: attack, 1: defense, 2: summon, 3: other.
  float m_charPanelScroll = 0.0f;
  float m_lastContentHeight = 0.0f;
};

} // namespace NoMoreDay::ui
