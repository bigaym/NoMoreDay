#include "game/application/ui/SkillTreeController.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/TooltipController.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/foundation/components/Common.hpp"

#include <algorithm>

namespace NoMoreDay::ui {

SkillTreeController::SkillTreeController(UiRuntime& runtime,
                                         TooltipController* tooltip,
                                         GameUiHost* uiHost)
    : m_runtime(runtime) {
  UiNodeDesc desc;
  desc.id = entt::hashed_string("ui_skill_tree");
  desc.parent = kRootUiId;
  desc.layout.kind = UiLayoutKind::Overlay;
  desc.layout.width = UiLength::Fraction(1.0f);
  desc.layout.height = UiLength::Fraction(1.0f);
  desc.visible = true;
  desc.hitTestVisible = false;
  desc.capturePointer = false;
  desc.focusable = false;
  desc.captureKeyboard = false;
  desc.acceptsText = false;
  desc.modal = false;
  desc.zIndex = static_cast<std::int32_t>(UiDrawLayer::Panels);
  desc.customPainter = kInvalidUiResourceId;
  if (m_runtime.CreateNode(desc)) {
    m_rootNodeId = desc.id;
    // Hidden until EnterGameplay / Toggle; mirrors the panel default.
    m_runtime.SetNodeVisible(m_rootNodeId, false);
  }
  // U8: wire the hover channel (tree -> tooltip) and the sibling-close
  // channel (host). Both are optional for headless unit tests.
  m_tree.SetTooltip(tooltip);
  m_hub.SetHost(uiHost);
  m_uiHost = uiHost;
}

void SkillTreeController::EnterGameplay() {
  m_inGameplay = true;
  m_visible = false;
  m_alpha = 0.0f;
  m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
  if (m_rootNodeId != kInvalidUiId) {
    m_runtime.SetNodeVisible(m_rootNodeId, false);
  }
}

void SkillTreeController::LeaveGameplay() {
  m_visible = false;
  m_alpha = 0.0f;
  m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
  if (m_rootNodeId != kInvalidUiId) {
    m_runtime.SetNodeVisible(m_rootNodeId, false);
  }
  m_inGameplay = false;
}

void SkillTreeController::UpdateAlpha(float dt) {
  const float alphaSpeed = 6.0f;
  if (m_visible) {
    m_alpha = std::min(1.0f, m_alpha + dt * alphaSpeed);
  } else {
    m_alpha = std::max(0.0f, m_alpha - dt * alphaSpeed);
  }
}

void SkillTreeController::Toggle() {
  m_visible = !m_visible;
  if (m_visible) {
    // U8: the sibling closes that used to write UISystem::State now route
    // through the host channels when present (matching the legacy behavior:
    // opening the skill tree closes inventory / character / context menu).
    // R8: the legacy shared-context closeAstrolabe callback is gone; the host
    // channel closes the hosted AstrolabeController.
    if (m_uiHost != nullptr) {
      m_uiHost->CloseInventory();
      m_uiHost->CloseCharacterPanel();
      m_uiHost->CloseContextMenu();
      m_uiHost->CloseAstrolabe();
    }
  } else {
    m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID; // Reset view
    m_hub.ResetSelection();
  }
  if (m_rootNodeId != kInvalidUiId) {
    m_runtime.SetNodeVisible(m_rootNodeId, m_visible);
  }
}

void SkillTreeController::Close() {
  m_visible = false;
  m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
  if (m_rootNodeId != kInvalidUiId) {
    m_runtime.SetNodeVisible(m_rootNodeId, false);
  }
}

bool SkillTreeController::IsVisible() const noexcept {
  return m_visible;
}

bool SkillTreeController::IsInGameplay() const noexcept {
  return m_inGameplay;
}

void SkillTreeController::Update(const GameUiSnapshot& snapshot,
                                 const UiInputFrame& input) {
  if (!m_visible) {
    return;
  }
  if (m_selectedSkillId == NoMoreDay::INVALID_SKILL_ID) {
    m_hub.UpdateInput(snapshot, input, m_alpha);
  } else {
    m_tree.UpdateInput(snapshot, input, m_selectedSkillId, m_uiHost, m_alpha);
  }
  // R8: the hub writes its selection into its own instance member (was the
  // State.selectedSkillId read-back round trip; the talent-tree back button
  // writes INVALID_SKILL_ID through its own member).
  m_selectedSkillId = m_hub.SelectedSkillId();
}

void SkillTreeController::Paint(UiDrawList& drawList, const UiViewport& viewport,
                                const GameUiSnapshot& snapshot) {
  if (!m_visible) {
    return;
  }
  if (m_selectedSkillId == NoMoreDay::INVALID_SKILL_ID) {
    m_hub.Paint(drawList, viewport, snapshot, m_alpha);
  } else {
    m_tree.Paint(drawList, viewport, m_selectedSkillId, m_alpha);
  }
}

UiId SkillTreeController::NodeId() const noexcept {
  return m_rootNodeId;
}

} // namespace NoMoreDay::ui
