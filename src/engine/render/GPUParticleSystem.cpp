#include "engine/render/GPUParticleSystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/particle/ForceFieldManager.hpp"
#include "engine/render/particle/ParticleTextureManager.hpp"
#include "game/components/Common.hpp"

// RenderConstants::ParticleCS defines binding point semantics
#include "engine/render/RenderConstants.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <rlgl.h>
#include <sstream>

// OpenGL constants
#ifndef GL_TRIANGLES
#define GL_TRIANGLES 0x0004
#endif
#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif

namespace NoMoreDay::systems {
using namespace NoMoreDay::RenderConstants;

namespace {

constexpr int kMaxShaderIncludeDepth = 8;

bool ReadTextFile(const std::filesystem::path &path, std::string &out) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return false;
  }
  std::stringstream ss;
  ss << file.rdbuf();
  out = ss.str();
  return true;
}

std::string ResolveShaderIncludes(const std::filesystem::path &path, int depth) {
  if (depth > kMaxShaderIncludeDepth) {
    LOG_ERROR("GPUParticleSystem: shader include depth exceeded at {}",
              path.string());
    return {};
  }

  std::string source;
  if (!ReadTextFile(path, source)) {
    LOG_ERROR("GPUParticleSystem: failed to read shader file {}", path.string());
    return {};
  }

  std::stringstream input(source);
  std::ostringstream output;
  std::string line;
  while (std::getline(input, line)) {
    const std::string includeTag = "#include \"";
    const size_t start = line.find(includeTag);
    if (start == std::string::npos) {
      output << line << '\n';
      continue;
    }

    const size_t pathStart = start + includeTag.size();
    const size_t endQuote = line.find('\"', pathStart);
    if (endQuote == std::string::npos) {
      output << line << '\n';
      continue;
    }

    const std::string relative = line.substr(pathStart, endQuote - pathStart);
    const std::filesystem::path includePath = path.parent_path() / relative;
    const std::string included = ResolveShaderIncludes(includePath, depth + 1);
    output << included << '\n';
  }

  return output.str();
}

Shader LoadShaderWithIncludes(const std::filesystem::path &vertexPath,
                              const std::filesystem::path &fragmentPath) {
  Shader shader = {};
  const std::string vertexSrc = ResolveShaderIncludes(vertexPath, 0);
  const std::string fragmentSrc = ResolveShaderIncludes(fragmentPath, 0);
  if (vertexSrc.empty() || fragmentSrc.empty()) {
    return shader;
  }

  unsigned int vsId = rlCompileShader(vertexSrc.c_str(), RL_VERTEX_SHADER);
  unsigned int fsId = rlCompileShader(fragmentSrc.c_str(), RL_FRAGMENT_SHADER);
  if (vsId == 0 || fsId == 0) {
    LOG_ERROR("GPUParticleSystem: shader compile failed for {} / {}",
              vertexPath.string(), fragmentPath.string());
    return shader;
  }

  const unsigned int programId = rlLoadShaderProgram(vsId, fsId);
  if (programId == 0) {
    LOG_ERROR("GPUParticleSystem: shader link failed for {} / {}",
              vertexPath.string(), fragmentPath.string());
    return shader;
  }

  shader.id = programId;
  shader.locs = static_cast<int *>(RL_CALLOC(RL_MAX_SHADER_LOCATIONS, sizeof(int)));
  for (int i = 0; i < RL_MAX_SHADER_LOCATIONS; ++i) {
    shader.locs[i] = -1;
  }
  return shader;
}

} // namespace


GPUParticleSystem &GPUParticleSystem::Get() {
  static GPUParticleSystem instance;
  return instance;
}

void GPUParticleSystem::Init(int maxParticles) {
  if (m_initialized) {
    LOG_WARN("GPUParticleSystem: Already initialized!");
    return;
  }

  m_maxParticles = maxParticles;
  LOG_INFO("Initializing GPUParticleSystem with {} max particles...",
           maxParticles);

  if (!utils::GPUUtils::IsInitialized()) {
    LOG_ERROR("GPUParticleSystem: GPUUtils must be initialized before "
              "GPUParticleSystem!");
    return;
  }

  // Create resources
  LoadShaders();
  CreateBuffers();
  CreateQuadVAO();
  render::ParticleTextureManager::Get().Init(
      64, NoMoreDay::Constants::GPU::TEXTURE_LAYER_SIZE);
  render::ParticleTextureManager::Get().LoadLayer(
      "assets/shaders/textures/particles/fire_01.png");
  render::ParticleTextureManager::Get().LoadLayer(
      "assets/shaders/textures/particles/smoke_01.png");
  render::ParticleTextureManager::Get().LoadLayer(
      "assets/shaders/textures/particles/spark_01.png");
  render::ForceFieldManager::Get().Init(
      NoMoreDay::Constants::GPU::MAX_FORCE_FIELDS);

  using namespace NoMoreDay::RenderConstants;
  m_initialized = true;

  LOG_INFO("GPUParticleSystem: Initialized successfully with Indirect Drawing "
           "support.");
}

