#pragma once

#include "engine/render/resources/FramebufferHandle.hpp"
#include <cstdint>

namespace NoMoreDay::render::resources {

class FramebufferManager {
public:
  [[nodiscard]] static FramebufferHandle
  Create(int width, int height, uint32_t internalFormat, bool withDepth = false);

  static void Destroy(FramebufferHandle &handle);
  static void Resize(FramebufferHandle &handle, int newWidth, int newHeight);
};

} // namespace NoMoreDay::render::resources
