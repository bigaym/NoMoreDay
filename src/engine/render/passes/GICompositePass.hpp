#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"

#include "raylib.h"
#include <cstdint>

class ResourceManager;

namespace NoMoreDay::render::passes {

class GICompositePass final : public graph::RenderPass {
public:
  GICompositePass();
  ~GICompositePass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "GICompositePass"; }

  bool Initialize(ResourceManager &resources);
  void Shutdown();
  void OnResize(int width, int height);
  void InvalidateHistory() noexcept { m_historyValid = false; }

private:
  bool EnsureResources(int width, int height);
  uint64_t BuildLightSignature() const;

  Shader m_compositeShader = {};

  int m_sceneResolutionLoc = -1;
  int m_radianceResolutionLoc = -1;
  int m_temporalWeightLoc = -1;
  int m_giIntensityLoc = -1;
  int m_resetHistoryLoc = -1;

  resources::FramebufferHandle m_outputScene = {};
  resources::FramebufferHandle m_historyA = {};
  resources::FramebufferHandle m_historyB = {};

  int m_cachedWidth = 0;
  int m_cachedHeight = 0;
  bool m_initialized = false;
  bool m_historyValid = false;
  bool m_readHistoryA = true;
  bool m_prevCameraValid = false;
  Vector2 m_prevCameraTarget = {0.0f, 0.0f};
  uint64_t m_prevLightSignature = 0u;
};

} // namespace NoMoreDay::render::passes

