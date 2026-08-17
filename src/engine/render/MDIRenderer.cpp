#include "engine/render/MDIRenderer.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/resource/TextureArrayManager.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "raymath.h"
#include <iostream>
#include <vector>

namespace NoMoreDay::render {

using namespace NoMoreDay::RenderConstants;

MDIRenderer *MDIRenderer::s_instance = nullptr;

MDIRenderer &MDIRenderer::Get() {
  if (!s_instance) {
    LOG_WARN("MDIRenderer::Get() called without initialization. "
             "Consider using RenderContext injection.");
    static MDIRenderer fallback;
    return fallback;
  }
  return *s_instance;
}

MDIRenderer::~MDIRenderer() {
  Shutdown();
  if (s_instance == this) {
    s_instance = nullptr;
  }
}

void MDIRenderer::Init(ResourceManager &rm, uint32_t maxEntities) {
  s_instance = this;
  if (m_quadVAO != 0)
    return; // Already initialized

  m_maxEntities = maxEntities;
  LOG_INFO("Initializing MDI Renderer (Max Entities: {})...", maxEntities);

  if (!utils::GPUUtils::IsInitialized()) {
    LOG_ERROR("MDIRenderer: GPUUtils must be initialized before MDIRenderer!");
    return;
  }

  // 1. Load Shaders
  m_renderShader =
      rm.loadShader("mdi_render"_hash, "assets/shaders/entity_mdi.vert",
                    "assets/shaders/entity_mdi.frag");

  m_cullShader =
      rm.loadComputeShader("mdi_cull"_hash, "assets/shaders/cull.compute");

  m_scatterShader =
      rm.loadComputeShader("mdi_scatter"_hash, "assets/shaders/scatter_stats.compute");

  m_packShader =
      rm.loadComputeShader("mdi_pack"_hash, "assets/shaders/instance_pack.compute");

  if (m_renderShader.id == 0 || m_cullShader.id == 0 || m_scatterShader.id == 0 ||
      m_packShader.id == 0) {
    LOG_ERROR(
        "MDIRenderer: shader initialization failed (render={}, cull={}, scatter={}, pack={})",
        m_renderShader.id, m_cullShader.id, m_scatterShader.id, m_packShader.id);
    return;
  }

  // 2. Create Buffers
  m_visibleBuffer.Create(maxEntities * sizeof(uint32_t), 3);
  m_commandBuffer.Create(sizeof(DrawArraysIndirectCommand), 3);
  m_statsBuffer.Create(maxEntities * sizeof(components::GPUVisualStats), 3);
  m_statsStaging.Create(maxEntities * sizeof(StatUpdateCmd), 3);
  m_packedBuffer.Create(maxEntities * sizeof(components::GPUPackedEntityInstance), 3);

  // 3. Create Quad VAO
  // Reordered for GL_TRIANGLE_STRIP: TL, BL, TR, BR
  float quadVertices[] = {
      -0.5f,  0.5f,  0.0f, 1.0f, // 0: TL
      -0.5f, -0.5f,  0.0f, 0.0f, // 1: BL
       0.5f,  0.5f,  1.0f, 1.0f, // 2: TR
       0.5f, -0.5f,  1.0f, 0.0f, // 3: BR
  };
  unsigned int quadIndices[] = {0, 1, 2, 1, 3, 2}; // Standard strip indices (unused by DrawArraysIndirect)

  m_quadVAO = rlLoadVertexArray();
  NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
      m_quadVAO, NoMoreDay::render::graph::ResourceKind::VertexArray,
      NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u, "MDIQuadVAO");
  rlEnableVertexArray(m_quadVAO);
  m_quadVBO = rlLoadVertexBuffer(quadVertices, sizeof(quadVertices), false);
  NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
      m_quadVBO, NoMoreDay::render::graph::ResourceKind::VertexBuffer,
      NoMoreDay::render::graph::RenderOwnerTag::Unknown, sizeof(quadVertices),
      "MDIQuadVBO");
  rlSetVertexAttribute(0, 2, RL_FLOAT, false, 4 * sizeof(float), 0);
  rlSetVertexAttribute(1, 2, RL_FLOAT, false, 4 * sizeof(float),
                       (int)(2 * sizeof(float)));
  rlEnableVertexAttribute(0);
  rlEnableVertexAttribute(1);

  // Vertex array element buffer
  // Note: rlgl manages ID for us
  rlLoadVertexBufferElement(quadIndices, sizeof(quadIndices), false);
  rlDisableVertexArray();

  LOG_INFO("MDIRenderer: Initialized successfully.");
}

