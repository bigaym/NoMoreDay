#include "game/application/ui/SkillTreeController.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/TooltipController.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/foundation/SharedContext.hpp"
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

void SkillTreeController::Toggle(entt::registry& registry) {
  m_visible = !m_visible;
  if (m_visible) {
    // U8: the sibling closes that used to write UISystem::State now route
    // through the host channels when present (matching the legacy behavior:
    // opening the skill tree closes inventory / character / context menu).
    if (m_uiHost != nullptr) {
      m_uiHost->CloseInventory();
      m_uiHost->CloseCharacterPanel();
      m_uiHost->CloseContextMenu();
    }
    // Also close Astrolabe if open (U7 group 5): the skill tree lives below
    // the UI composition root, so the sibling close routes through the
    // SharedContext callback filled by Game (host-owned AstrolabeController).
    if (auto* shared = registry.ctx().find<NoMoreDay::SharedContext*>()) {
      if (*shared && (*shared)->closeAstrolabe) {
        (*shared)->closeAstrolabe();
      }
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

void SkillTreeController::Draw(entt::registry& registry, entt::entity player) {
  if (!m_visible) {
    return;
  }
  if (m_selectedSkillId == NoMoreDay::INVALID_SKILL_ID) {
    m_hub.Draw(registry, player, m_alpha);
  } else {
    m_tree.Draw(registry, player, m_selectedSkillId, m_alpha);
  }
  // U8: the hub writes its selection into its own instance member (was the
  // State.selectedSkillId read-back round trip).
  m_selectedSkillId = m_hub.SelectedSkillId();
}

UiId SkillTreeController::NodeId() const noexcept {
  return m_rootNodeId;
}

} // namespace NoMoreDay::ui
