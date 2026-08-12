#pragma once

#include "game/application/ui/UISkillHub.hpp"
#include "game/application/ui/UISkillTalentTree.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/foundation/components/SkillDefs.hpp"

#include <cstdint>

#include <entt/entt.hpp>

namespace NoMoreDay::ui {

class GameUiHost;   // Back-pointer injected by the host (U8 sibling-close /
                    // hover channel).
class TooltipController; // Skill hover channel (U8: tree hover writes).

// Instance controller for the skill specialization UI (U7 group 4).
//
// Owns the two-stage skill UI: the mastery hub (UISkillHub) and the talent
// tree view (SkillTreeUI). The controller holds the panel state that used to
// live in UISystem::State (showSkillTree / skillTreeAlpha / selectedSkillId);
// with the U8 final narrowing the legacy State fields are gone and the panel
// bodies read controller/hub instance state directly (alpha parameter,
// hub-owned selection, tooltip hover channel).
class SkillTreeController {
public:
  // U8: optional tooltip/host back-pointers. The tooltip is bound to the
  // talent tree (hovered-skill channel) and the host routes the sibling
  // panel closes of Toggle. Both are null in headless unit tests.
  explicit SkillTreeController(UiRuntime& runtime,
                               TooltipController* tooltip = nullptr,
                               GameUiHost* uiHost = nullptr);
  ~SkillTreeController() = default;

  SkillTreeController(const SkillTreeController&) = delete;
  SkillTreeController& operator=(const SkillTreeController&) = delete;

  void EnterGameplay();
  void LeaveGameplay();

  // Runs the transition alpha animation (was UISystem::Update).
  void UpdateAlpha(float dt);

  // KEY_S handler (was UISystem::Update): toggles visibility and closes the
  // sibling panels exactly like the legacy code did (routed through the
  // host channels when present, otherwise the SharedContext closeAstrolabe
  // callback, matching the legacy static coupling).
  void Toggle(entt::registry& registry);

  // ESC handler (was UISystem::Update): closes the panel.
  void Close();

  [[nodiscard]] bool IsVisible() const noexcept;
  [[nodiscard]] bool IsInGameplay() const noexcept;
  // U8: instance alpha (authoritative; the legacy State.skillTreeAlpha mirror
  // is gone). Animated by UpdateAlpha.
  [[nodiscard]] float Alpha() const noexcept { return m_alpha; }
  // U8: hub-owned selection read back by Draw (was the State.selectedSkillId
  // round-trip); INVALID_SKILL_ID when the talent-tree view is closed.
  [[nodiscard]] uint32_t SelectedSkillId() const noexcept {
    return m_selectedSkillId;
  }

  // Draws the active stage with the animated alpha. The hub writes the
  // selection into its own instance member, which the controller reads back
  // afterwards (was the State.selectedSkillId round-trip).
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

  GameUiHost* m_uiHost = nullptr; // Sibling-close routing (Toggle).
};

} // namespace NoMoreDay::ui
