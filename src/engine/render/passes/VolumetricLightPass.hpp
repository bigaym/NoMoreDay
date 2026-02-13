#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"

#include "raylib.h"

namespace NoMoreDay::render::passes {

class VolumetricLightPass final : public graph::RenderPass {
public:
  VolumetricLightPass();
  ~VolumetricLightPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "VolumetricLightPass"; }

  bool Initialize();
  void Shutdown();
  void OnResize(int width, int height);
  bool ReloadShaders();
  [[nodiscard]] bool IsInitialized() const { return m_initialized; }

private:
  void DrawFullscreen(Shader shader, uint32_t sourceTexture);

  Shader m_volumetricShader = {0};
  resources::FramebufferHandle m_outputBuffer = {};

  int m_sceneTexLoc = -1;
  int m_lightCountLoc = -1;
  int m_sampleCountLoc = -1;
  int m_scatteringLoc = -1;
  int m_decayLoc = -1;
  int m_exposureLoc = -1;
  int m_cameraOffsetLoc = -1;
  int m_screenSizeLoc = -1;

  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  bool m_initialized = false;
};

} // namespace NoMoreDay::render::passes
