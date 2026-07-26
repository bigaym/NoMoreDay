#include "engine/render/passes/FluidSimulationPass.hpp"

#include "app/SharedContext.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raymath.h"
#include "rlgl.h"

#include <entt/entt.hpp>
#include <algorithm>
#include <cmath>

namespace NoMoreDay::render::passes {
namespace {

using namespace entt::literals;

constexpr uint32_t kLocalSize = 64u;
constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLTexture0 = 0x84C0;
constexpr uint32_t kGLReadOnly = 0x88B8;
constexpr uint32_t kGLReadWrite = 0x88BA;
constexpr uint32_t kGLWriteOnly = 0x88B9;
constexpr uint32_t kGLR8 = 0x8229;
constexpr uint32_t kGLR16f = 0x822D;
constexpr uint32_t kGLRgba16f = 0x881A;
constexpr uint32_t kTextureFetchBarrierBit = 0x00000008;
constexpr float kDefaultSmoothingRadius = 18.0f;
constexpr float kDefaultRestDensity = 1.0f;
constexpr float kDefaultStiffness = 22.0f;
constexpr float kDefaultViscosity = 0.14f;
constexpr float kDefaultSurfaceTension = 0.02f;
constexpr float kParticleRadius = 6.0f;

uint32_t DivUp(const uint32_t value, const uint32_t divisor) {
  return (value + divisor - 1u) / divisor;
}

float ClampDeltaTime(const float dt) {
  const float fallback = 1.0f / 60.0f;
  if (!(dt > 0.0f)) {
    return fallback;
  }
  return std::clamp(dt, 1.0f / 240.0f, 1.0f / 24.0f);
}

} // namespace

FluidSimulationPass::FluidSimulationPass() = default;

FluidSimulationPass::~FluidSimulationPass() { Shutdown(); }

void FluidSimulationPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::SceneHdrColor,
               graph::RenderOwnerTag::FluidSimulation);
  builder.Read(graph::RenderResourceTag::DistanceField,
               graph::RenderOwnerTag::FluidSimulation);
  builder.Read(graph::RenderResourceTag::RadianceMap,
               graph::RenderOwnerTag::FluidSimulation);
  builder.Write(graph::RenderResourceTag::SceneHdrColor,
                graph::RenderOwnerTag::FluidSimulation);
}

bool FluidSimulationPass::Initialize(ResourceManager &resources) {
  if (m_initialized) {
    return true;
  }
  if (!LoadShaders()) {
    if (m_lastFailureReason.empty()) {
      m_lastFailureReason = "fluid shader program load failed";
    }
    return false;
  }

  const float quadVertices[] = {
      -0.5f, -0.5f, 0.0f, 0.0f, 0.5f,  -0.5f, 1.0f, 0.0f, -0.5f,
      0.5f,  0.0f,  1.0f, 0.5f, -0.5f, 1.0f, 0.0f, 0.5f,  0.5f,
      1.0f,  1.0f,  -0.5f, 0.5f, 0.0f, 1.0f,
  };

  m_quadVao = rlLoadVertexArray();
  if (m_quadVao == 0u) {
    m_lastFailureReason = "failed to allocate fluid quad VAO";
    UnloadShaders();
    return false;
  }

  rlEnableVertexArray(m_quadVao);
  m_quadVbo = rlLoadVertexBuffer(quadVertices, sizeof(quadVertices), false);
  if (m_quadVbo == 0u) {
    rlDisableVertexArray();
    m_lastFailureReason = "failed to allocate fluid quad VBO";
    Shutdown();
    return false;
  }

  rlSetVertexAttribute(0, 2, RL_FLOAT, false, 4 * sizeof(float), 0);
  rlEnableVertexAttribute(0);
  rlSetVertexAttribute(1, 2, RL_FLOAT, false, 4 * sizeof(float),
                       2 * sizeof(float));
  rlEnableVertexAttribute(1);
  rlDisableVertexArray();

  // Keep initialization signature aligned with other passes that route through
  // ResourceManager.
  (void)resources;
  m_initialized = true;
  m_lastFailureReason.clear();
  return true;
}

void FluidSimulationPass::Shutdown() {
  ReleaseRuntimeBuffers();
  UnloadShaders();

  if (m_quadVao != 0u) {
    rlUnloadVertexArray(m_quadVao);
    m_quadVao = 0u;
  }
  if (m_quadVbo != 0u) {
    rlUnloadVertexBuffer(m_quadVbo);
    m_quadVbo = 0u;
  }

  m_initialized = false;
  m_particlesReadPing = true;
  m_frameIndex = 0u;
  m_maxParticles = 0u;
  m_lastFailureReason.clear();
}

void FluidSimulationPass::OnResize(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  if (m_cachedWidth == width && m_cachedHeight == height) {
    return;
  }
  m_cachedWidth = width;
  m_cachedHeight = height;
  ReleaseRuntimeBuffers();
}

