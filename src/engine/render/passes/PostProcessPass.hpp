#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"
#include <vector>

#include "raylib.h"

namespace NoMoreDay::render::passes {

class PostProcessPass final : public graph::RenderPass {
public:
  PostProcessPass();
  ~PostProcessPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "PostProcessPass"; }

  bool Initialize();
  void Shutdown();
  void OnResize(int width, int height);
  bool ReloadShaders();

  [[nodiscard]] const resources::FramebufferHandle &GetOutputBuffer() const {
    return m_finalOutputBuffer;
  }
  [[nodiscard]] int GetBloomMipCountForTesting() const {
    return static_cast<int>(m_bloomMips.size());
  }

private:
  struct BloomMip {
    resources::FramebufferHandle fbo;
    int width = 0;
    int height = 0;
    bool pooled = false;
  };

  void ExecuteBloom(const graph::RenderContext &context);
  // Fused tonemap + vignette + color grading (single full-screen draw).
  void ExecuteCombined(const graph::RenderContext &context);
  void ExecuteFXAA(const graph::RenderContext &context);
  bool LoadColorGradingLUT(int lutSize);

  void RebuildBloomMips(const graph::RenderContext &context, int baseWidth,
                        int baseHeight, int mipLevels);
  void DestroyBloomMips();
  void EnsureWorkingBuffers(const graph::RenderContext &context, int width,
                           int height);
  void DrawFullscreen(Shader shader, uint32_t sourceTexture);

  std::vector<BloomMip> m_bloomMips;

  Shader m_brightExtractShader = {0};
  Shader m_kawaseDownShader = {0};
  Shader m_kawaseUpShader = {0};
  Shader m_combinedShader = {0};
  Shader m_fxaaShader = {0};
  Texture2D m_colorGradingLut = {0};

  resources::FramebufferHandle m_ldrBuffer = {};
  resources::FramebufferHandle m_pingPongBuffer = {};
  resources::FramebufferHandle m_finalOutputBuffer = {};
  bool m_ldrBufferPooled = false;
  bool m_pingPongBufferPooled = false;

  int m_bloomThresholdLoc = -1;
  int m_bloomKneeLoc = -1;
  int m_bloomIntensityLoc = -1;
  int m_tonemapExposureLoc = -1;
  int m_fxaaTexelSizeLoc = -1;
  int m_vignetteIntensityLoc = -1;
  int m_vignetteRadiusLoc = -1;
  int m_vignetteEnabledLoc = -1;
  int m_colorGradingIntensityLoc = -1;
  int m_colorGradingLutSizeLoc = -1;
  int m_colorGradingEnabledLoc = -1;
  int m_combinedHdrSceneLoc = -1;
  int m_combinedBloomLoc = -1;
  int m_combinedLutLoc = -1;

  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  int m_cachedBloomMips = -1;
  int m_cachedLutSize = 0;
  bool m_initialized = false;
};

} // namespace NoMoreDay::render::passes
