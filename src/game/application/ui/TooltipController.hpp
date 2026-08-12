#pragma once

#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/foundation/components/SkillDefs.hpp"

#include <cstdint>

#include <entt/entt.hpp>
#include <raylib.h>

namespace NoMoreDay::ui {

class WorldUiFrame;
class GameUiHost; // U8 收尾: modal-input gate via host instance query.
class UiDrawList;
class UiViewport;

// Frame-scoped paint state handed to the registered tooltip painter (R8).
// The host registers the painter with &PaintState() as user data; the backend
// invokes it during Render for the Tooltip-layer Custom command. All data is
// already-resolved snapshot state — the painter never touches the registry.
struct TooltipPaintState {
  const GameUiSnapshot *snapshot = nullptr;
  std::uint64_t activeItemDomain = kInvalidDomainId; // 0 = no item tooltip.
  std::uint32_t activeSkillId = NoMoreDay::INVALID_SKILL_ID;
  int activeBuffIdx = -1;
  float alpha = 0.0f;
  bool initialized = false;
};

// Instance controller for the legacy tooltip state machine (U7 group 6-B).
//
// Owns the hover cache, the delay/fade state machine and the top-most tooltip
// draw that used to live inline in UISystem::Draw / DrawDraggingPhantom. All
// tooltip state is held as instance members; the controller keeps no
// process-global state and GameUiHost owns exactly one instance, driven with
// this per-frame contract:
//
//   1. ResetFrame  - clears the hover cache (was the frame-start hover reset).
//   2. hover write - the hotbar slot / buff strip call SetHoveredSkillSlot /
//                    SetHoveredBuff; the skill hub and talent tree write the
//                    hover through SetHoveredSkill; ground hover is produced
//                    by DetectGroundHover from the frame-scoped WorldUiFrame.
//                    Item hovers flow as stable domain ids
//                    (SetHoveredItemDomain / SetGroundHoverDomain), never as
//                    entt::entity values (R8, design §3.2 snapshot contract).
//   3. UpdateState - the state machine (was UISystem::Draw section 4), now
//                    snapshot-driven (R8): the hotbar-slot hover resolves
//                    against snapshot.skillBar.slots, item validity is the
//                    domain-id sentinel, buff indices index snapshot.buffs.
//                    Must run after every hover producer of the frame has
//                    written.
//   4. Paint       - issues the Tooltip-layer Custom command (R8); the
//                    registered painter renders the active tooltip from the
//                    frame-scoped snapshot (was the tail of
//                    UISystem::DrawDraggingPhantom), after the drag phantom.
//
// R8: entt::entity never crosses this surface. The modal-input gate
// (DetectGroundHover) queries the host's instance IsModalInputCaptured()
// through the host back-pointer bound by BindHost.
class TooltipController {
public:
  TooltipController() = default;
  ~TooltipController() = default;

  TooltipController(const TooltipController&) = delete;
  TooltipController& operator=(const TooltipController&) = delete;

  // U8 收尾: binds the owning GameUiHost for the modal-input gate. The host
  // calls this from its ctor body; null until then (headless tests are safe).
  void BindHost(GameUiHost* uiHost) noexcept;

  // Clears the per-frame hover cache. Called by the host right before the
  // legacy draw pass (was the frame-start hover reset in UISystem::Draw).
  void ResetFrame() noexcept;

  // Hover producers. The slot hover is resolved against the snapshot's
  // skillBar.slots inside UpdateState (R8), so the resolution happens at
  // state-machine time, exactly like the original.
  void SetHoveredSkillSlot(int slotIndex) noexcept;
  void SetHoveredBuff(int buffIdx) noexcept;
  // Direct skill-id hover (was the skill-hub / talent-tree writes).
  void SetHoveredSkill(uint32_t skillId) noexcept;
  // Direct item hover as a stable domain id (R8: was entt::entity).
  void SetHoveredItemDomain(std::uint64_t domainId) noexcept;

  // U8 host read-side migration: binds the frame-scoped WorldUiFrame the
  // render adapter fills with visible ground-item hit proxies. The controller
  // reads it for ground hover detection and writes the resolved hover back to
  // it for the render-side highlight (design §4.1 direction contract).
  void BindWorldFrame(WorldUiFrame *frame) noexcept;

