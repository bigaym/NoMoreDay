#include "engine/render/GPUParticleSystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "game/components/Common.hpp"

// RenderConstants::ParticleCS defines binding point semantics
#include "engine/render/RenderConstants.hpp"
#include <algorithm>
#include <cmath>
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

// Thread-local staging buffer to avoid lock contention on Emit
struct ThreadLocalParticleStaging {
    std::vector<components::GPUParticle> buffer;
    ThreadLocalParticleStaging() {
        buffer.reserve(1024);
        GPUParticleSystem::Get().RegisterThreadBuffer(&buffer);
    }
    ~ThreadLocalParticleStaging() {
        GPUParticleSystem::Get().UnregisterThreadBuffer(&buffer);
    }
};

static thread_local ThreadLocalParticleStaging t_staging;

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

  // VAO/VBO cleanup
  if (m_quadVBO != 0) {
    rlUnloadVertexBuffer(m_quadVBO);
    m_quadVBO = 0;
  }
  if (m_quadVAO != 0) {
    rlUnloadVertexArray(m_quadVAO);
    m_quadVAO = 0;
  }

  // Buffers are cleaned up by ComputeBuffer destructor

  m_emissionBuffer.Destroy();
  m_initialized = false;
}

void GPUParticleSystem::RegisterThreadBuffer(
    std::vector<components::GPUParticle> *buffer) {
  std::lock_guard<std::mutex> lock(m_threadBuffersMutex);
  m_allThreadBuffers.push_back(buffer);
}

void GPUParticleSystem::UnregisterThreadBuffer(
    std::vector<components::GPUParticle> *buffer) {
  std::lock_guard<std::mutex> lock(m_threadBuffersMutex);
  auto it = std::find(m_allThreadBuffers.begin(), m_allThreadBuffers.end(), buffer);
  if (it != m_allThreadBuffers.end()) {
    m_allThreadBuffers.erase(it);
  }
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
        m_computeTotalLoc =
            rlGetLocationUniform(m_computeShader.id, "totalParticles");
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

  // 2. Load Render Shaders
  m_renderShader = LoadShader("assets/shaders/particle.vert",
                              "assets/shaders/particle.frag");
  if (m_renderShader.id != 0) {
    LOG_INFO("GPUParticleSystem: Render shader loaded (ID: {})",
             m_renderShader.id);
    m_renderMvpLoc = GetShaderLocation(m_renderShader, "mvp");
  } else {
    LOG_ERROR("GPUParticleSystem: Render shader loading failed!");
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

  // DrawIndirect buffer (16 bytes)
  DrawArraysIndirectCommand cmd = {6, 0, 0,
                                   0}; // 6 vertices, 0 instances initially
  m_indirectBuffer.Create(sizeof(DrawArraysIndirectCommand));
  m_indirectBuffer.Update(&cmd, sizeof(cmd));
  LOG_DEBUG("GPUParticleSystem: Created indirect buffer");

  // Atomic counter buffers (4 bytes each)
  uint32_t zero = 0;
  m_atomicBufferPing.Create(sizeof(uint32_t));
  m_atomicBufferPing.Update(&zero, sizeof(zero));
  m_atomicBufferPong.Create(sizeof(uint32_t));
  m_atomicBufferPong.Update(&zero, sizeof(zero));
  LOG_DEBUG("GPUParticleSystem: Created double-buffered atomic counters");

  // Emission Buffer (Triple Buffered)
  using namespace NoMoreDay::Constants::Render;
  m_emissionBuffer.Create(PARTICLE_STAGING_RESERVE *
                          sizeof(components::GPUParticle));

  std::ifstream emitFile("assets/shaders/particle_emit.compute");
  if (emitFile.is_open()) {
    std::stringstream ss;
    ss << emitFile.rdbuf();
    unsigned int shId = rlCompileShader(ss.str().c_str(), RL_COMPUTE_SHADER);
    if (shId) {
      m_emitShader.id = rlLoadComputeShaderProgram(shId);
      m_emitCountLoc = rlGetLocationUniform(m_emitShader.id, "emitCount");
    }
  }
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
  if (t_staging.buffer.size() < (size_t)m_maxParticles) {
    t_staging.buffer.push_back(particle);
  }
}

void GPUParticleSystem::EmitBatch(
    const std::vector<components::GPUParticle> &particles) {
  if (particles.empty())
    return;

  size_t freeSpace = (size_t)m_maxParticles - t_staging.buffer.size();
  size_t toAdd = std::min(particles.size(), freeSpace);

  if (toAdd > 0) {
    t_staging.buffer.insert(t_staging.buffer.end(), particles.begin(),
                             particles.begin() + toAdd);
  }
}

void GPUParticleSystem::Update(float dt) {
  NoMoreDay::utils::ScopedTimer timer("Particle Update", 50);
  if (!m_initialized)
    return;

  using namespace NoMoreDay::RenderConstants;

  // 1. Swap buffers for ping-pong
  core::ComputeBuffer &bufIn = m_pingPong ? m_particleBuffer : m_compactBuffer;
  core::ComputeBuffer &bufOut = m_pingPong ? m_particleBuffer : m_compactBuffer;

  // 0.1 Async Readback from the "Ping" buffer (which was written to in Frame
  // N-1)
  core::ComputeBuffer &readCounter =
      m_atomicPingPong ? m_atomicBufferPong : m_atomicBufferPing;
  core::ComputeBuffer &writeCounter =
      m_atomicPingPong ? m_atomicBufferPing : m_atomicBufferPong;

  // Non-blocking read (data should be ready from previous frame)
  readCounter.Read(&m_lastKnownAliveCount, sizeof(uint32_t));

  // 1. Reset current atomic counter to 0
  uint32_t zero = 0;
  writeCounter.Update(&zero, sizeof(zero));

  // 2. Dispatch simulation compute shader
  {
    rlEnableShader(m_computeShader.id);

    using namespace NoMoreDay::Constants::Render;
    float clampedDt =
        (dt > MAX_DELTA_TIME_PARTICLES) ? DEFAULT_DELTA_TIME_PARTICLES : dt;
    rlSetUniform(m_computeDtLoc, &clampedDt, RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(m_computeTotalLoc, &m_currentParticleCount,
                 RL_SHADER_UNIFORM_INT, 1);

    // Bind SSBOs (RenderConstants::ParticleCS semantics)
    using namespace NoMoreDay::RenderConstants;
    bufIn.BindBase(ParticleCS::PARTICLES_IN);
    bufOut.BindBase(ParticleCS::PARTICLES_OUT);
    m_indirectBuffer.BindBase(ParticleCS::INDIRECT_CMD);
    writeCounter.BindBase(ParticleCS::ATOMIC_COUNT); // Write to CURRENT counter

    // Dispatch
    int workGroups = (m_currentParticleCount + (WORKGROUP_SIZE_PARTICLES - 1)) /
                     WORKGROUP_SIZE_PARTICLES;
    if (workGroups > 0) {
      rlComputeShaderDispatch(workGroups, 1, 1);
    }

    utils::GPUUtils::MemoryBarrier();
    rlDisableShader();
  }

  // 3. Emission Logic with Aggregation from Threads
  uint32_t survivors = m_lastKnownAliveCount;
  uint32_t totalAfterEmission = survivors;
  uint32_t totalNewToEmit = 0;

  // Aggregate particles from all threads
  {
    std::lock_guard<std::mutex> lock(m_threadBuffersMutex);
    
    // Calculate total new particles to emit
    for (auto* threadBuf : m_allThreadBuffers) {
      totalNewToEmit += (uint32_t)threadBuf->size();
    }

    if (totalNewToEmit > 0) {
      uint32_t allowedNew = totalNewToEmit;
      
      // Soft limit check
      if (survivors + totalNewToEmit > (uint32_t)m_maxParticles * 0.95f) {
        allowedNew = std::max(0, (int)(m_maxParticles * 0.95f) - (int)survivors);
        if (allowedNew < totalNewToEmit) {
          LOG_WARN("GPUParticleSystem: Approaching limit ({} / {}). Throttling emission.",
                   survivors, m_maxParticles);
        }
      }

      if (allowedNew > 0 && m_emitShader.id != 0) {
        // Upload particles to persistent buffer in chunks or all at once
        components::GPUParticle *mappedPtr = (components::GPUParticle *)m_emissionBuffer.BeginWrite();
        if (mappedPtr) {
          uint32_t copied = 0;
          for (auto* threadBuf : m_allThreadBuffers) {
            uint32_t toCopy = std::min((uint32_t)threadBuf->size(), allowedNew - copied);
            if (toCopy > 0) {
              memcpy(mappedPtr + copied, threadBuf->data(), toCopy * sizeof(components::GPUParticle));
              copied += toCopy;
            }
            threadBuf->clear(); // Clear all buffers even if partially dropped
            if (copied >= allowedNew) break;
          }
          
          m_emissionBuffer.Flush();

          // Dispatch Emit Shader
          rlEnableShader(m_emitShader.id);
          int newCountInt = (int)copied;
          rlSetUniform(m_emitCountLoc, &newCountInt, RL_SHADER_UNIFORM_INT, 1);

          m_emissionBuffer.BindBase(ParticleCS::PARTICLES_IN);
          bufOut.BindBase(ParticleCS::PARTICLES_OUT);
          writeCounter.BindBase(ParticleCS::ATOMIC_COUNT);

          int workGroups = (newCountInt + 255) / 256;
          rlComputeShaderDispatch(workGroups, 1, 1);
          utils::GPUUtils::MemoryBarrier();
          rlDisableShader();

          m_emissionBuffer.Lock();
          totalAfterEmission += copied;
        }
      } else {
        // Just clear buffers if we can't emit
        for (auto* threadBuf : m_allThreadBuffers) {
          threadBuf->clear();
        }
      }
    }
  }

  // 4. Update Indirect Buffer for rendering
  {
    // This ensure the RENDER step (which happens AFTER Update) uses valid counts.
    DrawArraysIndirectCommand cmd = {6, totalAfterEmission, 0, 0};
    m_indirectBuffer.Update(&cmd, sizeof(cmd));
  }

  // 5. Update state for next frame
  m_currentParticleCount = totalAfterEmission;
  m_pingPong = !m_pingPong;
  m_atomicPingPong = !m_atomicPingPong; // Swap atomic buffers
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

  // Build MVP matrix
  Matrix mvp = BuildMVP(camera);

  // Begin rendering
  BeginBlendMode(BLEND_ADDITIVE);
  BeginShaderMode(m_renderShader);

  // Set MVP uniform
  SetShaderValueMatrix(m_renderShader, m_renderMvpLoc, mvp);

  // Bind the buffer containing the valid particles for this frame
  // Since we flipped m_pingPong at the end of Update, the valid data is in the
  // buffer that was the Output (which matches the NEW value of m_pingPong
  // logic)
  using namespace NoMoreDay::RenderConstants;
  core::ComputeBuffer &bufferToRender =
      m_pingPong ? m_compactBuffer : m_particleBuffer;
  bufferToRender.BindBase(ParticleCS::PARTICLES_IN);

  // Enable VAO
  rlEnableVertexArray(m_quadVAO);
  rlDisableDepthTest();       // Ensure depth test is off
  rlDisableBackfaceCulling(); // Ensure we see both sides

  // Indirect Draw using the buffer updated by GPU
  m_indirectBuffer.Bind(GL_DRAW_INDIRECT_BUFFER);
  utils::GPUUtils::DrawArraysIndirect(GL_TRIANGLES, 0);
  utils::GPUUtils::BindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

  // Cleanup
  rlDisableVertexArray();
  EndShaderMode();
  EndBlendMode();
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
  p.growthRate = 0.1f;
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
    p.lifetime = 0.4f + (float)GetRandomValue(0, 60) / 100.0f;
    p.maxLifetime = p.lifetime;
    p.flags = 5;          // Soft ink splat
    p.growthRate = -0.5f; // Shrink over time
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

  p.lifetime = 0.6f;
  p.maxLifetime = 0.6f;
  p.scale = scale * 0.6f; // Reduced size
  p.flags = 2;            // Spark/diamond shape
  p.growthRate = -scale * 0.8f;
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
    core.lifetime = 0.15f + t * 0.1f;
    core.maxLifetime = core.lifetime;
    core.flags = 2; // Spark
    core.growthRate = -1.0f;
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
      glow.lifetime = 0.2f;
      glow.maxLifetime = 0.2f;
      glow.flags = 1; // Soft glow
      glow.growthRate = -0.5f;
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
    p.lifetime = 0.3f + (float)GetRandomValue(0, 30) / 100.0f;
    p.maxLifetime = p.lifetime;
    p.flags = 13; // Ink with soft edges
    p.growthRate = -0.8f;
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
    p.lifetime = duration * (0.7f + (float)GetRandomValue(0, 60) / 100.0f);
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
  p.lifetime = 0.2f;
  p.maxLifetime = 0.2f;
  p.flags = 2;                  // Diamond/spark shape
  p.growthRate = -scale * 2.0f; // Rapid shrink
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
    p.lifetime = 0.15f;
    p.maxLifetime = 0.15f;
    p.flags = 2; // Spark
    p.growthRate = -3.0f;
    res.push_back(p);
  }

  // Add core flash
  components::GPUParticle flash;
  flash.position = pos;
  flash.velocity = {dir.x * 50.0f, dir.y * 50.0f};
  flash.color = COLOR_WHITE_SPARK;
  flash.scale = 3.0f;
  flash.lifetime = 0.1f;
  flash.maxLifetime = 0.1f;
  flash.flags = 1; // Glow
  flash.growthRate = -10.0f;
  res.push_back(flash);
}

} // namespace NoMoreDay::systems