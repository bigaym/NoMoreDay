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
  static void ResizeSafe(FramebufferHandle &handle, int newWidth, int newHeight,
                         void *retireFence = nullptr);
  [[nodiscard]] static uint64_t GetTrackedBytes();
  static void ResetTrackedBytesForTesting();
};

} // namespace NoMoreDay::render::resources
