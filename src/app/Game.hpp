#pragma once
#include "game/foundation/SharedContext.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/RenderContext.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/application/persistence/SaveManager.hpp"
#include "game/application/render/GameplayRenderAdapter.hpp"
#include "game/application/render/GPUEntityAdapter.hpp"
#include "game/application/scene/SceneManager.hpp"
#include "game/application/scene/StateManager.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/WorldUiFrame.hpp"
#include "game/systems/item/HeirloomVault.hpp"
#include "game/systems/item/SharedStash.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
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

  // MS-8 W6 (M0-C): 生产环境游戏二进制硬件门禁入口。在常规 Game/App 初始化后运行；
  // 通过真实 registry/SharedContext/render 钩子驱动 GPUHardwareValidationGate，
  // 并向 stdout 输出唯一的 GPU_HARDWARE_GATE_RESULT 标记和带版本的 JSON 报告。
  // 返回进程退出码；裁决结果 (GO/NO_GO/NOT_RUN) 与退出码解耦 - 由 Python 运行器根据工件决定通过/失败。
  int runGpuGate(const std::string &revision, int sampleFramesPerFixture,
                 bool stressTest1Min, int toggleLoops);

private:
  void init();
  void cleanup();

  // 窗口设置
  int m_screenWidth;
  int m_screenHeight;
  const char *m_title;
  
  // 窗口状态处理
  bool m_isBorderlessFullscreen = false;
  int m_windowedWidth = 0;
  int m_windowedHeight = 0;
  int m_windowedPosX = 0;
  int m_windowedPosY = 0;

  void toggleFullScreen();

  // GPU 支持信息
  NoMoreDay::utils::GPUSupportInfo m_gpuInfo;

  // 1. 基础资源 (最后析构)
  entt::registry m_registry;
  ResourceManager m_resourceManager;
  tf::Executor m_executor;

  // 渲染系统 (显式管理)
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

  // 物品存储单轨服务 (T-P2)。归组合根所有，并通过 SharedContext::itemStorage 引用；
  // 传统 ECS 轨道在 T-P3-3 切换迁移标志前保持权威性。
  NoMoreDay::ItemStorageService m_itemStorage;

  // 持久化管理器与跨存档存储实例 (T-P6 收尾：单例治理)
  NoMoreDay::SaveManager m_saveManager;
  NoMoreDay::SharedStash m_sharedStash;
  NoMoreDay::HeirloomVault m_heirloomVault;

  // UI 组合根：持有常驻运行时核心，并在迁移期间转发给旧版外观。
  // 在引用它的共享上下文之后析构 (cleanup() 也会显式关闭它)。
  NoMoreDay::ui::GameUiHost m_uiHost;

  // U8: 组合根拥有的世界空间 UI 桥接器。渲染写入端 (GameplayRenderAdapter) 逐帧填充；
  // 主机读取端 (拾取命中测试、地面悬停、渲染高亮) 负责消费。
  NoMoreDay::ui::WorldUiFrame m_worldFrame;

  // 3. 逻辑管理器 (最先析构)
  std::unique_ptr<LevelManager> m_levelManager;
  std::unique_ptr<NoMoreDay::SceneManager> m_sceneManager;
  std::unique_ptr<NoMoreDay::StateManager> m_stateManager;
};