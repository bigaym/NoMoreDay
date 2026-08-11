#pragma once

#include "game/application/ui/UISkillHub.hpp"
#include "game/application/ui/UISkillTalentTree.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/foundation/components/SkillDefs.hpp"

#include <cstdint>

#include <entt/entt.hpp>

namespace NoMoreDay::ui {

// Instance controller for the skill specialization UI (U7 group 4).
//
// Owns the two-stage skill UI: the mastery hub (UISkillHub) and the talent
// tree view (SkillTreeUI). The controller holds the panel state that used to
// live in UISystem::State (showSkillTree / skillTreeAlpha / selectedSkillId);
// those UIContext fields remain as a *write-back mirror* while the migrated
// panel bodies (UISkillHub::Draw, SkillTreeUI::Draw) still read them. The
// mirror is removed when the panel internals are rewired in U8.
class SkillTreeController {
public:
  explicit SkillTreeController(UiRuntime& runtime);
  ~SkillTreeController() = default;

  SkillTreeController(const SkillTreeController&) = delete;
  SkillTreeController& operator=(const SkillTreeController&) = delete;

  void EnterGameplay();
  void LeaveGameplay();

  // Runs the transition alpha animation (was UISystem::Update). Mirrors the
  // result back to UISystem::State.skillTreeAlpha for the panel bodies.
  void UpdateAlpha(float dt);

  // KEY_S handler (was UISystem::Update): toggles visibility and closes the
  // sibling panels exactly like the legacy code did.
  void Toggle(entt::registry& registry);

  // ESC handler (was UISystem::Update): closes the panel.
  void Close();

  [[nodiscard]] bool IsVisible() const noexcept;
  [[nodiscard]] bool IsInGameplay() const noexcept;

  // Draws the active stage. Mirrors controller state into UISystem::State
  // first so the legacy panel bodies read current values, and reads back
  // State.selectedSkillId afterwards (UISkillHub writes it on selection).
  void Draw(entt::registry& registry, entt::entity player);

  // Runtime node id of the panel root (kInvalidUiId if creation failed).
  [[nodiscard]] UiId NodeId() const noexcept;

private:
  UiRuntime& m_runtime;
  UiId m_rootNodeId = kInvalidUiId;
  bool m_visible = false;
  bool m_inGameplay = false;
  float m_alpha = 0.0f;
  uint32_t m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID;

  UISkillHub m_hub;
  SkillTreeUI m_tree;
};

} // namespace NoMoreDay::ui
