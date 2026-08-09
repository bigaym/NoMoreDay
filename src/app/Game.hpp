#pragma once
#include "game/foundation/SharedContext.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/RenderContext.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/application/render/GameplayRenderAdapter.hpp"
#include "game/application/render/GPUEntityAdapter.hpp"
#include "game/application/scene/SceneManager.hpp"
#include "game/application/scene/StateManager.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "raylib.h"
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <taskflow/taskflow.hpp>


class Game {
public:
  Game(int width, int height, const char *title);
  ~Game();

  void run();

  // MS-8 W6 (M0-C): production game-binary hardware gate entry. Runs after
  // normal Game/App initialization; drives GPUHardwareValidationGate through
  // the real registry/SharedContext/render hooks and emits exactly one
  // GPU_HARDWARE_GATE_RESULT marker plus a versioned JSON report to stdout.
  // Returns the process exit code; the verdict (GO/NO_GO/NOT_RUN) is decoupled
  // from it - the Python runner decides pass/fail from the artifact.
  int runGpuGate(const std::string &revision, int sampleFramesPerFixture,
                 bool stressTest1Min, int toggleLoops);

private:
  void init();
  void cleanup();

  // Window settings
  int m_screenWidth;
  int m_screenHeight;
  const char *m_title;
  
  // Window State Handling
  bool m_isBorderlessFullscreen = false;
  int m_windowedWidth = 0;
  int m_windowedHeight = 0;
  int m_windowedPosX = 0;
  int m_windowedPosY = 0;

  void toggleFullScreen();

  // GPU Support info
  NoMoreDay::utils::GPUSupportInfo m_gpuInfo;

  // 1. 基础资源 (最后析构)
  entt::registry m_registry;
  ResourceManager m_resourceManager;
  tf::Executor m_executor;

  // Rendering Systems (Explicit management)
  NoMoreDay::systems::GPUEntitySystem m_gpuEntitySystem;
  NoMoreDay::render::MDIRenderer m_mdiRenderer;
  NoMoreDay::RenderContext m_renderContext;

  // Game 层 GPU 实体渲染适配器（ECS -> shadow buffer 投影）
  NoMoreDay::GPUEntityAdapter m_gpuEntityAdapter;

  // Game 层 Gameplay 绘制适配器（承接 RenderSystem 的 Game 专属绘制）
  NoMoreDay::GameplayRenderAdapter m_gameplayRenderAdapter;

  // 2. 共享上下文 (依赖资源)
  NoMoreDay::SharedContext m_context;
  NoMoreDay::GameSettings m_settings;

  // 3. 逻辑管理器 (最先析构)
  std::unique_ptr<LevelManager> m_levelManager;
  std::unique_ptr<NoMoreDay::SceneManager> m_sceneManager;
  std::unique_ptr<NoMoreDay::StateManager> m_stateManager;
};