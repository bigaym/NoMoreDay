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

} // namespace

void GPUTextSystem::Init(ResourceManager &resources, const uint32_t maxCommands,
                         const uint32_t maxQuads) {
  if (m_initialized) {
    return;
  }

  m_maxCommands = maxCommands;
  m_maxQuads = maxQuads;
  m_lastQuadCount = 0;

  m_layoutShader = resources.loadComputeShader(
      entt::hashed_string{"gpu_text_layout_cs"},
      "assets/shaders/text/text_layout.compute");
  if (m_layoutShader.id == 0) {
    LOG_ERROR("GPUTextSystem: failed to load text_layout.compute");
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

  m_cpuCommands.reserve(m_maxCommands);
  m_commandBuffer.Create(m_maxCommands * sizeof(components::GPUTextCommand));

  m_quadBuffer.Create(m_maxQuads * sizeof(components::GPUTextQuad), nullptr,
                      RL_DYNAMIC_DRAW);
  m_counterBuffer.Create(sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW);
  m_indirectBuffer.Create(sizeof(DrawArraysIndirectCommand), nullptr, RL_DYNAMIC_DRAW);

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
  m_renderShader = {};
  m_initialized = false;
}

void GPUTextSystem::BeginFrame() {
  m_cpuCommands.clear();
  m_lastQuadCount = 0;
  const DrawArraysIndirectCommand zeroCmd = {6u, 0u, 0u, 0u};
  m_indirectBuffer.Update(&zeroCmd, sizeof(zeroCmd), 0);
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
  if (!m_initialized || m_layoutShader.id == 0 || m_cpuCommands.empty() ||
      m_cpuGlyphMetrics.empty() || m_cpuStringMeta.empty() ||
      m_glyphIndexBuffer.GetId() == 0 || m_stringMetaBuffer.GetId() == 0) {
    const DrawArraysIndirectCommand zeroCmd = {6u, 0u, 0u, 0u};
    m_indirectBuffer.Update(&zeroCmd, sizeof(zeroCmd), 0);
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

  uint32_t outCount = 0;
  m_counterBuffer.Read(&outCount, sizeof(outCount), 0);
  m_lastQuadCount = (outCount > m_maxQuads) ? m_maxQuads : outCount;
  const DrawArraysIndirectCommand drawCmd = {6u, m_lastQuadCount, 0u, 0u};
  m_indirectBuffer.Update(&drawCmd, sizeof(drawCmd), 0);

  m_commandBuffer.Lock();
}

void GPUTextSystem::Render(const Matrix &viewProj) const {
  if (!m_initialized || m_renderShader.id == 0 || m_atlasTexture.id == 0 ||
      m_lastQuadCount == 0 || m_vao == 0 || m_indirectBuffer.GetId() == 0) {
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
