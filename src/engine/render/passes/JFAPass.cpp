#include "engine/render/passes/JFAPass.hpp"

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
#include <cmath>
#include <cstring>
#include <vector>

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

float HalfToFloat(const uint16_t bits) noexcept {
  const uint32_t sign = (static_cast<uint32_t>(bits & 0x8000u)) << 16u;
  const uint32_t exponent = (bits >> 10u) & 0x1Fu;
  const uint32_t mantissa = bits & 0x03FFu;
  uint32_t result = sign;
  if (exponent == 0u) {
    if (mantissa != 0u) {
      float value = std::ldexp(static_cast<float>(mantissa), -24);
      return (bits & 0x8000u) != 0u ? -value : value;
    }
  } else if (exponent == 0x1Fu) {
    result |= 0x7F800000u | (mantissa << 13u);
  } else {
    result |= ((exponent + 112u) << 23u) | (mantissa << 13u);
  }
  float value = 0.0f;
  std::memcpy(&value, &result, sizeof(value));
  return value;
}

std::vector<uint8_t> ReadMask(uint32_t texture, int width, int height) {
  void *pixels = rlReadTexturePixels(texture, width, height,
                                     RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
  if (pixels == nullptr) {
    return {};
  }
  std::vector<uint8_t> result(static_cast<size_t>(width) * static_cast<size_t>(height));
  std::memcpy(result.data(), pixels, result.size());
  RL_FREE(pixels);
  return result;
}

std::vector<float> ReadDistance(uint32_t texture, int width, int height) {
  void *pixels = rlReadTexturePixels(texture, width, height,
                                     RL_PIXELFORMAT_UNCOMPRESSED_R16);
  if (pixels == nullptr) {
    return {};
  }
  const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
  const auto *halfPixels = static_cast<const uint16_t *>(pixels);
  std::vector<float> result(count);
  for (size_t i = 0; i < count; ++i) {
    result[i] = HalfToFloat(halfPixels[i]);
  }
  RL_FREE(pixels);
  return result;
}

} // namespace

JFAPass::JFAPass() = default;

JFAPass::~JFAPass() { Shutdown(); }

gi::JFAUpdateDecision JFAPass::ApplyProductionUpdatePolicy(
    gi::JFAUpdateDecision decision, const bool incrementalEnabled) noexcept {
  if (decision.mode == gi::JFAUpdateMode::Incremental && !incrementalEnabled) {
    decision.mode = gi::JFAUpdateMode::Full;
    decision.fullReason = gi::JFAFullReasons::kProductionDefaultFull;
  }
  return decision;
}

void JFAPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::OccluderMask,
               graph::RenderOwnerTag::OccluderExtract);
  // DistanceField is produced by the resolve/upsample compute dispatches
  // (image stores) and consumed by RadianceCascadesPass via image loads.
  // Declared as a Compute write so the graph emits the cross-pass
  // Image|TexFetch transition at RadianceCascades entry instead of a manual
  // barrier at the end of this pass.
  builder.Write(graph::RenderResourceTag::DistanceField, graph::RenderOwnerTag::JFA,
                graph::PipelineStage::Compute, graph::ResourceUsage::StorageWrite);

  // Same-pass phase barrier: seed init -> jump flood -> distance resolve run
  // back-to-back inside this Execute and exchange data via image loads and
  // stores (plus the overflow SSBO in the jump step). Declared here and emitted
  // via EmitPhaseBarrier after each dispatch (the exact execution points).
  builder.AddPhaseBarrier(
      graph::PipelineStage::Compute, graph::PipelineStage::Compute,
      static_cast<uint32_t>(RenderConstants::Barrier::Image) |
          static_cast<uint32_t>(RenderConstants::Barrier::Buffer) |
          kTextureFetchBarrierBit);

  graph::TypedResourceDescriptor seedDesc;
  seedDesc.name = "JFASeedField";
  seedDesc.tag = graph::RenderResourceTag::Custom;
  seedDesc.ownerTag = graph::RenderOwnerTag::JFA;
  seedDesc.kind = graph::ResourceKind::Texture2D;
  seedDesc.format = graph::ResourceFormat::RG16F;
  seedDesc.lifetime = graph::ResourceLifetime::Transient;
  builder.DeclareResource(seedDesc);

  graph::TypedResourceDescriptor distanceDesc;
  distanceDesc.name = "DistanceFieldSubresource";
  distanceDesc.tag = graph::RenderResourceTag::DistanceField;
  distanceDesc.ownerTag = graph::RenderOwnerTag::JFA;
  distanceDesc.kind = graph::ResourceKind::Texture2D;
  distanceDesc.format = graph::ResourceFormat::R16F;
  distanceDesc.lifetime = graph::ResourceLifetime::Persistent;
  builder.DeclareResource(distanceDesc);

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
  m_seedInitRectMinLoc = rlGetLocationUniform(m_seedInitShader.id, "uRectMin");

  m_jumpStepSizeLoc = rlGetLocationUniform(m_jumpFloodShader.id, "uStepSize");
  m_jumpWorkResolutionLoc =
      rlGetLocationUniform(m_jumpFloodShader.id, "uWorkResolution");
  m_jumpFullResolutionLoc =
      rlGetLocationUniform(m_jumpFloodShader.id, "uFullResolution");
  m_jumpRectMinLoc = rlGetLocationUniform(m_jumpFloodShader.id, "uRectMin");

  m_distanceWorkResolutionLoc =
      rlGetLocationUniform(m_distanceResolveShader.id, "uWorkResolution");
  m_distanceFullResolutionLoc =
      rlGetLocationUniform(m_distanceResolveShader.id, "uFullResolution");
  m_distanceMaskTextureLoc =
      rlGetLocationUniform(m_distanceResolveShader.id, "uMaskTexture");
  m_distanceRectMinLoc = rlGetLocationUniform(m_distanceResolveShader.id, "uRectMin");

  m_upsampleHalfResolutionLoc =
      rlGetLocationUniform(m_upsampleShader.id, "uHalfResolution");
  m_upsampleFullResolutionLoc =
      rlGetLocationUniform(m_upsampleShader.id, "uFullResolution");
  m_upsampleRectMinLoc = rlGetLocationUniform(m_upsampleShader.id, "uRectMin");
  m_upsampleMaskTextureLoc =
      rlGetLocationUniform(m_upsampleShader.id, "uMaskTexture");

  m_overflowCounterBuffer.Create(sizeof(uint32_t), 3);
  if (m_overflowCounterBuffer.GetId() == 0u) {
    Shutdown();
    return false;
  }

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

  m_overflowCounterBuffer.Destroy();
  m_lastReadyOverflowCount = 0u;

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
  m_upsampleRectMinLoc = -1;
  m_upsampleMaskTextureLoc = -1;

  m_fullWidth = 0;
  m_fullHeight = 0;
  m_workWidth = 0;
  m_workHeight = 0;
  m_frameIndex = 0u;
  m_lastOverflowCount = 0u;
  m_previousOccluderCount = 0u;
  m_occluderCountSnapshot = 0u;
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
  if (m_overflowCounterBuffer.GetId() == 0u) {
    return false;
  }
  uint32_t *ptr = static_cast<uint32_t *>(m_overflowCounterBuffer.BeginWrite());
  if (ptr != nullptr) {
    *ptr = 0u;
  }
  m_overflowCounterBuffer.Flush();
  return true;
}

uint32_t JFAPass::ReadOverflowCounter() const {
  return m_lastReadyOverflowCount;
}

uint32_t JFAPass::ReadOverflowCounterImmediateForTesting() const {
  if (m_overflowCounterBuffer.GetId() == 0u) {
    return 0u;
  }
  uint32_t overflow = 0u;
  m_overflowCounterBuffer.Read(&overflow, sizeof(uint32_t));
  return overflow;
}

