#include "game/application/ui/TooltipController.hpp"

#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/application/ui/WorldUiFrame.hpp"
#include "engine/render/CoordSystem.hpp"
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
  m_hoveredItemDomain = kInvalidDomainId;
  m_hoveredBuffIdx = -1;
  m_groundHoverDomain = kInvalidDomainId;
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

void TooltipController::SetHoveredItemDomain(std::uint64_t domainId) noexcept {
  m_hoveredItemDomain = domainId;
}

void TooltipController::BindHost(GameUiHost *uiHost) noexcept {
  m_uiHost = uiHost;
}

void TooltipController::BindWorldFrame(WorldUiFrame *frame) noexcept {
  m_frame = frame;
}

void TooltipController::SetGroundHoverDomain(std::uint64_t domainId) noexcept {
  m_groundHoverDomain = domainId;
}

void TooltipController::ClearGroundHover() noexcept {
  m_groundHoverDomain = kInvalidDomainId;
}

void TooltipController::DetectGroundHover(const Camera2D &camera) {
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
  // R3 (remediation, design §3.5.3): world hover reads proxies only through a
  // valid current-pass view. A bound-but-never-opened frame or a stale frame
  // (rotated token) yields an invalid view => no world target; the previous
  // frame's proxies must not be consumed (H-01). Degrades to no ground hover.
  const WorldUiFrame::View worldView = m_frame->AcquireView();
  if (!worldView.IsValid()) {
    return;
  }

  const Vector2 mouseWorldPos = NoMoreDay::render::coord::ScenePixelToWorld(
      NoMoreDay::render::coord::Camera2DTransform::From(camera),
      GetMousePosition());

  // Iterate ONLY visible items (already culled by RenderSystem). First hit is
  // the top-most item, mirroring the legacy loop. R8: the frame's proxies carry
  // entt::entity handles; only the stable domain id crosses this surface.
  for (const auto &itemData : worldView.VisibleItems()) {
    if (CheckCollisionPointRec(mouseWorldPos, itemData.worldRect)) {
      SetGroundHoverDomain(entt::to_integral(itemData.entity));
      SetHoveredItemDomain(entt::to_integral(itemData.entity));
      break;
    }
  }
}

void TooltipController::UpdateState(const GameUiSnapshot& snapshot,
                                    float deltaSeconds) {
  // Resolve the hover input with the original priority order (item > skill id
  // > hotbar slot > buff index). Item hover comes from the per-frame cache
  // (SetHoveredItemDomain; the ground path feeds it through
  // DetectGroundHover) with the frame-scoped ground hover as the fallback
  // source. R8: the hotbar slot hover resolves against the snapshot's
  // skillBar.slots (builder-resolved view model) instead of the registry, and
  // item validity is the domain-id sentinel (kInvalidDomainId) instead of
  // registry.valid(). The state machine timing contract is unchanged.
  uint32_t hoverSkillId = NoMoreDay::INVALID_SKILL_ID;
  std::uint64_t hoverItemDomain = kInvalidDomainId;
  int hoverBuffIdx = -1;

  const std::uint64_t itemDomain = (m_hoveredItemDomain != kInvalidDomainId)
                                       ? m_hoveredItemDomain
                                       : m_groundHoverDomain;
  // U8: mirror the resolved hover onto the frame object for the render-side
  // highlight. The render adapter reads the previous frame's write (its
  // UIWorldPass runs before Draw), matching the legacy UiShared::HoveredItem
  // timing (design §4.1 direction contract: UI writes, render reads). R8: the
  // frame object still keys on entt::entity internally (render-side culling
  // uses the raw handles); the domain id is converted back at this boundary —
  // the snapshot-driven surface itself stays entity-free.
  if (m_frame != nullptr) {
    if (itemDomain != kInvalidDomainId) {
      m_frame->SetHovered(static_cast<entt::entity>(
          static_cast<entt::id_type>(itemDomain)));
    } else {
      m_frame->ClearHovered();
    }
  }
  if (itemDomain != kInvalidDomainId) {
    hoverItemDomain = itemDomain;
  } else {
    const uint32_t skillId = m_hoveredSkillId;
    if (skillId != NoMoreDay::INVALID_SKILL_ID) {
      hoverSkillId = skillId;
    } else if (m_hoveredSkillSlot != -1) {
      // R8: resolve the hotbar slot hover from the frame snapshot's skill bar
      // view model (GameUiSkillBarSlotView.skillId), never from the registry.
      if (m_hoveredSkillSlot >= 0 &&
          m_hoveredSkillSlot <
              static_cast<int>(snapshot.skillBar.slots.size())) {
        hoverSkillId = snapshot.skillBar.slots[m_hoveredSkillSlot].skillId;
      }
    } else if (m_hoveredBuffIdx != -1) {
      hoverBuffIdx = m_hoveredBuffIdx;
    }
  }

  // --- State machine (ported verbatim from UISystem::Draw) ---
  const bool isAnythingHovered =
      (hoverSkillId != NoMoreDay::INVALID_SKILL_ID ||
       hoverItemDomain != kInvalidDomainId || hoverBuffIdx != -1);
  const bool targetChanged =
      isAnythingHovered &&
      (hoverSkillId != m_activeTooltipSkillId ||
       hoverItemDomain != m_activeTooltipItemDomain ||
       hoverBuffIdx != m_activeTooltipBuffIdx);

  if (targetChanged) {
    m_activeTooltipSkillId = hoverSkillId;
    m_activeTooltipItemDomain = hoverItemDomain;
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
      m_activeTooltipItemDomain = kInvalidDomainId;
      m_activeTooltipBuffIdx = -1;
      m_tooltipInitialized = false;
    }
  }
  m_tooltipHoveredLastFrame = isAnythingHovered;
}

