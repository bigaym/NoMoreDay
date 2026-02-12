#pragma once

#include <cstdint>

namespace NoMoreDay::render::resources {

class FullscreenQuad {
public:
  static void Draw();
  static void Shutdown();

private:
  static void EnsureInitialized();

  static uint32_t s_vao;
  static bool s_initialized;
};

} // namespace NoMoreDay::render::resources