bool JFAPass::RunSeedInit(const graph::RenderContext &context,
                          const uint32_t occluderMaskTexture, const int fullWidth,
                          const int fullHeight, const gi::JFARect *rect) {
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

  int rectMin[2] = {0, 0};
  uint32_t dispatchW = DivUp(static_cast<uint32_t>(m_workWidth), kComputeGroupSize);
  uint32_t dispatchH = DivUp(static_cast<uint32_t>(m_workHeight), kComputeGroupSize);

  if (rect != nullptr && !rect->IsEmpty()) {
    rectMin[0] = rect->minX;
    rectMin[1] = rect->minY;
    dispatchW = DivUp(static_cast<uint32_t>(rect->Width()), kComputeGroupSize);
    dispatchH = DivUp(static_cast<uint32_t>(rect->Height()), kComputeGroupSize);
  }

  if (m_seedInitRectMinLoc >= 0) {
    rlSetUniform(m_seedInitRectMinLoc, rectMin, RL_SHADER_UNIFORM_IVEC2, 1);
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
  {
    NoMoreDay::utils::GPUUtils::ScopedDebugGroup debugGroup("JFASeedInit");
    NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(dispatchW, dispatchH, 1);
  }
  rlDisableShader();

  // Same-pass sync before the first jump-flood dispatch reads the seed image.
  context.EmitPhaseBarrier(graph::PipelineStage::Compute,
                           graph::PipelineStage::Compute);
  return true;
}

bool JFAPass::RunJumpFloodStep(const graph::RenderContext &context,
                               const int stepSize, const int fullWidth,
                               const int fullHeight, const uint32_t inputSeedTexture,
                               const uint32_t outputSeedTexture,
                               const gi::JFARect *rect) {
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

  int rectMin[2] = {0, 0};
  uint32_t dispatchW = DivUp(static_cast<uint32_t>(m_workWidth), kComputeGroupSize);
  uint32_t dispatchH = DivUp(static_cast<uint32_t>(m_workHeight), kComputeGroupSize);

  if (rect != nullptr && !rect->IsEmpty()) {
    rectMin[0] = rect->minX;
    rectMin[1] = rect->minY;
    dispatchW = DivUp(static_cast<uint32_t>(rect->Width()), kComputeGroupSize);
    dispatchH = DivUp(static_cast<uint32_t>(rect->Height()), kComputeGroupSize);
  }

  if (m_jumpRectMinLoc >= 0) {
    rlSetUniform(m_jumpRectMinLoc, rectMin, RL_SHADER_UNIFORM_IVEC2, 1);
  }

  m_overflowCounterBuffer.BindBase(kOverflowBinding);
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::V5GI::kSeedInputImageBinding, inputSeedTexture, 0, false, 0,
      kGLReadOnly, kGLRg16ui);
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      RenderConstants::V5GI::kSeedOutputImageBinding, outputSeedTexture, 0, false, 0,
      kGLWriteOnly, kGLRg16ui);
  {
    NoMoreDay::utils::GPUUtils::ScopedDebugGroup debugGroup("JFAJumpFlood");
    NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(dispatchW, dispatchH, 1);
  }
  rlDisableShader();

  // Same-pass sync before the next jump step (or distance resolve) reads the
  // seed/overflow results: emitted at this exact execution point.
  context.EmitPhaseBarrier(graph::PipelineStage::Compute,
                           graph::PipelineStage::Compute);
  return true;
}