void MDIRenderer::Shutdown() {
  if (s_instance == this) {
    s_instance = nullptr;
  }

  if (m_quadVBO != 0) {
    NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
        m_quadVBO, NoMoreDay::render::graph::ResourceKind::VertexBuffer);
    rlUnloadVertexBuffer(m_quadVBO);
    m_quadVBO = 0;
  }
  if (m_quadVAO != 0) {
    NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
        m_quadVAO, NoMoreDay::render::graph::ResourceKind::VertexArray);
    rlUnloadVertexArray(m_quadVAO);
    m_quadVAO = 0;
  }

  m_visibleBuffer.Destroy();
  m_commandBuffer.Destroy();
  m_statsBuffer.Destroy();
  m_statsStaging.Destroy();
  m_packedBuffer.Destroy();
}

void MDIRenderer::Update(ResourceManager &rm, const PersistentBuffer &entities,
                         float alpha) {
  // Convenience wrapper
}

void MDIRenderer::UpdateStat(uint32_t entityIdx, const components::GPUVisualStats& stats) {
    if (entityIdx >= m_maxEntities) return;
    StatUpdateCmd cmd;
    cmd.index = entityIdx;
    cmd.stats = stats;
    m_pendingUpdates.push_back(cmd);
}

void MDIRenderer::FlushStatsUpdates(ResourceManager& rm) {
    if (m_pendingUpdates.empty()) return;
    
    uint32_t count = (uint32_t)m_pendingUpdates.size();
    void* ptr = m_statsStaging.BeginWrite();
    if (ptr) {
        memcpy(ptr, m_pendingUpdates.data(), count * sizeof(StatUpdateCmd));
    }
    m_statsStaging.Flush();
    
    rlEnableShader(m_scatterShader.id);
    int locCount = rlGetLocationUniform(m_scatterShader.id, "updateCount");
    if (locCount != -1) {
        rlSetUniform(locCount, &count, RL_SHADER_UNIFORM_INT, 1);
    }
    
    m_statsStaging.BindBase(StatsScatterCS::UPDATES);
    m_statsBuffer.BindBase(StatsScatterCS::MAIN_STATS);
    
    utils::GPUUtils::ScopedDebugGroup debugGroup("StatsScatter");
    utils::GPUUtils::DispatchCompute((count + 63) / 64, 1, 1);
    rlDisableShader();
    
    m_statsStaging.Lock();
    m_pendingUpdates.clear();
}

void MDIRenderer::ResetCommand() {
  void *ptr = m_commandBuffer.BeginWrite();
  if (ptr) {
    DrawArraysIndirectCommand cmd = {4, 0, 0, 0}; // 4 vertices for Quad
    memcpy(ptr, &cmd, sizeof(cmd));
  }
  m_commandBuffer.Flush();
  // LOCK REMOVED: Must not advance slot here, as Cull/Render usage follows
}

void MDIRenderer::UpdateStats(
    const std::vector<components::GPUVisualStats> &stats, int count) {
  if (count <= 0)
    return;
  void *ptr = m_statsBuffer.BeginWrite();
  if (ptr) {
    memcpy(ptr, stats.data(), count * sizeof(components::GPUVisualStats));
  }
  m_statsBuffer.Flush();
  // LOCK removed here. We will lock at the end of Render() to ensure GPU is done.
}

void MDIRenderer::UpdateStatsNoFlush(
    const std::vector<components::GPUVisualStats> &stats, int count) {
  if (count <= 0)
    return;
  void *ptr = m_statsBuffer.BeginWrite();
  if (ptr) {
    memcpy(ptr, stats.data(), count * sizeof(components::GPUVisualStats));
  }
}

void MDIRenderer::FlushStatsRange(size_t count) {
  if (count <= 0) return;
  m_statsBuffer.FlushRange(0, count * sizeof(components::GPUVisualStats));
}

void MDIRenderer::Cull(ResourceManager &rm, const PersistentBuffer &entities, Vector4 viewBounds) {
  NoMoreDay::utils::ScopedTimer timer("MDI Cull", 3000); 
  using namespace NoMoreDay::RenderConstants;

  if (m_cullShader.id == 0)
    return;

  // Reset Command Counter manually if needed or in shader
  // Assuming cull.compute handle this
  rlEnableShader(m_cullShader.id);

  // Set maxEntities for boundary check
  int locMaxEntities = rlGetLocationUniform(m_cullShader.id, "maxEntities");
  if (locMaxEntities != -1) {
      int maxEnt = (int)m_maxEntities;
      rlSetUniform(locMaxEntities, &maxEnt, RL_SHADER_UNIFORM_INT, 1);
  }

  // Set view bounds
  int locBounds = rlGetLocationUniform(m_cullShader.id, "viewBounds");
  if (locBounds != -1) {
    float bounds[4] = {viewBounds.x, viewBounds.y, viewBounds.z, viewBounds.w};
    rlSetUniform(locBounds, bounds, RL_SHADER_UNIFORM_VEC4, 1);
  }

  // Bind Buffers for Cull
  // Bind Entity Buffer (Input) - Use Previous slot (from Update)
  entities.BindPreviousNoSync(static_cast<uint32_t>(Binding::SSBO_ENTITY_DATA));
  
  // Bind Output Buffers - Use Current slot (Base)
  m_visibleBuffer.BindBase(static_cast<uint32_t>(Binding::SSBO_VISIBLE_ID));
  m_commandBuffer.BindBase(static_cast<uint32_t>(Binding::SSBO_COMMAND));

  // Dispatch - No barrier here, Pack() will handle barrier as the Consumer
  uint32_t dispatchCount = (m_maxActiveEntities > 0) ? m_maxActiveEntities : m_maxEntities;
  utils::GPUUtils::ScopedDebugGroup debugGroup("MDICull");
  constexpr uint32_t kCullLocalSize = 256;
  uint32_t groups = (dispatchCount + kCullLocalSize - 1) / kCullLocalSize;
  utils::GPUUtils::DispatchComputeNoBarrier(groups, 1, 1);

  rlDisableShader();

  // Execute Instance Pack compute pass (AD-7 32B compact packing)
  Pack(rm, entities);
}

void MDIRenderer::Pack(ResourceManager &rm, const PersistentBuffer &entities) {
  if (m_packShader.id == 0)
    return;

  NoMoreDay::utils::ScopedTimer timer("MDI Pack", 3000);
  using namespace NoMoreDay::RenderConstants;

  // Ensure cull output (visibleIndices and DrawCommand instanceCount) is visible to compute
  utils::GPUUtils::MemoryBarrier(Barrier::SSBO | Barrier::Command);

  rlEnableShader(m_packShader.id);

  int locMaxEntities = rlGetLocationUniform(m_packShader.id, "maxEntities");
  if (locMaxEntities != -1) {
    int maxEnt = static_cast<int>(m_maxEntities);
    rlSetUniform(locMaxEntities, &maxEnt, RL_SHADER_UNIFORM_INT, 1);
  }

  // Bind inputs. DrawCommand (instanceCount guard) is read on physical slot 4
  // (pass-local alias of SSBO_LABEL_INSTANCE, see InstancePackCS::DRAW_COMMAND);
  // the label pass rebinds slot 4 before UI draws, so the alias is safe.
  entities.BindPreviousNoSync(InstancePackCS::ENTITY_DATA);
  m_visibleBuffer.BindBase(InstancePackCS::VISIBLE_ID);
  m_commandBuffer.BindBase(InstancePackCS::DRAW_COMMAND);
  m_statsBuffer.BindBase(InstancePackCS::VISUAL_STATS);

  // Bind output: packed 32B stream on physical slot 2 (pass-local alias of
  // SSBO_COMMAND), matching the read side in entity_mdi.vert.
  m_packedBuffer.BindBase(InstancePackCS::PACKED_INSTANCES);

  uint32_t dispatchCount = (m_maxActiveEntities > 0) ? m_maxActiveEntities : m_maxEntities;
  utils::GPUUtils::ScopedDebugGroup debugGroup("MDIPack");
  constexpr uint32_t kCullLocalSize = 256;
  uint32_t groups = (dispatchCount + kCullLocalSize - 1) / kCullLocalSize;
  utils::GPUUtils::DispatchCompute(groups, 1, 1);

  rlDisableShader();
}

