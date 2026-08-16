#include "engine/render/GPUTextSystem.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "rlgl.h"

#include <cstring>

namespace NoMoreDay::render {
namespace {

struct DrawArraysIndirectCommand {
  uint32_t count = 0;
  uint32_t instanceCount = 0;
  uint32_t first = 0;
  uint32_t baseInstance = 0;
};

constexpr uint32_t kGLCopyReadBuffer = 0x8F36;
constexpr uint32_t kGLCopyWriteBuffer = 0x8F37;
constexpr uint32_t kGLStreamRead = 0x88E1;
constexpr uint32_t kGLSyncGpuCommandsComplete = 0x9117;
constexpr uint32_t kGLAlreadySignaled = 0x911A;
constexpr uint32_t kGLConditionSatisfied = 0x911C;

} // namespace

GPUTextSystem::SnapshotPollOutcome
GPUTextSystem::TryPublishReadySnapshot(
    const bool slotArmed, const bool frameEligible, const bool fenceSignaled,
    const size_t readIndex, const size_t ringDepth,
    const uint32_t pendingSnapshot, const uint32_t currentSnapshot) noexcept {
  if (slotArmed && frameEligible && fenceSignaled) {
    return {
        .published = true,
        .snapshot = pendingSnapshot,
        .nextReadIndex = (ringDepth > 0) ? ((readIndex + 1) % ringDepth) : 0,
    };
  }
  return {
      .published = false,
      .snapshot = currentSnapshot,
      .nextReadIndex = readIndex,
  };
}

void GPUTextSystem::Init(ResourceManager &resources, const uint32_t maxCommands,
                         const uint32_t maxQuads) {
  if (m_initialized) {
    return;
  }

  m_maxCommands = maxCommands;
  m_maxQuads = maxQuads;
  m_lastQuadCount = 0;
  m_frameIndex = 0;

  m_layoutShader = resources.loadComputeShader(
      entt::hashed_string{"gpu_text_layout_cs"},
      "assets/shaders/text/text_layout.compute");
  if (m_layoutShader.id == 0) {
    LOG_ERROR("GPUTextSystem: failed to load text_layout.compute");
    return;
  }

  m_indirectArgsShader = resources.loadComputeShader(
      entt::hashed_string{"gpu_text_indirect_args_cs"},
      "assets/shaders/text/text_indirect_args.compute");
  if (m_indirectArgsShader.id == 0) {
    LOG_ERROR("GPUTextSystem: failed to load text_indirect_args.compute");
    return;
  }

  m_locCommandCount = rlGetLocationUniform(m_layoutShader.id, "uCommandCount");
  m_locGlyphMetricCount =
      rlGetLocationUniform(m_layoutShader.id, "uGlyphMetricCount");
  m_locMaxQuadCount = rlGetLocationUniform(m_layoutShader.id, "uMaxQuadCount");
  m_locStringMetaCount =
      rlGetLocationUniform(m_layoutShader.id, "uStringMetaCount");
  m_locTime = rlGetLocationUniform(m_layoutShader.id, "uTime");
  m_locAnimDuration = rlGetLocationUniform(m_layoutShader.id, "uAnimDuration");
  m_locIndirectMaxQuadCount =
      rlGetLocationUniform(m_indirectArgsShader.id, "uMaxQuadCount");

  m_cpuCommands.reserve(m_maxCommands);
  m_commandBuffer.Create(m_maxCommands * sizeof(components::GPUTextCommand));

  m_quadBuffer.Create(m_maxQuads * sizeof(components::GPUTextQuad), nullptr,
                      RL_DYNAMIC_DRAW);
  m_counterBuffer.Create(sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW);
  m_indirectBuffer.Create(sizeof(DrawArraysIndirectCommand), nullptr, RL_DYNAMIC_DRAW);

  const DrawArraysIndirectCommand zeroCmd = {6u, 0u, 0u, 0u};
  m_indirectBuffer.Update(&zeroCmd, sizeof(zeroCmd), 0);

  for (auto &slot : m_readbackRing) {
    if (slot.counterReadbackBufferId == 0) {
      utils::GPUUtils::GenBuffers(1, &slot.counterReadbackBufferId);
      if (slot.counterReadbackBufferId != 0) {
        utils::GPUUtils::BindBuffer(kGLCopyWriteBuffer, slot.counterReadbackBufferId);
        utils::GPUUtils::BufferData(kGLCopyWriteBuffer, sizeof(uint32_t), nullptr,
                                    kGLStreamRead);
        utils::GPUUtils::BindBuffer(kGLCopyWriteBuffer, 0);
      }
    }
    slot.fence = nullptr;
    slot.armed = false;
    slot.submittedFrame = 0;
  }
  m_ringWrite = 0;
  m_ringRead = 0;

  m_renderShader = NoMoreDay::utils::GPUUtils::LoadShaderLabeled(
      "assets/shaders/text/text_quad.vert", "assets/shaders/text/text_quad.frag");
  if (m_renderShader.id == 0) {
    LOG_ERROR("GPUTextSystem: failed to load text_quad shader");
    m_commandBuffer.Destroy();
    m_quadBuffer.Release();
    m_counterBuffer.Release();
    m_indirectBuffer.Release();
    return;
  }
  m_locRenderMvp = rlGetLocationUniform(m_renderShader.id, "mvp");
  m_locRenderAtlas = rlGetLocationUniform(m_renderShader.id, "uFontAtlas");

  const float vertices[] = {
      -0.5f, -0.5f, 0.0f, 0.0f, 0.5f,  -0.5f, 1.0f, 0.0f,
      0.5f,  0.5f,  1.0f, 1.0f, -0.5f, -0.5f, 0.0f, 0.0f,
      0.5f,  0.5f,  1.0f, 1.0f, -0.5f, 0.5f,  0.0f, 1.0f};
  m_vao = rlLoadVertexArray();
  NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
      m_vao, NoMoreDay::render::graph::ResourceKind::VertexArray,
      NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u, "TextQuadVAO");
  rlEnableVertexArray(m_vao);
  m_vbo = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
  NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
      m_vbo, NoMoreDay::render::graph::ResourceKind::VertexBuffer,
      NoMoreDay::render::graph::RenderOwnerTag::Unknown, sizeof(vertices),
      "TextQuadVBO");
  rlSetVertexAttribute(0, 2, RL_FLOAT, false, 4 * sizeof(float), 0);
  rlEnableVertexAttribute(0);
  rlSetVertexAttribute(1, 2, RL_FLOAT, false, 4 * sizeof(float),
                       2 * sizeof(float));
  rlEnableVertexAttribute(1);
  rlDisableVertexArray();

  m_initialized = true;
  LOG_INFO("GPUTextSystem: initialized maxCommands={} maxQuads={}", m_maxCommands,
           m_maxQuads);
}

