#pragma once

#include "engine/render/core/RenderConstants.hpp"
#include <cstdint>

namespace NoMoreDay::render::resources {

struct FramebufferHandle {
  uint32_t fbo = 0;
  uint32_t colorTexture = 0;
  uint32_t depthRbo = 0;
  int width = 0;
  int height = 0;
  uint32_t internalFormat = 0;
  uint64_t trackedBytes = 0;
  // H5 (P2 AD-8): the QualityTier the resource was acquired under. GPUTexturePool
  // records it at Acquire time so Release() can rebuild the exact TexturePoolKey
  // and return the resource to the correct (format, tier, sizeClass) bucket.
  core::QualityTier tier = core::QualityTier::Medium;

  [[nodiscard]] bool IsValid() const { return fbo != 0 && colorTexture != 0; }
};

} // namespace NoMoreDay::render::resources
