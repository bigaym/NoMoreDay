#pragma once

#include "game/application/ui/SwordIntentWidget.hpp"
#include "game/application/ui/UiRuntime.hpp"

#include <string>

#include <entt/entt.hpp>

// Forward declarations of the blade-resource view-model components (defined in
// game/foundation/components). The helpers below only pass them by const-ref,
// so declarations suffice in this header.
namespace NoMoreDay {
struct BladeMasteryComponent;
struct BladeResourceComponent;
struct CombatStats;
} // namespace NoMoreDay

namespace NoMoreDay::ui {

// Instance controller for the gameplay Player HUD.
//
// Ports the legacy static panel NoMoreDay::systems::PlayerHUD into a hostable
// instance: the controller owns a UiRuntime root node (created in the ctor)
// and renders the HUD with the exact same visual output as the original
// PlayerHUD::Draw. It holds no static mutable UI state; all animation is
// time-driven through raylib GetTime() exactly like the legacy implementation.
//
// The controller is meant to be owned by GameUiHost; the host drives
// EnterGameplay/LeaveGameplay around gameplay sessions, feeds Update once per
// frame, and calls Draw during the UI render pass.
class PlayerHudController {
public:
  explicit PlayerHudController(UiRuntime& runtime);
  ~PlayerHudController() = default;

  PlayerHudController(const PlayerHudController&) = delete;
  PlayerHudController& operator=(const PlayerHudController&) = delete;

  // Resets session-scoped state when a gameplay session begins. Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the HUD root node. Idempotent.
  void LeaveGameplay();

  // Per-frame update: mirrors the player-data queries performed by Draw so the
  // controller observes the same registry data source. Does not draw anything.
  void Update(entt::registry& registry);

  // Draws the HUD (FPS counter, HP/barrier/mana bars, blade-resource widget,
  // summon status). Visual output is equivalent to the legacy PlayerHUD::Draw.
  void Draw(entt::registry& registry);

  // Toggles the HUD root node visibility in the runtime.
  void SetVisible(bool visible);
  [[nodiscard]] bool IsVisible() const noexcept;
  [[nodiscard]] bool IsInGameplay() const noexcept;
  [[nodiscard]] bool HasPlayerData() const noexcept;

  // Runtime node id of the HUD root (kInvalidUiId if the node could not be
  // created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept;

private:
  // --- Blade-resource view-model helpers (ported verbatim from PlayerHUD) ---
  static const char* ResolveBladeResourceLabel(
      const BladeResourceComponent& bladeResource);
  static std::string ResolveBladeResourceDetailText(
      const BladeMasteryComponent& mastery,
      const BladeResourceComponent& bladeResource);
  static std::string ResolveBladeResourceFeedbackText(
      const BladeMasteryComponent& mastery,
      const BladeResourceComponent& bladeResource);
  static std::string ResolveBladeResourceRuntimeDetailText(
      const entt::registry& registry, entt::entity player,
      const BladeMasteryComponent& mastery,
      const BladeResourceComponent& bladeResource, const CombatStats& stats);
  static std::string ResolveBladeResourceRuntimeFeedbackText(
      const entt::registry& registry, entt::entity player,
      const BladeMasteryComponent& mastery,
      const BladeResourceComponent& bladeResource, const CombatStats& stats);
  static std::string ResolveSwordFlowFeedbackText(
      const BladeResourceComponent& bladeResource);

  UiRuntime& m_runtime;
  UiId m_rootNodeId = kInvalidUiId;
  bool m_visible = true;      // Mirrors the runtime node visibility.
  bool m_inGameplay = false;  // Session state set by Enter/LeaveGameplay.
  bool m_hasPlayerData = false; // Refreshed by Update from the registry.

  // Instance blade-resource widget (U7 cleanup: legacy static mutable state
  // was migrated into this member).
  NoMoreDay::systems::ui::SwordIntentWidget m_swordIntent;
};

} // namespace NoMoreDay::ui