  // Ground hover producers (U8): DetectGroundHover resolves the mouse-over
  // ground item against the bound frame's visible proxies and feeds the
  // resolved domain id through SetGroundHoverDomain + SetHoveredItemDomain.
  // R8: the camera-only signature; entities never cross this surface (the
  // frame's proxies are converted to domain ids at detection time).
  void SetGroundHoverDomain(std::uint64_t domainId) noexcept;
  void ClearGroundHover() noexcept;
  void DetectGroundHover(const Camera2D &camera);

  // Runs the delay/fade state machine (was UISystem::Draw section 4). Reads
  // the hover cache plus the frame snapshot (R8) and updates the active-tooltip
  // members. Deterministic in deltaSeconds; the host passes GetFrameTime().
  void UpdateState(const GameUiSnapshot& snapshot, float deltaSeconds);

  // R8: issues the Tooltip-layer Custom command carrying the current paint
  // state. The registered painter (host Initialize) renders the active tooltip
  // from the snapshot during the backend submit (was DrawTooltip(registry)).
  void Paint(UiDrawList& drawList, const UiViewport& viewport,
             const GameUiSnapshot& snapshot);

  // Clears all tooltip state (session scoping; was the UISystem
  // ResetSessionState tooltip block). Idempotent.
  void EnterGameplay() noexcept;
  void LeaveGameplay() noexcept;

  // Read accessors (tests / observers). R8: item tooltips surface as domain
  // ids (kInvalidDomainId when none), matching the snapshot contract.
  [[nodiscard]] uint32_t ActiveTooltipSkillId() const noexcept {
    return m_activeTooltipSkillId;
  }
  [[nodiscard]] std::uint64_t ActiveTooltipItemDomain() const noexcept {
    return m_activeTooltipItemDomain;
  }
  [[nodiscard]] int ActiveTooltipBuffIdx() const noexcept {
    return m_activeTooltipBuffIdx;
  }
  [[nodiscard]] float Alpha() const noexcept { return m_tooltipAlpha; }
  [[nodiscard]] float DelayTimer() const noexcept {
    return m_tooltipDelayTimer;
  }
  [[nodiscard]] bool TooltipInitialized() const noexcept {
    return m_tooltipInitialized;
  }
  [[nodiscard]] bool HoveredLastFrame() const noexcept {
    return m_tooltipHoveredLastFrame;
  }

  // R8: the frame-scoped paint state the host registers with the backend
  // (painter user data). Mutable so the painter can be wired from the host;
  // the controller owns and fills it in Paint().
  [[nodiscard]] TooltipPaintState& PaintState() noexcept {
    return m_paintState;
  }

private:
  // Clears every member (hover cache + active tooltip state).
  void ResetAll() noexcept;

  // Frame-scoped hover input cache (filled by the Set* methods, cleared by
  // ResetFrame). Item hovers are stable domain ids (0 = none).
  int m_hoveredSkillSlot = -1;
  uint32_t m_hoveredSkillId = NoMoreDay::INVALID_SKILL_ID;
  std::uint64_t m_hoveredItemDomain = kInvalidDomainId;
  int m_hoveredBuffIdx = -1;

  // U8: frame-scoped world UI bridge (bound by the composition root) and the
  // ground hover resolved from it each frame (cleared by ResetFrame).
  WorldUiFrame *m_frame = nullptr;
  std::uint64_t m_groundHoverDomain = kInvalidDomainId;

  // U8 收尾: owning host for the modal-input gate (null until BindHost).
  GameUiHost *m_uiHost = nullptr;

  // Active tooltip state (the state machine output).
  uint32_t m_activeTooltipSkillId = NoMoreDay::INVALID_SKILL_ID;
  std::uint64_t m_activeTooltipItemDomain = kInvalidDomainId;
  int m_activeTooltipBuffIdx = -1;
  float m_tooltipAlpha = 0.0f;
  float m_tooltipDelayTimer = 0.0f;
  bool m_tooltipInitialized = false;
  bool m_tooltipHoveredLastFrame = false;

  // R8: frame-scoped paint state filled by Paint() for the registered painter.
  TooltipPaintState m_paintState;
};

// R8: painter entry point. Registered by the host with &PaintState() as user
// data; renders the active tooltip (item / skill / buff) from the snapshot
// during the backend submit. Declared here (no storage) so the host can wire
// it without reaching into the controller internals.
void TooltipPaintCallback(void *userData, UiRect nativeBounds);

} // namespace NoMoreDay::ui
