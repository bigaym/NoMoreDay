#include "game/application/ui/TooltipController.hpp"

#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/WorldUiFrame.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>

namespace NoMoreDay::ui {

void TooltipController::ResetFrame() noexcept {
  m_hoveredSkillSlot = -1;
  m_hoveredSkillId = NoMoreDay::INVALID_SKILL_ID;
  m_hoveredItem = entt::null;
  m_hoveredBuffIdx = -1;
  m_groundHover = entt::null;
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

void TooltipController::BindHost(GameUiHost *uiHost) noexcept {
  m_uiHost = uiHost;
}

void TooltipController::BindWorldFrame(WorldUiFrame *frame) noexcept {
  m_frame = frame;
}

void TooltipController::SetGroundHover(entt::entity entity) noexcept {
  m_groundHover = entity;
}

void TooltipController::ClearGroundHover() noexcept {
  m_groundHover = entt::null;
}

void TooltipController::DetectGroundHover(entt::registry &registry,
                                          const Camera2D &camera) {
  // Ground hover detection migrated out of UISystem::Draw (U8 host read-side
  // migration; was UISystem::Draw section 3). Read-only: resolves the
  // mouse-over visible ground item against the frame-scoped WorldUiFrame and
  // feeds the hit through the existing tooltip hover channel. Runs before the
  // legacy UISystem::Draw pass; the detection only depends on the frame /
  // mouse / camera / modal gate, never on hotbar frame state, so the early
  // position is behaviour-equivalent.
  if (m_frame == nullptr) {
    return; // Frame not bound: no visible item proxies to test against.
  }
  // U8 收尾: modal-input gate 经 host 实例查询（原 UISystem::IsModalInputCaptured）。
  if (m_uiHost != nullptr && m_uiHost->IsModalInputCaptured()) {
    return; // Modal UI captures pointer input, matching the legacy gate.
  }

  const Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);

  // Iterate ONLY visible items (already culled by RenderSystem). First hit is
  // the top-most item, mirroring the legacy loop.
  for (const auto &itemData : m_frame->VisibleItems()) {
    if (CheckCollisionPointRec(mouseWorldPos, itemData.worldRect)) {
      SetGroundHover(itemData.entity);
      SetHoveredItem(itemData.entity); // Reuse the existing tooltip hover channel.
      break;
    }
  }
}

void TooltipController::UpdateState(entt::registry& registry) {
  UpdateState(registry, GetFrameTime());
}

void TooltipController::UpdateState(entt::registry& registry,
                                    float deltaSeconds) {
  // Resolve the hover input with the original priority order (item > skill id
  // > hotbar slot > buff index). Item hover comes from the per-frame cache
  // (SetHoveredItem; the U8 ground path feeds it through DetectGroundHover)
  // with the frame-scoped ground hover as the fallback source; direct
  // skill-id hover keeps its legacy live source
  // (UISystem::State.hoveredSkillId written by the skill hub and talent
  // tree); the hotbar slot and buff-strip hovers arrive through the per-frame
  // cache. The slot hover is resolved against ActiveSkillsComponent at
  // state-machine time, exactly like the original inline block.
  uint32_t hoverSkillId = NoMoreDay::INVALID_SKILL_ID;
  entt::entity hoverItem = entt::null;
  int hoverBuffIdx = -1;

  const entt::entity item = (m_hoveredItem != entt::null)
                                ? m_hoveredItem
                                : m_groundHover;
  // U8: mirror the resolved hover onto the frame object for the render-side
  // highlight. The render adapter reads the previous frame's write (its
  // UIWorldPass runs before Draw), matching the legacy UiShared::HoveredItem
  // timing (design §4.1 direction contract: UI writes, render reads).
  if (m_frame != nullptr) {
    if (item != entt::null) {
      m_frame->SetHovered(item);
    } else {
      m_frame->ClearHovered();
    }
  }
  if (item != entt::null && registry.valid(item)) {
    hoverItem = item;
  } else {
    // U8 收尾: skill-hub / talent-tree hovers 已改经 SetHoveredSkill 写缓存，
    // 原 UISystem::State.hoveredSkillId fallback 已删。
    const uint32_t skillId = m_hoveredSkillId;
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
    // U8 收尾: forceDraw = !locked 让技能 tooltip 在 hover 目标切换时重置
    // 位置锁定（UIRenderer 内 renderer-local 锁定，原 State.tooltipPos）。
    UIRenderer::DrawSkillTooltip(UISystem::GetFont(), registry,
                                 m_activeTooltipSkillId, m_tooltipAlpha,
                                 !m_tooltipInitialized);
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
}

} // namespace NoMoreDay::ui
