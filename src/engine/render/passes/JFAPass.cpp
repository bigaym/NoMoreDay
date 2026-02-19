#include "engine/render/passes/JFAPass.hpp"

#include "app/SharedContext.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include <entt/entt.hpp>
#include <algorithm>

namespace NoMoreDay::render::passes {
namespace {

using namespace entt::literals;

constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLTexture0 = 0x84C0;
constexpr uint32_t kGLReadOnly = 0x88B8;
constexpr uint32_t kGLWriteOnly = 0x88B9;
constexpr uint32_t kGLR8 = 0x8229;
constexpr uint32_t kGLR16f = 0x822D;
constexpr uint32_t kGLRg16ui = 0x823A;
constexpr uint32_t kGLShaderStorageBuffer = 0x90D2;
constexpr uint32_t kGLDynamicDraw = 0x88E8;
constexpr uint32_t kTextureFetchBarrierBit = 0x00000008;
constexpr uint32_t kComputeGroupSize = 8u;
constexpr uint32_t kOverflowBinding = 0u;

uint32_t DivUp(const uint32_t value, const uint32_t divisor) {
  return (value + divisor - 1u) / divisor;
}

int HighestPowerOfTwoLessEqual(const int value) noexcept {
  if (value <= 1) {
    return 1;
  }
  int result = 1;
  while ((result << 1) <= value) {
    result <<= 1;
  }
  return result;
}

} // namespace

JFAPass::JFAPass() = default;

JFAPass::~JFAPass() { Shutdown(); }

void JFAPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::OccluderMask,
               graph::RenderOwnerTag::OccluderExtract);
  builder.Write(graph::RenderResourceTag::DistanceField, graph::RenderOwnerTag::JFA);
}

bool JFAPass::Initialize(ResourceManager &resources) {
  if (m_initialized) {
    return true;
  }

  m_seedInitShader = resources.loadComputeShader(
      "v5_jfa_seed_init_compute"_hs, "assets/shaders/lighting/v5_seed_init.comp");
  m_jumpFloodShader = resources.loadComputeShader(
      "v5_jfa_jump_flood_compute"_hs,
      "assets/shaders/lighting/v5_jump_flood.comp");
  m_distanceResolveShader = resources.loadComputeShader(
      "v5_jfa_distance_resolve_compute"_hs,
      "assets/shaders/lighting/v5_distance_field.comp");
  m_upsampleShader = resources.loadComputeShader(
      "v5_jfa_upsample_compute"_hs,
      "assets/shaders/lighting/v5_distance_upsample.comp");
  if (m_seedInitShader.id == 0 || m_jumpFloodShader.id == 0 ||
      m_distanceResolveShader.id == 0 || m_upsampleShader.id == 0) {
    Shutdown();
    return false;
  }

  m_seedInitWorkResolutionLoc =
      rlGetLocationUniform(m_seedInitShader.id, "uWorkResolution");
  m_seedInitFullResolutionLoc =
      rlGetLocationUniform(m_seedInitShader.id, "uFullResolution");
  m_seedInitMaskTextureLoc = rlGetLocationUniform(m_seedInitShader.id, "uMaskTexture");

  m_jumpStepSizeLoc = rlGetLocationUniform(m_jumpFloodShader.id, "uStepSize");
  m_jumpWorkResolutionLoc =
      rlGetLocationUniform(m_jumpFloodShader.id, "uWorkResolution");
  m_jumpFullResolutionLoc =
      rlGetLocationUniform(m_jumpFloodShader.id, "uFullResolution");

  m_distanceWorkResolutionLoc =
      rlGetLocationUniform(m_distanceResolveShader.id, "uWorkResolution");
  m_distanceFullResolutionLoc =
      rlGetLocationUniform(m_distanceResolveShader.id, "uFullResolution");
  m_distanceMaskTextureLoc =
      rlGetLocationUniform(m_distanceResolveShader.id, "uMaskTexture");

  m_upsampleHalfResolutionLoc =
      rlGetLocationUniform(m_upsampleShader.id, "uHalfResolution");
  m_upsampleFullResolutionLoc =
      rlGetLocationUniform(m_upsampleShader.id, "uFullResolution");

  NoMoreDay::utils::GPUUtils::GenBuffers(1, &m_overflowCounterBuffer);
  if (m_overflowCounterBuffer == 0u) {
    Shutdown();
    return false;
  }
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer,
                                         m_overflowCounterBuffer);
  const uint32_t initialCounter = 0u;
  NoMoreDay::utils::GPUUtils::BufferData(kGLShaderStorageBuffer,
                                         sizeof(uint32_t), &initialCounter,
                                         kGLDynamicDraw);
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer, 0u);

  m_initialized = true;
  return true;
}