void MDIRenderer::Render(ResourceManager &rm, const PersistentBuffer &entities,
                         float renderAlpha) {
  NoMoreDay::utils::ScopedTimer timer("MDI Render", 50);
  using namespace NoMoreDay::RenderConstants;

  if (m_renderShader.id == 0)
    return;

  // 1. Flush Raylib batch
  rlDrawRenderBatchActive();

  // 2. MVP
  Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());

  rlEnableShader(m_renderShader.id);

  // 3. Bind Texture Array
  unsigned int texArray = rm.getEntityTextureArray();
  if (texArray != 0) {
    int locTex = rlGetLocationUniform(m_renderShader.id, "entityTextures");
    if (locTex != -1) {
      utils::GPUUtils::ActiveTexture(TextureUnit::TEX_ENTITY_ARRAY);
      utils::GPUUtils::BindTexture(GL::TEXTURE_2D_ARRAY, texArray);
      int unit = static_cast<int>(TextureUnit::TEX_ENTITY_ARRAY);
      rlSetUniform(locTex, &unit, RL_SHADER_UNIFORM_INT, 1);
    }
  }

  // 4. Set Uniforms
  int locVP = rlGetLocationUniform(m_renderShader.id, "viewProj");
  if (locVP != -1)
    rlSetUniformMatrix(locVP, mvp);

  int locInterp =
      rlGetLocationUniform(m_renderShader.id, "interpolationFactor");
  if (locInterp != -1)
    rlSetUniform(locInterp, &renderAlpha, RL_SHADER_UNIFORM_FLOAT, 1);

  int locTime = rlGetLocationUniform(m_renderShader.id, "uTime");
  if (locTime != -1) {
      float time = (float)GetTime();
      rlSetUniform(locTime, &time, RL_SHADER_UNIFORM_FLOAT, 1);
  }

  int materialQualityLevel = 0;
  int normalLightingEnabled = 0;
  int specularEnabled = 0;
  float shadowFactor = 1.0f;
  if (core::QualityTierManager::Get().IsInitialized()) {
    const auto tier = core::QualityTierManager::Get().GetTier();
    const auto &config = core::QualityTierManager::Get().GetConfig();
    if (tier == core::QualityTier::Low || tier == core::QualityTier::Medium) {
      materialQualityLevel = 0;
      normalLightingEnabled = 0;
      specularEnabled = 0;
    } else if (tier == core::QualityTier::High) {
      materialQualityLevel = std::max(1, static_cast<int>(config.materialQualityLevel));
      normalLightingEnabled = 1;
      specularEnabled = 0;
    } else {
      materialQualityLevel = std::max(3, static_cast<int>(config.materialQualityLevel));
      normalLightingEnabled = config.normalLightingEnabled ? 1 : 0;
      specularEnabled = config.specularEnabled ? 1 : 0;
    }
    shadowFactor = config.shadowEnabled ? 0.95f : 1.0f;
  }

  const int locMaterialQuality =
      rlGetLocationUniform(m_renderShader.id, "uMaterialQualityLevel");
  if (locMaterialQuality != -1) {
    rlSetUniform(locMaterialQuality, &materialQualityLevel, RL_SHADER_UNIFORM_INT,
                 1);
  }
  const int locNormalLighting =
      rlGetLocationUniform(m_renderShader.id, "uNormalLightingEnabled");
  if (locNormalLighting != -1) {
    rlSetUniform(locNormalLighting, &normalLightingEnabled, RL_SHADER_UNIFORM_INT,
                 1);
  }
  const int locSpecular = rlGetLocationUniform(m_renderShader.id, "uSpecularEnabled");
  if (locSpecular != -1) {
    rlSetUniform(locSpecular, &specularEnabled, RL_SHADER_UNIFORM_INT, 1);
  }
  const int locShadowFactor = rlGetLocationUniform(m_renderShader.id, "uShadowFactor");
  if (locShadowFactor != -1) {
    rlSetUniform(locShadowFactor, &shadowFactor, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  const int locLinearPipeline =
      rlGetLocationUniform(m_renderShader.id, "uLinearPipeline");
  if (locLinearPipeline != -1) {
    const int linearPipeline =
        core::QualityTierManager::Get().IsLinearPipelineEnabled() ? 1 : 0;
    rlSetUniform(locLinearPipeline, &linearPipeline, RL_SHADER_UNIFORM_INT, 1);
  }
  const int locMaterialCount =
      rlGetLocationUniform(m_renderShader.id, "uMaterialCount");
  if (locMaterialCount != -1) {
    const int materialCount = MaterialManager::Get().GetMaterialCount();
    rlSetUniform(locMaterialCount, &materialCount, RL_SHADER_UNIFORM_INT, 1);
  }
  const int normalUnit = static_cast<int>(TextureUnit::TEX_MATERIAL_NORMAL_ARRAY);
  const int locNormalArray =
      rlGetLocationUniform(m_renderShader.id, "materialNormalArray");
  if (locNormalArray != -1) {
    rlSetUniform(locNormalArray, &normalUnit, RL_SHADER_UNIFORM_INT, 1);
  }
  const int maskUnit = static_cast<int>(TextureUnit::TEX_MATERIAL_MASK_ARRAY);
  const int locMaskArray =
      rlGetLocationUniform(m_renderShader.id, "materialMaskArray");
  if (locMaskArray != -1) {
    rlSetUniform(locMaskArray, &maskUnit, RL_SHADER_UNIFORM_INT, 1);
  }
  const int detailUnit = static_cast<int>(TextureUnit::TEX_MATERIAL_DETAIL_ARRAY);
  const int locDetailArray =
      rlGetLocationUniform(m_renderShader.id, "materialDetailArray");
  if (locDetailArray != -1) {
    rlSetUniform(locDetailArray, &detailUnit, RL_SHADER_UNIFORM_INT, 1);
  }

  // 5. EXPLICITLY BIND PASS-LOCAL SSBOs
  // Entity-MDI vertex shader consumes 32B packed instances at local slot 2
  m_packedBuffer.BindBase(static_cast<uint32_t>(Binding::SSBO_COMMAND));

  // Bind Command Buffer for Indirect Draw (Current)
  m_commandBuffer.Bind(GL::DRAW_INDIRECT_BUFFER); // Defaults to Current
  MaterialManager::Get().BindSSBO(Binding::SSBO_MATERIAL_DATA);

  bool boundMaterialArrays = false;
  if (materialQualityLevel > 0 && TextureArrayManager::Get().IsInitialized()) {
    TextureArrayManager::Get().Bind(
        TextureArraySemantic::Normal,
        static_cast<uint32_t>(TextureUnit::TEX_MATERIAL_NORMAL_ARRAY));
    TextureArrayManager::Get().Bind(
        TextureArraySemantic::Mask,
        static_cast<uint32_t>(TextureUnit::TEX_MATERIAL_MASK_ARRAY));
    TextureArrayManager::Get().Bind(
        TextureArraySemantic::Detail,
        static_cast<uint32_t>(TextureUnit::TEX_MATERIAL_DETAIL_ARRAY));
    boundMaterialArrays = true;
  }

  // 6. Execute Indirect Draw
  rlEnableVertexArray(m_quadVAO);

  // Ensure all previous commands/writes are visible to Indirect Draw
  // Consumer Responsibility: Sync Command, SSBO, and Buffer updates
  utils::GPUUtils::MemoryBarrier(Barrier::Command | Barrier::SSBO |
                                 Barrier::Buffer);

  // Use TRIANGLE_STRIP for 4-vertex Quad (BL, BR, TR, TL)
  utils::GPUUtils::DrawArraysIndirect(GL::TRIANGLE_STRIP, m_commandBuffer.GetCurrentSlotOffset());
  rlDisableVertexArray();

  rlDisableShader();
  MaterialManager::Get().UnbindSSBO(Binding::SSBO_MATERIAL_DATA);

  // RESTORE GLOBAL SSBO SLOT 2 (SSBO_COMMAND)
  m_commandBuffer.BindBase(static_cast<uint32_t>(Binding::SSBO_COMMAND));

  if (boundMaterialArrays) {
    TextureArrayManager::Get().Unbind(
        static_cast<uint32_t>(TextureUnit::TEX_MATERIAL_NORMAL_ARRAY));
    TextureArrayManager::Get().Unbind(
        static_cast<uint32_t>(TextureUnit::TEX_MATERIAL_MASK_ARRAY));
    TextureArrayManager::Get().Unbind(
        static_cast<uint32_t>(TextureUnit::TEX_MATERIAL_DETAIL_ARRAY));
  }

  // LOCK BUFFERS at end of frame usage
  m_commandBuffer.Lock();
  m_visibleBuffer.Lock();
  m_statsBuffer.Lock();
  m_packedBuffer.Lock();
}

} // namespace NoMoreDay::render
