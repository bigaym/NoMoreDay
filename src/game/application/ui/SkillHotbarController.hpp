#pragma once

#include "game/application/ui/TooltipController.hpp"
#include "game/application/ui/UiRuntime.hpp"

#include <string>

#include <entt/entt.hpp>

// Forward declarations of the gameplay components read by the hotbar/buff
// panels (defined in game/foundation/components). Draw queries them through
// the registry and only touches them by const-ref, so declarations suffice in
// this header.
namespace NoMoreDay {
struct ActiveSkillsComponent;
struct ActiveEffectsComponent;
struct CombatStats;
} // namespace NoMoreDay

namespace NoMoreDay::ui {

// Instance controller for the gameplay skill hotbar and buff/debuff strip.
//
// Ports the legacy static panels UISystem::DrawSkillHotbar and UISystem::DrawBuffs
// into a hostable instance: the controller owns a UiRuntime root node (created
// in the ctor) and renders both strips with the exact same visual output as the
// original implementation. It holds no static mutable UI state.
//
// The controller is meant to be owned by GameUiHost; the host drives
// EnterGameplay/LeaveGameplay around gameplay sessions, feeds Update once per
// frame, and calls Draw during the UI render pass (after UISystem::Draw so the
// legacy panels underneath keep their original z-order).
class SkillHotbarController {
public:
  explicit SkillHotbarController(UiRuntime& runtime,
                                 TooltipController* tooltipController = nullptr);
  ~SkillHotbarController() = default;

  SkillHotbarController(const SkillHotbarController&) = delete;
  SkillHotbarController& operator=(const SkillHotbarController&) = delete;

  // Resets session-scoped state when a gameplay session begins. Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the hotbar root node. Idempotent.
  void LeaveGameplay();

  // Per-frame update: mirrors the player-data queries performed by Draw so the
  // controller observes the same registry data source. Does not draw anything.
  void Update(entt::registry& registry);

  // Draws the skill hotbar (Q/W/E/R/RMB) and the buff/debuff strip above the
  // HP/mana bars. Visual output is equivalent to the legacy
  // UISystem::DrawSkillHotbar + UISystem::DrawBuffs.
  void Draw(entt::registry& registry);

  // Toggles the hotbar root node visibility in the runtime.
  void SetVisible(bool visible);
  [[nodiscard]] bool IsVisible() const noexcept;
  [[nodiscard]] bool IsInGameplay() const noexcept;
  [[nodiscard]] bool HasPlayerData() const noexcept;

  // Runtime node id of the hotbar root (kInvalidUiId if the node could not be
  // created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept;

private:
  // Painted bottom-center of the screen (ported verbatim from
  // UISystem::DrawSkillHotbar).
  void DrawHotbar(entt::registry& registry);
  // Painted above the HP/mana bars (ported verbatim from UISystem::DrawBuffs).
  void DrawBuffStrip(entt::registry& registry);

  // U7 group 6-B: optional tooltip controller the hover writes route through
  // (SetHoveredSkillSlot / SetHoveredBuff) instead of UISystem::State. GameUiHost
  // injects its own controller; a null pointer keeps the legacy hotbar tests
  // compiling and simply skips the hover write.
  TooltipController* m_tooltip = nullptr;
  UiRuntime& m_runtime;
  UiId m_rootNodeId = kInvalidUiId;
  bool m_visible = true;        // Mirrors the runtime node visibility.
  bool m_inGameplay = false;    // Session state set by Enter/LeaveGameplay.
  bool m_hasPlayerData = false; // Refreshed by Update from the registry.
};

} // namespace NoMoreDay::ui
