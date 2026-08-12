#pragma once

#include "game/application/ui/AstrolabeRenderer.hpp"
#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"

#include <cstdint>
#include <string>

namespace NoMoreDay::ui {

class GameUiHost;

// Instance controller for the astrolabe panel (U7 group 5).
//
// R8: the panel runs the snapshot/intent/draw-list pipeline. Update() takes the
// frame snapshot + UiInputFrame (interaction: camera, hit test, node/star
// clicks that enqueue intents, vow hold-to-confirm), Paint() emits one custom
// painter command on the Panels layer and the registered backend painter draws
// through the AstrolabeRenderer. The controller never reads the registry, never
// writes gameplay state (addPointToNode/takeVow go through
// GameUiCommandHandler intents) and never calls raylib draw functions itself.
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

  // R8: intent sink (GameUiHost owns the enqueue queue). May be null in
  // headless tests; interactions then degrade to UI-local state only.
  void SetHost(GameUiHost* host) noexcept;

  // Loads the talent registry and initializes the renderer (legacy
  // UIAstrolabe::Initialize). Idempotent; must run after UISystem::Initialize
  // so the asset system is up. GameUiHost calls it once in Initialize().
  void Initialize();

  // R8: per-frame interaction phase. Reads the frame snapshot + UiInputFrame;
  // node/star clicks and the vow hold-to-confirm enqueue intents (executed by
  // the command handler on the next gameplay Update). Never touches the
  // registry or gameplay components directly.
  void Update(const GameUiSnapshot& snapshot, const UiInputFrame& input);

  // R8: emits a single custom painter command on the Panels layer when the
  // panel is visible or still fading out. The registered backend painter
  // (kAstrolabePainterResourceId) performs the actual drawing through
  // AstrolabeRenderer.
  void Paint(UiDrawList& drawList, const UiViewport& viewport);

  // R8: no longer needs the registry/player (pure panel-local visibility).
  void Toggle();

  // R8: canvas draw (registered custom-painter target). Draws the astrolabe
  // panel with AstrolabeRenderer; read-only over the paint state. Public so
  // the registered painter callback and tech tests can reach it without
  // touching the private internals.
  void PaintCanvas(UiRect nativeBounds);

  // Mirror of the legacy read accessor: visible once the panel is open or
  // still fading out.
  [[nodiscard]] bool IsVisible() const noexcept;

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
  void DrawInternal(); // painter-side: full canvas render
  void HandleCameraInput(const UiInputFrame& input);
  void HandleInteraction(const GameUiSnapshot& snapshot,
                         const UiInputFrame& input);
  void UpdateVowDialog(const GameUiSnapshot& snapshot,
                       const UiInputFrame& input);
  void DrawOverlay(float scale);
  void DrawTooltips(float scale);
  void DrawVowDialog(float scale);

  void EnsureLoaded();
  void SetNodeVisible(bool visible);

  UiRuntime& m_runtime;
  GameUiHost* m_uiHost = nullptr;
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

  // R8: painter-side payload rebuilt from the frame snapshot each Update.
  // The custom painter reads this (via the controller) instead of the registry.
  struct PaintState {
    const GameUiSnapshot* snapshot = nullptr;
    uint32_t hoverId = 0;
    int hoveredStarIndex = -1;
  };
  PaintState m_paintState;
};

// R8: backend painter callback (registered by GameUiHost with
// kAstrolabePainterResourceId). userData points to the controller.
void AstrolabePaintCallback(void* userData, UiRect nativeBounds);

} // namespace NoMoreDay::ui