void GPUParticleSystem::Shutdown() {
  if (!m_initialized)
    return;

  LOG_INFO("GPUParticleSystem: Shutting down...");

  // Clean up shaders
  if (m_computeShader.id != 0) {
    rlUnloadShaderProgram(m_computeShader.id);
    m_computeShader.id = 0;
  }
  if (m_renderShader.id != 0) {
    UnloadShader(m_renderShader);
  }
  if (m_emitShader.id != 0) {
    rlUnloadShaderProgram(m_emitShader.id);
    m_emitShader.id = 0;
  }
  if (m_subEmitShader.id != 0) {
    rlUnloadShaderProgram(m_subEmitShader.id);
    m_subEmitShader.id = 0;
  }
  if (m_finalizeShader.id != 0) {
    rlUnloadShaderProgram(m_finalizeShader.id);
    m_finalizeShader.id = 0;
  }

  // VAO/VBO cleanup
  if (m_quadVBO != 0) {
    rlUnloadVertexBuffer(m_quadVBO);
    m_quadVBO = 0;
  }
  if (m_quadVAO != 0) {
    rlUnloadVertexArray(m_quadVAO);
    m_quadVAO = 0;
  }

  m_emissionBuffer.Destroy();
  m_subEmissionBuffer.Release();
  m_subEmitCountBuffer.Destroy();
  render::ParticleTextureManager::Get().Shutdown();
  render::ForceFieldManager::Get().Shutdown();

  m_initialized = false;
}

void GPUParticleSystem::HardResetGPU() {
    // Deprecated: Logical Clear is sufficient and faster.
    Clear();
}

void GPUParticleSystem::Clear() {
  if (!m_initialized)
    return;

  // 1. Reset CPU state to stop Dispatch in Update()
  m_lastKnownAliveCount = 0;
  m_currentParticleCount = 0;
  m_emitHead = 0;
  
  // 2. Reset ALL slots in the Triple Buffer chain
  const int bufferCount = m_atomicBuffer.GetBufferCount(); 
  
  uint32_t zeroAtomic = 0;
  // Physically reset the indirect command to 0 instances to force stop rendering
  DrawArraysIndirectCommand zeroCmd = {6, 0, 0, 0};

  for (int i = 0; i < bufferCount; ++i) {
      // Clear Atomic Counter
      uint32_t* atomicPtr = (uint32_t*)m_atomicBuffer.BeginWrite();
      if (atomicPtr) *atomicPtr = zeroAtomic;
      m_atomicBuffer.Flush();
      m_atomicBuffer.Lock(); // Advance

      // Clear Indirect Buffer - Critical to stop rendering immediately
      void* indirectPtr = m_indirectBuffer.BeginWrite();
      if (indirectPtr) memcpy(indirectPtr, &zeroCmd, sizeof(zeroCmd));
      m_indirectBuffer.Flush();
      m_indirectBuffer.Lock(); // Advance
  }
  
  LOG_INFO("GPUParticleSystem: Logical and Physical clear executed (All slots and GPU counters reset).");
}

void GPUParticleSystem::LoadShaders() {
  // 1. Load Compute Shader
  // Using existing V2 shader files as we are porting V2 code
  std::ifstream compFile("assets/shaders/particle.compute");
  if (compFile.is_open()) {
    std::stringstream ss;
    ss << compFile.rdbuf();
    std::string source = ss.str();

    unsigned int compId = rlCompileShader(source.c_str(), RL_COMPUTE_SHADER);
    if (compId != 0) {
      m_computeShader.id = rlLoadComputeShaderProgram(compId);
      if (m_computeShader.id != 0) {
        LOG_INFO("GPUParticleSystem: Compute shader loaded (ID: {})",
                 m_computeShader.id);
        m_computeDtLoc = rlGetLocationUniform(m_computeShader.id, "dt");
        m_computeTimeLoc = rlGetLocationUniform(m_computeShader.id, "time");
        m_computeTotalLoc =
            rlGetLocationUniform(m_computeShader.id, "totalParticles");
        m_computeForceFieldCountLoc =
            rlGetLocationUniform(m_computeShader.id, "forceFieldCount");
        m_computeSubEmitterEnabledLoc =
            rlGetLocationUniform(m_computeShader.id, "subEmitterEnabled");
      } else {
        LOG_ERROR("GPUParticleSystem: Compute shader linking failed!");
      }
    } else {
      LOG_ERROR("GPUParticleSystem: Compute shader compilation failed!");
    }
  } else {
    LOG_ERROR(
        "GPUParticleSystem: Could not open assets/shaders/particle.compute");
  }

  // 2. Load Render Shaders (with local #include support for ABI snippets)
  m_renderShader = LoadShaderWithIncludes("assets/shaders/particle.vert",
                                          "assets/shaders/particle.frag");
  if (m_renderShader.id != 0) {
    LOG_INFO("GPUParticleSystem: Render shader loaded (ID: {})",
             m_renderShader.id);
    m_renderMvpLoc = GetShaderLocation(m_renderShader, "mvp");
    m_renderAtlasLoc = GetShaderLocation(m_renderShader, "particleAtlas");
    m_renderBlendPassLoc = GetShaderLocation(m_renderShader, "uBlendPass");
    m_renderMaterialCountLoc = GetShaderLocation(m_renderShader, "uMaterialCount");
  } else {
    LOG_ERROR("GPUParticleSystem: Render shader loading failed!");
  }

  // 3. Load Emission Shader
  std::ifstream emitFile("assets/shaders/particle_emit.compute");
  if (emitFile.is_open()) {
    std::stringstream ss;
    ss << emitFile.rdbuf();
    unsigned int emitCompId =
        rlCompileShader(ss.str().c_str(), RL_COMPUTE_SHADER);
    if (emitCompId != 0) {
      m_emitShader.id = rlLoadComputeShaderProgram(emitCompId);
      if (m_emitShader.id != 0) {
        m_emitCountLoc = rlGetLocationUniform(m_emitShader.id, "emitCount");
      }
    }
  }

  // 4. Load Sub-Emission Merge Shader
  std::ifstream subEmitFile("assets/shaders/particle_sub_emit.compute");
  if (subEmitFile.is_open()) {
    std::stringstream ss;
    ss << subEmitFile.rdbuf();
    unsigned int subEmitCompId =
        rlCompileShader(ss.str().c_str(), RL_COMPUTE_SHADER);
    if (subEmitCompId != 0) {
      m_subEmitShader.id = rlLoadComputeShaderProgram(subEmitCompId);
      if (m_subEmitShader.id != 0) {
        m_subEmitCountLoc = rlGetLocationUniform(m_subEmitShader.id, "subEmitCount");
        m_subEmitMaxParticlesLoc =
            rlGetLocationUniform(m_subEmitShader.id, "maxParticles");
      }
    }
  }

  // 5. Load Finalize Shader
  std::ifstream finalizeFile("assets/shaders/particle_finalize.compute");
  if (finalizeFile.is_open()) {
    std::stringstream ss;
    ss << finalizeFile.rdbuf();
    unsigned int finCompId =
        rlCompileShader(ss.str().c_str(), RL_COMPUTE_SHADER);
    if (finCompId != 0) {
      m_finalizeShader.id = rlLoadComputeShaderProgram(finCompId);
    }
  }

  // Verify core shaders
  if (m_computeShader.id == 0 || m_renderShader.id == 0 ||
      m_emitShader.id == 0 || m_subEmitShader.id == 0 ||
      m_finalizeShader.id == 0) {
    LOG_ERROR("GPUParticleSystem: [AG] Shader initialization incomplete!");
  }
}

void GPUParticleSystem::CreateBuffers() {
  size_t particleSize = sizeof(components::GPUParticle);
  size_t totalSize = m_maxParticles * particleSize;

  // Particle buffer (all particles)
  m_particleBuffer.Create(totalSize);
  LOG_DEBUG("GPUParticleSystem: Created particle buffer ({} bytes)", totalSize);

  // Compact buffer (alive particles only)
  m_compactBuffer.Create(totalSize);
  LOG_DEBUG("GPUParticleSystem: Created compact buffer ({} bytes)", totalSize);

  // DrawIndirect buffer (16 bytes - Persistent/Triple Buffered)
  m_indirectBuffer.Create(sizeof(DrawArraysIndirectCommand), 3);
  {
      DrawArraysIndirectCommand cmd = {6, 0, 0, 0};
      void* ptr = m_indirectBuffer.BeginWrite();
      if (ptr) memcpy(ptr, &cmd, sizeof(cmd));
      m_indirectBuffer.Flush();
      m_indirectBuffer.Lock();
  }
  LOG_DEBUG("GPUParticleSystem: Created persistent indirect buffer");

  // Atomic counter buffer (Persistent/Triple Buffered)
  m_atomicBuffer.Create(sizeof(uint32_t), 3);
  LOG_DEBUG("GPUParticleSystem: Created persistent atomic counter");

  // Emission Buffer (Triple Buffered)
  using namespace NoMoreDay::Constants::Render;
  m_emissionCap = PARTICLE_STAGING_RESERVE;
  m_emissionBuffer.Create(m_emissionCap * sizeof(components::GPUParticle));
  m_mappedPtr = (components::GPUParticle*)m_emissionBuffer.BeginWrite();
  m_emitHead = 0;

  // Sub-emission payload buffer (generated on GPU when particles die)
  m_subEmissionCap = 2048;
  m_subEmissionBuffer.Create(static_cast<size_t>(m_subEmissionCap) *
                             sizeof(components::GPUParticle));

  // Sub-emission atomic counter (triple-buffered to avoid GPU/CPU stalls)
  m_subEmitCountBuffer.Create(sizeof(uint32_t), 3);
  uint32_t *subEmitCountPtr =
      static_cast<uint32_t *>(m_subEmitCountBuffer.BeginWrite());
  if (subEmitCountPtr != nullptr) {
    *subEmitCountPtr = 0;
  }
  m_subEmitCountBuffer.Flush();
  m_subEmitCountBuffer.Lock();

}

void GPUParticleSystem::CreateQuadVAO() {
  // Simple quad vertices: two triangles forming a square
  // Positions are in [-0.5, 0.5] range, centered at origin
  float vertices[] = {
      // Triangle 1
      -0.5f,
      -0.5f,
      0.5f,
      -0.5f,
      0.5f,
      0.5f,
      // Triangle 2
      -0.5f,
      -0.5f,
      0.5f,
      0.5f,
      -0.5f,
      0.5f,
  };

  m_quadVAO = rlLoadVertexArray();
  rlEnableVertexArray(m_quadVAO);

  m_quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
  rlSetVertexAttribute(0, 2, RL_FLOAT, false, 2 * sizeof(float), 0);
  rlEnableVertexAttribute(0);

  rlDisableVertexArray();

  LOG_DEBUG("GPUParticleSystem: Created quad VAO (ID: {})", m_quadVAO);
}

void GPUParticleSystem::Emit(const components::GPUParticle &particle) {
  Emit(particle, 0);
}

void GPUParticleSystem::Emit(const components::GPUParticle &particle,
                             int materialId) {
  components::GPUParticle packed = particle;
  components::GPUFlags::PackMaterialId(packed.flags, materialId);
  uint32_t idx = m_emitHead.fetch_add(1);
  if (idx < m_emissionCap) {
    m_mappedPtr[idx] = packed;
  }
}

void GPUParticleSystem::EmitBatch(
    const std::vector<components::GPUParticle> &particles) {
  EmitBatch(particles, 0);
}

void GPUParticleSystem::EmitBatch(
    const std::vector<components::GPUParticle> &particles, int materialId) {
  if (particles.empty())
    return;

  uint32_t count = (uint32_t)particles.size();
  uint32_t startIdx = m_emitHead.fetch_add(count);
  
  if (startIdx < m_emissionCap) {
    uint32_t toCopy = std::min(count, m_emissionCap - startIdx);
    if (materialId <= 0) {
      memcpy(m_mappedPtr + startIdx, particles.data(),
             toCopy * sizeof(components::GPUParticle));
      return;
    }

    for (uint32_t i = 0; i < toCopy; ++i) {
      components::GPUParticle packed = particles[i];
      components::GPUFlags::PackMaterialId(packed.flags, materialId);
      m_mappedPtr[startIdx + i] = packed;
    }
  }
}

void GPUParticleSystem::Update(float dt) {
  NoMoreDay::utils::ScopedTimer timer("Particle Update", 3000); 
  if (!m_initialized)
    return;

  bool forceFieldEnabled = false;
  bool subEmitterEnabled = false;
  if (render::core::QualityTierManager::Get().IsInitialized()) {
    const auto &config = render::core::QualityTierManager::Get().GetConfig();
    forceFieldEnabled = config.forceFieldEnabled;
    subEmitterEnabled = config.subEmitterEnabled;
  }

  auto &forceFieldManager = render::ForceFieldManager::Get();
  if (forceFieldManager.IsInitialized()) {
    forceFieldManager.SyncToGPU();
  }

  // 1. Swap buffers for ping-pong
  core::ComputeBuffer &bufIn = m_pingPong ? m_compactBuffer : m_particleBuffer;
  core::ComputeBuffer &bufOut = m_pingPong ? m_particleBuffer : m_compactBuffer;

  // 0.1 Throttled Readback (Once every 60 frames)
  // This drastically reduces GPU->CPU sync stalls
  m_readbackFrameCounter++;
  if (m_readbackFrameCounter >= 60) {
      m_atomicBuffer.Read(&m_lastKnownAliveCount, sizeof(uint32_t));
      m_readbackFrameCounter = 0;
  }

  // 1. Reset current atomic counter and initialize INDIRECT buffer slot
  uint32_t* atomicPtr = (uint32_t*)m_atomicBuffer.BeginWrite();
  if (atomicPtr) {
      *atomicPtr = 0;
  }
  m_atomicBuffer.Flush();
  uint32_t *subEmitCountPtr = nullptr;
  if (subEmitterEnabled) {
    subEmitCountPtr =
        static_cast<uint32_t *>(m_subEmitCountBuffer.BeginWrite());
    if (subEmitCountPtr != nullptr) {
      *subEmitCountPtr = 0;
    }
    m_subEmitCountBuffer.Flush();
  }

  // Initialize Indirect Buffer Command for THIS frame
  // The GPU will later overwrite instanceCount in the compute shader
  {
      void* ptr = m_indirectBuffer.BeginWrite();
      if (ptr) {
          DrawArraysIndirectCommand cmd = {6, 0, 0, 0}; 
          memcpy(ptr, &cmd, sizeof(cmd));
      }
      m_indirectBuffer.Flush();
  }

  // 2. Dispatch simulation compute shader
  {
    const uint32_t pendingEmit = m_emitHead.load();

    // Phase 5: Adaptive Dispatch Range
    // If we have no alive particles AND no pending emissions, we can idle.
    if (m_lastKnownAliveCount == 0 && pendingEmit == 0 && m_currentParticleCount == 0) {
        m_targetDispatchCount = 0;
    } else {
        // Use the conservative estimate to ensure all particles are updated
        // If lastKnown is 0 but pendingEmit > 0, we still need to run simulation 
        // on existing slots if m_currentParticleCount > 0
        m_targetDispatchCount = (int)std::max({m_lastKnownAliveCount + 2048, (uint32_t)m_currentParticleCount, pendingEmit});
        m_targetDispatchCount = (int)std::min<uint32_t>((uint32_t)m_targetDispatchCount, (uint32_t)m_maxParticles);
    }

    if (m_targetDispatchCount > 0) {
        rlEnableShader(m_computeShader.id);

        using namespace NoMoreDay::Constants::Render;
        float clampedDt =
            (dt > MAX_DELTA_TIME_PARTICLES) ? DEFAULT_DELTA_TIME_PARTICLES : dt;
        rlSetUniform(m_computeDtLoc, &clampedDt, RL_SHADER_UNIFORM_FLOAT, 1);
        rlSetUniform(m_computeTimeLoc, &m_totalTime, RL_SHADER_UNIFORM_FLOAT, 1);
        rlSetUniform(m_computeTotalLoc, &m_currentParticleCount,
                    RL_SHADER_UNIFORM_INT, 1);
        const int forceFieldCount =
            forceFieldEnabled ? forceFieldManager.GetActiveCount() : 0;
        const int subEmitterEnabledInt = subEmitterEnabled ? 1 : 0;
        if (m_computeForceFieldCountLoc >= 0) {
          rlSetUniform(m_computeForceFieldCountLoc, &forceFieldCount,
                       RL_SHADER_UNIFORM_INT, 1);
        }
        if (m_computeSubEmitterEnabledLoc >= 0) {
          rlSetUniform(m_computeSubEmitterEnabledLoc, &subEmitterEnabledInt,
                       RL_SHADER_UNIFORM_INT, 1);
        }

        // Bind SSBOs (RenderConstants::ParticleCS semantics)
        using namespace NoMoreDay::RenderConstants;
        bufIn.BindBase(ParticleCS::PARTICLES_IN);
        bufOut.BindBase(ParticleCS::PARTICLES_OUT);
        m_indirectBuffer.BindBase(ParticleCS::INDIRECT_CMD);
        m_atomicBuffer.BindBase(ParticleCS::ATOMIC_COUNT); // Write to CURRENT counter
        if (forceFieldManager.IsInitialized()) {
          forceFieldManager.BindSSBO(ParticleCS::FORCE_FIELDS);
        }
        if (subEmitterEnabled) {
          m_subEmissionBuffer.BindBase(ParticleCS::SUB_EMISSION);
          m_subEmitCountBuffer.BindBase(ParticleCS::SUB_EMIT_COUNTER);
        }

        // Dispatch
        int workGroups = (m_targetDispatchCount + (WORKGROUP_SIZE_PARTICLES - 1)) /
                        WORKGROUP_SIZE_PARTICLES;
        if (workGroups > 0) {
        rlComputeShaderDispatch(workGroups, 1, 1);
        }

        utils::GPUUtils::MemoryBarrier(Barrier::All);
        rlDisableShader();
    }
  }

  // 3. Emission Logic (Lock-Free)
  uint32_t survivors = m_lastKnownAliveCount;
  uint32_t totalAfterEmission = survivors;
  
  uint32_t totalNewToEmit = m_emitHead.load();
  if (totalNewToEmit > 0) {
      uint32_t allowedNew = std::min<uint32_t>(totalNewToEmit, m_emissionCap);
      
      // Accumulate to our CPU estimate so next frame's dispatch includes these
      m_currentParticleCount += (int)allowedNew; 
      m_currentParticleCount = (int)std::min<uint32_t>((uint32_t)m_currentParticleCount, (uint32_t)m_maxParticles);
      
      if (allowedNew > 0 && m_emitShader.id != 0) {
          // Sync CPU writes to GPU
          m_emissionBuffer.Flush();

          // Dispatch Emit Shader
          rlEnableShader(m_emitShader.id);
          int newCountInt = (int)allowedNew;
          rlSetUniform(m_emitCountLoc, &newCountInt, RL_SHADER_UNIFORM_INT, 1);

          m_emissionBuffer.BindBase(ParticleCS::PARTICLES_IN);
          bufOut.BindBase(ParticleCS::PARTICLES_OUT);
          m_indirectBuffer.BindBase(ParticleCS::INDIRECT_CMD); // Ensure GPU can update the count
          m_atomicBuffer.BindBase(ParticleCS::ATOMIC_COUNT);

          int workGroups = (newCountInt + 255) / 256;
          rlComputeShaderDispatch(workGroups, 1, 1);
          utils::GPUUtils::MemoryBarrier(Barrier::All);
          rlDisableShader();

          totalAfterEmission += allowedNew; 
      }
  }

  // 4. Merge GPU-generated sub-emissions into the compact output buffer.
  if (subEmitterEnabled && m_subEmitShader.id != 0) {
    uint32_t subEmitCount = 0;
    m_subEmitCountBuffer.ReadFromSlot(&subEmitCount, sizeof(uint32_t),
                                      m_subEmitCountBuffer.GetCurrentSlot());
    subEmitCount = std::min(subEmitCount, m_subEmissionCap);

    if (subEmitCount > 0) {
      rlEnableShader(m_subEmitShader.id);
      const int subEmitCountInt = static_cast<int>(subEmitCount);
      if (m_subEmitCountLoc >= 0) {
        rlSetUniform(m_subEmitCountLoc, &subEmitCountInt, RL_SHADER_UNIFORM_INT,
                     1);
      }
      if (m_subEmitMaxParticlesLoc >= 0) {
        rlSetUniform(m_subEmitMaxParticlesLoc, &m_maxParticles,
                     RL_SHADER_UNIFORM_INT, 1);
      }

      using namespace NoMoreDay::RenderConstants;
      m_subEmissionBuffer.BindBase(ParticleCS::SUB_EMISSION);
      m_subEmitCountBuffer.BindBase(ParticleCS::SUB_EMIT_COUNTER);
      bufOut.BindBase(ParticleCS::PARTICLES_OUT);
      m_atomicBuffer.BindBase(ParticleCS::ATOMIC_COUNT);

      const int workGroups = (subEmitCountInt + 255) / 256;
      if (workGroups > 0) {
        rlComputeShaderDispatch(workGroups, 1, 1);
      }
      utils::GPUUtils::MemoryBarrier(Barrier::All);
      rlDisableShader();

      totalAfterEmission += subEmitCount;
    }
  }

  // 5. Finalize the frame (Copy atomic to indirect)
  FinalizeFrame();

  // 6. Finalize emission buffer and lock ALL persistent buffers for this frame
  m_emissionBuffer.Lock();
  m_mappedPtr = (components::GPUParticle*)m_emissionBuffer.BeginWrite();
  m_emitHead = 0;
  if (subEmitterEnabled) {
    if (subEmitCountPtr != nullptr) {
      *subEmitCountPtr = 0;
    }
    m_subEmitCountBuffer.Flush();
  }
  
  m_totalTime += dt;

  // Advance Slot for next frame
  m_indirectBuffer.Lock(); 

  // 7. Reset Particle Count Estimate 
  m_currentParticleCount = static_cast<int>(totalAfterEmission);
  
  m_pingPong = !m_pingPong;
  if (subEmitterEnabled) {
    m_subEmitCountBuffer.Lock();
  }
  m_atomicBuffer.Lock(); // Advance atomic slot and insert fence
}