void GPUTextSystem::Shutdown() {
  if (!m_initialized) {
    return;
  }

  m_commandBuffer.Destroy();
  m_glyphMetricsBuffer.Release();
  m_glyphIndexBuffer.Release();
  m_stringMetaBuffer.Release();
  m_quadBuffer.Release();
  m_counterBuffer.Release();
  m_indirectBuffer.Release();

  for (auto &slot : m_readbackRing) {
    if (slot.fence != nullptr) {
      utils::GPUUtils::DeleteSync(slot.fence);
      slot.fence = nullptr;
    }
    if (slot.counterReadbackBufferId != 0) {
      utils::GPUUtils::DeleteBuffers(1, &slot.counterReadbackBufferId);
      slot.counterReadbackBufferId = 0;
    }
    slot.armed = false;
    slot.submittedFrame = 0;
  }
  m_ringWrite = 0;
  m_ringRead = 0;
  m_frameIndex = 0;
  m_lastQuadCount = 0;

  m_cpuCommands.clear();
  m_cpuGlyphMetrics.clear();
  m_cpuGlyphIndices.clear();
  m_cpuStringMeta.clear();

  if (m_ownsAtlasTexture && m_atlasTexture.id != 0) {
    UnloadTexture(m_atlasTexture);
  }
  m_atlasTexture = {};
  m_ownsAtlasTexture = false;
  if (m_renderShader.id != 0) {
    UnloadShader(m_renderShader);
  }
  if (m_vao != 0) {
    NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
        m_vao, NoMoreDay::render::graph::ResourceKind::VertexArray);
    rlUnloadVertexArray(m_vao);
    m_vao = 0;
  }
  if (m_vbo != 0) {
    NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
        m_vbo, NoMoreDay::render::graph::ResourceKind::VertexBuffer);
    rlUnloadVertexBuffer(m_vbo);
    m_vbo = 0;
  }

  m_layoutShader = {};
  m_indirectArgsShader = {};
  m_renderShader = {};
  m_initialized = false;
}

void GPUTextSystem::BeginFrame() {
  m_cpuCommands.clear();
}