bool FluidSimulationPass::EnsureRuntimeBuffers(const uint32_t maxParticles,
                                               const int width, const int height,
                                               const Camera2D &camera) {
  if (maxParticles == 0u || width <= 0 || height <= 0) {
    return false;
  }
  if (m_runtimeBuffersReady && maxParticles == m_maxParticles &&
      width == m_cachedWidth && height == m_cachedHeight) {
    return true;
  }

  ReleaseRuntimeBuffers();

  m_maxParticles = maxParticles;
  m_cachedWidth = width;
  m_cachedHeight = height;
  m_particlesReadPing = true;

  const float zoom = std::max(0.0001f, camera.zoom);
  const float worldWidth = static_cast<float>(width) / zoom;
  const float worldHeight = static_cast<float>(height) / zoom;
  const float cellSize = std::max(1.0f, kDefaultSmoothingRadius);
  m_gridWidth =
      std::max(1, static_cast<int>(std::ceil(worldWidth / cellSize)) + 2);
  m_gridHeight =
      std::max(1, static_cast<int>(std::ceil(worldHeight / cellSize)) + 2);
  const size_t gridCellCount =
      static_cast<size_t>(m_gridWidth) * static_cast<size_t>(m_gridHeight);

  const size_t particleBytes =
      static_cast<size_t>(maxParticles) * sizeof(components::GPUFluidParticle);
  const size_t cellCoordBytes = static_cast<size_t>(maxParticles) * sizeof(uint32_t);
  const size_t cellCountBytes = std::max<size_t>(1u, gridCellCount) * sizeof(uint32_t);
  const size_t neighborListBytes = static_cast<size_t>(maxParticles) * kMaxNeighbors *
                                   sizeof(uint32_t);
  const size_t neighborCountBytes =
      static_cast<size_t>(maxParticles) * sizeof(uint32_t);

  m_particlePing.Create(particleBytes, nullptr, RL_DYNAMIC_DRAW);
  m_particlePong.Create(particleBytes, nullptr, RL_DYNAMIC_DRAW);
  m_cellCoordBuffer.Create(cellCoordBytes, nullptr, RL_DYNAMIC_DRAW);
  m_cellCountBuffer.Create(cellCountBytes, nullptr, RL_DYNAMIC_DRAW);
  m_neighborListBuffer.Create(neighborListBytes, nullptr, RL_DYNAMIC_DRAW);
  m_neighborCountBuffer.Create(neighborCountBytes, nullptr, RL_DYNAMIC_DRAW);
  m_configBuffer.Create(sizeof(components::GPUFluidConfig), nullptr, RL_DYNAMIC_DRAW);

  if (m_particlePing.GetId() == 0 || m_particlePong.GetId() == 0 ||
      m_cellCoordBuffer.GetId() == 0 || m_cellCountBuffer.GetId() == 0 ||
      m_neighborListBuffer.GetId() == 0 || m_neighborCountBuffer.GetId() == 0 ||
      m_configBuffer.GetId() == 0) {
    m_lastFailureReason = "fluid runtime buffer allocation failed";
    ReleaseRuntimeBuffers();
    return false;
  }

  m_zeroCellCounts.assign(std::max<size_t>(1u, gridCellCount), 0u);
  m_seedData.resize(maxParticles);
  SeedParticles(maxParticles, camera, width, height);
  m_particlePing.Update(m_seedData.data(), particleBytes, 0u);
  m_particlePong.Update(m_seedData.data(), particleBytes, 0u);

  m_runtimeBuffersReady = true;
  m_lastFailureReason.clear();
  return true;
}

void FluidSimulationPass::SeedParticles(const uint32_t count, const Camera2D &camera,
                                        const int width, const int height) {
  if (count == 0u) {
    return;
  }

  const float zoom = std::max(0.0001f, camera.zoom);
  const Vector2 worldMin = {
      camera.target.x - (camera.offset.x / zoom),
      camera.target.y - (camera.offset.y / zoom),
  };
  const Vector2 worldSize = {
      static_cast<float>(width) / zoom,
      static_cast<float>(height) / zoom,
  };
  const Vector2 emitterCenter = {worldMin.x + worldSize.x * 0.5f,
                                 worldMin.y + worldSize.y * 0.55f};

  const uint32_t columns = std::max<uint32_t>(
      1u, static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count)))));
  const float spacing = kDefaultSmoothingRadius * 0.62f;

  for (uint32_t i = 0u; i < count; ++i) {
    const uint32_t row = i / columns;
    const uint32_t col = i % columns;
    const float jitterX = static_cast<float>((i * 13u) % 17u) * 0.11f - 0.9f;
    const float jitterY = static_cast<float>((i * 7u) % 19u) * 0.09f - 0.8f;

    components::GPUFluidParticle particle = {};
    particle.position = {emitterCenter.x +
                             (static_cast<float>(col) - static_cast<float>(columns) * 0.5f) *
                                 spacing +
                             jitterX,
                         emitterCenter.y + static_cast<float>(row) * spacing + jitterY};
    particle.velocity = {0.0f, 0.0f};
    particle.density = kDefaultRestDensity;
    particle.pressure = 0.0f;
    particle.color = {0.95f, 0.33f, 0.15f, 1.2f};
    particle.lifetime = static_cast<float>(i % 360u) / 360.0f;
    particle.flags = 1u;
    m_seedData[i] = particle;
  }
}