void JFAPass::Shutdown() {
  m_seedInitShader = {};
  m_jumpFloodShader = {};
  m_distanceResolveShader = {};
  m_upsampleShader = {};

  resources::FramebufferManager::Destroy(m_seedPing);
  resources::FramebufferManager::Destroy(m_seedPong);
  resources::FramebufferManager::Destroy(m_distanceFieldWork);
  resources::FramebufferManager::Destroy(m_distanceFieldFull);

  if (m_overflowCounterBuffer != 0u) {
    NoMoreDay::utils::GPUUtils::DeleteBuffers(1, &m_overflowCounterBuffer);
    m_overflowCounterBuffer = 0u;
  }

  m_seedInitWorkResolutionLoc = -1;
  m_seedInitFullResolutionLoc = -1;
  m_seedInitMaskTextureLoc = -1;
  m_jumpStepSizeLoc = -1;
  m_jumpWorkResolutionLoc = -1;
  m_jumpFullResolutionLoc = -1;
  m_distanceWorkResolutionLoc = -1;
  m_distanceFullResolutionLoc = -1;
  m_distanceMaskTextureLoc = -1;
  m_upsampleHalfResolutionLoc = -1;
  m_upsampleFullResolutionLoc = -1;

  m_fullWidth = 0;
  m_fullHeight = 0;
  m_workWidth = 0;
  m_workHeight = 0;
  m_frameIndex = 0u;
  m_lastOverflowCount = 0u;
  m_initialized = false;
  m_halfResolutionMode = false;
  m_usedFallbackPlus2ThisFrame = false;
  m_forceFallbackPlus2ForTesting = false;
  m_incrementalExperimentEnabled = false;
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = false;
  m_barrierAuditLogged = false;
  m_lastFailureReason.clear();
}

void JFAPass::OnResize(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  m_fullWidth = width;
  m_fullHeight = height;
  EnsureResources(width, height, m_halfResolutionMode);
}

bool JFAPass::EnsureResources(const int fullWidth, const int fullHeight,
                              const bool halfResolution) {
  if (fullWidth <= 0 || fullHeight <= 0) {
    return false;
  }

  const int workWidth =
      halfResolution ? std::max(1, (fullWidth + 1) / 2) : fullWidth;
  const int workHeight =
      halfResolution ? std::max(1, (fullHeight + 1) / 2) : fullHeight;
  m_halfResolutionMode = halfResolution;

  const uint32_t seedFormat = RenderConstants::V5GI::kSeedFieldFormat;
  const uint32_t distanceFormat = RenderConstants::V5GI::kDistanceFieldFormat;

  if (!m_seedPing.IsValid()) {
    m_seedPing = resources::FramebufferManager::Create(workWidth, workHeight,
                                                       seedFormat, false);
  } else if (m_seedPing.width != workWidth || m_seedPing.height != workHeight) {
    resources::FramebufferManager::Resize(m_seedPing, workWidth, workHeight);
  }

  if (!m_seedPong.IsValid()) {
    m_seedPong = resources::FramebufferManager::Create(workWidth, workHeight,
                                                       seedFormat, false);
  } else if (m_seedPong.width != workWidth || m_seedPong.height != workHeight) {
    resources::FramebufferManager::Resize(m_seedPong, workWidth, workHeight);
  }

  if (!m_distanceFieldWork.IsValid()) {
    m_distanceFieldWork = resources::FramebufferManager::Create(
        workWidth, workHeight, distanceFormat, false);
  } else if (m_distanceFieldWork.width != workWidth ||
             m_distanceFieldWork.height != workHeight) {
    resources::FramebufferManager::Resize(m_distanceFieldWork, workWidth,
                                          workHeight);
  }

  if (!m_distanceFieldFull.IsValid()) {
    m_distanceFieldFull = resources::FramebufferManager::Create(
        fullWidth, fullHeight, distanceFormat, false);
  } else if (m_distanceFieldFull.width != fullWidth ||
             m_distanceFieldFull.height != fullHeight) {
    resources::FramebufferManager::Resize(m_distanceFieldFull, fullWidth,
                                          fullHeight);
  }

  m_fullWidth = fullWidth;
  m_fullHeight = fullHeight;
  m_workWidth = workWidth;
  m_workHeight = workHeight;

  return m_seedPing.IsValid() && m_seedPong.IsValid() &&
         m_distanceFieldWork.IsValid() && m_distanceFieldFull.IsValid();
}

