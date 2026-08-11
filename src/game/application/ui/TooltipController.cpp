#include "game/application/ui/TooltipController.hpp"

#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/ui_shared/UiShared.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>

namespace NoMoreDay::ui {

void TooltipController::ResetFrame() noexcept {
  m_hoveredSkillSlot = -1;
  m_hoveredSkillId = NoMoreDay::INVALID_SKILL_ID;
  m_hoveredItem = entt::null;
  m_hoveredBuffIdx = -1;
}

void TooltipController::SetHoveredSkillSlot(int slotIndex) noexcept {
  m_hoveredSkillSlot = slotIndex;
}

void TooltipController::SetHoveredBuff(int buffIdx) noexcept {
  m_hoveredBuffIdx = buffIdx;
}

void TooltipController::SetHoveredSkill(uint32_t skillId) noexcept {
  m_hoveredSkillId = skillId;
}

void TooltipController::SetHoveredItem(entt::entity item) noexcept {
  m_hoveredItem = item;
}

void TooltipController::UpdateState(entt::registry& registry) {
  UpdateState(registry, GetFrameTime());
}

void TooltipController::UpdateState(entt::registry& registry,
                                    float deltaSeconds) {
  // Resolve the hover input with the original priority order (item > skill id
  // > hotbar slot > buff index). Item and direct skill-id hover keep their
  // legacy live sources (UiShared::HoveredItem / UISystem::State.hoveredSkillId
  // written by the skill hub and talent tree); the hotbar slot and buff-strip
  // hovers arrive through the per-frame cache. The slot hover is resolved
  // against ActiveSkillsComponent at state-machine time, exactly like the
  // original inline block.
  uint32_t hoverSkillId = NoMoreDay::INVALID_SKILL_ID;
  entt::entity hoverItem = entt::null;
  int hoverBuffIdx = -1;

  const entt::entity item = (m_hoveredItem != entt::null)
                                ? m_hoveredItem
                                : UiShared::HoveredItem();
  if (item != entt::null && registry.valid(item)) {
    hoverItem = item;
  } else {
    const uint32_t skillId = (m_hoveredSkillId != NoMoreDay::INVALID_SKILL_ID)
                                 ? m_hoveredSkillId
                                 : UISystem::State.hoveredSkillId;
    if (skillId != NoMoreDay::INVALID_SKILL_ID) {
      hoverSkillId = skillId;
    } else if (m_hoveredSkillSlot != -1) {
      auto view = registry.view<PlayerTag, ActiveSkillsComponent>();
      if (view.begin() != view.end()) {
        const auto &active = view.get<ActiveSkillsComponent>(view.front());
        if (m_hoveredSkillSlot >= 0 &&
            m_hoveredSkillSlot < static_cast<int>(active.slots.size())) {
          hoverSkillId = active.slots[m_hoveredSkillSlot].id;
        }
      }
    } else if (m_hoveredBuffIdx != -1) {
      hoverBuffIdx = m_hoveredBuffIdx;
    }
  }

  // --- State machine (ported verbatim from UISystem::Draw) ---
  const bool isAnythingHovered =
      (hoverSkillId != NoMoreDay::INVALID_SKILL_ID || hoverItem != entt::null ||
       hoverBuffIdx != -1);
  const bool targetChanged =
      isAnythingHovered &&
      (hoverSkillId != m_activeTooltipSkillId ||
       hoverItem != m_activeTooltipItem ||
       hoverBuffIdx != m_activeTooltipBuffIdx);

  if (targetChanged) {
    m_activeTooltipSkillId = hoverSkillId;
    m_activeTooltipItem = hoverItem;
    m_activeTooltipBuffIdx = hoverBuffIdx;
    m_tooltipDelayTimer = (m_tooltipAlpha > 0.01f) ? 0.05f : 0.12f;
    m_tooltipInitialized = false;
  }

  if (isAnythingHovered) {
    if (m_tooltipDelayTimer > 0.0f) {
      m_tooltipDelayTimer = std::max(0.0f, m_tooltipDelayTimer - deltaSeconds);
    } else {
      m_tooltipAlpha = std::min(1.0f, m_tooltipAlpha + deltaSeconds * 10.0f);
    }
  } else {
    if (m_tooltipHoveredLastFrame) {
      m_tooltipDelayTimer = 0.08f;
    }

    if (m_tooltipDelayTimer > 0.0f) {
      m_tooltipDelayTimer = std::max(0.0f, m_tooltipDelayTimer - deltaSeconds);
    } else if (m_tooltipAlpha > 0.0f) {
      m_tooltipAlpha = std::max(0.0f, m_tooltipAlpha - deltaSeconds * 8.0f);
    }

    if (m_tooltipAlpha <= 0.0f && m_tooltipDelayTimer <= 0.0f) {
      m_activeTooltipSkillId = NoMoreDay::INVALID_SKILL_ID;
      m_activeTooltipItem = entt::null;
      m_activeTooltipBuffIdx = -1;
      m_tooltipInitialized = false;
    }
  }
  m_tooltipHoveredLastFrame = isAnythingHovered;

  MirrorToState();
}

void TooltipController::DrawTooltip(entt::registry& registry) {
  // Top-most overlay pass (was the tail of UISystem::DrawDraggingPhantom).
  if (m_tooltipAlpha <= 0.01f) {
    return;
  }
  if (m_activeTooltipItem != entt::null && registry.valid(m_activeTooltipItem)) {
    SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    UIRenderer::DrawTooltip(UISystem::GetFont(), registry, m_activeTooltipItem,
                            m_tooltipAlpha);
  } else if (m_activeTooltipSkillId != NoMoreDay::INVALID_SKILL_ID) {
    UIRenderer::DrawSkillTooltip(UISystem::GetFont(), registry,
                                 m_activeTooltipSkillId, m_tooltipAlpha);
  } else if (m_activeTooltipBuffIdx != -1) {
    auto view = registry.view<PlayerTag, ActiveEffectsComponent>();
    if (view.begin() != view.end()) {
      const auto &effects = view.get<ActiveEffectsComponent>(view.front());
      if (m_activeTooltipBuffIdx < static_cast<int>(effects.effects.size())) {
        UIRenderer::DrawBuffTooltip(UISystem::GetFont(),
                                    effects.effects[m_activeTooltipBuffIdx],
                                    m_tooltipAlpha);
      }
    }
  }
}

void TooltipController::EnterGameplay() noexcept { ResetAll(); }

void TooltipController::LeaveGameplay() noexcept { ResetAll(); }

void TooltipController::ResetAll() noexcept {
  ResetFrame();
  m_activeTooltipSkillId = NoMoreDay::INVALID_SKILL_ID;
  m_activeTooltipItem = entt::null;
  m_activeTooltipBuffIdx = -1;
  m_tooltipAlpha = 0.0f;
  m_tooltipDelayTimer = 0.0f;
  m_tooltipInitialized = false;
  m_tooltipHoveredLastFrame = false;
  MirrorToState();
}

void TooltipController::MirrorToState() noexcept {
  auto &state = UISystem::State;
  state.activeTooltipSkillId = m_activeTooltipSkillId;
  state.activeTooltipItem = m_activeTooltipItem;
  state.activeTooltipBuffIdx = m_activeTooltipBuffIdx;
  state.tooltipAlpha = m_tooltipAlpha;
  state.tooltipDelayTimer = m_tooltipDelayTimer;
  state.tooltipInitialized = m_tooltipInitialized;
  state.tooltipHoveredLastFrame = m_tooltipHoveredLastFrame;
}

} // namespace NoMoreDay::ui