void FluidSimulationPass::UploadConfig(const uint32_t maxParticles) {
  if (m_configBuffer.GetId() == 0u) {
    return;
  }
  components::GPUFluidConfig config = {};
  config.smoothingRadius = kDefaultSmoothingRadius;
  config.restDensity = kDefaultRestDensity;
  config.stiffness = kDefaultStiffness;
  config.viscosity = kDefaultViscosity;
  config.gravity = {0.0f, 120.0f};
  config.surfaceTension = kDefaultSurfaceTension;
  config.maxParticles = maxParticles;
  m_configBuffer.Update(&config, sizeof(config), 0u);
}

bool FluidSimulationPass::DispatchGridHash(const uint32_t particleCount) {
  if (m_gridHashShader.id == 0 || m_cellCountBuffer.GetId() == 0u ||
      m_cellCoordBuffer.GetId() == 0u || particleCount == 0u) {
    return false;
  }

  if (!m_zeroCellCounts.empty()) {
    m_cellCountBuffer.Update(m_zeroCellCounts.data(),
                             m_zeroCellCounts.size() * sizeof(uint32_t), 0u);
  }

  rlEnableShader(m_gridHashShader.id);
  const int particleCountInt = static_cast<int>(particleCount);
  if (m_gridParticleCountLoc >= 0) {
    rlSetUniform(m_gridParticleCountLoc, &particleCountInt, RL_SHADER_UNIFORM_INT, 1);
  }
  const float cellSize = kDefaultSmoothingRadius;
  if (m_gridCellSizeLoc >= 0) {
    rlSetUniform(m_gridCellSizeLoc, &cellSize, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  const float gridOrigin[2] = {0.0f, 0.0f};
  if (m_gridOriginLoc >= 0) {
    rlSetUniform(m_gridOriginLoc, gridOrigin, RL_SHADER_UNIFORM_VEC2, 1);
  }
  const int gridDim[2] = {m_gridWidth, m_gridHeight};
  if (m_gridDimLoc >= 0) {
    rlSetUniform(m_gridDimLoc, gridDim, RL_SHADER_UNIFORM_IVEC2, 1);
  }

  utils::GPUUtils::BindBufferBase(0u, CurrentParticleBufferId());
  utils::GPUUtils::BindBufferBase(1u, m_cellCoordBuffer.GetId());
  utils::GPUUtils::BindBufferBase(2u, m_cellCountBuffer.GetId());
  utils::GPUUtils::DispatchComputeNoBarrier(DivUp(particleCount, kLocalSize), 1u, 1u);
  rlDisableShader();
  utils::GPUUtils::MemoryBarrier(static_cast<uint32_t>(RenderConstants::Barrier::Buffer));
  return true;
}

bool FluidSimulationPass::DispatchNeighborSearch(const uint32_t particleCount) {
  if (m_neighborSearchShader.id == 0 || m_neighborListBuffer.GetId() == 0u ||
      m_neighborCountBuffer.GetId() == 0u || particleCount == 0u) {
    return false;
  }

  rlEnableShader(m_neighborSearchShader.id);
  const int particleCountInt = static_cast<int>(particleCount);
  if (m_neighborParticleCountLoc >= 0) {
    rlSetUniform(m_neighborParticleCountLoc, &particleCountInt, RL_SHADER_UNIFORM_INT,
                 1);
  }
  const int maxNeighborsInt = static_cast<int>(kMaxNeighbors);
  if (m_neighborMaxNeighborsLoc >= 0) {
    rlSetUniform(m_neighborMaxNeighborsLoc, &maxNeighborsInt, RL_SHADER_UNIFORM_INT,
                 1);
  }
  const float radius = kDefaultSmoothingRadius;
  if (m_neighborRadiusLoc >= 0) {
    rlSetUniform(m_neighborRadiusLoc, &radius, RL_SHADER_UNIFORM_FLOAT, 1);
  }

  utils::GPUUtils::BindBufferBase(0u, CurrentParticleBufferId());
  utils::GPUUtils::BindBufferBase(1u, m_cellCoordBuffer.GetId());
  utils::GPUUtils::BindBufferBase(3u, m_neighborListBuffer.GetId());
  utils::GPUUtils::BindBufferBase(4u, m_neighborCountBuffer.GetId());
  utils::GPUUtils::DispatchComputeNoBarrier(DivUp(particleCount, kLocalSize), 1u, 1u);
  rlDisableShader();
  utils::GPUUtils::MemoryBarrier(static_cast<uint32_t>(RenderConstants::Barrier::Buffer));
  return true;
}

bool FluidSimulationPass::DispatchDensity(const uint32_t particleCount,
                                          const float deltaTime) {
  (void)deltaTime;
  if (m_densityShader.id == 0 || particleCount == 0u) {
    return false;
  }

  rlEnableShader(m_densityShader.id);
  const int particleCountInt = static_cast<int>(particleCount);
  const int maxNeighborsInt = static_cast<int>(kMaxNeighbors);
  if (m_densityParticleCountLoc >= 0) {
    rlSetUniform(m_densityParticleCountLoc, &particleCountInt, RL_SHADER_UNIFORM_INT,
                 1);
  }
  if (m_densityMaxNeighborsLoc >= 0) {
    rlSetUniform(m_densityMaxNeighborsLoc, &maxNeighborsInt, RL_SHADER_UNIFORM_INT, 1);
  }

  utils::GPUUtils::BindBufferBase(0u, CurrentParticleBufferId());
  utils::GPUUtils::BindBufferBase(3u, m_neighborListBuffer.GetId());
  utils::GPUUtils::BindBufferBase(4u, m_neighborCountBuffer.GetId());
  utils::GPUUtils::BindBufferBase(5u, AlternateParticleBufferId());
  utils::GPUUtils::BindBufferBase(6u, m_configBuffer.GetId());
  utils::GPUUtils::DispatchComputeNoBarrier(DivUp(particleCount, kLocalSize), 1u, 1u);
  rlDisableShader();
  utils::GPUUtils::MemoryBarrier(static_cast<uint32_t>(RenderConstants::Barrier::Buffer));
  SwapParticleBuffers();
  return true;
}

bool FluidSimulationPass::DispatchForce(const uint32_t particleCount,
                                        const float deltaTime) {
  if (m_forceShader.id == 0 || particleCount == 0u) {
    return false;
  }

  rlEnableShader(m_forceShader.id);
  const int particleCountInt = static_cast<int>(particleCount);
  const int maxNeighborsInt = static_cast<int>(kMaxNeighbors);
  if (m_forceParticleCountLoc >= 0) {
    rlSetUniform(m_forceParticleCountLoc, &particleCountInt, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_forceMaxNeighborsLoc >= 0) {
    rlSetUniform(m_forceMaxNeighborsLoc, &maxNeighborsInt, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_forceDeltaTimeLoc >= 0) {
    rlSetUniform(m_forceDeltaTimeLoc, &deltaTime, RL_SHADER_UNIFORM_FLOAT, 1);
  }

  utils::GPUUtils::BindBufferBase(0u, CurrentParticleBufferId());
  utils::GPUUtils::BindBufferBase(3u, m_neighborListBuffer.GetId());
  utils::GPUUtils::BindBufferBase(4u, m_neighborCountBuffer.GetId());
  utils::GPUUtils::BindBufferBase(5u, AlternateParticleBufferId());
  utils::GPUUtils::BindBufferBase(6u, m_configBuffer.GetId());
  utils::GPUUtils::DispatchComputeNoBarrier(DivUp(particleCount, kLocalSize), 1u, 1u);
  rlDisableShader();
  utils::GPUUtils::MemoryBarrier(static_cast<uint32_t>(RenderConstants::Barrier::Buffer));
  SwapParticleBuffers();
  return true;
}

bool FluidSimulationPass::DispatchIntegrate(const graph::RenderContext &context,
                                            const uint32_t particleCount,
                                            const float deltaTime) {
  if (m_integrateShader.id == 0 || particleCount == 0u || context.camera == nullptr) {
    return false;
  }

  rlEnableShader(m_integrateShader.id);

  const int particleCountInt = static_cast<int>(particleCount);
  if (m_integrateParticleCountLoc >= 0) {
    rlSetUniform(m_integrateParticleCountLoc, &particleCountInt, RL_SHADER_UNIFORM_INT,
                 1);
  }
  if (m_integrateDeltaTimeLoc >= 0) {
    rlSetUniform(m_integrateDeltaTimeLoc, &deltaTime, RL_SHADER_UNIFORM_FLOAT, 1);
  }

  const float zoom = std::max(context.camera->zoom, 0.0001f);
  const float screenSize[2] = {static_cast<float>(m_cachedWidth) / zoom,
                               static_cast<float>(m_cachedHeight) / zoom};
  const float cameraOffset[2] = {
      context.camera->target.x - (context.camera->offset.x / zoom),
      context.camera->target.y - (context.camera->offset.y / zoom),
  };
  const float boundsMin[2] = {cameraOffset[0], cameraOffset[1]};
  const float boundsMax[2] = {cameraOffset[0] + screenSize[0],
                              cameraOffset[1] + screenSize[1]};
  const float emitter[2] = {cameraOffset[0] + screenSize[0] * 0.5f,
                            cameraOffset[1] + screenSize[1] * 0.25f};

  if (m_integrateBoundsMinLoc >= 0) {
    rlSetUniform(m_integrateBoundsMinLoc, boundsMin, RL_SHADER_UNIFORM_VEC2, 1);
  }
  if (m_integrateBoundsMaxLoc >= 0) {
    rlSetUniform(m_integrateBoundsMaxLoc, boundsMax, RL_SHADER_UNIFORM_VEC2, 1);
  }
  if (m_integrateEmitterLoc >= 0) {
    rlSetUniform(m_integrateEmitterLoc, emitter, RL_SHADER_UNIFORM_VEC2, 1);
  }
  const int frameIndexInt = static_cast<int>(m_frameIndex);
  if (m_integrateFrameIndexLoc >= 0) {
    rlSetUniform(m_integrateFrameIndexLoc, &frameIndexInt, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_integrateCameraOffsetLoc >= 0) {
    rlSetUniform(m_integrateCameraOffsetLoc, cameraOffset, RL_SHADER_UNIFORM_VEC2, 1);
  }
  if (m_integrateScreenSizeLoc >= 0) {
    rlSetUniform(m_integrateScreenSizeLoc, screenSize, RL_SHADER_UNIFORM_VEC2, 1);
  }

  const int distanceFieldTextureUnit = 0;
  const bool hasDistanceField = context.giDistanceFieldTexture != 0u &&
                                context.giDistanceFieldWidth > 0 &&
                                context.giDistanceFieldHeight > 0;
  if (m_integrateDistanceFieldTexLoc >= 0) {
    rlSetUniform(m_integrateDistanceFieldTexLoc, &distanceFieldTextureUnit,
                 RL_SHADER_UNIFORM_INT, 1);
  }
  const int useDistanceField = hasDistanceField ? 1 : 0;
  if (m_integrateUseDistanceFieldLoc >= 0) {
    rlSetUniform(m_integrateUseDistanceFieldLoc, &useDistanceField,
                 RL_SHADER_UNIFORM_INT, 1);
  }
  if (hasDistanceField) {
    utils::GPUUtils::ActiveTexture(kGLTexture0);
    utils::GPUUtils::BindTexture(kGLTexture2D, context.giDistanceFieldTexture);
    utils::GPUUtils::BindImageTexture(RenderConstants::V5GI::kDistanceFieldImageBinding,
                                      context.giDistanceFieldTexture, 0, false, 0,
                                      kGLReadOnly, kGLR16f);
  }

  utils::GPUUtils::BindBufferBase(0u, CurrentParticleBufferId());
  utils::GPUUtils::BindBufferBase(5u, AlternateParticleBufferId());
  utils::GPUUtils::BindBufferBase(6u, m_configBuffer.GetId());
  utils::GPUUtils::DispatchComputeNoBarrier(DivUp(particleCount, kLocalSize), 1u, 1u);
  rlDisableShader();

  const uint32_t barrierBits = static_cast<uint32_t>(RenderConstants::Barrier::Buffer) |
                               kTextureFetchBarrierBit;
  utils::GPUUtils::MemoryBarrier(barrierBits);
  SwapParticleBuffers();
  return true;
}

void FluidSimulationPass::InjectEmissive(const graph::RenderContext &context,
                                         const uint32_t particleCount) {
  // SPH Isolation: Fluid simulation is prohibited from writing to production GI resources.
  (void)context;
  (void)particleCount;
  return;
}

void FluidSimulationPass::InjectOccluderMask(const graph::RenderContext &context,
                                             const uint32_t particleCount) {
  // SPH Isolation: Fluid simulation is prohibited from writing to production GI resources.
  (void)context;
  (void)particleCount;
  return;
}

void FluidSimulationPass::RenderParticles(const graph::RenderContext &context,
                                          const uint32_t particleCount) {
  if (m_renderShader.id == 0 || m_quadVao == 0u || particleCount == 0u ||
      context.camera == nullptr) {
    return;
  }

  const Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  const int particleCountInt = static_cast<int>(particleCount);
  const int radianceTextureUnit = 0;
  const int useRadiance = (context.giRadianceTexture != 0u) ? 1 : 0;

  const float zoom = std::max(context.camera->zoom, 0.0001f);
  const float cameraOffset[2] = {
      context.camera->target.x - (context.camera->offset.x / zoom),
      context.camera->target.y - (context.camera->offset.y / zoom),
  };
  const float screenSize[2] = {static_cast<float>(m_cachedWidth) / zoom,
                               static_cast<float>(m_cachedHeight) / zoom};

  rlDrawRenderBatchActive();
  BeginBlendMode(BLEND_ALPHA);
  rlDisableDepthTest();
  rlDisableBackfaceCulling();

  rlEnableShader(m_renderShader.id);
  if (m_renderMvpLoc >= 0) {
    rlSetUniformMatrix(m_renderMvpLoc, mvp);
  }
  if (m_renderRadiusLoc >= 0) {
    rlSetUniform(m_renderRadiusLoc, &kParticleRadius, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  if (m_renderParticleCountLoc >= 0) {
    rlSetUniform(m_renderParticleCountLoc, &particleCountInt, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_renderRadianceLoc >= 0) {
    rlSetUniform(m_renderRadianceLoc, &radianceTextureUnit, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_renderUseRadianceLoc >= 0) {
    rlSetUniform(m_renderUseRadianceLoc, &useRadiance, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_renderCameraOffsetLoc >= 0) {
    rlSetUniform(m_renderCameraOffsetLoc, cameraOffset, RL_SHADER_UNIFORM_VEC2, 1);
  }
  if (m_renderScreenSizeLoc >= 0) {
    rlSetUniform(m_renderScreenSizeLoc, screenSize, RL_SHADER_UNIFORM_VEC2, 1);
  }
  if (m_renderRestDensityLoc >= 0) {
    rlSetUniform(m_renderRestDensityLoc, &kDefaultRestDensity, RL_SHADER_UNIFORM_FLOAT,
                 1);
  }

  if (useRadiance != 0) {
    utils::GPUUtils::ActiveTexture(kGLTexture0);
    utils::GPUUtils::BindTexture(kGLTexture2D, context.giRadianceTexture);
  }

  utils::GPUUtils::BindBufferBase(0u, CurrentParticleBufferId());
  rlEnableVertexArray(m_quadVao);
  rlDrawVertexArrayInstanced(0, 6, static_cast<int>(particleCount));
  rlDisableVertexArray();
  rlDisableShader();

  EndBlendMode();
  rlDrawRenderBatchActive();
}

bool FluidSimulationPass::ShouldEnableGiInteraction() const {
  return m_interactionEnabled && m_occluderExtractPass != nullptr &&
         m_radiancePass != nullptr;
}

bool FluidSimulationPass::LoadShaders() {
  // Shader creation is delayed until Execute() where ResourceManager is always
  // available through SharedContext.
  return true;
}

void FluidSimulationPass::UnloadShaders() {
  m_gridHashShader = {};
  m_neighborSearchShader = {};
  m_densityShader = {};
  m_forceShader = {};
  m_integrateShader = {};
  m_emissiveInjectShader = {};
  m_occluderInjectShader = {};
  m_renderShader = {};

  m_gridParticleCountLoc = -1;
  m_gridCellSizeLoc = -1;
  m_gridOriginLoc = -1;
  m_gridDimLoc = -1;

  m_neighborParticleCountLoc = -1;
  m_neighborMaxNeighborsLoc = -1;
  m_neighborRadiusLoc = -1;

  m_densityParticleCountLoc = -1;
  m_densityMaxNeighborsLoc = -1;

  m_forceParticleCountLoc = -1;
  m_forceMaxNeighborsLoc = -1;
  m_forceDeltaTimeLoc = -1;

  m_integrateParticleCountLoc = -1;
  m_integrateDeltaTimeLoc = -1;
  m_integrateBoundsMinLoc = -1;
  m_integrateBoundsMaxLoc = -1;
  m_integrateEmitterLoc = -1;
  m_integrateFrameIndexLoc = -1;
  m_integrateCameraOffsetLoc = -1;
  m_integrateScreenSizeLoc = -1;
  m_integrateDistanceFieldTexLoc = -1;
  m_integrateUseDistanceFieldLoc = -1;

  m_emissiveParticleCountLoc = -1;
  m_emissiveResolutionLoc = -1;
  m_emissiveCameraOffsetLoc = -1;
  m_emissiveScreenSizeLoc = -1;
  m_emissiveThresholdLoc = -1;

  m_occluderParticleCountLoc = -1;
  m_occluderResolutionLoc = -1;
  m_occluderCameraOffsetLoc = -1;
  m_occluderScreenSizeLoc = -1;
  m_occluderDensityThresholdLoc = -1;

  m_renderMvpLoc = -1;
  m_renderRadiusLoc = -1;
  m_renderParticleCountLoc = -1;
  m_renderRadianceLoc = -1;
  m_renderUseRadianceLoc = -1;
  m_renderCameraOffsetLoc = -1;
  m_renderScreenSizeLoc = -1;
  m_renderRestDensityLoc = -1;
}

void FluidSimulationPass::ReleaseRuntimeBuffers() {
  m_particlePing.Release();
  m_particlePong.Release();
  m_cellCoordBuffer.Release();
  m_cellCountBuffer.Release();
  m_neighborListBuffer.Release();
  m_neighborCountBuffer.Release();
  m_configBuffer.Release();

  m_seedData.clear();
  m_zeroCellCounts.clear();
  m_runtimeBuffersReady = false;
  m_gridWidth = 0;
  m_gridHeight = 0;
  m_maxParticles = 0u;
  m_particlesReadPing = true;
}

uint32_t FluidSimulationPass::CurrentParticleBufferId() const {
  return m_particlesReadPing ? m_particlePing.GetId() : m_particlePong.GetId();
}

uint32_t FluidSimulationPass::AlternateParticleBufferId() const {
  return m_particlesReadPing ? m_particlePong.GetId() : m_particlePing.GetId();
}

void FluidSimulationPass::SwapParticleBuffers() {
  m_particlesReadPing = !m_particlesReadPing;
}

void FluidSimulationPass::Execute(graph::RenderContext &context) {
  ++m_frameIndex;
  m_lastFailureReason.clear();

  if (context.qualityManager == nullptr || context.shared == nullptr ||
      context.shared->resources == nullptr || context.camera == nullptr) {
    m_lastFailureReason = "missing fluid render prerequisites";
    return;
  }

  const auto &config = context.qualityManager->GetConfig();
  if (!config.fluidEnabled || config.fluidMaxParticles == 0u ||
      !context.hdrSceneBuffer.IsValid()) {
    if (m_runtimeBuffersReady) {
      ReleaseRuntimeBuffers();
    }
    return;
  }

  if (!m_initialized) {
    m_gridHashShader = context.shared->resources->loadComputeShader(
        "v5_fluid_gridhash_compute"_hs,
        "assets/shaders/lighting/v5_fluid_gridhash.comp");
    m_neighborSearchShader = context.shared->resources->loadComputeShader(
        "v5_fluid_neighbor_search_compute"_hs,
        "assets/shaders/lighting/v5_fluid_neighbor_search.comp");
    m_densityShader = context.shared->resources->loadComputeShader(
        "v5_fluid_density_compute"_hs,
        "assets/shaders/lighting/v5_fluid_density.comp");
    m_forceShader = context.shared->resources->loadComputeShader(
        "v5_fluid_force_compute"_hs,
        "assets/shaders/lighting/v5_fluid_force.comp");
    m_integrateShader = context.shared->resources->loadComputeShader(
        "v5_fluid_integrate_compute"_hs,
        "assets/shaders/lighting/v5_fluid_integrate.comp");
    m_emissiveInjectShader = context.shared->resources->loadComputeShader(
        "v5_fluid_emissive_inject_compute"_hs,
        "assets/shaders/lighting/v5_fluid_emissive_inject.comp");
    m_occluderInjectShader = context.shared->resources->loadComputeShader(
        "v5_fluid_occluder_inject_compute"_hs,
        "assets/shaders/lighting/v5_fluid_occluder_inject.comp");
    m_renderShader = context.shared->resources->loadShader(
        "v5_fluid_render_shader"_hs, "assets/shaders/lighting/v5_fluid_render.vert",
        "assets/shaders/lighting/v5_fluid_render.frag");

    if (m_gridHashShader.id == 0 || m_neighborSearchShader.id == 0 ||
        m_densityShader.id == 0 || m_forceShader.id == 0 || m_integrateShader.id == 0 ||
        m_emissiveInjectShader.id == 0 || m_occluderInjectShader.id == 0 ||
        m_renderShader.id == 0) {
      m_lastFailureReason = "failed to load one or more fluid shaders";
      Shutdown();
      return;
    }

    m_gridParticleCountLoc = rlGetLocationUniform(m_gridHashShader.id, "uParticleCount");
    m_gridCellSizeLoc = rlGetLocationUniform(m_gridHashShader.id, "uCellSize");
    m_gridOriginLoc = rlGetLocationUniform(m_gridHashShader.id, "uGridOrigin");
    m_gridDimLoc = rlGetLocationUniform(m_gridHashShader.id, "uGridDim");

    m_neighborParticleCountLoc =
        rlGetLocationUniform(m_neighborSearchShader.id, "uParticleCount");
    m_neighborMaxNeighborsLoc =
        rlGetLocationUniform(m_neighborSearchShader.id, "uMaxNeighbors");
    m_neighborRadiusLoc = rlGetLocationUniform(m_neighborSearchShader.id, "uSearchRadius");

    m_densityParticleCountLoc = rlGetLocationUniform(m_densityShader.id, "uParticleCount");
    m_densityMaxNeighborsLoc = rlGetLocationUniform(m_densityShader.id, "uMaxNeighbors");

    m_forceParticleCountLoc = rlGetLocationUniform(m_forceShader.id, "uParticleCount");
    m_forceMaxNeighborsLoc = rlGetLocationUniform(m_forceShader.id, "uMaxNeighbors");
    m_forceDeltaTimeLoc = rlGetLocationUniform(m_forceShader.id, "uDeltaTime");

    m_integrateParticleCountLoc =
        rlGetLocationUniform(m_integrateShader.id, "uParticleCount");
    m_integrateDeltaTimeLoc = rlGetLocationUniform(m_integrateShader.id, "uDeltaTime");
    m_integrateBoundsMinLoc = rlGetLocationUniform(m_integrateShader.id, "uBoundsMin");
    m_integrateBoundsMaxLoc = rlGetLocationUniform(m_integrateShader.id, "uBoundsMax");
    m_integrateEmitterLoc = rlGetLocationUniform(m_integrateShader.id, "uEmitter");
    m_integrateFrameIndexLoc = rlGetLocationUniform(m_integrateShader.id, "uFrameIndex");
    m_integrateCameraOffsetLoc =
        rlGetLocationUniform(m_integrateShader.id, "uCameraOffset");
    m_integrateScreenSizeLoc = rlGetLocationUniform(m_integrateShader.id, "uScreenSize");
    m_integrateDistanceFieldTexLoc =
        rlGetLocationUniform(m_integrateShader.id, "uDistanceFieldTex");
    m_integrateUseDistanceFieldLoc =
        rlGetLocationUniform(m_integrateShader.id, "uUseDistanceField");

    m_emissiveParticleCountLoc =
        rlGetLocationUniform(m_emissiveInjectShader.id, "uParticleCount");
    m_emissiveResolutionLoc =
        rlGetLocationUniform(m_emissiveInjectShader.id, "uResolution");
    m_emissiveCameraOffsetLoc =
        rlGetLocationUniform(m_emissiveInjectShader.id, "uCameraOffset");
    m_emissiveScreenSizeLoc =
        rlGetLocationUniform(m_emissiveInjectShader.id, "uScreenSize");
    m_emissiveThresholdLoc =
        rlGetLocationUniform(m_emissiveInjectShader.id, "uThreshold");

    m_occluderParticleCountLoc =
        rlGetLocationUniform(m_occluderInjectShader.id, "uParticleCount");
    m_occluderResolutionLoc =
        rlGetLocationUniform(m_occluderInjectShader.id, "uResolution");
    m_occluderCameraOffsetLoc =
        rlGetLocationUniform(m_occluderInjectShader.id, "uCameraOffset");
    m_occluderScreenSizeLoc =
        rlGetLocationUniform(m_occluderInjectShader.id, "uScreenSize");
    m_occluderDensityThresholdLoc =
        rlGetLocationUniform(m_occluderInjectShader.id, "uDensityThreshold");

    m_renderMvpLoc = rlGetLocationUniform(m_renderShader.id, "uViewProj");
    m_renderRadiusLoc = rlGetLocationUniform(m_renderShader.id, "uParticleRadius");
    m_renderParticleCountLoc = rlGetLocationUniform(m_renderShader.id, "uParticleCount");
    m_renderRadianceLoc = rlGetLocationUniform(m_renderShader.id, "uRadianceMap");
    m_renderUseRadianceLoc = rlGetLocationUniform(m_renderShader.id, "uUseRadiance");
    m_renderCameraOffsetLoc = rlGetLocationUniform(m_renderShader.id, "uCameraOffset");
    m_renderScreenSizeLoc = rlGetLocationUniform(m_renderShader.id, "uScreenSize");
    m_renderRestDensityLoc = rlGetLocationUniform(m_renderShader.id, "uRestDensity");

    if (!Initialize(*context.shared->resources)) {
      if (m_lastFailureReason.empty()) {
        m_lastFailureReason = "fluid initialization failed";
      }
      return;
    }
  }

  const int width = context.hdrSceneBuffer.width;
  const int height = context.hdrSceneBuffer.height;
  const uint32_t particleCount =
      std::min<uint32_t>(config.fluidMaxParticles, 10000u);
  if (!EnsureRuntimeBuffers(particleCount, width, height, *context.camera)) {
    if (m_lastFailureReason.empty()) {
      m_lastFailureReason = "fluid runtime buffers unavailable";
    }
    return;
  }

  const float deltaTime = ClampDeltaTime(GetFrameTime());
  UploadConfig(particleCount);

  if (!DispatchGridHash(particleCount) || !DispatchNeighborSearch(particleCount) ||
      !DispatchDensity(particleCount, deltaTime) ||
      !DispatchForce(particleCount, deltaTime) ||
      !DispatchIntegrate(context, particleCount, deltaTime)) {
    if (m_lastFailureReason.empty()) {
      m_lastFailureReason = "fluid compute pipeline dispatch failed";
    }
    return;
  }

  if (ShouldEnableGiInteraction() && config.giEnabled) {
    InjectEmissive(context, particleCount);
    InjectOccluderMask(context, particleCount);
  }

  RenderParticles(context, particleCount);
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes
