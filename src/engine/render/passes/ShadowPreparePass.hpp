#pragma once

#include "engine/render/GPUData.hpp"
#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/shadow/ShadowAtlasAllocator.hpp"
#include "engine/render/shadow/StableLightIdTracker.hpp"

#include "raylib.h"

#include <cstdint>
#include <vector>

namespace NoMoreDay::render::passes {

struct ShadowPreparedLight {
  uint32_t lightId = 0;
  uint32_t lightIndex = 0;
  uint32_t atlasTileIndex = 0;
  float priorityScore = 0.0f;
  float screenInfluence = 0.0f;
  float compositeScore = 0.0f;
  bool usesAtlas = false;
  components::GPULight gpuLight = {};
  float atlasRect[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // xywh normalized
};

struct ShadowPrepareLightInput {
  uint32_t sourceIndex = 0;
  uint8_t priority = 0;
  components::GPULight gpuLight = {};
};

class ShadowPreparePass final : public graph::RenderPass {
public:
  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "ShadowPreparePass"; }

  [[nodiscard]] uint32_t GetFrameIndex() const noexcept { return m_frameIndex; }
  [[nodiscard]] uint32_t GetAtlasOverflowCount() const noexcept {
    return m_atlasOverflowCount;
  }
  [[nodiscard]] uint32_t GetAtlasAllocatedCount() const noexcept {
    return m_atlasAllocatedCount;
  }
  [[nodiscard]] uint32_t GetAtlasTilesPerRow() const noexcept {
    return m_atlasTilesPerRow;
  }
  [[nodiscard]] uint32_t GetAtlasTileSize() const noexcept { return m_atlasTileSize; }
  [[nodiscard]] uint32_t GetAtlasSize() const noexcept { return m_atlasSize; }
  [[nodiscard]] const std::vector<ShadowPreparedLight> &GetPreparedLights() const {
    return m_preparedLights;
  }

  [[nodiscard]] static uint32_t
  BuildStableLightId(const components::GPULight &light,
                     uint32_t fallbackIndex) noexcept;
  [[nodiscard]] static float
  ComputeScreenInfluence(const components::GPULight &light,
                         const Camera2D &camera, float screenWidth,
                         float screenHeight) noexcept;
  [[nodiscard]] static float ComputeCompositeScore(
      uint8_t priority, float screenInfluence) noexcept;
  [[nodiscard]] static std::vector<ShadowPreparedLight> RankTopNForAtlas(
      const std::vector<ShadowPrepareLightInput> &inputs, const Camera2D &camera,
      float screenWidth, float screenHeight, uint32_t maxShadowedLights);

private:
  void EnsureAllocator(uint32_t atlasSize);
  void ComputeAtlasRect(uint32_t tileIndex, float outRect[4]) const noexcept;
  [[nodiscard]] uint64_t
  ComputeLightFingerprint(const components::GPULight &light) const noexcept;
  void AssignStableLightIds(std::vector<ShadowPreparedLight> &ranked);

  uint32_t m_frameIndex = 0;
  uint32_t m_atlasOverflowCount = 0;
  uint32_t m_atlasAllocatedCount = 0;
  uint32_t m_atlasSize = 0;
  uint32_t m_atlasTileSize = 256;
  uint32_t m_atlasTilesPerRow = 1;
  uint32_t m_lastLoggedOverflow = 0;
  std::vector<ShadowPreparedLight> m_preparedLights;
  shadow::ShadowAtlasAllocator m_atlasAllocator = shadow::ShadowAtlasAllocator(1, 0);
  shadow::StableLightIdTracker m_lightIdTracker;
};

} // namespace NoMoreDay::render::passes
