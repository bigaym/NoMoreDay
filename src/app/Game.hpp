#pragma once
#include "app/SharedContext.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/RenderContext.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "engine/scene/SceneManager.hpp"
#include "engine/scene/StateManager.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "raylib.h"
#include <entt/entt.hpp>
#include <memory>
#include <taskflow/taskflow.hpp>


class Game {
public:
  Game(int width, int height, const char *title);
  ~Game();

  void run();

private:
  void init();
  void cleanup();

  // Window settings
  int m_screenWidth;
  int m_screenHeight;
  const char *m_title;

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

  // 2. 共享上下文 (依赖资源)
  NoMoreDay::SharedContext m_context;
  NoMoreDay::GameSettings m_settings;

  // 3. 逻辑管理器 (最先析构)
  std::unique_ptr<LevelManager> m_levelManager;
  std::unique_ptr<NoMoreDay::SceneManager> m_sceneManager;
  std::unique_ptr<NoMoreDay::StateManager> m_stateManager;
};