#pragma once

#include "game/foundation/components/SkillDefs.hpp"

#include <cstdint>

#include <entt/entt.hpp>

namespace NoMoreDay::ui {

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
//                    SetHoveredBuff; the skill hub and talent tree still write
//                    UISystem::State.hoveredSkillId (adopted in UpdateState);
//                    ground items still live in UiShared::HoveredItem().
//   3. UpdateState - the state machine (was UISystem::Draw section 4). Must
//                    run after every hover producer of the frame has written.
//   4. DrawTooltip - the top-most tooltip pass (was the tail of
//                    UISystem::DrawDraggingPhantom), after the drag phantom.
//
// The active-tooltip members are written back to the UISystem::State mirror
// (activeTooltip* / tooltipAlpha / tooltipDelayTimer / tooltipInitialized /
// tooltipHoveredLastFrame) because UIRenderer still reads
// State.tooltipInitialized for the tooltip position lock and the
// null-controller draw fallback reads the mirrors. The mirror is removed in
// U8.
class TooltipController {
public:
  TooltipController() = default;
  ~TooltipController() = default;

  TooltipController(const TooltipController&) = delete;
  TooltipController& operator=(const TooltipController&) = delete;

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
  // Clears every member (hover cache + active tooltip state) and mirrors the
  // cleared state back to UISystem::State.
  void ResetAll() noexcept;
  // Writes the active-tooltip members into UISystem::State for the legacy
  // readers (UIRenderer position lock, null-controller draw fallback).
  void MirrorToState() noexcept;

  // Frame-scoped hover input cache (filled by the Set* methods, cleared by
  // ResetFrame).
  int m_hoveredSkillSlot = -1;
  uint32_t m_hoveredSkillId = NoMoreDay::INVALID_SKILL_ID;
  entt::entity m_hoveredItem = entt::null;
  int m_hoveredBuffIdx = -1;

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