bool JFAPass::ClearOverflowCounter() {
  if (m_overflowCounterBuffer == 0u) {
    return false;
  }
  const uint32_t zero = 0u;
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer,
                                         m_overflowCounterBuffer);
  NoMoreDay::utils::GPUUtils::BufferSubData(kGLShaderStorageBuffer, 0,
                                            sizeof(uint32_t), &zero);
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer, 0u);
  return true;
}

uint32_t JFAPass::ReadOverflowCounter() const {
  if (m_overflowCounterBuffer == 0u) {
    return 0u;
  }
  uint32_t overflow = 0u;
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer,
                                         m_overflowCounterBuffer);
  NoMoreDay::utils::GPUUtils::GetBufferSubData(kGLShaderStorageBuffer, 0,
                                               sizeof(uint32_t), &overflow);
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer, 0u);
  return overflow;
}

bool JFAPass::RunSeedInit(const uint32_t occluderMaskTexture, const int fullWidth,
                          const int fullHeight) {
  if (m_seedInitShader.id == 0 || m_seedPing.colorTexture == 0u ||
      occluderMaskTexture == 0u) {
    return false;
  }

  rlEnableShader(m_seedInitShader.id);
  const int workResolution[2] = {m_workWidth, m_workHeight};
  const int fullResolution[2] = {fullWidth, fullHeight};
  if (m_seedInitWorkResolutionLoc >= 0) {
    rlSetUniform(m_seedInitWorkResolutionLoc, workResolution,
                 RL_SHADER_UNIFORM_IVEC2, 1);
  }
  if (m_seedInitFullResolutionLoc >= 0) {
    rlSetUniform(m_seedInitFullResolutionLoc, fullResolution,
                 RL_SHADER_UNIFORM_IVEC2, 1);
  }

  const int maskTexUnit = 0;
  if (m_seedInitMaskTextureLoc >= 0) {
    rlSetUniform(m_seedInitMaskTextureLoc, &maskTexUnit, RL_SHADER_UNIFORM_INT, 1);
  }

  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, occluderMaskTexture);
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::V5GI::kSeedOutputImageBinding, m_seedPing.colorTexture, 0,
      false, 0, kGLWriteOnly, kGLRg16ui);
  NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
      DivUp(static_cast<uint32_t>(m_workWidth), kComputeGroupSize),
      DivUp(static_cast<uint32_t>(m_workHeight), kComputeGroupSize), 1);
  rlDisableShader();

  const uint32_t barrierBits = static_cast<uint32_t>(RenderConstants::Barrier::Image) |
                               kTextureFetchBarrierBit;
  NoMoreDay::utils::GPUUtils::MemoryBarrier(barrierBits);
  return true;
}

