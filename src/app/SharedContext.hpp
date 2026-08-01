#pragma once

#include "app/Settings.hpp"
#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>


class ResourceManager;
class LevelManager;
namespace NoMoreDay {
class SceneManager;
}

namespace NoMoreDay {
struct RenderContext;
namespace render {
class GameplayRenderHooks;
}
namespace systems {
class SpatialHashGrid;
}

struct SharedContext {
  entt::registry *registry = nullptr;
  ResourceManager *resources = nullptr;
  LevelManager *levelManager = nullptr;
  SceneManager *sceneManager = nullptr;
  tf::Executor *executor = nullptr;
  GameSettings *settings = nullptr;
  systems::SpatialHashGrid *spatialGrid = nullptr;
  float renderAlpha = 0.0f; // Interpolation factor [0, 1) for smooth rendering
                            // between physics frames
  RenderContext *renderContext = nullptr;
  // Gameplay render adapter (Game layer) driven by the Engine render hooks.
  // nullptr/empty hooks skip the gameplay draw segment (gate/harness paths).
  render::GameplayRenderHooks *gameplayRenderHooks = nullptr;
  // Window* window; // Raylib uses global state mostly, add if wrapper exists
};

} // namespace NoMoreDay