bool GPUTextSystem::EnqueueCommand(const components::GPUTextCommand &command) {
  if (!m_initialized) {
    return false;
  }
  if (m_cpuCommands.size() >= m_maxCommands) {
    return false;
  }
  m_cpuCommands.push_back(command);
  return true;
}

void GPUTextSystem::UploadGlyphMetrics(
    const std::vector<components::GPUGlyphMetrics> &metrics) {
  if (!m_initialized) {
    return;
  }

  m_cpuGlyphMetrics = metrics;
  if (m_cpuGlyphMetrics.empty()) {
    m_glyphMetricsBuffer.Release();
    return;
  }

  m_glyphMetricsBuffer.Create(
      m_cpuGlyphMetrics.size() * sizeof(components::GPUGlyphMetrics),
      m_cpuGlyphMetrics.data(), RL_DYNAMIC_DRAW);
}

void GPUTextSystem::UploadStringTable(
    const std::vector<uint32_t> &glyphIndices,
    const std::vector<GPUTextStringMeta> &meta) {
  if (!m_initialized) {
    return;
  }

  m_cpuGlyphIndices = glyphIndices;
  m_cpuStringMeta = meta;

  if (m_cpuGlyphIndices.empty()) {
    m_glyphIndexBuffer.Release();
  } else {
    m_glyphIndexBuffer.Create(m_cpuGlyphIndices.size() * sizeof(uint32_t),
                              m_cpuGlyphIndices.data(), RL_DYNAMIC_DRAW);
  }

  if (m_cpuStringMeta.empty()) {
    m_stringMetaBuffer.Release();
  } else {
    m_stringMetaBuffer.Create(m_cpuStringMeta.size() * sizeof(GPUTextStringMeta),
                              m_cpuStringMeta.data(), RL_DYNAMIC_DRAW);
  }
}

