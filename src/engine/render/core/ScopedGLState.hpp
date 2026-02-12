#pragma once

#include "raylib.h"
#include "rlgl.h"

namespace NoMoreDay::render::core {

// Guards pass boundaries by forcing a predictable 2D GL state on exit.
class ScopedGLState {
public:
  ScopedGLState() { rlDrawRenderBatchActive(); }

  ~ScopedGLState() {
    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlEnableDepthMask();
    rlDisableBackfaceCulling();
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlActiveTextureSlot(0);
  }
};

} // namespace NoMoreDay::render::core