bool JFAPass::RunDistanceResolve(const graph::RenderContext &context,
                                 const uint32_t occluderMaskTexture,
                                 const int fullWidth, const int fullHeight,
                                 const uint32_t inputSeedTexture,
                                 const uint32_t outputDistanceTexture,
                                 const gi::JFARect *rect) {
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

  int rectMin[2] = {0, 0};
  uint32_t dispatchW = DivUp(static_cast<uint32_t>(m_workWidth), kComputeGroupSize);
  uint32_t dispatchH = DivUp(static_cast<uint32_t>(m_workHeight), kComputeGroupSize);

  if (rect != nullptr && !rect->IsEmpty()) {
    rectMin[0] = rect->minX;
    rectMin[1] = rect->minY;
    dispatchW = DivUp(static_cast<uint32_t>(rect->Width()), kComputeGroupSize);
    dispatchH = DivUp(static_cast<uint32_t>(rect->Height()), kComputeGroupSize);
  }

  if (m_distanceRectMinLoc >= 0) {
    rlSetUniform(m_distanceRectMinLoc, rectMin, RL_SHADER_UNIFORM_IVEC2, 1);
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
  {
    NoMoreDay::utils::GPUUtils::ScopedDebugGroup debugGroup("JFADistanceResolve");
    NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(dispatchW, dispatchH, 1);
  }
  rlDisableShader();

  // Same-pass sync before the upsample dispatch reads the half-resolution
  // distance field work image. The subsequent full-resolution write is a
  // cross-pass boundary covered by the graph transition fired at the
  // RadianceCascades entry; no manual barrier here.
  context.EmitPhaseBarrier(graph::PipelineStage::Compute,
                           graph::PipelineStage::Compute);
  return true;
}

bool JFAPass::RunUpsample(const uint32_t occluderMaskTexture, const int fullWidth,
                          const int fullHeight, const gi::JFARect *rect) {
  if (occluderMaskTexture == 0u || m_upsampleShader.id == 0 ||
      !m_distanceFieldWork.IsValid() || !m_distanceFieldFull.IsValid()) {
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

  int rectMin[2] = {0, 0};
  uint32_t dispatchW = DivUp(static_cast<uint32_t>(fullWidth), kComputeGroupSize);
  uint32_t dispatchH = DivUp(static_cast<uint32_t>(fullHeight), kComputeGroupSize);

  if (rect != nullptr && !rect->IsEmpty()) {
    const float scaleX = static_cast<float>(fullWidth) / static_cast<float>(m_workWidth);
    const float scaleY = static_cast<float>(fullHeight) / static_cast<float>(m_workHeight);
    rectMin[0] = static_cast<int>(std::floor(static_cast<float>(rect->minX) * scaleX));
    rectMin[1] = static_cast<int>(std::floor(static_cast<float>(rect->minY) * scaleY));
    const int fullMaxX = std::min(fullWidth, static_cast<int>(std::ceil(static_cast<float>(rect->maxX) * scaleX)));
    const int fullMaxY = std::min(fullHeight, static_cast<int>(std::ceil(static_cast<float>(rect->maxY) * scaleY)));
    dispatchW = DivUp(static_cast<uint32_t>(fullMaxX - rectMin[0]), kComputeGroupSize);
    dispatchH = DivUp(static_cast<uint32_t>(fullMaxY - rectMin[1]), kComputeGroupSize);
  }

  if (m_upsampleRectMinLoc >= 0) {
    rlSetUniform(m_upsampleRectMinLoc, rectMin, RL_SHADER_UNIFORM_IVEC2, 1);
  }

  constexpr int kUpsampleMaskUnit = 2;
  if (m_upsampleMaskTextureLoc >= 0) {
    rlSetUniform(m_upsampleMaskTextureLoc, &kUpsampleMaskUnit, RL_SHADER_UNIFORM_INT, 1);
  }
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0 + kUpsampleMaskUnit);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, occluderMaskTexture);

  constexpr uint32_t kHalfInputBinding = 0u;
  constexpr uint32_t kFullOutputBinding = 1u;
  NoMoreDay::utils::GPUUtils::BindImageTexture(kHalfInputBinding,
                                               m_distanceFieldWork.colorTexture, 0,
                                               false, 0, kGLReadOnly, kGLR16f);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kFullOutputBinding,
                                               m_distanceFieldFull.colorTexture, 0,
                                               false, 0, kGLWriteOnly, kGLR16f);
  {
    NoMoreDay::utils::GPUUtils::ScopedDebugGroup debugGroup("JFAUpsample");
    NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(dispatchW, dispatchH, 1);
  }

  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, 0u);
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  rlDisableShader();

  // Cross-pass sync: RadianceCascadesPass consumes DistanceField via image
  // loads. Covered by the graph transition (Write Compute/StorageWrite ->
  // Read) fired at RadianceCascades' pass entry; no manual barrier here.
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

  if (context.resources == nullptr ||
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
  if (!m_initialized && !Initialize(*context.resources)) {
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

  gi::JFAViewKey currentViewKey;
  currentViewKey.cameraVersion = static_cast<uint32_t>(m_occluderExtractPass->GetCameraInvalidateCount());
  currentViewKey.staticContentVersion = static_cast<uint32_t>(m_occluderExtractPass->GetStaticRebuildCount());
  currentViewKey.qualityTier = static_cast<uint32_t>(context.qualityManager->GetTier());
  currentViewKey.width = m_workWidth;
  currentViewKey.height = m_workHeight;
  currentViewKey.halfResolution = halfResolution;



  gi::JFARect previousBounds = m_previousOccluderBounds;
  gi::JFARect currentBounds{};
  if (m_testBoundsOverridden) {
    previousBounds = m_testPreviousBounds;
    currentBounds = m_testCurrentBounds;
    m_testBoundsOverridden = false;
  } else {
    previousBounds = m_occluderExtractPass->GetPreviousOccluderScreenBounds();
    currentBounds = m_occluderExtractPass->GetCurrentOccluderScreenBounds();
  }

  const uint32_t currentOccluderCount = m_occluderExtractPass->GetOccluderCount();
  m_occluderCountSnapshot = currentOccluderCount;
  const bool occluderCountChanged = (!firstFrame) && (currentOccluderCount != m_previousOccluderCount);

  gi::DecideUpdateParams decideParams;
  decideParams.previousViewKey = m_previousViewKey;
  decideParams.currentViewKey = currentViewKey;
  decideParams.previousOccluderBounds = previousBounds;
  decideParams.currentOccluderBounds = currentBounds;
  decideParams.occluderCountChanged = occluderCountChanged;
  decideParams.hasValidSeedContext = true;

  gi::JFAUpdateDecision decision = gi::JFADistanceFieldEvaluator::DecideUpdate(decideParams);

  if (!shouldUpdate && m_distanceFieldFull.IsValid()) {
    decision.mode = gi::JFAUpdateMode::Skip;
  }

  // T7.1: If occluder count is 0 (empty scene), host decides to skip JFA execution (dispatch = 0, decision.mode = Skip).
  if (currentOccluderCount == 0u) {
    decision.mode = gi::JFAUpdateMode::Skip;
    decision.dirtyRect = {};
    decision.expandedRect = {};
  }

  decision = ApplyProductionUpdatePolicy(decision, m_incrementalExperimentEnabled);

  // If occluder count is 0, ensure mode remains Skip even after policy check
  if (currentOccluderCount == 0u) {
    decision.mode = gi::JFAUpdateMode::Skip;
  }

  if (decision.mode == gi::JFAUpdateMode::Skip && m_distanceFieldFull.IsValid()) {
    m_previousViewKey = currentViewKey;
    m_previousOccluderBounds = currentBounds;
    m_previousOccluderCount = currentOccluderCount;
    context.giDistanceFieldTexture = m_distanceFieldFull.colorTexture;
    context.giDistanceFieldWidth = m_distanceFieldFull.width;
    context.giDistanceFieldHeight = m_distanceFieldFull.height;
    m_lastReport = JFAFrameReport{
        .mode = gi::JFAUpdateMode::Skip,
        .dirtyRect = {},
        .expandedRect = {},
        .dispatchTexelCount = 0,
        .occluderVersion = static_cast<uint32_t>(m_occluderExtractPass->GetMaskVersion()),
        .sdfVersion = m_sdfVersion,
        .fullReason = ""
    };
    MarkSuccess();
    return;
  }

  const gi::JFARect *dispatchRect = nullptr;
  if (decision.mode == gi::JFAUpdateMode::Incremental) {
    dispatchRect = &decision.expandedRect;
  }

  const uint32_t occluderMaskTexture = m_occluderExtractPass->GetOccluderMaskTexture();
  const auto runJfa = [&](const gi::JFARect *rect) {
    if (!RunSeedInit(context, occluderMaskTexture, fullWidth, fullHeight, rect)) {
      return false;
    }
    uint32_t seedInput = m_seedPing.colorTexture;
    uint32_t seedOutput = m_seedPong.colorTexture;
    int stepSize = HighestPowerOfTwoLessEqual(std::max(m_workWidth, m_workHeight));
    stepSize = std::max(1, stepSize / 2);
    while (stepSize >= 1) {
      if (!RunJumpFloodStep(context, stepSize, fullWidth, fullHeight, seedInput, seedOutput, rect)) {
        return false;
      }
      std::swap(seedInput, seedOutput);
      stepSize /= 2;
    }
    if (std::max(m_workWidth, m_workHeight) >= 2) {
      if (!RunJumpFloodStep(context, 2, fullWidth, fullHeight, seedInput, seedOutput, rect)) {
        return false;
      }
      std::swap(seedInput, seedOutput);
    }
    if (!RunJumpFloodStep(context, 1, fullWidth, fullHeight, seedInput, seedOutput, rect)) {
      return false;
    }
    std::swap(seedInput, seedOutput);

    // Poll delayed overflow snapshot from ready fence (non-blocking, zero CPU stall)
    uint32_t delayedOverflow = 0u;
    if (m_overflowCounterBuffer.TryReadNonBlocking(&delayedOverflow, sizeof(uint32_t), 1)) {
      m_lastReadyOverflowCount = delayedOverflow;
    }
    m_lastOverflowCount = m_lastReadyOverflowCount;
    const bool useFallbackPlus2 =
        m_forceFallbackPlus2ForTesting ||
        (m_occluderExtractPass->GetOccluderCount() > 0u && m_lastOverflowCount > 0u);
    if (useFallbackPlus2) {
      m_usedFallbackPlus2ThisFrame = true;
      constexpr int fallbackSteps[] = {4, 2, 1};
      for (const int fallbackStep : fallbackSteps) {
        if (fallbackStep > std::max(m_workWidth, m_workHeight)) {
          continue;
        }
        if (!RunJumpFloodStep(context, fallbackStep, fullWidth, fullHeight, seedInput,
                              seedOutput, rect)) {
          return false;
        }
        std::swap(seedInput, seedOutput);
      }
    }
    const uint32_t distanceOutput =
        halfResolution ? m_distanceFieldWork.colorTexture : m_distanceFieldFull.colorTexture;
    if (!RunDistanceResolve(context, occluderMaskTexture, fullWidth, fullHeight, seedInput,
                            distanceOutput, rect)) {
      return false;
    }
    return !halfResolution || RunUpsample(occluderMaskTexture, fullWidth, fullHeight, rect);
  };

  if (!runJfa(dispatchRect)) {
    ReportFailure("JFA execution failed");
    return;
  }

  JFAFrameReport verificationReport{};
  if (decision.mode == gi::JFAUpdateMode::Incremental) {
    verificationReport.verificationAttempted = true;
    verificationReport.verificationArtifact = "gpu-r16f-vs-cpu-full-jfa-and-edt";
    bool verificationRejected = false;
    if (halfResolution) {
      verificationReport.verificationResult = "unsupported-half-resolution-reference";
      verificationRejected = true;
    } else {
      const auto mask = ReadMask(occluderMaskTexture, fullWidth, fullHeight);
      const auto candidate = ReadDistance(m_distanceFieldFull.colorTexture, fullWidth, fullHeight);
      if (mask.empty() || candidate.empty()) {
        verificationReport.verificationResult = "readback-unavailable";
        verificationRejected = true;
      } else {
        const auto fullReference = gi::JFADistanceFieldEvaluator::BuildApproximateJfaDistanceField(
            mask, fullWidth, fullHeight, true, false);
        const auto edtReference = gi::JFADistanceFieldEvaluator::BuildExactSignedDistanceField(
            mask, fullWidth, fullHeight);
        const auto fullStats = gi::JFADistanceFieldEvaluator::ComputeErrorStats(
            fullReference, candidate);
        const auto edtStats = gi::JFADistanceFieldEvaluator::ComputeErrorStats(
            edtReference, candidate);
        verificationRejected =
            gi::JFADistanceFieldEvaluator::NeedsJfaPlus2Fallback(fullStats, 0.5f, 2.0f) ||
            gi::JFADistanceFieldEvaluator::NeedsJfaPlus2Fallback(edtStats, 2.0f, 4.0f);
        verificationReport.verificationPassed = !verificationRejected;
        verificationReport.verificationResult = verificationRejected ? "mismatch" : "match";
      }
    }
    if (verificationRejected) {
      decision.mode = gi::JFAUpdateMode::Revert;
      decision.fullReason = gi::JFAFullReasons::kVerificationFull;
      verificationReport.verificationFallback = true;
      m_usedFallbackPlus2ThisFrame = false;
      if (!runJfa(nullptr)) {
        ReportFailure("verification full fallback failed");
        return;
      }
    }
  }

  m_previousViewKey = currentViewKey;
  m_previousOccluderBounds = currentBounds;
  m_previousOccluderCount = currentOccluderCount;
  ++m_sdfVersion;


  m_lastReport = JFAFrameReport{
      .mode = decision.mode,
      .dirtyRect = decision.dirtyRect,
      .expandedRect = decision.expandedRect,
       .dispatchTexelCount = (decision.mode == gi::JFAUpdateMode::Incremental)
                                 ? static_cast<uint32_t>(decision.expandedRect.Area())
                                 : static_cast<uint32_t>(m_workWidth * m_workHeight),
       .occluderVersion = static_cast<uint32_t>(m_occluderExtractPass->GetMaskVersion()),
       .sdfVersion = m_sdfVersion,
       .fullReason = decision.fullReason,
       .verificationAttempted = verificationReport.verificationAttempted,
       .verificationPassed = verificationReport.verificationPassed,
       .verificationFallback = verificationReport.verificationFallback,
       .verificationResult = verificationReport.verificationResult,
       .verificationArtifact = verificationReport.verificationArtifact
   };

  context.giDistanceFieldTexture = m_distanceFieldFull.colorTexture;
  context.giDistanceFieldWidth = m_distanceFieldFull.width;
  context.giDistanceFieldHeight = m_distanceFieldFull.height;

  m_overflowCounterBuffer.Lock();
  LogBarrierAuditOnce();
  MarkSuccess();
  core::ApplyRlglFlushTemplate();
}


} // namespace NoMoreDay::render::passes