void GPUTextSystem::DispatchLayout(const float timeSeconds,
                                   const float animDurationSeconds) {
  if (!m_initialized || m_layoutShader.id == 0 || m_indirectArgsShader.id == 0) {
    return;
  }

  ++m_frameIndex;

  // 1. Poll delayed readback ring slot (non-blocking, timeout 0). Debug/test
  // builds refresh the snapshot every frame; production keeps the last
  // published snapshot (zero synchronous readback on the render path).
  if (m_readbackEnabledForTesting) {
    auto &readSlot = m_readbackRing[m_ringRead];
    const bool frameEligible =
        readSlot.armed && m_frameIndex > readSlot.submittedFrame;
    bool fenceSignaled = false;
    if (frameEligible && readSlot.fence != nullptr) {
      const uint32_t status =
          utils::GPUUtils::ClientWaitSync(readSlot.fence, 0, 0); // timeout 0
      fenceSignaled = (status == kGLAlreadySignaled ||
                       status == kGLConditionSatisfied);
    }

    uint32_t pendingSnapshot = m_lastQuadCount;
    if (fenceSignaled) {
      uint32_t readCount = 0;
      utils::GPUUtils::BindBuffer(kGLCopyReadBuffer,
                                  readSlot.counterReadbackBufferId);
      utils::GPUUtils::GetBufferSubData(kGLCopyReadBuffer, 0, sizeof(readCount),
                                        &readCount);
      utils::GPUUtils::BindBuffer(kGLCopyReadBuffer, 0);
      pendingSnapshot = (readCount > m_maxQuads) ? m_maxQuads : readCount;
    }

    const SnapshotPollOutcome pollOutcome = TryPublishReadySnapshot(
        readSlot.armed, frameEligible, fenceSignaled, m_ringRead, kRingDepth,
        pendingSnapshot, m_lastQuadCount);
    if (pollOutcome.published) {
      m_lastQuadCount = pollOutcome.snapshot;
      utils::GPUUtils::DeleteSync(readSlot.fence);
      readSlot.fence = nullptr;
      readSlot.armed = false;
      m_ringRead = pollOutcome.nextReadIndex;
    }
  }

  // 2. Early return if nothing to layout; clear GPU counter and generate zero indirect args.
  if (m_cpuCommands.empty() || m_cpuGlyphMetrics.empty() ||
      m_cpuStringMeta.empty() || m_glyphIndexBuffer.GetId() == 0 ||
      m_stringMetaBuffer.GetId() == 0) {
    const uint32_t zero = 0;
    m_counterBuffer.Update(&zero, sizeof(zero), 0);

    rlEnableShader(m_indirectArgsShader.id);
    if (m_locIndirectMaxQuadCount >= 0) {
      const int maxQuadCount = static_cast<int>(m_maxQuads);
      rlSetUniform(m_locIndirectMaxQuadCount, &maxQuadCount,
                   RL_SHADER_UNIFORM_INT, 1);
    }
    m_counterBuffer.BindBase(
        NoMoreDay::RenderConstants::TextIndirectArgsCS::COUNTER_BUFFER);
    m_indirectBuffer.BindBase(
        NoMoreDay::RenderConstants::TextIndirectArgsCS::COMMAND_BUFFER);
    {
      utils::GPUUtils::ScopedDebugGroup debugGroup("TextIndirectArgsZero");
      utils::GPUUtils::DispatchCompute(1, 1, 1);
    }
    rlDisableShader();
    NoMoreDay::utils::GPUUtils::BindBufferBase(
        NoMoreDay::RenderConstants::TextIndirectArgsCS::COUNTER_BUFFER, 0);
    NoMoreDay::utils::GPUUtils::BindBufferBase(
        NoMoreDay::RenderConstants::TextIndirectArgsCS::COMMAND_BUFFER, 0);
    NoMoreDay::utils::GPUUtils::MemoryBarrier(
        NoMoreDay::RenderConstants::Barrier::SSBO |
        NoMoreDay::RenderConstants::Barrier::Command);
    return;
  }

  auto *dst =
      static_cast<components::GPUTextCommand *>(m_commandBuffer.BeginWrite());
  memcpy(dst, m_cpuCommands.data(),
         m_cpuCommands.size() * sizeof(components::GPUTextCommand));
  m_commandBuffer.FlushRange(0,
                             m_cpuCommands.size() * sizeof(components::GPUTextCommand));

  const uint32_t zero = 0;
  m_counterBuffer.Update(&zero, sizeof(zero), 0);

  rlEnableShader(m_layoutShader.id);
  const int commandCount = static_cast<int>(m_cpuCommands.size());
  const int metricCount = static_cast<int>(m_cpuGlyphMetrics.size());
  const int maxQuadCount = static_cast<int>(m_maxQuads);
  const int stringMetaCount = static_cast<int>(m_cpuStringMeta.size());
  const float timeValue = timeSeconds;
  const float durationValue = animDurationSeconds;

  if (m_locCommandCount >= 0) {
    rlSetUniform(m_locCommandCount, &commandCount, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_locGlyphMetricCount >= 0) {
    rlSetUniform(m_locGlyphMetricCount, &metricCount, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_locMaxQuadCount >= 0) {
    rlSetUniform(m_locMaxQuadCount, &maxQuadCount, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_locStringMetaCount >= 0) {
    rlSetUniform(m_locStringMetaCount, &stringMetaCount, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_locTime >= 0) {
    rlSetUniform(m_locTime, &timeValue, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  if (m_locAnimDuration >= 0) {
    rlSetUniform(m_locAnimDuration, &durationValue, RL_SHADER_UNIFORM_FLOAT, 1);
  }

  using namespace NoMoreDay::RenderConstants;
  m_commandBuffer.BindBase(TextLayoutCS::COMMAND_BUFFER);
  m_glyphMetricsBuffer.BindBase(TextLayoutCS::GLYPH_METRICS);
  m_glyphIndexBuffer.BindBase(TextLayoutCS::GLYPH_INDICES);
  m_quadBuffer.BindBase(TextLayoutCS::QUAD_BUFFER);
  m_counterBuffer.BindBase(TextLayoutCS::COUNTER_BUFFER);
  m_stringMetaBuffer.BindBase(TextLayoutCS::STRING_META);

  const uint32_t groups =
      static_cast<uint32_t>((m_cpuCommands.size() + 255u) / 256u);
  {
    utils::GPUUtils::ScopedDebugGroup debugGroup("TextLayout");
    utils::GPUUtils::DispatchCompute(groups, 1, 1);
  }
  rlDisableShader();

  // SSBO barrier: ensure layout compute writes to counterBuffer are visible to indirect args compute
  NoMoreDay::utils::GPUUtils::MemoryBarrier(NoMoreDay::RenderConstants::Barrier::SSBO);

  // 3. Phase-local indirect args compute pass
  rlEnableShader(m_indirectArgsShader.id);
  if (m_locIndirectMaxQuadCount >= 0) {
    rlSetUniform(m_locIndirectMaxQuadCount, &maxQuadCount,
                 RL_SHADER_UNIFORM_INT, 1);
  }
  m_counterBuffer.BindBase(TextIndirectArgsCS::COUNTER_BUFFER);
  m_indirectBuffer.BindBase(TextIndirectArgsCS::COMMAND_BUFFER);
  {
    utils::GPUUtils::ScopedDebugGroup debugGroup("TextIndirectArgs");
    utils::GPUUtils::DispatchCompute(1, 1, 1);
  }
  rlDisableShader();

  // Clean up phase-local bindings
  NoMoreDay::utils::GPUUtils::BindBufferBase(
      TextIndirectArgsCS::COUNTER_BUFFER, 0);
  NoMoreDay::utils::GPUUtils::BindBufferBase(
      TextIndirectArgsCS::COMMAND_BUFFER, 0);

  // Memory barrier: ensure indirect command is visible for drawing
  NoMoreDay::utils::GPUUtils::MemoryBarrier(
      NoMoreDay::RenderConstants::Barrier::SSBO |
      NoMoreDay::RenderConstants::Barrier::Command);

  // 4. Submit non-blocking debug readback copy to write slot (debug/test only)
  if (m_readbackEnabledForTesting) {
    auto &writeSlot = m_readbackRing[m_ringWrite];
    if (CanSubmitReadbackCopy(writeSlot.armed)) {
      const uint32_t counterSrc = m_counterBuffer.GetId();
      if (counterSrc != 0 && writeSlot.counterReadbackBufferId != 0) {
        utils::GPUUtils::BindBuffer(kGLCopyReadBuffer, counterSrc);
        utils::GPUUtils::BindBuffer(kGLCopyWriteBuffer,
                                    writeSlot.counterReadbackBufferId);
        utils::GPUUtils::CopyBufferSubData(kGLCopyReadBuffer, kGLCopyWriteBuffer,
                                           0, 0, sizeof(uint32_t));
        utils::GPUUtils::BindBuffer(kGLCopyReadBuffer, 0);
        utils::GPUUtils::BindBuffer(kGLCopyWriteBuffer, 0);

        void *fence =
            utils::GPUUtils::FenceSync(kGLSyncGpuCommandsComplete, 0);
        if (fence != nullptr) {
          writeSlot.fence = fence;
          writeSlot.armed = true;
          writeSlot.submittedFrame = m_frameIndex;
          m_ringWrite = (m_ringWrite + 1) % kRingDepth;
        }
      }
    }
  }

  m_commandBuffer.Lock();
}

void GPUTextSystem::Render(const Matrix &viewProj) const {
  if (!m_initialized || m_renderShader.id == 0 || m_atlasTexture.id == 0 ||
      m_vao == 0 || m_indirectBuffer.GetId() == 0) {
    return;
  }

  rlEnableShader(m_renderShader.id);
  if (m_locRenderMvp >= 0) {
    rlSetUniformMatrix(m_locRenderMvp, viewProj);
  }
  if (m_locRenderAtlas >= 0) {
    const int atlasUnit =
        static_cast<int>(NoMoreDay::RenderConstants::TextureUnit::TEX_FONT_ATLAS);
    rlSetUniform(m_locRenderAtlas, &atlasUnit, RL_SHADER_UNIFORM_INT, 1);
  }

  m_quadBuffer.BindBase(NoMoreDay::RenderConstants::TextPassBinding::QUAD_SSBO);
  rlActiveTextureSlot(static_cast<int>(
      NoMoreDay::RenderConstants::TextureUnit::TEX_FONT_ATLAS));
  rlEnableTexture(m_atlasTexture.id);
  m_indirectBuffer.Bind(NoMoreDay::RenderConstants::GL::DRAW_INDIRECT_BUFFER);

  rlEnableVertexArray(m_vao);
  NoMoreDay::utils::GPUUtils::DrawArraysIndirect(GL_TRIANGLES, 0);
  rlDisableVertexArray();

  NoMoreDay::utils::GPUUtils::BindBuffer(
      NoMoreDay::RenderConstants::GL::DRAW_INDIRECT_BUFFER, 0);
  rlActiveTextureSlot(0);
  rlDisableTexture();
  rlDisableShader();
}

void GPUTextSystem::SetAtlasTexture(const Texture2D atlas, const bool takeOwnership) {
  if (m_ownsAtlasTexture && m_atlasTexture.id != 0 && m_atlasTexture.id != atlas.id) {
    UnloadTexture(m_atlasTexture);
  }
  m_atlasTexture = atlas;
  m_ownsAtlasTexture = takeOwnership;
}

} // namespace NoMoreDay::render
