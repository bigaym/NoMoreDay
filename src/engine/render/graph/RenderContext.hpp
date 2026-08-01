#pragma once

#include "engine/render/resources/FramebufferHandle.hpp"
#include "raylib.h"
#include <entt/entt.hpp>
#include <cstdint>

class ResourceManager;

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
  ResourceManager *resources = nullptr;
  const Camera2D *camera = nullptr;
  resources::TransientResourcePool *transientPool = nullptr;
  core::QualityTierManager *qualityManager = nullptr;
  debug::RenderProfiler *renderProfiler = nullptr;
  resources::FramebufferHandle hdrSceneBuffer = {};
  uint32_t giDistanceFieldTexture = 0u;
  int giDistanceFieldWidth = 0;
  int giDistanceFieldHeight = 0;
  uint32_t giEmissiveTexture = 0u;
  int giEmissiveWidth = 0;
  int giEmissiveHeight = 0;
  uint32_t giRadianceTexture = 0u;
  int giRadianceWidth = 0;
  int giRadianceHeight = 0;

  bool IsValid() const {
    return registry != nullptr && resources != nullptr && camera != nullptr;
  }
};

} // namespace NoMoreDay::render::graph
