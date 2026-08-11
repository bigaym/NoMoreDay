#pragma once

#include "game/application/ui/AstrolabeRenderer.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"

#include <cstdint>
#include <string>

#include <entt/entt.hpp>

namespace NoMoreDay::ui {

// Instance controller for the astrolabe panel (U7 group 5).
//
// Ports the legacy static class UIAstrolabe into a hostable instance: the
// controller owns a UiRuntime root node (created in the ctor), a private
// AstrolabeRenderer instance and all the panel state that used to be static
// members (view, visibility, alpha, failure message, vow confirmation). The
// host (GameUiHost) owns one instance and drives EnterGameplay/LeaveGameplay
// around gameplay sessions; KEY_N / ESC / per-frame Update/Draw route in-place
// through UISystem::Update/Draw parameters (frame-order coupling with the
// sibling panels, same as the stash/crafting/skill-tree controllers).
//
// The renderer is created lazily by Initialize() (reads the galaxy/node
// shaders through AssetLoadingSystem, mirroring the legacy load path) and is
// unloaded by its destructor; the controller itself performs no GL work in
// the ctor so it can be constructed and tested headless.
class AstrolabeController {
public:
  struct VisibilityState {
    bool visible;
    float alpha;
  };

  explicit AstrolabeController(UiRuntime& runtime);
  ~AstrolabeController() = default;

  AstrolabeController(const AstrolabeController&) = delete;
  AstrolabeController& operator=(const AstrolabeController&) = delete;

  // Loads the talent registry and initializes the renderer (legacy
  // UIAstrolabe::Initialize). Idempotent; must run after UISystem::Initialize
  // so the asset system is up. GameUiHost calls it once in Initialize().
  void Initialize();

  // Per-frame update (legacy UIAstrolabe::Update; currently a no-op).
  void Update(entt::registry& registry);

  // Draws the panel (legacy UIAstrolabe::Draw): early-outs when hidden and
  // falls back to the first PlayerTag entity. Route in-place inside
  // UISystem::Draw (frame-order coupling with the sibling panels).
  void Draw(entt::registry& registry);

  // Flips panel visibility (legacy UIAstrolabe::Toggle). Opening loads the
  // data/renderer, resets the camera view and closes the sibling panels
  // (showInventory/showCharacterPanel/showContextMenu/showSkillTree) exactly
  // like the legacy KEY_N handler did. Closing hides the panel; the fade
  // alpha is animated down by the per-frame Draw.
  void Toggle(entt::registry& registry, entt::entity player);

  // Mirror of the legacy read accessor: visible once the panel is open or
  // still fading out.
  [[nodiscard]] bool IsVisible(entt::registry& registry, entt::entity player) const;

  void Show();
  void Hide();

  // Resets the camera to the initial zoom/target (KEY_N repeat press and
  // in-panel N key).
  void ResetView();

  // Lets tests isolate input gates without changing the fade behavior.
  [[nodiscard]] VisibilityState CaptureVisibilityState() const;
  void RestoreVisibilityState(VisibilityState state);

  // U7 group 5: unconditional close, used by the skill-tree sibling coupling
  // (a legacy Toggle call that only ever ran while the panel was open).
  void Close();

  // Gameplay session scoping: resets panel/failure/vow state and hides the
  // root node so nothing leaks into the next run.
  void EnterGameplay();
  void LeaveGameplay();

  [[nodiscard]] bool IsInGameplay() const noexcept;

  // Runtime node id of the panel root (kInvalidUiId if creation failed).
  [[nodiscard]] UiId NodeId() const noexcept;

private:
  void DrawInternal(entt::registry& registry, entt::entity player);
  void DrawVowDialog(entt::registry& registry, entt::entity player, const ProfessionStar& star);

  // Refactored components
  void HandleCameraInput(float dt);
  void HandleInteraction(entt::registry& registry, entt::entity player, const TalentGraph& graph, const AstrolabeComponent* comp, uint32_t hoverId, const AstrolabeTalentNode* hoveredNode, const ProfessionStar* hoveredStar);
  void DrawOverlay(const AstrolabeComponent* comp, float scale);
  void DrawTooltips(const TalentGraph& graph, const AstrolabeComponent* comp, uint32_t hoverId, const AstrolabeTalentNode* hoveredNode, const ProfessionStar* hoveredStar, float scale);

  void EmitEnergyFlow(const TalentGraph& graph, ProfessionID from, const AstrolabeTalentNode& to);
  void EmitSupernova(const AstrolabeTalentNode& node);

  void EnsureLoaded();
  void SetNodeVisible(bool visible);

  UiRuntime& m_runtime;
  UiId m_rootNodeId = kInvalidUiId;
  bool m_visible = false;
  bool m_inGameplay = false;
  NoMoreDay::AstrolabeRenderer m_renderer;

  AstrolabeView m_view;
  bool m_loaded = false;
  float m_alpha = 0.0f;

  // Failure message state
  std::string m_failMessage;
  float m_failMessageTimer = 0.0f;

  // Vow confirmation state
  ProfessionID m_pendingVowProfession = ProfessionID::BladeAscendant;
  float m_vowHoldProgress = 0.0f;
  static constexpr float VOW_HOLD_DURATION = 2.0f;
  bool m_showVowDialog = false;
};

} // namespace NoMoreDay::ui
