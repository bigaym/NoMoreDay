#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"

#include "raylib.h"
#include <entt/entt.hpp>
#include <string>
#include <vector>

class ResourceManager;

namespace NoMoreDay::render::passes {

class ShadowPreparePass;

class ShadowBuildPass final : public graph::RenderPass {
public:
  ShadowBuildPass();
  ~ShadowBuildPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "ShadowBuildPass"; }

  bool Initialize(ResourceManager &resources);
  void Shutdown();
  void OnResize(int width, int height);
  void SetPreparePass(const ShadowPreparePass *preparePass) {
    m_preparePass = preparePass;
  }

  [[nodiscard]] bool HasSdfField() const { return m_sdfField.IsValid(); }
  [[nodiscard]] uint32_t GetSdfTexture() const { return m_sdfField.colorTexture; }
  [[nodiscard]] bool HasShadowAtlas() const { return m_shadowAtlas.IsValid(); }
  [[nodiscard]] uint32_t GetShadowAtlasTexture() const {
    return m_shadowAtlas.colorTexture;
  }
  [[nodiscard]] int GetShadowAtlasSize() const { return m_shadowAtlasSize; }
  [[nodiscard]] int GetSdfWidth() const { return m_cachedWidth; }
  [[nodiscard]] int GetSdfHeight() const { return m_cachedHeight; }
  [[nodiscard]] uint32_t GetOccluderCount() const { return m_occluderCount; }
  [[nodiscard]] uint32_t GetFrameIndex() const { return m_frameIndex; }
  [[nodiscard]] bool DidFailThisFrame() const { return m_lastExecuteFailure; }
  [[nodiscard]] bool SucceededThisFrame() const { return m_lastExecuteSuccess; }
  [[nodiscard]] const std::string &GetLastFailureReason() const {
    return m_lastFailureReason;
  }

private:
  void ReportFailure(const char *reason);
  void MarkSuccess();
  bool UploadOccluders(entt::registry &registry, uint32_t maxShadowCasters);
  bool InitializeAtlasPath(ResourceManager &resources);
  void EnsureAtlasSize(int atlasSize);
  void RenderAtlasTiles(const graph::RenderContext &context);

  const ShadowPreparePass *m_preparePass = nullptr;
  Shader m_sdfComputeShader = {};
  Shader m_atlasTileShader = {};
  resources::FramebufferHandle m_sdfField = {};
  resources::FramebufferHandle m_shadowAtlas = {};
  NoMoreDay::core::ComputeBuffer m_occluderBuffer;
  std::vector<NoMoreDay::components::GPUShadowCaster> m_occluderStaging;

  int m_resolutionLoc = -1;
  int m_occluderCountLoc = -1;
  int m_cameraOffsetLoc = -1;
  int m_screenSizeLoc = -1;
  int m_tileOriginLoc = -1;
  int m_tileSizeLoc = -1;
  int m_lightPosLoc = -1;
  int m_lightRadiusLoc = -1;
  int m_atlasCameraOffsetLoc = -1;
  int m_atlasScreenSizeLoc = -1;
  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  int m_shadowAtlasSize = 0;
  uint32_t m_frameIndex = 0;
  uint32_t m_occluderCount = 0;
  bool m_lastExecuteFailure = false;
  bool m_lastExecuteSuccess = false;
  std::string m_lastFailureReason;
  bool m_initialized = false;
};

} // namespace NoMoreDay::render::passes
