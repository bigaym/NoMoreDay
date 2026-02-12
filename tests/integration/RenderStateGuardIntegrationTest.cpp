#include "TestCommon.hpp"
#include "doctest.h"
#include "engine/render/core/ScopedGLState.hpp"
#include "rlgl.h"
#include "GLFW/glfw3.h"

TEST_CASE("[Integration] RenderGraph - ScopedGLState restores GL state") {
  constexpr GLint kGlActiveTexture = 0x84E0; // GL_ACTIVE_TEXTURE
  constexpr GLint kGlTexture0 = 0x84C0;      // GL_TEXTURE0

  // Simulate a pass that dirties GL state.
  rlDrawRenderBatchActive();
  rlEnableDepthTest();
  rlDisableDepthMask();
  rlEnableBackfaceCulling();
  rlActiveTextureSlot(5);

  {
    NoMoreDay::render::core::ScopedGLState guard;
    rlEnableDepthTest();
    rlDisableDepthMask();
    rlEnableBackfaceCulling();
    rlActiveTextureSlot(7);
  }

  rlDrawRenderBatchActive();

  CHECK(glIsEnabled(GL_DEPTH_TEST) == GL_FALSE);
  CHECK(glIsEnabled(GL_CULL_FACE) == GL_FALSE);

  GLboolean depthMask = GL_FALSE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
  CHECK(depthMask == GL_TRUE);

  GLint activeTex = 0;
  glGetIntegerv(kGlActiveTexture, &activeTex);
  CHECK(activeTex == kGlTexture0);
}
