#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"

#include "raylib.h"
#include <string>

namespace NoMoreDay::render::passes {

class ShadowResolvePass;

class LightingPass final : public graph::RenderPass {
public:
  LightingPass();
  ~LightingPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "LightingPass"; }

  bool Initialize();
  void Shutdown();
  void OnResize(int width, int height);
  bool ReloadShaders();
  [[nodiscard]] bool IsInitialized() const { return m_initialized; }
  void SetShadowResolvePass(const ShadowResolvePass *shadowResolvePass) {
    m_shadowResolvePass = shadowResolvePass;
  }
  [[nodiscard]] bool WasShadowAppliedLastFrame() const {
    return m_lastShadowApplied;
  }
  [[nodiscard]] bool UsedV2FallbackLastFrame() const {
    return m_lastUsedV2Fallback;
  }
  [[nodiscard]] const std::string &GetLastShadowFallbackReason() const {
    return m_lastShadowFallbackReason;
  }

private:
  void DrawFullscreen(Shader shader, uint32_t sourceTexture);

  Shader m_lightAccumShader = {0};
  resources::FramebufferHandle m_litBuffer = {};

  int m_sceneTexLoc = -1;
  int m_ambientColorLoc = -1;
  int m_ambientIntensityLoc = -1;
  int m_lightCountLoc = -1;
  int m_cameraOffsetLoc = -1;
  int m_screenSizeLoc = -1;
  int m_shadowMaskTexLoc = -1;
  int m_shadowEnabledLoc = -1;

  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  uint32_t m_frameIndex = 0;
  bool m_lastShadowApplied = false;
  bool m_lastUsedV2Fallback = false;
  std::string m_lastShadowFallbackReason;
  bool m_initialized = false;
  const ShadowResolvePass *m_shadowResolvePass = nullptr;
};

} // namespace NoMoreDay::render::passes
