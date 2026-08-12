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
struct UiInputFrame; // R8: snapshot-driven interaction input.
class UiDrawList;    // R8: render-phase paint target.
class UiViewport;    // R8: logical viewport for the paint target.

// Instance controller for the skill specialization UI (U7 group 4).
//
// Owns the two-stage skill UI: the mastery hub (UISkillHub) and the talent
// tree view (SkillTreeUI). The controller holds the panel state that used to
// live in UISystem::State (showSkillTree / skillTreeAlpha / selectedSkillId);
// with the U8 final narrowing the legacy State fields are gone and the panel
// bodies read controller/hub instance state directly (alpha parameter,
// hub-owned selection, tooltip hover channel).
//
// R8 (remediation, design §3.1/§3.3): the controller is a snapshot/intent
// surface. Update runs the active stage's interaction phase against the frame
// snapshot + input (gameplay writes enqueue intents through the host);
// Paint emits the stage's custom command through the draw list during
// PrepareRender. The registry/player parameters are gone; the stage bodies
// (hub + talent tree) resolve their data from the snapshot and their raylib
// drawing lives in the registered backend painters only.
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
  // sibling panels exactly like the legacy code did (routed through the host
  // channels when present). R8: registry-free (the legacy SharedContext
  // closeAstrolabe callback is gone; the host channel closes the astrolabe).
  void Toggle();

  // ESC handler (was UISystem::Update): closes the panel.
  void Close();

  [[nodiscard]] bool IsVisible() const noexcept;
  [[nodiscard]] bool IsInGameplay() const noexcept;
  // U8: instance alpha (authoritative; the legacy State.skillTreeAlpha mirror
  // is gone). Animated by UpdateAlpha.
  [[nodiscard]] float Alpha() const noexcept { return m_alpha; }
  // U8: hub-owned selection read back by Update (was the State.selectedSkillId
  // round-trip); INVALID_SKILL_ID when the talent-tree view is closed.
  [[nodiscard]] uint32_t SelectedSkillId() const noexcept {
    return m_selectedSkillId;
  }

  // R8: interaction phase (replaces Draw(registry, player)). Runs the active
  // stage's UpdateInput against the frame snapshot + input; the hub/tree
  // enqueue gameplay-writing intents through the host. The hub's selection is
  // read back afterwards (was the State.selectedSkillId round-trip).
  void Update(const GameUiSnapshot& snapshot, const UiInputFrame& input);

  // R8: render-phase paint. Emits the active stage's custom command (Panels
  // layer) through the draw list during PrepareRender; the registered backend
  // painter draws the canvas from the stage's captured paint state.
  void Paint(UiDrawList& drawList, const UiViewport& viewport,
             const GameUiSnapshot& snapshot);

  // R8: stage accessors for the host painter registration (userData targets).
  [[nodiscard]] UISkillHub& Hub() noexcept { return m_hub; }
  [[nodiscard]] SkillTreeUI& Tree() noexcept { return m_tree; }

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