void GPUParticleSystem::FinalizeFrame() {
    if (m_finalizeShader.id == 0) return;

    rlEnableShader(m_finalizeShader.id);
    
    // Bind current buffers
    // Binding points must match particle_finalize.compute
    m_indirectBuffer.BindBase(2); // INDIRECT_CMD
    m_atomicBuffer.BindBase(3);   // ATOMIC_COUNT
    
    rlComputeShaderDispatch(1, 1, 1);
    utils::GPUUtils::MemoryBarrier(Barrier::All);
    
    rlDisableShader();
}

Matrix GPUParticleSystem::BuildMVP(const Camera2D &camera) const {
  float w = (float)GetScreenWidth();
  float h = (float)GetScreenHeight();

  // View matrix: camera transform
  Matrix view = MatrixIdentity();

  // 1. Translate to camera target (negate for view)
  view = MatrixMultiply(
      view, MatrixTranslate(-camera.target.x, -camera.target.y, 0.0f));

  // 2. Apply rotation
  if (camera.rotation != 0.0f) {
    view = MatrixMultiply(view, MatrixRotateZ(camera.rotation * DEG2RAD));
  }

  // 3. Apply zoom
  view = MatrixMultiply(view, MatrixScale(camera.zoom, camera.zoom, 1.0f));

  // 4. Translate by offset (screen center)
  view = MatrixMultiply(
      view, MatrixTranslate(camera.offset.x, camera.offset.y, 0.0f));

  // Projection matrix: orthographic, Y-down (Raylib convention)
  Matrix proj = MatrixOrtho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

  // MVP = View * Projection
  return MatrixMultiply(view, proj);
}

