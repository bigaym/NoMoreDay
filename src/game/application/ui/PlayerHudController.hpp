#pragma once

#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/SwordIntentWidget.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::ui {

// Instance controller for the gameplay Player HUD.
//
// R5 migration: the HUD is a snapshot-only panel. Update(const GameUiSnapshot&)
// resolves all dynamic text (HP/mana numbers, blade feedback, summon rows) into
// fixed controller-owned buffers; Paint(UiDrawList&) emits draw-list commands
// only (Hud layer). The controller never reads the ECS registry and never
// allocates per-frame (no std::string / std::map in the paint path).
//
// The controller is owned by GameUiHost; the host drives
// EnterGameplay/LeaveGameplay around gameplay sessions, feeds Update once per
// frame with the frame snapshot, and calls Paint during PrepareRender.
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

  // Per-frame snapshot update: formats the HP/mana/feedback strings into
  // fixed buffers (rebuilt when the revision changes) and caches the summon
  // rows. Zero per-frame allocation.
  void Update(const GameUiSnapshot& snapshot, int fps, float timeSeconds);

  // Paints the HUD (FPS counter, HP/barrier/mana bars, blade-resource widget,
  // summon status) into the draw list at the HUD layer.
  void Paint(UiDrawList& drawList, const UiViewport& viewport) const;

  // Toggles the HUD root node visibility in the runtime.
  void SetVisible(bool visible);
  [[nodiscard]] bool IsVisible() const noexcept;
  [[nodiscard]] bool IsInGameplay() const noexcept;
  [[nodiscard]] bool HasPlayerData() const noexcept;

  // Runtime node id of the HUD root (kInvalidUiId if the node could not be
  // created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept;

private:
  // Fixed text buffers (rebuilt only when the underlying values change).
  std::array<char, 48> m_fpsText{};
  std::array<char, 64> m_hpText{};
  std::array<char, 64> m_manaText{};
  std::array<char, 96> m_feedbackText{};
  std::array<char, 160> m_bladeDetailText{};
  // Cached summon rows (fixed capacity; the builder groups by key).
  struct SummonRow {
    std::uint32_t iconId = 0;
    float lifeRatio = 0.0f;
    std::uint32_t count = 0;
    std::array<char, 40> displayName{};
  };
  std::array<SummonRow, 16> m_summonRows{};
  std::size_t m_summonRowCount = 0;

  // Cached bar metrics (resolved from the snapshot in Update, painted in Paint).
  float m_hpPct = 0.0f;
  float m_manaPct = 0.0f;
  float m_barrierPct = 0.0f;
  float m_barrierDisplayValue = 0.0f;
  float m_maxBarrier = 0.0f;
  bool m_hasBarrier = false;
  bool m_barrierOverflow = false;
  bool m_hasBladeResource = false;
  bool m_hasSwordIntent = false;
  bool m_showRestartReady = false;

  std::uint64_t m_lastRevision = 0;
  float m_lastTimeSeconds = 0.0f;

  UiRuntime& m_runtime;
  UiId m_rootNodeId = kInvalidUiId;
  bool m_visible = true;      // Mirrors the runtime node visibility.
  bool m_inGameplay = false;  // Session state set by Enter/LeaveGameplay.
  bool m_hasPlayerData = false; // Set by Update from the snapshot.

  // Instance blade-resource widget (R5: paints through the draw list).
  NoMoreDay::systems::ui::SwordIntentWidget m_swordIntent;
};

} // namespace NoMoreDay::ui
