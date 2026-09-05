#pragma once

#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/validation/GlDebugCollector.hpp"
#include "engine/render/validation/FixtureRenderDriver.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace NoMoreDay::render::validation {

// S7b: heuristic classification of the GL_RENDERER string. Known software
// rasterizers (WARP, llvmpipe, "Microsoft Basic Render Driver", generic
// "Software" renderers) are treated as non-hardware; anything else non-empty
// (NVIDIA/AMD/Intel/Apple/Mesa-with-radeon drivers) is treated as a real GPU.
inline bool IsHardwareRenderer(std::string_view renderer) {
  if (renderer.empty() || renderer == "Unknown") {
    return false;
  }
  if (renderer.find("WARP") != std::string_view::npos ||
      renderer.find("llvmpipe") != std::string_view::npos ||
      renderer.find("Basic Render Driver") != std::string_view::npos ||
      renderer.find("Software") != std::string_view::npos) {
    return false;
  }
  return true;
}

// S7b: builds the synthetic GI pass trace exactly like the matrix cell does,
// so the paired legs carry two different pass traces when GI is flipped.
inline std::string BuildGiPassTrace(bool giEnabled) {
  std::string trace;
  const auto appendType = [&trace](NoMoreDay::render::graph::RenderPassType passType) {
    const size_t index = static_cast<size_t>(passType);
    if (!trace.empty()) {
      trace += ",";
    }
    trace += NoMoreDay::render::graph::kRenderPassNames[index].full;
  };
  appendType(NoMoreDay::render::graph::RenderPassType::Scene);
  appendType(NoMoreDay::render::graph::RenderPassType::Lighting);
  appendType(NoMoreDay::render::graph::RenderPassType::HeightShadow);
  appendType(NoMoreDay::render::graph::RenderPassType::OccluderExtract);
  if (giEnabled) {
    appendType(NoMoreDay::render::graph::RenderPassType::JFA);
    appendType(NoMoreDay::render::graph::RenderPassType::RadianceCascades);
    appendType(NoMoreDay::render::graph::RenderPassType::GIComposite);
  }
  appendType(NoMoreDay::render::graph::RenderPassType::VFX);
  appendType(NoMoreDay::render::graph::RenderPassType::UIWorld);
  appendType(NoMoreDay::render::graph::RenderPassType::PostProcess);
  appendType(NoMoreDay::render::graph::RenderPassType::Composite);
  return trace;
}

// W6 (M0-C) High-2: reads back the full offscreen FBO and CPU-crops the ROI at
// its true origin (x,y). rlReadScreenPixels cannot sample an offset region, so
// reading the whole target and cropping here is the only correct way to make
// the sampled region match the declared ROI. GL state is bound/restored.
inline float ReadRoiMeanLuma(uint32_t offscreenFbo, int fboW, int fboH, int roiX,
                      int roiY, int roiW, int roiH) {
  constexpr uint32_t kGLFramebuffer = 0x8D40;
  if (roiW <= 0 || roiH <= 0 || fboW <= 0 || fboH <= 0) {
    return 0.0f;
  }
  if (roiX < 0 || roiY < 0 || roiX + roiW > fboW || roiY + roiH > fboH) {
    return 0.0f;
  }
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, offscreenFbo);
  unsigned char *pixels = rlReadScreenPixels(fboW, fboH);
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
  if (pixels == nullptr) {
    return 0.0f;
  }
  const float luma = GPUHardwareValidationGate::ComputeRoiMeanLuma(
      pixels, static_cast<size_t>(fboW) * static_cast<size_t>(fboH) * 4, fboW,
      fboH, roiX, roiY, roiW, roiH);
  RL_FREE(pixels);
  return luma;
}

// Gate-fin: the gate renders into raw GL framebuffers (not raylib
// RenderTexture2D), so the window-sized default raylib projection would map
// every hook / batch draw through the 2560x1440 window ortho instead of the
// 1280x720 offscreen target. These helpers mirror what BeginTextureMode does
// for render textures (rcore.c: rlOrtho(0, rt.width, rt.height, 0, 0, 1)) so
// scene content lands at the correct position/scale inside the offscreen FBO.
inline void ApplyTargetProjection(int width, int height) {
  rlMatrixMode(RL_PROJECTION);
  rlLoadIdentity();
  rlOrtho(0, static_cast<double>(width), static_cast<double>(height), 0, 0.0,
          1.0);
  rlMatrixMode(RL_MODELVIEW);
}

inline void RestoreWindowProjection() {
  rlMatrixMode(RL_PROJECTION);
  rlLoadIdentity();
  rlOrtho(0, static_cast<double>(GetScreenWidth()),
          static_cast<double>(GetScreenHeight()), 0, 0.0, 1.0);
  rlMatrixMode(RL_MODELVIEW);
}

} // namespace NoMoreDay::render::validation