void GPUParticleSystem::Render(const Camera2D &camera) {
  NoMoreDay::utils::ScopedTimer timer("Particle Render", 50);
  if (!m_initialized || m_renderShader.id == 0)
    return;

  // Flush any pending raylib draw calls before we start manual GL drawing
  rlDrawRenderBatchActive();

  // Build MVP matrix
  Matrix mvp = BuildMVP(camera);

  // Set buffer state shared by both passes.
  SetShaderValueMatrix(m_renderShader, m_renderMvpLoc, mvp);

  bool bindTextureAtlas = false;
  if (render::core::QualityTierManager::Get().IsInitialized()) {
    const auto &config = render::core::QualityTierManager::Get().GetConfig();
    bindTextureAtlas = config.particleTexturesEnabled &&
                       render::ParticleTextureManager::Get().IsInitialized() &&
                       render::ParticleTextureManager::Get().GetLayerCount() > 0;
  }
  if (m_renderAtlasLoc >= 0) {
    const int atlasUnit = static_cast<int>(TextureUnit::TEX_PARTICLE_ATLAS);
    SetShaderValue(m_renderShader, m_renderAtlasLoc, &atlasUnit,
                   SHADER_UNIFORM_INT);
  }
  if (m_renderMaterialCountLoc >= 0) {
    const int materialCount = render::MaterialManager::Get().GetMaterialCount();
    SetShaderValue(m_renderShader, m_renderMaterialCountLoc, &materialCount,
                   SHADER_UNIFORM_INT);
  }
  if (bindTextureAtlas) {
    render::ParticleTextureManager::Get().Bind(
        static_cast<uint32_t>(TextureUnit::TEX_PARTICLE_ATLAS));
  }

  // Bind the buffer containing the valid particles for this frame.
  using namespace NoMoreDay::RenderConstants;
  core::ComputeBuffer &bufferToRender =
      m_pingPong ? m_compactBuffer : m_particleBuffer;
  bufferToRender.BindBase(ParticleCS::PARTICLES_IN);

  // Enable VAO once for both blend passes.
  rlEnableVertexArray(m_quadVAO);
  rlDisableDepthTest();       // Ensure depth test is off
  rlDisableBackfaceCulling(); // Ensure we see both sides

  // We use the PREVIOUS slot because Update() just finished writing to it and
  // advanced the slot.
  m_indirectBuffer.Bind(GL_DRAW_INDIRECT_BUFFER, 0);

  auto drawPass = [&](int blendPass, int raylibBlendMode) {
    BeginBlendMode(raylibBlendMode);
    BeginShaderMode(m_renderShader);
    
    rlEnableVertexArray(m_quadVAO); // Re-ensure VAO is active for this pass

    if (m_renderBlendPassLoc >= 0) {
      SetShaderValue(m_renderShader, m_renderBlendPassLoc, &blendPass,
                     SHADER_UNIFORM_INT);
    }
    utils::GPUUtils::DrawArraysIndirect(GL_TRIANGLES,
                                        m_indirectBuffer.GetPreviousSlotOffset());
    EndShaderMode();
    EndBlendMode();
  };

  // Pass 0: Alpha particles
  drawPass(0, BLEND_ALPHA);
  // Pass 1: Additive particles
  drawPass(1, BLEND_ADDITIVE);

  utils::GPUUtils::BindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

  // Cleanup
  if (bindTextureAtlas) {
    render::ParticleTextureManager::Get().Unbind(
        static_cast<uint32_t>(TextureUnit::TEX_PARTICLE_ATLAS));
  }
  rlDisableVertexArray();
}

// ==================== InkEffectHelper Implementation ====================

components::GPUParticle InkEffectHelper::CreateInkTrail(Vector2 pos,
                                                        Vector2 vel,
                                                        float scale,
                                                        float life) {
  components::GPUParticle p;
  p.position = pos;
  p.velocity = vel;
  p.color = COLOR_INK_LIGHT;
  p.lifetime = life;
  p.maxLifetime = life;
  p.scale = scale * 0.5f; // Reduced size
  p.flags = 0;
  p.growthRate = -scale * 0.5f; // Shrink instead of grow for trails
  return p;
}

std::vector<components::GPUParticle>
InkEffectHelper::CreateInkSplash(Vector2 pos, int count, float radius,
                                 float force) {
  std::vector<components::GPUParticle> res;
  res.reserve(count);
  AppendInkSplash(res, pos, count, radius, force);
  return res;
}

void InkEffectHelper::AppendInkSplash(std::vector<components::GPUParticle> &res,
                                      Vector2 pos, int count, float radius,
                                      float force) {
  // Color palette for variety
  static const Color colors[] = {
      COLOR_INK_DARK,    COLOR_INK_LIGHT,
      COLOR_SWORD_QI,    {60, 70, 90, 200}, // Blue-grey ink
      {50, 50, 60, 180},                    // Dark grey
  };
  static const int colorCount = sizeof(colors) / sizeof(colors[0]);

  for (int i = 0; i < count; ++i) {
    float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
    float r = sqrtf((float)GetRandomValue(0, 1000) / 1000.0f) *
              radius; // Uniform distribution

    components::GPUParticle p;
    p.position = {pos.x + cosf(angle) * r, pos.y + sinf(angle) * r};
    p.velocity = {
        cosf(angle) * force * (0.5f + (float)GetRandomValue(0, 100) / 200.0f),
        sinf(angle) * force * (0.5f + (float)GetRandomValue(0, 100) / 200.0f)};

    // Random color from palette
    p.color = colors[GetRandomValue(0, colorCount - 1)];

    // Much smaller particles: 1.5 to 3.5 instead of 10
    p.scale = 1.5f + (float)GetRandomValue(0, 200) / 100.0f;
    // Reduced lifetime: 0.1s to 0.25s (was 0.2s to 0.5s)
    p.lifetime = 0.1f + (float)GetRandomValue(0, 15) / 100.0f;
    p.maxLifetime = p.lifetime;
    p.flags = 5;          // Soft ink splat
    p.growthRate = -1.5f; // Even faster shrink
    res.push_back(p);
  }
}

components::GPUParticle
InkEffectHelper::CreateGoldParticle(Vector2 pos, Vector2 vel, float scale) {
  components::GPUParticle p;
  p.position = pos;
  p.velocity = vel;

  // Slight color variation for gold
  int variation = GetRandomValue(-20, 20);
  p.color = {(unsigned char)(255),
             (unsigned char)(std::clamp(215 + variation, 180, 255)),
             (unsigned char)(std::clamp(variation * 2, 0, 80)), 255};

  p.lifetime = 0.3f; // Reduced from 0.6s
  p.maxLifetime = 0.3f;
  p.scale = scale * 0.6f; // Reduced size
  p.flags = 2;            // Spark/diamond shape
  p.growthRate = -scale * 1.5f;
  return p;
}

std::vector<components::GPUParticle>
InkEffectHelper::CreateProjectileTrail(Vector2 pos, Vector2 dir,
                                       Color coreColor, Color glowColor,
                                       float trailLength, int count) {
  std::vector<components::GPUParticle> res;
  res.reserve(count * 2);
  AppendProjectileTrail(res, pos, dir, coreColor, glowColor, trailLength,
                        count);
  return res;
}

void InkEffectHelper::AppendProjectileTrail(
    std::vector<components::GPUParticle> &res, Vector2 pos, Vector2 dir,
    Color coreColor, Color glowColor, float trailLength, int count) {

  // Normalize direction
  float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
  if (len < 0.001f) {
    dir = {1.0f, 0.0f};
    len = 1.0f;
  }
  dir.x /= len;
  dir.y /= len;

  for (int i = 0; i < count; ++i) {
    float t = (float)i / (float)(count - 1);
    float offset = -trailLength * t; // Behind the projectile

    // Core particle (bright, small)
    components::GPUParticle core;
    core.position = {pos.x + dir.x * offset, pos.y + dir.y * offset};
    core.velocity = {-dir.x * 20.0f, -dir.y * 20.0f}; // Slight backward drift
    core.color = coreColor;
    core.scale = 1.0f - t * 0.5f; // Smaller at trail end
    core.lifetime = 0.07f + t * 0.05f; // Reduced from 0.15s
    core.maxLifetime = core.lifetime;
    core.flags = 2; // Spark
    core.growthRate = -2.0f;
    res.push_back(core);

    // Glow particle (softer, larger)
    if (i % 2 == 0) {
      components::GPUParticle glow;
      glow.position = core.position;
      glow.position.x += (float)GetRandomValue(-5, 5);
      glow.position.y += (float)GetRandomValue(-5, 5);
      glow.velocity = {0, 0};
      glow.color = glowColor;
      glow.scale = 2.0f - t * 1.0f;
      glow.lifetime = 0.1f; // Reduced from 0.2s
      glow.maxLifetime = 0.1f;
      glow.flags = 1; // Soft glow
      glow.growthRate = -1.0f;
      res.push_back(glow);
    }
  }
}

std::vector<components::GPUParticle>
InkEffectHelper::CreateDashEffect(Vector2 startPos, Vector2 dir, Color color,
                                  float dashLength, int count) {
  std::vector<components::GPUParticle> res;
  res.reserve(count);
  AppendDashEffect(res, startPos, dir, color, dashLength, count);
  return res;
}

void InkEffectHelper::AppendDashEffect(
    std::vector<components::GPUParticle> &res, Vector2 startPos, Vector2 dir,
    Color color, float dashLength, int count) {

  // Normalize direction
  float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
  if (len < 0.001f) {
    dir = {1.0f, 0.0f};
    len = 1.0f;
  }
  dir.x /= len;
  dir.y /= len;

  // Perpendicular direction for spread
  Vector2 perp = {-dir.y, dir.x};

  for (int i = 0; i < count; ++i) {
    float t = (float)i / (float)(count - 1);

    // Particles concentrated at start and along initial direction
    float distAlongPath = dashLength * t * 0.3f; // Only first 30% of dash
    float perpSpread = (float)GetRandomValue(-20, 20);

    components::GPUParticle p;
    p.position = {startPos.x + dir.x * distAlongPath + perp.x * perpSpread,
                  startPos.y + dir.y * distAlongPath + perp.y * perpSpread};

    // Velocity fans out from dash direction
    float speedVariation = 50.0f + (float)GetRandomValue(0, 100);
    p.velocity = {dir.x * speedVariation * 0.5f +
                      perp.x * (float)GetRandomValue(-30, 30),
                  dir.y * speedVariation * 0.5f +
                      perp.y * (float)GetRandomValue(-30, 30)};

    p.color = color;
    p.color.a = (unsigned char)(200 - t * 80); // Fade with distance
    p.scale = 1.5f + (float)GetRandomValue(0, 100) / 100.0f;
    p.lifetime = 0.15f + (float)GetRandomValue(0, 15) / 100.0f; // Reduced from 0.3s
    p.maxLifetime = p.lifetime;
    p.flags = 13; // Ink with soft edges
    p.growthRate = -1.5f;
    res.push_back(p);
  }
}

std::vector<components::GPUParticle>
InkEffectHelper::CreateAreaEffect(Vector2 center, float radius, Color coreColor,
                                  Color edgeColor, int count, float duration) {
  std::vector<components::GPUParticle> res;
  res.reserve(count);
  AppendAreaEffect(res, center, radius, coreColor, edgeColor, count, duration);
  return res;
}

void InkEffectHelper::AppendAreaEffect(
    std::vector<components::GPUParticle> &res, Vector2 center, float radius,
    Color coreColor, Color edgeColor, int count, float duration) {

  for (int i = 0; i < count; ++i) {
    // Random position within area (uniform distribution)
    float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
    float r = sqrtf((float)GetRandomValue(0, 1000) / 1000.0f) * radius;

    components::GPUParticle p;
    p.position = {center.x + cosf(angle) * r, center.y + sinf(angle) * r};

    // Slow swirling motion within area
    float tangentAngle = angle + 1.57f; // 90 degrees offset
    float speed = 20.0f + (float)GetRandomValue(0, 30);
    p.velocity = {cosf(tangentAngle) * speed, sinf(tangentAngle) * speed};

    // Color gradient: core color in center, edge color at boundary
    float colorT = r / radius;
    p.color = {
        (unsigned char)(coreColor.r * (1.0f - colorT) + edgeColor.r * colorT),
        (unsigned char)(coreColor.g * (1.0f - colorT) + edgeColor.g * colorT),
        (unsigned char)(coreColor.b * (1.0f - colorT) + edgeColor.b * colorT),
        (unsigned char)(coreColor.a * (1.0f - colorT * 0.5f))};

    p.scale = 1.5f + (float)GetRandomValue(0, 150) / 100.0f;
    // Reduced duration scale by 50%
    p.lifetime = duration * 0.5f * (0.7f + (float)GetRandomValue(0, 60) / 100.0f);
    p.maxLifetime = p.lifetime;
    p.flags = 1;         // Soft glow
    p.growthRate = 0.3f; // Slight growth
    res.push_back(p);
  }
}

components::GPUParticle InkEffectHelper::CreateSpark(Vector2 pos, Vector2 vel,
                                                     Color color, float scale) {
  components::GPUParticle p;
  p.position = pos;
  p.velocity = vel;
  p.color = color;
  p.scale = scale * 0.8f; // Small sparks
  p.lifetime = 0.1f; // Reduced from 0.2s
  p.maxLifetime = 0.1f;
  p.flags = 2;                  // Diamond/spark shape
  p.growthRate = -scale * 4.0f; // Faster shrink
  return p;
}

std::vector<components::GPUParticle>
InkEffectHelper::CreateSlashEffect(Vector2 pos, Vector2 dir, Color color,
                                   float length) {
  std::vector<components::GPUParticle> res;
  res.reserve(12);
  AppendSlashEffect(res, pos, dir, color, length);
  return res;
}

void InkEffectHelper::AppendSlashEffect(
    std::vector<components::GPUParticle> &res, Vector2 pos, Vector2 dir,
    Color color, float length) {

  // Normalize direction
  float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
  if (len < 0.001f) {
    dir = {1.0f, 0.0f};
    len = 1.0f;
  }
  dir.x /= len;
  dir.y /= len;

  // Perpendicular direction for slash width
  Vector2 perp = {-dir.y, dir.x};

  // Create slash line
  for (int i = 0; i < 10; ++i) {
    float t = (float)i / 9.0f - 0.5f; // -0.5 to 0.5
    float offset = t * length;

    components::GPUParticle p;
    p.position = {pos.x + perp.x * offset, pos.y + perp.y * offset};

    // Slash moves in direction
    p.velocity = {dir.x * 100.0f, dir.y * 100.0f};

    p.color = color;
    p.scale = 2.0f - fabsf(t) * 2.0f; // Thicker in center
    p.lifetime = 0.075f; // Reduced from 0.15s
    p.maxLifetime = 0.075f;
    p.flags = 2; // Spark
    p.growthRate = -5.0f;
    res.push_back(p);
  }

  // Add core flash
  components::GPUParticle flash;
  flash.position = pos;
  flash.velocity = {dir.x * 50.0f, dir.y * 50.0f};
  flash.color = COLOR_WHITE_SPARK;
  flash.scale = 3.0f;
  flash.lifetime = 0.05f; // Reduced from 0.1s
  flash.maxLifetime = 0.05f;
  flash.flags = 1; // Glow
  flash.growthRate = -20.0f;
  res.push_back(flash);
}

} // namespace NoMoreDay::systems