bool JFAPass::RunJumpFloodStep(const int stepSize, const int fullWidth,
                               const int fullHeight,
                               const uint32_t inputSeedTexture,
                               const uint32_t outputSeedTexture) {
  if (m_jumpFloodShader.id == 0 || inputSeedTexture == 0u ||
      outputSeedTexture == 0u || !ClearOverflowCounter()) {
    return false;
  }

  rlEnableShader(m_jumpFloodShader.id);
  const int workResolution[2] = {m_workWidth, m_workHeight};
  const int fullResolution[2] = {fullWidth, fullHeight};
  if (m_jumpStepSizeLoc >= 0) {
    rlSetUniform(m_jumpStepSizeLoc, &stepSize, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_jumpWorkResolutionLoc >= 0) {
    rlSetUniform(m_jumpWorkResolutionLoc, workResolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }
  if (m_jumpFullResolutionLoc >= 0) {
    rlSetUniform(m_jumpFullResolutionLoc, fullResolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }

  NoMoreDay::utils::GPUUtils::BindBufferBase(kOverflowBinding,
                                             m_overflowCounterBuffer);
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::V5GI::kSeedInputImageBinding, inputSeedTexture, 0, false, 0,
      kGLReadOnly, kGLRg16ui);
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::V5GI::kSeedOutputImageBinding, outputSeedTexture, 0, false, 0,
      kGLWriteOnly, kGLRg16ui);
  NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
      DivUp(static_cast<uint32_t>(m_workWidth), kComputeGroupSize),
      DivUp(static_cast<uint32_t>(m_workHeight), kComputeGroupSize), 1);
  rlDisableShader();

  const uint32_t barrierBits = static_cast<uint32_t>(RenderConstants::Barrier::Image) |
                               static_cast<uint32_t>(RenderConstants::Barrier::Buffer) |
                               kTextureFetchBarrierBit;
  NoMoreDay::utils::GPUUtils::MemoryBarrier(barrierBits);
  return true;
}

bool JFAPass::RunDistanceResolve(const uint32_t occluderMaskTexture,
                                 const int fullWidth, const int fullHeight,
                                 const uint32_t inputSeedTexture,
                                 const uint32_t outputDistanceTexture) {
  if (m_distanceResolveShader.id == 0 || occluderMaskTexture == 0u ||
      inputSeedTexture == 0u || outputDistanceTexture == 0u) {
    return false;
  }

  rlEnableShader(m_distanceResolveShader.id);
  const int workResolution[2] = {m_workWidth, m_workHeight};
  const int fullResolution[2] = {fullWidth, fullHeight};
  if (m_distanceWorkResolutionLoc >= 0) {
    rlSetUniform(m_distanceWorkResolutionLoc, workResolution,
                 RL_SHADER_UNIFORM_IVEC2, 1);
  }
  if (m_distanceFullResolutionLoc >= 0) {
    rlSetUniform(m_distanceFullResolutionLoc, fullResolution,
                 RL_SHADER_UNIFORM_IVEC2, 1);
  }

  const int maskTexUnit = 0;
  if (m_distanceMaskTextureLoc >= 0) {
    rlSetUniform(m_distanceMaskTextureLoc, &maskTexUnit, RL_SHADER_UNIFORM_INT, 1);
  }
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, occluderMaskTexture);

  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::V5GI::kSeedInputImageBinding, inputSeedTexture, 0, false, 0,
      kGLReadOnly, kGLRg16ui);
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::V5GI::kDistanceFieldImageBinding, outputDistanceTexture, 0,
      false, 0, kGLWriteOnly, kGLR16f);
  NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
      DivUp(static_cast<uint32_t>(m_workWidth), kComputeGroupSize),
      DivUp(static_cast<uint32_t>(m_workHeight), kComputeGroupSize), 1);
  rlDisableShader();

  const uint32_t barrierBits = static_cast<uint32_t>(RenderConstants::Barrier::Image) |
                               kTextureFetchBarrierBit;
  NoMoreDay::utils::GPUUtils::MemoryBarrier(barrierBits);
  return true;
}

