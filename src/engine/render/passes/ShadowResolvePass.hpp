#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"

#include "raylib.h"
#include <string>

namespace NoMoreDay::render::passes {

class ShadowBuildPass;

class ShadowResolvePass final : public graph::RenderPass {
public:
  ShadowResolvePass();
  ~ShadowResolvePass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "ShadowResolvePass"; }

  void SetBuildPass(const ShadowBuildPass *buildPass) { m_buildPass = buildPass; }
  bool Initialize();
  void Shutdown();
  void OnResize(int width, int height);
  bool ReloadShaders();

  [[nodiscard]] bool HasShadowMask() const { return m_shadowMask.IsValid(); }
  [[nodiscard]] uint32_t GetShadowMaskTexture() const {
    return m_shadowMask.colorTexture;
  }
  [[nodiscard]] uint32_t GetShadowMaskFramebuffer() const { return m_shadowMask.fbo; }
  [[nodiscard]] bool IsShadowReadyForCurrentFrame() const {
    return m_shadowReadyThisFrame;
  }
  [[nodiscard]] bool HadFailureThisFrame() const { return m_lastExecuteFailed; }
  [[nodiscard]] uint32_t GetFrameIndex() const { return m_frameIndex; }
  [[nodiscard]] const std::string &GetLastFailureReason() const {
    return m_lastFailureReason;
  }

private:
  void ReportFailure(const std::string &reason);

  // B11 (RG-3 owner metadata): reclassify the shadow mask observer records to
  // the RenderGraph owner contract (Shadow) after create/resize. See the call
  // site in OnResize.
  void ReclassifyShadowMask();

  const ShadowBuildPass *m_buildPass = nullptr;
  Shader m_shadowResolveShader = {};
  resources::FramebufferHandle m_shadowMask = {};

  int m_shadowSdfTexLoc = -1;
  int m_shadowSoftnessLoc = -1;
  int m_sdfTexelSizeLoc = -1;
  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  uint32_t m_frameIndex = 0;
  bool m_shadowReadyThisFrame = false;
  bool m_lastExecuteFailed = false;
  std::string m_lastFailureReason;
  bool m_initialized = false;
};

} // namespace NoMoreDay::render::passes
