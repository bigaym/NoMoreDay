#pragma once

#include "game/systems/physics/SpatialGrid.hpp"
#include "game/scene/State.hpp"
#include "raylib.h"
#include <entt/entt.hpp>
#include <memory>
#include <taskflow/taskflow.hpp>


// Forward declaration

namespace NoMoreDay {

// Forward declaration in namespace
class PortalSystem;

class GameplayState : public IState {
public:
  GameplayState(StateManager &stateManager, SharedContext &context,
                RenderContext &renderContext);

  void OnEnter() override;
  void OnExit() override;
  bool OnUpdate(float dt) override;
  void OnRender() override;
  ~GameplayState()
      override; // Destructor needed for unique_ptr with forward-declared type

  // GameplayState is opaque (draws background)
  bool IsTransparent() const override { return false; }

private:
  void InitializeEntities();
  void UpdatePhysics(float dt);
  void RenderMapAffixOverlay();
  void EnsurePlayerHasDimensionalFragment(entt::registry& registry, entt::entity player);
  void OpenDimensionalLevelSelect(entt::registry& registry, entt::entity player);
  void ClearActiveRiftForNewRun(entt::registry& registry);
  bool HandleRiftDialogs(entt::registry& registry);
  void RenderRiftDialogs();
  void UpdateSceneRT();

  Camera2D m_camera = {0};
  RenderTexture2D m_sceneRT = {0};
  Shader m_activeFilterShader = {0};
  std::string m_lastFilterPath;
  int m_filterLocTime = -1;
  int m_filterLocCam = -1;
  int m_filterLocZoom = -1;
  int m_filterLocScreen = -1;
  int m_filterLocPlayer = -1;
  int m_filterLocVision = -1;
  float m_vfxHotReloadAccumulator = 0.0f;

  tf::Taskflow m_taskflow;
  systems::SpatialHashGrid m_spatialGrid{
      100, 100, 50}; // Initial size, resized in OnEnter/Init
  std::vector<entt::entity> m_physicsEntities;

  // Portal System
  std::unique_ptr<PortalSystem> m_portalSystem;
  RenderContext *m_renderContext = nullptr;

  bool m_showGateResumeOrNewDialog = false;
  bool m_showGateStartNewConfirmDialog = false;
  bool m_showRiftCompletedDialog = false;
  entt::entity m_gateDialogPlayer = entt::null;
};

} // namespace NoMoreDay