bool JFAPass::RunUpsample(const int fullWidth, const int fullHeight) {
  if (m_upsampleShader.id == 0 || !m_distanceFieldWork.IsValid() ||
      !m_distanceFieldFull.IsValid()) {
    return false;
  }

  rlEnableShader(m_upsampleShader.id);
  const int halfResolution[2] = {m_workWidth, m_workHeight};
  const int fullResolution[2] = {fullWidth, fullHeight};
  if (m_upsampleHalfResolutionLoc >= 0) {
    rlSetUniform(m_upsampleHalfResolutionLoc, halfResolution,
                 RL_SHADER_UNIFORM_IVEC2, 1);
  }
  if (m_upsampleFullResolutionLoc >= 0) {
    rlSetUniform(m_upsampleFullResolutionLoc, fullResolution,
                 RL_SHADER_UNIFORM_IVEC2, 1);
  }

  constexpr uint32_t kHalfInputBinding = 0u;
  constexpr uint32_t kFullOutputBinding = 1u;
  NoMoreDay::utils::GPUUtils::BindImageTexture(kHalfInputBinding,
                                               m_distanceFieldWork.colorTexture, 0,
                                               false, 0, kGLReadOnly, kGLR16f);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kFullOutputBinding,
                                               m_distanceFieldFull.colorTexture, 0,
                                               false, 0, kGLWriteOnly, kGLR16f);
  NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
      DivUp(static_cast<uint32_t>(fullWidth), kComputeGroupSize),
      DivUp(static_cast<uint32_t>(fullHeight), kComputeGroupSize), 1);
  rlDisableShader();

  const uint32_t barrierBits = static_cast<uint32_t>(RenderConstants::Barrier::Image) |
                               kTextureFetchBarrierBit;
  NoMoreDay::utils::GPUUtils::MemoryBarrier(barrierBits);
  return true;
}

void JFAPass::ReportFailure(const char *reason) {
  m_lastExecuteFailure = true;
  m_lastExecuteSuccess = false;
  m_lastFailureReason = (reason != nullptr) ? reason : "unknown";
  LOG_WARN("JFAPass fallback: frame={} reason={}", m_frameIndex, m_lastFailureReason);
}

void JFAPass::MarkSuccess() {
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = true;
  m_lastFailureReason.clear();
}

void JFAPass::LogBarrierAuditOnce() {
  if (m_barrierAuditLogged) {
    return;
  }
  m_barrierAuditLogged = true;
  LOG_INFO(
      "JFAPass barrier audit: seed_init=Image|TextureFetch, jump=Image|Buffer|"
      "TextureFetch, distance=Image|TextureFetch, upsample=Image|TextureFetch");
}

