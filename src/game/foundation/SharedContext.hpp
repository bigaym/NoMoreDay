#pragma once

#include "game/foundation/Settings.hpp"
#include <entt/entt.hpp>
#include <functional>
#include <taskflow/taskflow.hpp>


class ResourceManager;
class LevelManager;
namespace NoMoreDay {
class SceneManager;
class ItemStorageService;
class SaveManager;
class HeirloomVault;
class SharedStash;
}

namespace NoMoreDay {
struct RenderContext;
namespace render {
class GameplayRenderHooks;
}
namespace systems {
class SpatialHashGrid;
}
namespace ui {
class GameUiHost;
}

struct SharedContext {
  entt::registry *registry = nullptr;
  ResourceManager *resources = nullptr;
  LevelManager *levelManager = nullptr;
  SceneManager *sceneManager = nullptr;
  tf::Executor *executor = nullptr;
  GameSettings *settings = nullptr;
  ItemStorageService *itemStorage = nullptr;
  SaveManager *saveManager = nullptr;
  SharedStash *sharedStash = nullptr;
  HeirloomVault *heirloomVault = nullptr;
  systems::SpatialHashGrid *spatialGrid = nullptr;
  float renderAlpha = 0.0f; // Interpolation factor [0, 1) for smooth rendering
                            // between physics frames
  RenderContext *renderContext = nullptr;
  // Gameplay render adapter (Game layer) driven by the Engine render hooks.
  // nullptr/empty hooks skip the gameplay draw segment (gate/harness paths).
  render::GameplayRenderHooks *gameplayRenderHooks = nullptr;
  // UI composition root owned by the application. Injected by Game so states
  // (e.g. GameplayState) can borrow it without constructing their own host.
  ui::GameUiHost *uiHost = nullptr;
  // 切图或触发自动存档的回调请求 (slotIndex 默认为 0)
  std::function<void(int slot)> requestSave;
  // U7 group 3: cross-layer crafting entry points. InventorySystem (Legendary
  // Core use) and UIRenderer (context-menu craft) live below the UI layer, so
  // they route through these callbacks instead of the legacy static
  // UICrafting. Game fills them with lambdas capturing the host-owned
  // UICraftingController.
  // R10 (收尾): craftingSetTargetItem is gone — it had no caller (the overlay
  // Craft action routes through the host channel GameUiHost::
  // CraftingSetTargetItem since R6, and InventorySystem's Legendary Core path
  // only opens the merge panel). Only the merge-panel callback remains live.
  std::function<void()> openCraftingMergePanel;
  // R8: the legacy closeAstrolabe callback is gone — the skill-tree controller
  // routes the astrolabe close through the host channel (GameUiHost::
  // CloseAstrolabe) since U8, so no SharedContext coupling is left.
  // U8: gameplay-layer message box notifications. Gameplay systems
  // (InventorySystem) sit below the UI layer and must not include UI headers,
  // so they route through this callback instead of the legacy static
  // UISystem::State.showMessageBox. Game fills it with a lambda capturing the
  // host-owned OverlayController message box API; an empty callback is silent.
  std::function<void(const char* text)> showMessageBox;
  // Window* window; // Raylib uses global state mostly, add if wrapper exists
};

/**
 * @brief 从 ECS 注册表上下文中获取 SharedContext 指针的统一访问器。
 */
inline SharedContext* GetSharedContext(entt::registry& registry) noexcept {
  if (registry.ctx().contains<SharedContext*>()) {
    return registry.ctx().get<SharedContext*>();
  }
  return nullptr;
}

inline const SharedContext* GetSharedContext(const entt::registry& registry) noexcept {
  if (registry.ctx().contains<SharedContext*>()) {
    return registry.ctx().get<SharedContext*>();
  }
  return nullptr;
}

} // namespace NoMoreDay