void TooltipController::Paint(UiDrawList& drawList,
                              const UiViewport& viewport,
                              const GameUiSnapshot& snapshot) {
  // R8: top-most overlay pass (was the tail of UISystem::DrawDraggingPhantom /
  // DrawTooltip(registry)). Issues a single Tooltip-layer Custom command; the
  // registered painter renders the active tooltip from the frame-scoped
  // snapshot during the backend submit. The paint state carries only resolved
  // domain ids / snapshot data — no registry, no entt::entity.
  (void)viewport;
  if (m_tooltipAlpha <= 0.01f) {
    return;
  }
  m_paintState.snapshot = &snapshot;
  m_paintState.activeItemDomain = m_activeTooltipItemDomain;
  m_paintState.activeSkillId = m_activeTooltipSkillId;
  m_paintState.activeBuffIdx = m_activeTooltipBuffIdx;
  m_paintState.alpha = m_tooltipAlpha;
  m_paintState.initialized = m_tooltipInitialized;
  // Full logical viewport bounds; the painter receives the native-space rect
  // converted by the backend and renders the tooltip at the mouse position.
  drawList.Custom(UiDrawLayer::Tooltip, 0,
                  {0.0f, 0.0f,
                   static_cast<float>(UI_REF_WIDTH),
                   static_cast<float>(UI_REF_HEIGHT)},
                  kTooltipPainterResourceId);
}

void TooltipPaintCallback(void *userData, UiRect nativeBounds) {
  // Registered by the host with &PaintState() as user data. Renders the active
  // tooltip through the snapshot-driven UIRenderer variants; raylib drawing is
  // confined to the backend painter contract (design §3.4).
  (void)nativeBounds;
  auto *state = static_cast<TooltipPaintState *>(userData);
  if (state == nullptr || state->snapshot == nullptr ||
      state->alpha <= 0.01f) {
    return;
  }
  const Font &font = UISystem::GetFont();
  if (state->activeItemDomain != kInvalidDomainId) {
    SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    UIRenderer::DrawTooltipFromSnapshot(font, *state->snapshot,
                                        state->activeItemDomain,
                                        state->alpha);
  } else if (state->activeSkillId != NoMoreDay::INVALID_SKILL_ID) {
    // forceDraw = !initialized lets the skill tooltip reset its position lock
    // on hover-target switch (renderer-local lock, same as the legacy path).
    UIRenderer::DrawSkillTooltipFromSnapshot(font, *state->snapshot,
                                             state->activeSkillId,
                                             state->alpha,
                                             !state->initialized);
  } else if (state->activeBuffIdx != -1 &&
             state->activeBuffIdx <
                 static_cast<int>(state->snapshot->buffs.size())) {
    UIRenderer::DrawBuffTooltipFromView(
        font, state->snapshot->buffs[state->activeBuffIdx], state->alpha);
  }
}

void TooltipController::EnterGameplay() noexcept { ResetAll(); }

void TooltipController::LeaveGameplay() noexcept { ResetAll(); }

void TooltipController::ResetAll() noexcept {
  ResetFrame();
  m_activeTooltipSkillId = NoMoreDay::INVALID_SKILL_ID;
  m_activeTooltipItemDomain = kInvalidDomainId;
  m_activeTooltipBuffIdx = -1;
  m_tooltipAlpha = 0.0f;
  m_tooltipDelayTimer = 0.0f;
  m_tooltipInitialized = false;
  m_tooltipHoveredLastFrame = false;
  m_paintState = TooltipPaintState{};
}

} // namespace NoMoreDay::ui
