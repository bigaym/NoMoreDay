#pragma once

#include "game/foundation/components/SkillDefs.hpp"

#include <cstdint>

#include <entt/entt.hpp>
#include <raylib.h>

namespace NoMoreDay::ui {

class WorldUiFrame;
class GameUiHost; // U8 收尾: modal-input gate via host instance query.

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
//                    hover through SetHoveredSkill (U8 收尾: 原
//                    State.hoveredSkillId fallback 已删); ground hover is
//                    produced by DetectGroundHover from the frame-scoped
//                    WorldUiFrame (U8; was UiShared::HoveredItem).
//   3. UpdateState - the state machine (was UISystem::Draw section 4). Must
//                    run after every hover producer of the frame has written.
//   4. DrawTooltip - the top-most tooltip pass (was the tail of
//                    UISystem::DrawDraggingPhantom), after the drag phantom.
//
// U8 收尾: the UISystem::State mirror is gone; all tooltip state is instance
// state. The modal-input gate (DetectGroundHover) queries the host's instance
// IsModalInputCaptured() through the host back-pointer bound by BindHost.
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

  // Hover producers (was the hotbar/buff-strip writes to
  // UISystem::State.hoveredSkillSlot / hoveredBuffIdx). The slot hover is
  // resolved against ActiveSkillsComponent::slots inside UpdateState so the
  // resolution happens at state-machine time, exactly like the original.
  void SetHoveredSkillSlot(int slotIndex) noexcept;
  void SetHoveredBuff(int buffIdx) noexcept;
  // Direct skill-id hover (was the skill-hub / talent-tree writes; the legacy
  // panels still write UISystem::State.hoveredSkillId, which UpdateState
  // adopts as the fallback source).
  void SetHoveredSkill(uint32_t skillId) noexcept;
  // Direct item hover. Production ground items still flow through
  // UiShared::HoveredItem(); the cache takes priority when set (tests, and
  // future U8 rewires).
  void SetHoveredItem(entt::entity item) noexcept;

  // U8 host read-side migration: binds the frame-scoped WorldUiFrame the
  // render adapter fills with visible ground-item hit proxies. The controller
  // reads it for ground hover detection and writes the resolved hover back to
  // it for the render-side highlight (design §4.1 direction contract).
  void BindWorldFrame(WorldUiFrame *frame) noexcept;

  // Ground hover producers (U8): DetectGroundHover resolves the mouse-over
  // ground item against the bound frame's visible proxies and feeds the
  // resolved entity through SetGroundHover + SetHoveredItem; the frame is
  // bound from the composition root (Game), null before that.
  void SetGroundHover(entt::entity entity) noexcept;
  void ClearGroundHover() noexcept;
  void DetectGroundHover(entt::registry &registry, const Camera2D &camera);

  // Runs the delay/fade state machine (was UISystem::Draw section 4). Reads
  // the hover cache plus the legacy live inputs and updates the active-tooltip
  // members + the UISystem::State mirror. The delta-seconds overload is
  // deterministic and used by tests; the no-arg overload reads GetFrameTime()
  // for the host call.
  void UpdateState(entt::registry& registry);
  void UpdateState(entt::registry& registry, float deltaSeconds);

  // Draws the active tooltip at the top-most overlay layer (was the tail of
  // UISystem::DrawDraggingPhantom), after the drag phantom.
  void DrawTooltip(entt::registry& registry);

  // Clears all tooltip state (session scoping; was the UISystem
  // ResetSessionState tooltip block). Idempotent.
  void EnterGameplay() noexcept;
  void LeaveGameplay() noexcept;

  // Read accessors (tests / observers).
  [[nodiscard]] uint32_t ActiveTooltipSkillId() const noexcept {
    return m_activeTooltipSkillId;
  }
  [[nodiscard]] entt::entity ActiveTooltipItem() const noexcept {
    return m_activeTooltipItem;
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

private:
  // Clears every member (hover cache + active tooltip state).
  void ResetAll() noexcept;

  // Frame-scoped hover input cache (filled by the Set* methods, cleared by
  // ResetFrame).
  int m_hoveredSkillSlot = -1;
  uint32_t m_hoveredSkillId = NoMoreDay::INVALID_SKILL_ID;
  entt::entity m_hoveredItem = entt::null;
  int m_hoveredBuffIdx = -1;

  // U8: frame-scoped world UI bridge (bound by the composition root) and the
  // ground hover resolved from it each frame (cleared by ResetFrame).
  WorldUiFrame *m_frame = nullptr;
  entt::entity m_groundHover = entt::null;

  // U8 收尾: owning host for the modal-input gate (null until BindHost).
  GameUiHost *m_uiHost = nullptr;

  // Active tooltip state (the state machine output).
  uint32_t m_activeTooltipSkillId = NoMoreDay::INVALID_SKILL_ID;
  entt::entity m_activeTooltipItem = entt::null;
  int m_activeTooltipBuffIdx = -1;
  float m_tooltipAlpha = 0.0f;
  float m_tooltipDelayTimer = 0.0f;
  bool m_tooltipInitialized = false;
  bool m_tooltipHoveredLastFrame = false;
};

} // namespace NoMoreDay::ui