void JFAPass::Execute(graph::RenderContext &context) {
  ++m_frameIndex;
  m_lastOverflowCount = 0u;
  m_usedFallbackPlus2ThisFrame = false;
  m_lastExecuteFailure = false;
  m_lastExecuteSuccess = false;
  m_lastFailureReason.clear();

  if (context.shared == nullptr || context.shared->resources == nullptr ||
      context.qualityManager == nullptr || m_occluderExtractPass == nullptr) {
    ReportFailure("missing JFA prerequisites");
    return;
  }

  const auto &config = context.qualityManager->GetConfig();
  if (!config.giEnabled) {
    MarkSuccess();
    return;
  }

  if (!context.hdrSceneBuffer.IsValid()) {
    ReportFailure("hdr scene buffer unavailable");
    return;
  }
  if (!m_occluderExtractPass->HasOccluderMask()) {
    ReportFailure("occluder mask unavailable");
    return;
  }
  if (!m_initialized && !Initialize(*context.shared->resources)) {
    ReportFailure("failed to initialize JFA shaders");
    return;
  }

  const int fullWidth = context.hdrSceneBuffer.width;
  const int fullHeight = context.hdrSceneBuffer.height;
  const bool halfResolution = config.giHalfResolution;
  if (!EnsureResources(fullWidth, fullHeight, halfResolution)) {
    ReportFailure("failed to allocate JFA textures");
    return;
  }

  const uint32_t updateInterval = std::max<uint32_t>(1u, config.giSdfUpdateInterval);
  const bool firstFrame = (m_frameIndex <= 1u);
  const bool occluderChanged = m_occluderExtractPass->WasMaskChangedThisFrame();
  const bool intervalTick = ((m_frameIndex % updateInterval) == 0u);

  bool shouldUpdate = firstFrame || intervalTick || occluderChanged;
  if (m_incrementalExperimentEnabled && !occluderChanged && !firstFrame) {
    shouldUpdate = intervalTick;
  }

  if (!shouldUpdate && m_distanceFieldFull.IsValid()) {
    context.giDistanceFieldTexture = m_distanceFieldFull.colorTexture;
    context.giDistanceFieldWidth = m_distanceFieldFull.width;
    context.giDistanceFieldHeight = m_distanceFieldFull.height;
    MarkSuccess();
    return;
  }

  const uint32_t occluderMaskTexture = m_occluderExtractPass->GetOccluderMaskTexture();
  if (!RunSeedInit(occluderMaskTexture, fullWidth, fullHeight)) {
    ReportFailure("seed initialization failed");
    return;
  }

  uint32_t seedInput = m_seedPing.colorTexture;
  uint32_t seedOutput = m_seedPong.colorTexture;
  int stepSize = HighestPowerOfTwoLessEqual(std::max(m_workWidth, m_workHeight));
  stepSize = std::max(1, stepSize / 2);
  while (stepSize >= 1) {
    if (!RunJumpFloodStep(stepSize, fullWidth, fullHeight, seedInput, seedOutput)) {
      ReportFailure("jump flood iteration failed");
      return;
    }
    std::swap(seedInput, seedOutput);
    stepSize /= 2;
  }

  if (std::max(m_workWidth, m_workHeight) >= 2) {
    if (!RunJumpFloodStep(2, fullWidth, fullHeight, seedInput, seedOutput)) {
      ReportFailure("JFA+1 step=2 failed");
      return;
    }
    std::swap(seedInput, seedOutput);
  }
  if (!RunJumpFloodStep(1, fullWidth, fullHeight, seedInput, seedOutput)) {
    ReportFailure("JFA+1 step=1 failed");
    return;
  }
  std::swap(seedInput, seedOutput);

  m_lastOverflowCount = ReadOverflowCounter();

  const bool shouldUseFallbackPlus2 =
      m_forceFallbackPlus2ForTesting ||
      (m_occluderExtractPass->GetOccluderCount() > 0u && m_lastOverflowCount > 0u);
  if (shouldUseFallbackPlus2) {
    m_usedFallbackPlus2ThisFrame = true;
    constexpr int fallbackSteps[] = {4, 2, 1};
    for (const int fallbackStep : fallbackSteps) {
      if (fallbackStep > std::max(m_workWidth, m_workHeight)) {
        continue;
      }
      if (!RunJumpFloodStep(fallbackStep, fullWidth, fullHeight, seedInput,
                            seedOutput)) {
        ReportFailure("JFA+2 fallback failed");
        return;
      }
      std::swap(seedInput, seedOutput);
    }
    m_lastOverflowCount = ReadOverflowCounter();
  }

  const uint32_t distanceOutput =
      halfResolution ? m_distanceFieldWork.colorTexture : m_distanceFieldFull.colorTexture;
  if (!RunDistanceResolve(occluderMaskTexture, fullWidth, fullHeight, seedInput,
                          distanceOutput)) {
    ReportFailure("distance resolve failed");
    return;
  }

  if (halfResolution && !RunUpsample(fullWidth, fullHeight)) {
    ReportFailure("half-res upsample failed");
    return;
  }

  context.giDistanceFieldTexture = m_distanceFieldFull.colorTexture;
  context.giDistanceFieldWidth = m_distanceFieldFull.width;
  context.giDistanceFieldHeight = m_distanceFieldFull.height;

  LogBarrierAuditOnce();
  MarkSuccess();
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes
