#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/Common.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <rlgl.h>
#include <sstream>


// For glfwGetProcAddress
#include <GLFW/glfw3.h>

// OpenGL function pointers for indirect drawing
typedef void (*PFNGLDRAWARRAYSINDIRECTPROC)(unsigned int mode,
                                            const void *indirect);
typedef void (*PFNGLBINDBUFFERPROC)(unsigned int target, unsigned int buffer);

static PFNGLDRAWARRAYSINDIRECTPROC glDrawArraysIndirectPtr = nullptr;
static PFNGLBINDBUFFERPROC glBindBufferPtr = nullptr;

// OpenGL constants
#define GL_TRIANGLES 0x0004
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F

namespace NoMoreDay::systems {

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

  // Get OpenGL extension for indirect drawing using GLFW
  glDrawArraysIndirectPtr =
      (PFNGLDRAWARRAYSINDIRECTPROC)glfwGetProcAddress("glDrawArraysIndirect");
  glBindBufferPtr = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");

  if (!glDrawArraysIndirectPtr) {
    LOG_ERROR("GPUParticleSystem: glDrawArraysIndirect not available!");
    return;
  }

  if (!glBindBufferPtr) {
    LOG_ERROR("GPUParticleSystem: glBindBuffer not available!");
    return;
  }

  // Create resources
  LoadShaders();
  CreateBuffers();
  CreateQuadVAO();

  using namespace NoMoreDay::Constants::Render;
  m_stagedParticles.reserve(PARTICLE_STAGING_RESERVE);
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

  m_stagedParticles.clear();
  m_initialized = false;
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

  // Atomic counter buffer (4 bytes)
  uint32_t zero = 0;
  m_atomicBuffer.Create(sizeof(uint32_t));
  m_atomicBuffer.Update(&zero, sizeof(zero));
  LOG_DEBUG("GPUParticleSystem: Created atomic buffer");
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
  if (m_stagedParticles.size() < (size_t)m_maxParticles) {
    m_stagedParticles.push_back(particle);
  }
}

void GPUParticleSystem::EmitBatch(
    const std::vector<components::GPUParticle> &particles) {
  if (particles.empty())
    return;
  for (const auto &p : particles) {
    Emit(p);
  }
}

void GPUParticleSystem::Update(float dt) {
  if (!m_initialized || m_computeShader.id == 0)
    return;

  // 0. Determine Buffers for Ping-Pong
  // m_pingPong = false: Input=ParticleBuffer, Output=CompactBuffer
  // m_pingPong = true:  Input=CompactBuffer,  Output=ParticleBuffer
  core::ComputeBuffer &bufIn = m_pingPong ? m_compactBuffer : m_particleBuffer;
  core::ComputeBuffer &bufOut = m_pingPong ? m_particleBuffer : m_compactBuffer;

  // 1. Reset atomic counter to 0 (This counts ALIVE particles compacted)
  uint32_t zero = 0;
  m_atomicBuffer.Update(&zero, sizeof(zero));

  // 2. Dispatch compute shader
  rlEnableShader(m_computeShader.id);

  using namespace NoMoreDay::Constants::Render;
  float clampedDt = (dt > MAX_DELTA_TIME_PARTICLES) ? DEFAULT_DELTA_TIME_PARTICLES : dt;
  rlSetUniform(m_computeDtLoc, &clampedDt, RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(m_computeTotalLoc, &m_currentParticleCount,
               RL_SHADER_UNIFORM_INT, 1);

  // Bind SSBOs
  bufIn.BindBase(0);  // Input: Read from last frame's valid state
  bufOut.BindBase(1); // Output: Write alive particles here
  m_indirectBuffer.BindBase(2);
  m_atomicBuffer.BindBase(3);

  // Dispatch
  using namespace NoMoreDay::Constants::Render;
  int workGroups = (m_currentParticleCount + (WORKGROUP_SIZE_PARTICLES - 1)) / WORKGROUP_SIZE_PARTICLES;
  if (workGroups > 0) {
    rlComputeShaderDispatch(workGroups, 1, 1);
  }

  // Memory barrier to ensure compute results are visible
  utils::GPUUtils::MemoryBarrier();

  rlDisableShader();

  // 3. OPTIMIZATION: Removed sync readback of alive count for rendering.
  // The compute shader now writes instanceCount directly to the indirect
  // buffer. We only read it back if we need it for CPU-side appending logic,
  // which we can optimize.
  uint32_t aliveCount = 0;
  m_atomicBuffer.Read(&aliveCount, sizeof(uint32_t));

  // 4. Append New Particles to Output Buffer
  // They are appended AFTER the compacted alive particles
  if (!m_stagedParticles.empty()) {
    int newCount = (int)m_stagedParticles.size();
    size_t structSize = sizeof(components::GPUParticle);

    // Ensure we don't overflow
    if (aliveCount + newCount > (uint32_t)m_maxParticles) {
      newCount = m_maxParticles - aliveCount;
    }

    if (newCount > 0) {
      bufOut.Update(m_stagedParticles.data(), newCount * structSize,
                    aliveCount * structSize);
      aliveCount += newCount;
    }
    m_stagedParticles.clear();
  }

  // 5. Update state for next frame
  m_currentParticleCount = aliveCount;

  // 6. Indirect Buffer is already updated by GPU (instanceCount)
  // DrawArraysIndirectCommand cmd = { 6, aliveCount, 0, 0 };
  // m_indirectBuffer.Update(&cmd, sizeof(cmd));

  // 7. Swap Buffers for next frame
  m_pingPong = !m_pingPong;
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
  core::ComputeBuffer &bufferToRender =
      m_pingPong ? m_compactBuffer : m_particleBuffer;
  bufferToRender.BindBase(0);

  // Enable VAO
  rlEnableVertexArray(m_quadVAO);
  rlDisableDepthTest();       // Ensure depth test is off
  rlDisableBackfaceCulling(); // Ensure we see both sides

  // Indirect Draw using the buffer updated by GPU
  if (glDrawArraysIndirectPtr) {
    m_indirectBuffer.Bind(GL_DRAW_INDIRECT_BUFFER);
    glDrawArraysIndirectPtr(GL_TRIANGLES, 0);
    glBindBufferPtr(GL_DRAW_INDIRECT_BUFFER, 0);
  } else if (m_currentParticleCount > 0) {
    // Fallback for safety
    rlDrawVertexArrayInstanced(0, 6, m_currentParticleCount);
  }

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
  return res;
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
  return res;
}

std::vector<components::GPUParticle>
InkEffectHelper::CreateDashEffect(Vector2 startPos, Vector2 dir, Color color,
                                  float dashLength, int count) {

  std::vector<components::GPUParticle> res;
  res.reserve(count);

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
  return res;
}

std::vector<components::GPUParticle>
InkEffectHelper::CreateAreaEffect(Vector2 center, float radius, Color coreColor,
                                  Color edgeColor, int count, float duration) {

  std::vector<components::GPUParticle> res;
  res.reserve(count);

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
  return res;
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

  return res;
}

} // namespace NoMoreDay::systems