#include "game/application/ui/SkillTreeController.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/foundation/SharedContext.hpp"
#include "game/foundation/components/Common.hpp"

#include <algorithm>

namespace NoMoreDay::ui {

SkillTreeController::SkillTreeController(UiRuntime& runtime) : m_runtime(runtime) {
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
}

void SkillTreeController::EnterGameplay() {
  m_inGameplay = true;
  m_visible = false;
  m_alpha = 0.0f;
  m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
  UISystem::State.showSkillTree = false;
  UISystem::State.selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
  if (m_rootNodeId != kInvalidUiId) {
    m_runtime.SetNodeVisible(m_rootNodeId, false);
  }
}

void SkillTreeController::LeaveGameplay() {
  m_visible = false;
  m_alpha = 0.0f;
  m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
  UISystem::State.showSkillTree = false;
  UISystem::State.selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
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
  // Mirror: the legacy panel bodies still read UISystem::State.
  UISystem::State.skillTreeAlpha = m_alpha;
}

void SkillTreeController::Toggle(entt::registry& registry) {
  m_visible = !m_visible;
  if (m_visible) {
    UISystem::State.showInventory = false;
    UISystem::State.showCharacterPanel = false;
    UISystem::State.showContextMenu = false;
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
    UISystem::State.selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
  }
  UISystem::State.showSkillTree = m_visible;
  if (m_rootNodeId != kInvalidUiId) {
    m_runtime.SetNodeVisible(m_rootNodeId, m_visible);
  }
}

void SkillTreeController::Close() {
  m_visible = false;
  m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID;
  UISystem::State.showSkillTree = false;
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
  // Mirror into UISystem::State: the legacy panel bodies read these fields.
  UISystem::State.showSkillTree = m_visible;
  UISystem::State.skillTreeAlpha = m_alpha;
  UISystem::State.selectedSkillId = m_selectedSkillId;

  if (!m_visible) {
    return;
  }
  if (m_selectedSkillId == NoMoreDay::INVALID_SKILL_ID) {
    m_hub.Draw(registry, player);
  } else {
    m_tree.Draw(registry, player, m_selectedSkillId);
  }
  // Read back: UISkillHub::Draw writes State.selectedSkillId on selection.
  m_selectedSkillId = UISystem::State.selectedSkillId;
}

UiId SkillTreeController::NodeId() const noexcept {
  return m_rootNodeId;
}

} // namespace NoMoreDay::ui
