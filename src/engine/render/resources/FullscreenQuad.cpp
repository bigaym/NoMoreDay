#include "engine/render/resources/FullscreenQuad.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "rlgl.h"

namespace NoMoreDay::render::resources {
namespace {

constexpr uint32_t kGLTriangles = 0x0004;

}

uint32_t FullscreenQuad::s_vao = 0;
bool FullscreenQuad::s_initialized = false;

void FullscreenQuad::EnsureInitialized() {
  if (s_initialized) {
    return;
  }

  s_vao = rlLoadVertexArray();
  if (s_vao == 0) {
    LOG_ERROR("FullscreenQuad failed to create VAO");
    return;
  }

  s_initialized = true;
}

void FullscreenQuad::Draw() {
  EnsureInitialized();
  if (!s_initialized) {
    return;
  }

  rlDrawRenderBatchActive();
  rlEnableVertexArray(s_vao);
  NoMoreDay::utils::GPUUtils::DrawArrays(kGLTriangles, 0, 3);
  rlDisableVertexArray();
}

void FullscreenQuad::Shutdown() {
  if (s_vao != 0) {
    rlUnloadVertexArray(s_vao);
    s_vao = 0;
  }
  s_initialized = false;
}

} // namespace NoMoreDay::render::resources
