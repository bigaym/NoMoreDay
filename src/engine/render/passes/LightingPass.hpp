#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"

#include "raylib.h"

namespace NoMoreDay::render::passes {

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
  [[nodiscard]] bool IsInitialized() const { return m_initialized; }

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

  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  bool m_initialized = false;
};

} // namespace NoMoreDay::render::passes
