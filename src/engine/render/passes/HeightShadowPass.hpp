#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/lighting/GlobalHeightField.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"

#include "raylib.h"

namespace NoMoreDay::render::passes {

class HeightShadowPass final : public graph::RenderPass {
public:
  HeightShadowPass();
  ~HeightShadowPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "HeightShadowPass"; }
  graph::RenderPassType Type() const override {
    return graph::RenderPassType::HeightShadow;
  }

  bool Initialize();
  void Shutdown();
  void OnResize(int width, int height);
  bool ReloadShaders();
  [[nodiscard]] bool IsInitialized() const { return m_initialized; }
  [[nodiscard]] uint32_t GetHeightFieldTexture() const noexcept {
    return m_heightField.GetTextureId();
  }

private:
  void DrawFullscreen(Shader shader, uint32_t sourceTexture,
                      uint32_t heightFieldTexture);

  Shader m_heightShadowShader = {0};
  resources::FramebufferHandle m_outputBuffer = {};

  int m_sceneTexLoc = -1;
  int m_heightFieldTexLoc = -1;
  int m_stepsLoc = -1;
  int m_selfShadowEnabledLoc = -1;
  int m_selfShadowStepsLoc = -1;
  int m_pomEnabledLoc = -1;
  int m_pomLayersLoc = -1;
  int m_cameraOffsetLoc = -1;
  int m_screenSizeLoc = -1;
  int m_heightWorldOriginLoc = -1;
  int m_heightWorldSizeLoc = -1;

  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  bool m_initialized = false;
  bool m_heightFieldInitialized = false;
  lighting::GlobalHeightField m_heightField = {};
};

} // namespace NoMoreDay::render::passes
