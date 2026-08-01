#pragma once

#include "engine/render/lighting/GlobalHeightField.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"
#include "raylib.h"
#include <entt/entt.hpp>
#include <cstdint>

class ResourceManager;

namespace NoMoreDay::components {
struct GPUShadowCaster;
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

  // Game-side occluder projection injected before graph execution (filled by the
  // gameplay adapter via the shared OccluderProjector). Points into an
  // Engine-owned staging buffer; count == casters.size(). Consumed by
  // OccluderExtractPass/ShadowBuildPass instead of reading game components.
  const components::GPUShadowCaster *occluders = nullptr;
  uint32_t occluderCount = 0u;
  uint32_t occluderStaticCount = 0u;
  uint32_t occluderDynamicCount = 0u;
  uint64_t occluderStaticSignature = 0u;
  uint64_t occluderDynamicSignature = 0u;

  // Game-side height-field projection injected before graph execution (filled by
  // the gameplay adapter via the shared HeightFieldAdapter). Points into an
  // Engine-owned staging buffer; count == stamps.size(). Consumed by
  // HeightShadowPass instead of reading game components.
  const lighting::GlobalHeightField::HeightStamp *heightFieldStamps = nullptr;
  uint32_t heightFieldStampCount = 0u;

  // Game-side world semantics injected by the gameplay adapter (previously the
  // game Constants::World values read by HeightShadowPass).
  float worldWidth = 0.0f;
  float worldHeight = 0.0f;
  float tileWorldSize = 0.0f;

  bool IsValid() const {
    return registry != nullptr && resources != nullptr && camera != nullptr;
  }
};

} // namespace NoMoreDay::render::graph
