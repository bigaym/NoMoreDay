#pragma once

#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/TooltipController.hpp"
#include "game/application/ui/UIPanelDragService.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"

#include <array>
#include <cstdint>

#include <entt/entt.hpp>

// Forward declarations of the gameplay components read by the hotbar/buff
// interaction path (defined in game/foundation/components). Update() keeps the
// drag-drop write (registry mutation) for the R8 intent migration; the paint
// path never touches the registry.
namespace NoMoreDay {
struct ActiveSkillsComponent;
struct ActiveEffectsComponent;
struct CombatStats;
} // namespace NoMoreDay

namespace NoMoreDay::ui {

class GameUiHost; // U8 back-pointer for the skill-drag session (same pattern
                  // as UIStashController).

// Instance controller for the gameplay skill hotbar and buff/debuff strip.
//
// R5 migration: paint is snapshot-only. Update(const GameUiSnapshot&) resolves
// the slot/buff display data into fixed controller-owned caches (zero per-frame
// allocation); Paint(UiDrawList&) emits draw-list commands only (Hud layer).
// The interaction path (hover tooltip, drag-drop assignment, right-click
// context menu) stays in Update and keeps its registry write for the R8 intent
// migration; the paint path never reads the ECS registry.
//
// The controller is owned by GameUiHost; the host drives
// EnterGameplay/LeaveGameplay around gameplay sessions, feeds Update once per
// frame with the frame snapshot, and calls Paint during PrepareRender.
class SkillHotbarController {
public:
  explicit SkillHotbarController(UiRuntime& runtime,
                                 TooltipController* tooltipController = nullptr,
                                 GameUiHost* uiHost = nullptr);
  ~SkillHotbarController() = default;

  SkillHotbarController(const SkillHotbarController&) = delete;
  SkillHotbarController& operator=(const SkillHotbarController&) = delete;

  // Resets session-scoped state when a gameplay session begins. Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the hotbar root node. Idempotent.
  void LeaveGameplay();

  // Per-frame update: caches the slot/buff display data from the snapshot
  // (rebuilt when the revision changes) and runs the interaction path
  // (hover/drag-drop/right-click). R8: drag-drop now enqueues a SkillAssign
  // intent (handled by GameUiCommandHandler) instead of mutating the registry;
  // the hotbar surface never writes gameplay state directly.
  void Update(const GameUiSnapshot& snapshot, const UiInputFrame& input);

  // Paints the skill hotbar (Q/W/E/R/RMB) and the buff/debuff strip above the
  // HP/mana bars into the draw list (Hud layer). Snapshot-only, zero
  // allocation: reads the caches filled by Update.
  void Paint(UiDrawList& drawList, const UiViewport& viewport) const;

  // Toggles the hotbar root node visibility in the runtime.
  void SetVisible(bool visible);
  [[nodiscard]] bool IsVisible() const noexcept;
  [[nodiscard]] bool IsInGameplay() const noexcept;
  [[nodiscard]] bool HasPlayerData() const noexcept;

  // Runtime node id of the hotbar root (kInvalidUiId if the node could not be
  // created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept;

private:
  // Rebuilds the slot/buff display caches from the snapshot (revision-gated).
  void CacheFromSnapshot(const GameUiSnapshot& snapshot);
  // Interaction path: hover tooltip / drag-drop / right-click. R8: the
  // drag-drop write is an enqueued SkillAssign intent; paint never reads
  // gameplay state.
  void ProcessInteraction(const GameUiSnapshot& snapshot,
                          const UiInputFrame& input);

  // Cached skill-slot display data (paint reads this, never the registry).
  struct SlotCache {
    std::uint32_t iconAssetId = 0; // SkillData::icon_id (texture asset id).
    float cooldownRatio = 0.0f;    // clamp(slot.cooldown / cooldownMax).
    float remainingCooldown = 0.0f;
    float manaCost = 0.0f;
    int maxCharges = 1;
    int currentCharges = 0;
    bool hasEnoughMana = true;
    const char* keyLabel = nullptr; // "Q"/"W"/"E"/"R"/"RMB" (static).
  };
  std::array<SlotCache, 5> m_slotCache{};

  // Cached buff/debuff display data (paint reads this, never the registry).
  struct BuffCache {
    std::uint32_t iconAssetId = 0; // BuffVisualData::icon_asset->id.
    const char* iconText = nullptr; // BuffVisualData::icon_text (static).
    float ratio = 0.0f;             // clamp(remaining / duration).
    int stacks = 0;
    bool isDebuff = false;
  };
  static constexpr std::size_t kMaxBuffIcons = 24; // 10/row, 2 rows per side.
  std::array<BuffCache, kMaxBuffIcons> m_buffCache{};
  std::size_t m_buffCount = 0;

  // Interaction results (consumed by Paint for hover highlights).
  int m_hoveredSlot = -1;
  int m_hoveredBuff = -1;

  TooltipController* m_tooltip = nullptr;
  GameUiHost* m_uiHost = nullptr;
  UiRuntime& m_runtime;
  UiId m_rootNodeId = kInvalidUiId;
  bool m_visible = true;        // Mirrors the runtime node visibility.
  bool m_inGameplay = false;    // Session state set by Enter/LeaveGameplay.
  bool m_hasPlayerData = false; // Refreshed by Update from the snapshot.
  std::uint64_t m_lastRevision = 0;

  // U8 drag session accessor (same pattern as UIStashController).
  UIDragSession& DragSession() noexcept;
  UIDragSession m_localDragSession;
};

} // namespace NoMoreDay::ui
