#pragma once

#include "engine/render/GPUData.hpp"
#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"

#include "raylib.h"

#include <array>

namespace NoMoreDay::render::passes {

class DistortionPass final : public graph::RenderPass {
public:
  DistortionPass();
  ~DistortionPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "DistortionPass"; }
  graph::RenderPassType Type() const override {
    return graph::RenderPassType::Distortion;
  }

  bool Initialize();
  void Shutdown();
  void OnResize(int width, int height);
  bool ReloadShaders();

  void SetInputBuffer(const resources::FramebufferHandle *inputBuffer) {
    m_inputBuffer = inputBuffer;
  }
  [[nodiscard]] const resources::FramebufferHandle &GetOutputBuffer() const {
    return m_finalOutputBuffer;
  }
  [[nodiscard]] int GetActiveSourceCountForTesting() const { return m_activeCount; }

  void AddDistortionSource(float worldX, float worldY, float radius, float strength);
  void ResetSources();

  static constexpr int MAX_DISTORTION_SOURCES = 32;

private:
  void EnsureWorkingBuffers(const graph::RenderContext &context, int width,
                            int height);

  resources::FramebufferHandle m_distortionBuffer = {};
  resources::FramebufferHandle m_applyBuffer = {};
  resources::FramebufferHandle m_finalOutputBuffer = {};
  bool m_distortionBufferPooled = false;
  bool m_applyBufferPooled = false;
  const resources::FramebufferHandle *m_inputBuffer = nullptr;

  Shader m_distortionWriteShader = {0};
  Shader m_distortionApplyShader = {0};

  std::array<components::GPUDistortionSource, MAX_DISTORTION_SOURCES> m_sources{};
  int m_activeCount = 0;
  uint32_t m_ssbo = 0;

  int m_sourceCountLoc = -1;
  int m_cameraOffsetLoc = -1;
  int m_screenSizeLoc = -1;

  int m_applySceneLoc = -1;
  int m_applyDistortionLoc = -1;
  int m_applyScaleLoc = -1;

  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  bool m_initialized = false;
};

} // namespace NoMoreDay::render::passes
