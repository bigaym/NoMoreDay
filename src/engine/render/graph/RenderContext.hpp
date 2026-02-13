#pragma once

#include "engine/render/resources/FramebufferHandle.hpp"
#include "raylib.h"
#include <entt/entt.hpp>

namespace NoMoreDay {
struct SharedContext;
}

namespace NoMoreDay::render::core {
class QualityTierManager;
}

namespace NoMoreDay::render::debug {
class RenderProfiler;
}

namespace NoMoreDay::render::resources {
class TransientResourcePool;
}

namespace NoMoreDay::render::graph {

struct RenderContext {
  entt::registry *registry = nullptr;
  const NoMoreDay::SharedContext *shared = nullptr;
  const Camera2D *camera = nullptr;
  resources::TransientResourcePool *transientPool = nullptr;
  core::QualityTierManager *qualityManager = nullptr;
  debug::RenderProfiler *renderProfiler = nullptr;
  resources::FramebufferHandle hdrSceneBuffer = {};

  bool IsValid() const {
    return registry != nullptr && shared != nullptr && camera != nullptr;
  }
};

} // namespace NoMoreDay::render::graph
