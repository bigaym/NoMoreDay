#pragma once

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

  [[nodiscard]] bool IsValid() const { return fbo != 0 && colorTexture != 0; }
};

} // namespace NoMoreDay::render::resources
