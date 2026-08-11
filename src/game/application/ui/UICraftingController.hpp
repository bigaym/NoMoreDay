#pragma once

#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"

#include <cstdint>

#include <entt/entt.hpp>

namespace NoMoreDay::ui {

// Instance controller for the crafting panel (锻造 / 融合 / 分解).
//
// Ports the legacy static panel UICrafting into a hostable instance: the
// controller owns a UiRuntime root node (created in the ctor), performs the
// same per-frame update as the original UICrafting::Update (alpha animation of
// the panel visibility plus dropping stale entity references) and renders the
// forging, merging and salvaging tabs with the exact same visuals as the
// original UICrafting::Draw. It holds no static mutable UI state; the legacy
// static members were migrated into instance members and are reset by
// EnterGameplay/LeaveGameplay so no session state leaks into the next run.
//
// The controller is meant to be owned by GameUiHost; the host drives
// EnterGameplay/LeaveGameplay around gameplay sessions and feeds Update once
// per frame. The runtime node visibility mirrors the legacy m_visible panel
// flag (Toggle/SetTargetItem/OpenMergePanel keep the two in sync).
class UICraftingController {
public:
  explicit UICraftingController(UiRuntime& runtime);
  ~UICraftingController() = default;

  UICraftingController(const UICraftingController&) = delete;
  UICraftingController& operator=(const UICraftingController&) = delete;

  // Resets session-scoped state (forge/merge/salvage targets, salvage filter,
  // tab, alpha) and mirrors the panel node when a gameplay session begins.
  // Like the legacy UICrafting, the panel starts closed on EnterGameplay.
  // Idempotent.
  void EnterGameplay();

  // Clears session-scoped state and hides the panel root node. Idempotent.
  void LeaveGameplay();

  // Per-frame update: mirrors the legacy UICrafting::Update (alpha animation
  // of the panel plus dropping stale entity references). Does not draw.
  void Update(entt::registry& registry);

  // Draws the crafting panel (forging/merging/salvaging tabs). Visual output
  // is equivalent to the legacy UICrafting::Draw.
  void Draw(entt::registry& registry);

  // Sets the item in the forge slot and opens the panel (same semantics as the
  // legacy UICrafting::SetTargetItem, used by the item context menu).
  void SetTargetItem(entt::entity item);

  // Item currently placed in the forge slot (entt::null when empty).
  [[nodiscard]] entt::entity GetTargetItem() const noexcept;

  // Clears the forge slot without closing the panel.
  void ClearTargetItem();

  // Toggles panel visibility (and the runtime node). Idempotent.
  void Toggle();

  [[nodiscard]] bool IsVisible() const noexcept;

  // Opens the panel directly on the legendary-merge tab (used when consuming
  // a Legendary Core item).
  void OpenMergePanel();

  // Runtime node id of the panel root (kInvalidUiId if the node could not be
  // created, e.g. a duplicate id already exists in the runtime).
  [[nodiscard]] UiId NodeId() const noexcept;

  [[nodiscard]] bool IsInGameplay() const noexcept;

private:
  // Batch salvage filter (ported from the legacy namespace-scope SalvageFilter
  // struct). Defaults are applied in the ctor and on every session reset; the
  // Rarity enumerators they need are only complete in the .cpp.
  struct SalvageFilter {
    std::uint32_t rarityMask = 0;
    std::uint32_t categoryMask = 0;
    bool keepIfTier6Plus = true;
    bool excludeLocked = true;
  };

  enum class CraftingTab { Forging, Merging, Salvaging };

  void DrawCraftingPanel(entt::registry& registry);
  void DrawMergePanel(entt::registry& registry, float startX, float startY,
                      float panelW, float panelH, float alpha);
  void DrawAffixList(entt::registry& registry, entt::entity entity,
                     float panelStartX, float panelStartY);
  void DrawSalvagePanel(entt::registry& registry, float startX, float startY,
                        float panelW, float panelH, float alpha);

  // Restores the migrated session state to its defaults.
  void ResetSessionState() noexcept;
  void SetNodeVisible(bool visible);

  UiRuntime& m_runtime;
  UiId m_rootNodeId = kInvalidUiId;
  bool m_inGameplay = false; // Session state set by Enter/LeaveGameplay.

  // Session-scoped panel state migrated from the legacy static members of
  // UICrafting (U7 cleanup: static mutable state -> instance members).
  entt::entity m_forgeItem = entt::null;
  entt::entity m_mergeBase = entt::null;
  entt::entity m_mergeFodder = entt::null;
  entt::entity m_mergeCatalyst = entt::null;
  int m_selectedAffixIndex = -1;
  entt::entity m_salvageItem = entt::null;
  SalvageFilter m_salvageFilter;
  bool m_showSalvageFilter = false;
  CraftingTab m_currentTab = CraftingTab::Forging;
  float m_craftingAlpha = 0.0f;
  bool m_visible = false; // Mirrored to the runtime node visibility.
};

} // namespace NoMoreDay::ui
