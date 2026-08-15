#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/PersistentBuffer.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "raylib.h"

#include <cstdint>
#include <vector>

namespace NoMoreDay::render {

struct GPUTextStringMeta {
  uint32_t glyphOffset = 0;
  uint16_t glyphCount = 0;
  uint8_t animStyle = 0;
  uint8_t reserved = 0;
};
static_assert(sizeof(GPUTextStringMeta) == 8,
              "GPUTextStringMeta must be exactly 8 bytes");

class GPUTextSystem {
public:
  static constexpr uint32_t StringIdDigit0 = 0u;
  static constexpr uint32_t StringIdDigit9 = 9u;
  static constexpr uint32_t StringIdCrit = 10u;
  static constexpr uint32_t StringIdStatusGeneric = 11u;

  static GPUTextSystem &Get() {
    static GPUTextSystem instance;
    return instance;
  }

  void Init(ResourceManager &resources, uint32_t maxCommands = 4096,
            uint32_t maxQuads = 16384);
  void Shutdown();

  void BeginFrame();
  bool EnqueueCommand(const components::GPUTextCommand &command);

  void UploadGlyphMetrics(const std::vector<components::GPUGlyphMetrics> &metrics);
  void UploadStringTable(const std::vector<uint32_t> &glyphIndices,
                         const std::vector<GPUTextStringMeta> &meta);
  void DispatchLayout(float timeSeconds, float animDurationSeconds = 1.0f);
  void Render(const Matrix &viewProj) const;
  void SetAtlasTexture(Texture2D atlas, bool takeOwnership);

  [[nodiscard]] Texture2D GetAtlasTexture() const noexcept { return m_atlasTexture; }
  [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }
  [[nodiscard]] uint32_t GetLastQuadCount() const noexcept { return m_lastQuadCount; }
  [[nodiscard]] const NoMoreDay::core::ComputeBuffer &GetQuadBuffer() const noexcept {
    return m_quadBuffer;
  }

private:
  GPUTextSystem() = default;

  bool m_initialized = false;
  uint32_t m_maxCommands = 0;
  uint32_t m_maxQuads = 0;
  uint32_t m_lastQuadCount = 0;

  Shader m_layoutShader = {0};
  int m_locCommandCount = -1;
  int m_locGlyphMetricCount = -1;
  int m_locMaxQuadCount = -1;
  int m_locStringMetaCount = -1;
  int m_locTime = -1;
  int m_locAnimDuration = -1;
  int m_locRenderMvp = -1;
  int m_locRenderAtlas = -1;

  std::vector<components::GPUTextCommand> m_cpuCommands;
  std::vector<components::GPUGlyphMetrics> m_cpuGlyphMetrics;
  std::vector<uint32_t> m_cpuGlyphIndices;
  std::vector<GPUTextStringMeta> m_cpuStringMeta;

  PersistentBuffer m_commandBuffer;
  NoMoreDay::core::ComputeBuffer m_glyphMetricsBuffer;
  NoMoreDay::core::ComputeBuffer m_glyphIndexBuffer;
  NoMoreDay::core::ComputeBuffer m_stringMetaBuffer;
  NoMoreDay::core::ComputeBuffer m_quadBuffer;
  NoMoreDay::core::ComputeBuffer m_counterBuffer;
  NoMoreDay::core::ComputeBuffer m_indirectBuffer;
  Shader m_renderShader = {0};
  Texture2D m_atlasTexture = {};
  bool m_ownsAtlasTexture = false;
  uint32_t m_vao = 0;
  uint32_t m_vbo = 0;
};

} // namespace NoMoreDay::render
