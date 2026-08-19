#include "engine/render/RenderSystem.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "engine/render/ComputeBuffer.hpp" 
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUABIContract.hpp"
#include "engine/render/GameplayRenderHooks.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/GPUTextSystem.hpp"
#include "engine/render/GPULootSystem.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/trail/GPUTrailRenderer.hpp"
#include "engine/render/debug/GLDebugCallback.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/debug/RenderProfiler.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/DistortionPass.hpp"
#include "engine/render/passes/FluidSimulationPass.hpp"
#include "engine/render/passes/GICompositePass.hpp"
#include "engine/render/passes/GPULootPass.hpp"
#include "engine/render/passes/GPUTextPass.hpp"
#include "engine/render/passes/HeightShadowPass.hpp"
#include "engine/render/passes/JFAPass.hpp"
#include "engine/render/passes/LightCullingPass.hpp"
#include "engine/render/lighting/ClusteredLightingState.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/PostProcessPass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/render/passes/VFXEmissionSnapshotPass.hpp"
#include "engine/render/passes/ShadowBuildPass.hpp"
#include "engine/render/passes/ShadowPreparePass.hpp"
#include "engine/render/passes/ShadowResolvePass.hpp"
#include "engine/render/passes/VolumetricLightPass.hpp"
#include "engine/render/PopupRenderer.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/UIWorldPass.hpp"
#include "engine/render/passes/VFXPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/GPUTexturePool.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "engine/render/resources/TransientResourcePool.hpp"
#include "engine/render/resource/TextureArrayManager.hpp"
#include "engine/render/core/AdaptiveQualityController.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/ScopedGLState.hpp"
#include "engine/render/RenderConstants.hpp" 
#include "engine/render/RenderContext.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "raymath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "engine/render/LootTextBatcher.hpp"
#include "rlgl.h"

// Static Buffers
std::vector<NoMoreDay::components::GPULabelInstance>
    RenderSystem::s_labelBuffer;
std::vector<NoMoreDay::components::GPUGlyphInstance>
    RenderSystem::s_glyphBuffer;
NoMoreDay::render::resources::FramebufferHandle RenderSystem::s_hdrSceneBuffer;

// Static Members Definition
bool RenderSystem::s_initialized = false;
float RenderSystem::s_trauma = 0.0f;
float RenderSystem::s_shakeMultiplier = 1.0f;
Shader RenderSystem::s_labelShader = {0};
int RenderSystem::s_labelMvpLoc = -1;
std::unique_ptr<NoMoreDay::core::ComputeBuffer>
    RenderSystem::s_labelInstanceBuffer = nullptr;

Shader RenderSystem::s_glyphShader = {0};
int RenderSystem::s_glyphMvpLoc = -1;
int RenderSystem::s_glyphTexLoc = -1;
std::unique_ptr<NoMoreDay::core::ComputeBuffer>
    RenderSystem::s_glyphInstanceBuffer = nullptr;
Shader RenderSystem::s_glyphMsdfShader = {0};
int RenderSystem::s_glyphMsdfMvpLoc = -1;
int RenderSystem::s_glyphMsdfTexLoc = -1;
int RenderSystem::s_glyphMsdfPxRangeLoc = -1;

// Screen Shake Implementation
void RenderSystem::AddScreenShake(float intensity) {
    if (s_shakeMultiplier < 1e-4f) return;
    s_trauma = std::clamp(s_trauma + intensity * s_shakeMultiplier, 0.0f, 1.0f);
}

void RenderSystem::UpdateShake(float dt) {
    if (s_trauma > 1e-4f) {
        s_trauma = std::max(0.0f, s_trauma - dt * 1.5f);
    } else {
        s_trauma = 0.0f;
    }
}

Vector2 RenderSystem::GetShakeOffset() {
    if (s_trauma < 1e-4f || s_shakeMultiplier < 1e-4f) return {0, 0};
    float shake = s_trauma * s_trauma;
    float maxOffset = 15.0f * shake * s_shakeMultiplier;
    return {
        (float)NoMoreDay::utils::ThreadSafeRandom::GetFloat(-1.0f, 1.0f) * maxOffset,
        (float)NoMoreDay::utils::ThreadSafeRandom::GetFloat(-1.0f, 1.0f) * maxOffset
    };
}

// Phase 2: Beam Instancing
// GPUBeamInstance + s_beamBuffer live here; the Game adapter fills the buffer
// through NoMoreDay::render::GameplayRenderFrame::beamBuffer and Engine performs the draw.
static Shader s_beamShader = {0};
static int s_beamMvpLoc = -1;
static std::unique_ptr<NoMoreDay::core::ComputeBuffer> s_beamInstanceBuffer =
    nullptr;
static std::vector<NoMoreDay::render::GPUBeamInstance> s_beamBuffer;
static std::vector<NoMoreDay::components::GPUShadowCaster> s_occluderBuffer;
static std::vector<NoMoreDay::components::GPULight> s_lightCandidateBuffer;
static std::vector<NoMoreDay::render::lighting::GlobalHeightField::HeightStamp>
    s_heightFieldBuffer;
static std::vector<NoMoreDay::components::GPULootInstance> s_lootInstanceBuffer;
static std::vector<NoMoreDay::components::EmissiveStampInput>
    s_emissiveStampBuffer;

namespace {

struct RenderFrameData {
  entt::registry &registry;
  const NoMoreDay::render::RenderFrameInput &context;
  const Camera2D &camera;
  Font font = {}; // glyph atlas out-field filled by the GameplayRenderAdapter
  std::vector<NoMoreDay::components::GPULabelInstance> *labelBuffer = nullptr;
  std::vector<NoMoreDay::components::GPUGlyphInstance> *glyphBuffer = nullptr;
  Shader *labelShader = nullptr;
  Shader *glyphShader = nullptr;
  int labelMvpLoc = -1;
  int glyphMvpLoc = -1;
  int glyphTexLoc = -1;
  // B4/H-01: MSDF glyph variant resources + adapter-decision out-fields. The
  // Engine broadcasts its MSDF resource readiness via glyphMsdfEngineReady so
  // the adapter never enables MSDF templates without Engine support.
  Shader *glyphMsdfShader = nullptr;
  int glyphMsdfMvpLoc = -1;
  int glyphMsdfTexLoc = -1;
  int glyphMsdfPxRangeLoc = -1;
  bool glyphMsdfEngineReady = false;
  bool glyphMsdfEnabled = false;
  float glyphMsdfPxRange = 1.0f;
  NoMoreDay::core::ComputeBuffer *labelInstanceBuffer = nullptr;
  NoMoreDay::core::ComputeBuffer *glyphInstanceBuffer = nullptr;
  bool gpuTextEnabled = false;
  bool gpuLootEnabled = false;
  bool gpuLootGlowEnabled = false;
  uint32_t occluderStaticCount = 0u;
  uint32_t occluderDynamicCount = 0u;
  uint64_t occluderStaticSignature = 0u;
  uint64_t occluderDynamicSignature = 0u;
  int ecsLights = 0;
  float worldWidth = 0.0f;
  float worldHeight = 0.0f;
  float tileWorldSize = 0.0f;

  // Build the engine-safe DTO handed to the gameplay render adapter.
  NoMoreDay::render::GameplayRenderFrame ToHooksFrame() const {
    return NoMoreDay::render::GameplayRenderFrame{
        registry,        camera,  labelBuffer, glyphBuffer, &s_beamBuffer,
        &s_occluderBuffer, &s_lightCandidateBuffer, &s_heightFieldBuffer,
        &s_lootInstanceBuffer, &s_emissiveStampBuffer,
        gpuTextEnabled, gpuLootEnabled, gpuLootGlowEnabled, font,
        glyphMsdfEngineReady, glyphMsdfEnabled, glyphMsdfPxRange,
        occluderStaticCount, occluderDynamicCount, occluderStaticSignature,
        occluderDynamicSignature, ecsLights, worldWidth, worldHeight,
        tileWorldSize};
  }
};

NoMoreDay::render::resources::TransientResourcePool g_transientPool;
std::shared_ptr<NoMoreDay::render::passes::PostProcessPass> g_postProcessPass;
std::shared_ptr<NoMoreDay::render::passes::LightCullingPass> g_lightCullingPass;
std::shared_ptr<NoMoreDay::render::passes::LightingPass> g_lightingPass;
std::shared_ptr<NoMoreDay::render::passes::HeightShadowPass> g_heightShadowPass;
std::shared_ptr<NoMoreDay::render::passes::OccluderExtractPass> g_occluderExtractPass;
std::shared_ptr<NoMoreDay::render::passes::JFAPass> g_jfaPass;
std::shared_ptr<NoMoreDay::render::passes::RadianceCascadesPass>
    g_radianceCascadesPass;
std::shared_ptr<NoMoreDay::render::passes::GICompositePass> g_giCompositePass;
std::shared_ptr<NoMoreDay::render::passes::FluidSimulationPass> g_fluidSimulationPass;
std::shared_ptr<NoMoreDay::render::passes::VolumetricLightPass> g_volumetricPass;
std::shared_ptr<NoMoreDay::render::passes::DistortionPass> g_distortionPass;
std::shared_ptr<NoMoreDay::render::passes::ShadowPreparePass> g_shadowPreparePass;
std::shared_ptr<NoMoreDay::render::passes::ShadowBuildPass> g_shadowBuildPass;
std::shared_ptr<NoMoreDay::render::passes::ShadowResolvePass> g_shadowResolvePass;
std::unique_ptr<NoMoreDay::render::debug::RenderProfiler> g_renderProfiler;

// W6 (M0-C): the pass order of the graph actually compiled inside the last
// render() call. The hardware gate reads this (via GetLastExecutedPassOrder)
// so its pass trace is the real execution path, not a synthetic test graph.
std::vector<std::string> s_lastExecutedPassOrder;

void ReleaseV3RuntimeResourcesSkeleton() {
  if (g_distortionPass != nullptr) {
    g_distortionPass->ResetSources();
  }
  NoMoreDay::render::lighting::ClusteredLightingState::Get().Shutdown();
  LOG_INFO("RenderSystem: V3 runtime toggle disabled, released V3 placeholder "
           "resources.");
}

void HandleV3RuntimeToggle(bool v3Enabled) {
  static bool initialized = false;
  static bool previous = false;

  if (!initialized) {
    previous = v3Enabled;
    initialized = true;
    return;
  }
  if (previous == v3Enabled) {
    return;
  }

  if (!v3Enabled) {
    ReleaseV3RuntimeResourcesSkeleton();
  } else {
    LOG_INFO("RenderSystem: V3 runtime toggle enabled (baseline no-op passes).");
  }

  previous = v3Enabled;
}

using CompositeTargetState = OffscreenTargetDescriptor;

CompositeTargetState CaptureCompositeTargetState() {
  CompositeTargetState state = {};
  state.viewportWidth = GetScreenWidth();
  state.viewportHeight = GetScreenHeight();
  state.renderExtentWidth = GetScreenWidth();
  state.renderExtentHeight = GetScreenHeight();
  state.ownsFramebuffer = false;
  state.flipY = false;
  state.internalFormat = 0;

#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_43)
  constexpr uint32_t kGLFramebufferBinding = 0x8CA6;
  constexpr uint32_t kGLViewport = 0x0BA2;
  constexpr uint32_t kGLScissorTest = 0x0C11;
  constexpr uint32_t kGLScissorBox = 0x0C10;
  GLint boundFramebuffer = 0;
  GLint viewport[4] = {0, 0, state.viewportWidth, state.viewportHeight};
  glGetIntegerv(kGLFramebufferBinding, &boundFramebuffer);
  glGetIntegerv(kGLViewport, viewport);
  state.framebuffer = static_cast<uint32_t>(boundFramebuffer);
  state.viewportX = viewport[0];
  state.viewportY = viewport[1];
  state.viewportWidth = viewport[2];
  state.viewportHeight = viewport[3];

  if (state.framebuffer != 0u) {
    state.renderExtentWidth = state.viewportWidth;
    state.renderExtentHeight = state.viewportHeight;
    state.flipY = true;
  } else {
    state.renderExtentWidth = state.viewportWidth;
    state.renderExtentHeight = state.viewportHeight;
    state.flipY = false;
  }

  GLboolean scissorEnabled = GL_FALSE;
  glGetBooleanv(kGLScissorTest, &scissorEnabled);
  state.scissorEnabled = (scissorEnabled == GL_TRUE);
  if (state.scissorEnabled) {
    GLint scissorBox[4] = {0, 0, 0, 0};
    glGetIntegerv(kGLScissorBox, scissorBox);
    state.scissorX = scissorBox[0];
    state.scissorY = scissorBox[1];
    state.scissorWidth = scissorBox[2];
    state.scissorHeight = scissorBox[3];
  }
#endif

  return state;
}

bool IsHdrPostProcessRequested(
    const NoMoreDay::render::core::RenderConfig &config) {
  return config.bloomEnabled || config.fxaaEnabled || config.vignetteEnabled ||
         (config.colorGradingEnabled && config.colorGradingLutSize > 0);
}

bool IsHdrScenePipelineRequested(
    const NoMoreDay::render::core::RenderConfig &config) {
  return config.dynamicLightingEnabled || config.volumetricLightEnabled ||
         config.v3Enabled || IsHdrPostProcessRequested(config);
}

// Phase D (RG-1): GI pass sizing on same-resolution (re)enable is now driven by
// RenderGraph::OnResize, which fans out to every node.pass->OnResize including
// the four GI passes. The manual per-pass fan-out (former EnsureGiPassesSized)
// is removed; the Execute-time Ensure* calls remain the authoritative sizer.

struct AutoDegradeRuntimeState {
  bool initialized = false;
  NoMoreDay::render::core::QualityTier trackedTier =
      NoMoreDay::render::core::QualityTier::Medium;
  double overBudgetSince = 0.0;
  double underBudgetSince = 0.0;
  double lastTransitionAt = 0.0;
  uint64_t lastSampleFrameIndex = 0;
};

AutoDegradeRuntimeState g_autoDegradeState = {};

NoMoreDay::render::core::AdaptiveQualityController g_adaptiveQualityController;
bool g_adaptiveQualityConfigured = false;
NoMoreDay::render::core::QualityTier g_adaptiveQualityTier =
    NoMoreDay::render::core::QualityTier::Medium;

void ConfigureAdaptiveQualityController(
    const NoMoreDay::render::core::RenderConfig &config,
    NoMoreDay::render::core::QualityTier tier) {
  if (g_adaptiveQualityConfigured && g_adaptiveQualityTier == tier) {
    return;
  }

  auto settings = config.adaptiveQuality;
  const auto tierBudget =
      NoMoreDay::render::core::QualityTierManager::GetAutoDegradeBudgetThresholds(
          tier);
  if (settings.downThresholdMs <= 0.0f) {
    settings.downThresholdMs = tierBudget.degradeTriggerMs;
  }
  if (settings.upThresholdMs <= 0.0f) {
    settings.upThresholdMs = tierBudget.recoverTriggerMs;
  }
  g_adaptiveQualityController.Configure(settings);
  g_adaptiveQualityController.Reset(GetTime());
  g_adaptiveQualityTier = tier;
  g_adaptiveQualityConfigured = true;
  LOG_INFO("AdaptiveQuality: configured enabled={} locked={} scale={:.3f} range=[{:.3f},{:.3f}] "
           "downMs={:.3f} upMs={:.3f} cooldown={:.1f}s",
           settings.dynamicResolutionEnabled ? 1 : 0,
           settings.renderScaleLocked ? 1 : 0, settings.renderScale,
           settings.minRenderScale, settings.maxRenderScale,
           settings.downThresholdMs, settings.upThresholdMs,
           settings.cooldownSeconds);
}

float PickPassCostMs(const NoMoreDay::render::debug::PassTimingStats &stats) {
  if (stats.gpuState == NoMoreDay::render::debug::QueryState::Valid &&
      stats.gpuMeanMs > 0.0f) {
    return stats.gpuMeanMs;
  }
  return stats.cpuMeanMs;
}

float ComputeAggregateFrameCostMs(
    const std::array<NoMoreDay::render::debug::PassTimingStats,
                     static_cast<size_t>(
                         NoMoreDay::render::debug::RenderPassId::Count)>
        &passStats) {
  float sumMs = 0.0f;
  for (const auto &stats : passStats) {
    sumMs += PickPassCostMs(stats);
  }
  return sumMs;
}

float ComputeAggregateBudgetMs(
    const std::array<NoMoreDay::render::debug::PassTimingStats,
                     static_cast<size_t>(
                         NoMoreDay::render::debug::RenderPassId::Count)>
        &passStats) {
  float sumMs = 0.0f;
  for (const auto &stats : passStats) {
    sumMs += stats.budgetMs;
  }
  return sumMs;
}

std::string BuildPassTimingSummary(
    const std::array<NoMoreDay::render::debug::PassTimingStats,
                     static_cast<size_t>(
                         NoMoreDay::render::debug::RenderPassId::Count)>
        &passStats) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(3);
  for (size_t i = 0; i < passStats.size(); ++i) {
    const auto passId = static_cast<NoMoreDay::render::debug::RenderPassId>(i);
    if (i > 0) {
      oss << ", ";
    }
    oss << NoMoreDay::render::debug::RenderProfiler::ToString(passId) << "="
        << PickPassCostMs(passStats[i]);
  }
  return oss.str();
}

void UpdateAutoDegradePolicy(double nowSeconds) {
  auto &qualityManager = NoMoreDay::render::core::QualityTierManager::Get();
  if (!qualityManager.IsInitialized()) {
    return;
  }

  const auto &config = qualityManager.GetConfig();
  if (config.adaptiveQuality.dynamicResolutionEnabled &&
      !config.adaptiveQuality.renderScaleLocked) {
    return;
  }

  const auto tier = qualityManager.GetTier();
  const auto thresholds =
      NoMoreDay::render::core::QualityTierManager::GetAutoDegradeBudgetThresholds(
          tier);
  if (!g_autoDegradeState.initialized || g_autoDegradeState.trackedTier != tier) {
    g_autoDegradeState.initialized = true;
    g_autoDegradeState.trackedTier = tier;
    g_autoDegradeState.overBudgetSince = 0.0;
    g_autoDegradeState.underBudgetSince = 0.0;
    g_autoDegradeState.lastTransitionAt = nowSeconds;
    g_autoDegradeState.lastSampleFrameIndex = 0;
    qualityManager.ResetAutoDegrade("tier_changed");
    return;
  }

  const auto frameResult =
      NoMoreDay::render::debug::GPUTimerQueryRing::Get().GetFrameResult();
  if (frameResult.state != NoMoreDay::render::debug::QueryState::Valid ||
      frameResult.frameIndex == 0 ||
      frameResult.frameIndex == g_autoDegradeState.lastSampleFrameIndex) {
    return;
  }
  g_autoDegradeState.lastSampleFrameIndex = frameResult.frameIndex;

  const float frameMs = static_cast<float>(frameResult.gpuTimeMs);
  const float budgetMs = thresholds.degradeTriggerMs;
  const bool overBudget = frameMs > thresholds.degradeTriggerMs;
  const bool underBudget = frameMs < thresholds.recoverTriggerMs;
  const bool cooldownReady =
      (nowSeconds - g_autoDegradeState.lastTransitionAt) >=
      static_cast<double>(thresholds.cooldownSeconds);

  std::string timingSummary = "frame_gpu_aggregate";
  if (g_renderProfiler != nullptr) {
    timingSummary = BuildPassTimingSummary(g_renderProfiler->GetAllStats());
  }

  if (overBudget) {
    if (g_autoDegradeState.overBudgetSince <= 0.0) {
      g_autoDegradeState.overBudgetSince = nowSeconds;
    }
    g_autoDegradeState.underBudgetSince = 0.0;
    const bool sustainedOverload =
        (nowSeconds - g_autoDegradeState.overBudgetSince) >=
        static_cast<double>(thresholds.sustainSeconds);
    if (cooldownReady && sustainedOverload &&
        qualityManager.IncreaseAutoDegradeLevel("budget_overflow", frameMs, budgetMs)) {
      g_autoDegradeState.lastTransitionAt = nowSeconds;
      g_autoDegradeState.overBudgetSince = 0.0;
      LOG_WARN(
          "RenderAutoDegrade: action=degrade reason=budget_overflow tier={} level={} "
          "frameMs={:.3f} budgetMs={:.3f} triggerMs={:.3f} recoverMs={:.3f} "
          "passTimings=[{}]",
          NoMoreDay::render::core::ToString(tier), qualityManager.GetAutoDegradeLevel(),
          frameMs, budgetMs, thresholds.degradeTriggerMs, thresholds.recoverTriggerMs,
          timingSummary);
    }
    return;
  }

  if (underBudget) {
    if (g_autoDegradeState.underBudgetSince <= 0.0) {
      g_autoDegradeState.underBudgetSince = nowSeconds;
    }
    g_autoDegradeState.overBudgetSince = 0.0;
    const bool sustainedRecovery =
        (nowSeconds - g_autoDegradeState.underBudgetSince) >=
        static_cast<double>(thresholds.sustainSeconds);
    if (cooldownReady && sustainedRecovery &&
        qualityManager.DecreaseAutoDegradeLevel("budget_recovered", frameMs, budgetMs)) {
      g_autoDegradeState.lastTransitionAt = nowSeconds;
      g_autoDegradeState.underBudgetSince = 0.0;
      LOG_INFO(
          "RenderAutoDegrade: action=recover reason=budget_recovered tier={} level={} "
          "frameMs={:.3f} budgetMs={:.3f} triggerMs={:.3f} recoverMs={:.3f} "
          "passTimings=[{}]",
          NoMoreDay::render::core::ToString(tier), qualityManager.GetAutoDegradeLevel(),
          frameMs, budgetMs, thresholds.degradeTriggerMs, thresholds.recoverTriggerMs,
          timingSummary);
    }
    return;
  }

  g_autoDegradeState.overBudgetSince = 0.0;
  g_autoDegradeState.underBudgetSince = 0.0;
}

void UpdateAdaptiveQualityPolicy(double nowSeconds) {
  auto &qualityManager = NoMoreDay::render::core::QualityTierManager::Get();
  if (!qualityManager.IsInitialized()) {
    return;
  }

  const auto &config = qualityManager.GetConfig();
  ConfigureAdaptiveQualityController(config, qualityManager.GetTier());
  const auto frameResult =
      NoMoreDay::render::debug::GPUTimerQueryRing::Get().GetFrameResult();
  const double p95Ms =
      NoMoreDay::render::debug::GPUTimerQueryRing::Get().GetValidFrameP95Ms();
  const NoMoreDay::render::core::AdaptiveQualityGpuWindow sample = {
      frameResult.state == NoMoreDay::render::debug::QueryState::Valid &&
          p95Ms >= 0.0,
      static_cast<float>(std::max(0.0, p95Ms)), frameResult.frameIndex};
  const auto decision = g_adaptiveQualityController.Update(sample, nowSeconds);

  if (decision.action ==
      NoMoreDay::render::core::AdaptiveQualityAction::RequestFeatureDegrade) {
    const float budgetMs = config.adaptiveQuality.downThresholdMs > 0.0f
                               ? config.adaptiveQuality.downThresholdMs
                               : NoMoreDay::render::core::QualityTierManager::
                                     GetAutoDegradeBudgetThresholds(
                                         qualityManager.GetTier())
                                     .degradeTriggerMs;
    qualityManager.IncreaseAutoDegradeLevel("drs_scale_floor",
                                            static_cast<float>(p95Ms), budgetMs);
  }

  if (decision.action != NoMoreDay::render::core::AdaptiveQualityAction::Keep) {
    LOG_INFO("AdaptiveQuality: action={} reason={} scale={:.3f}->{:.3f} "
             "gpuP95Ms={:.3f} sampleFrame={}",
             NoMoreDay::render::core::ToString(decision.action),
             NoMoreDay::render::core::ToString(decision.reason),
             decision.previousScale, decision.newScale, p95Ms,
             decision.sampleFrameIndex);
  }
}

Mesh &GetLabelQuadMesh() {
  static Mesh quadMesh = {0};
  if (quadMesh.vertexCount != 0) {
    return quadMesh;
  }

  Mesh mesh = {0};
  mesh.triangleCount = 2;
  mesh.vertexCount = 6;
  mesh.vertices = static_cast<float *>(MemAlloc(18 * sizeof(float)));
  mesh.texcoords = static_cast<float *>(MemAlloc(12 * sizeof(float)));

  const float vertices[] = {0, 0, 0, 0, 1, 0, 1, 1, 0,
                            0, 0, 0, 1, 1, 0, 1, 0, 0};
  const float texcoords[] = {0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0};
  std::memcpy(mesh.vertices, vertices, sizeof(vertices));
  std::memcpy(mesh.texcoords, texcoords, sizeof(texcoords));
  UploadMesh(&mesh, false);
  quadMesh = mesh;
  return quadMesh;
}

void ExecuteScenePass(RenderFrameData &frame,
                      NoMoreDay::render::GameplayRenderHooks *gameplayHooks) {
  BeginMode2D(frame.camera);

  // 1. Draw Map/Level Background and Scene Content first
  if (gameplayHooks != nullptr) {
    NoMoreDay::render::GameplayRenderFrame hooksFrame = frame.ToHooksFrame();
    gameplayHooks->onScene(hooksFrame);
  }

  // 2. Draw GPU entity MDI entities (monsters) on top of the map floor
  if (frame.context.renderContext != nullptr) {
    frame.context.renderContext->GPU().Render(
        {frame.context.resources, &frame.context.renderContext->MDI(),
         frame.context.renderAlpha},
        frame.camera);
  }
  rlDisableShader();
  rlSetBlendMode(RL_BLEND_ALPHA);
  rlActiveTextureSlot(0);
  NoMoreDay::utils::GPUUtils::ActiveTexture(0x84C0);

  EndMode2D();
}

void ExecuteVFXPass(RenderFrameData &frame,
                    NoMoreDay::render::GameplayRenderHooks *gameplayHooks) {
  BeginMode2D(frame.camera);
  Matrix viewProj = NoMoreDay::systems::GPUParticleSystem::Get().BuildMVP(
      frame.camera);
  // Keep VFX order stable: particles -> trails -> effect overlays.
  NoMoreDay::systems::GPUParticleSystem::Get().Render(frame.camera);
  NoMoreDay::render::GPUTrailRenderer::Get().Render(frame.camera);
  if (!frame.gpuTextEnabled) {
    NoMoreDay::render::PopupRenderer::Get().Render(viewProj);
  }

  if (gameplayHooks != nullptr) {
    // Game content: AttackEffect/VisualEffect switch, Projectile->GPUSkillEffect
    // submission, distortion + resist-overlay drains. GPUSkillEffectSystem
    // Render stays here (Engine-owned VFX singleton render).
    NoMoreDay::render::GameplayRenderFrame hooksFrame = frame.ToHooksFrame();
    gameplayHooks->onVFX(hooksFrame);
  }
  NoMoreDay::systems::GPUSkillEffectSystem::Get().Render(frame.camera);
  EndMode2D();
}

void ExecuteGPUTextPass(RenderFrameData &frame) {
  if (!frame.gpuTextEnabled) {
    return;
  }
  Matrix viewProj =
      NoMoreDay::systems::GPUParticleSystem::Get().BuildMVP(frame.camera);
  NoMoreDay::render::GPUTextSystem::Get().Render(viewProj);
}

void ExecuteGPULootPass(RenderFrameData &frame) {
  if (!frame.gpuLootEnabled) {
    return;
  }
  auto &gpuLoot = NoMoreDay::render::GPULootSystem::Get();
  gpuLoot.Dispatch(frame.camera, GetScreenWidth(), GetScreenHeight(), true);
  Matrix viewProj =
      NoMoreDay::systems::GPUParticleSystem::Get().BuildMVP(frame.camera);
  gpuLoot.Render(viewProj, frame.gpuLootGlowEnabled);
}

void ExecuteUIWorldPass(RenderFrameData &frame,
                        NoMoreDay::render::GameplayRenderHooks *gameplayHooks) {
  BeginMode2D(frame.camera);
  if (gameplayHooks != nullptr) {
    // Game content: CPU damage popups, loot label collection/sort/overlap
    // resolution, label/glyph/beam instance buffer fill.
    NoMoreDay::render::GameplayRenderFrame hooksFrame = frame.ToHooksFrame();
    gameplayHooks->onUIWorld(hooksFrame);
    frame.font = hooksFrame.font;
    // B4: MSDF glyph out-fields flow back the same way as frame.font — the
    // adapter decides the glyph mode inside BuildCpuLootLabels and the Engine
    // glyph draw below reads the Engine-side copy.
    frame.glyphMsdfEnabled = hooksFrame.glyphMsdfEnabled;
    frame.glyphMsdfPxRange = hooksFrame.glyphMsdfPxRange;
  }
  if (frame.gpuLootEnabled) {
    EndMode2D();
    return;
  }

  Mesh &quadMesh = GetLabelQuadMesh();

  rlDrawRenderBatchActive();
  rlDisableDepthMask();
  rlDisableDepthTest();
  rlDisableBackfaceCulling();
  rlSetBlendMode(RL_BLEND_ALPHA);

  if (!s_beamBuffer.empty() && s_beamShader.id != 0 && s_beamInstanceBuffer) {
    const size_t sz = s_beamBuffer.size() * sizeof(NoMoreDay::render::GPUBeamInstance);
    if (sz > s_beamInstanceBuffer->GetSize()) {
      s_beamInstanceBuffer->Create(sz * 2, s_beamBuffer.data(), RL_DYNAMIC_DRAW);
    } else {
      s_beamInstanceBuffer->OrphanAndUpload(s_beamBuffer.data(), sz,
                                            RL_DYNAMIC_DRAW);
    }
    s_beamInstanceBuffer->BindBase(static_cast<uint32_t>(
        NoMoreDay::RenderConstants::Binding::SSBO_BEAM_INSTANCE));
    BeginShaderMode(s_beamShader);
    const Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    SetShaderValueMatrix(s_beamShader, s_beamMvpLoc, mvp);
    rlEnableVertexArray(quadMesh.vaoId);
    rlDrawVertexArrayInstanced(0, 6, static_cast<int>(s_beamBuffer.size()));
    rlDisableVertexArray();
    EndShaderMode();
  }

  if (frame.labelBuffer != nullptr && !frame.labelBuffer->empty() &&
      frame.labelShader != nullptr && frame.labelShader->id != 0 &&
      frame.labelInstanceBuffer != nullptr) {
    const size_t sz = frame.labelBuffer->size() *
                      sizeof(NoMoreDay::components::GPULabelInstance);
    if (sz > frame.labelInstanceBuffer->GetSize()) {
      frame.labelInstanceBuffer->Create(sz * 2, frame.labelBuffer->data(),
                                        RL_DYNAMIC_DRAW);
    } else {
      frame.labelInstanceBuffer->OrphanAndUpload(frame.labelBuffer->data(), sz,
                                                 RL_DYNAMIC_DRAW);
    }
    frame.labelInstanceBuffer->BindBase(static_cast<uint32_t>(
        NoMoreDay::RenderConstants::Binding::SSBO_LABEL_INSTANCE));
    BeginShaderMode(*frame.labelShader);
    const Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    SetShaderValueMatrix(*frame.labelShader, frame.labelMvpLoc, mvp);
    rlEnableVertexArray(quadMesh.vaoId);
    rlDrawVertexArrayInstanced(0, 6,
                               static_cast<int>(frame.labelBuffer->size()));
    rlDisableVertexArray();
    EndShaderMode();
  }

  if (frame.glyphBuffer != nullptr && !frame.glyphBuffer->empty() &&
      frame.glyphShader != nullptr && frame.glyphShader->id != 0 &&
      frame.glyphInstanceBuffer != nullptr) {
    const size_t sz = frame.glyphBuffer->size() *
                      sizeof(NoMoreDay::components::GPUGlyphInstance);
    if (sz > frame.glyphInstanceBuffer->GetSize()) {
      frame.glyphInstanceBuffer->Create(sz * 2, frame.glyphBuffer->data(),
                                        RL_DYNAMIC_DRAW);
    } else {
      frame.glyphInstanceBuffer->OrphanAndUpload(frame.glyphBuffer->data(), sz,
                                                 RL_DYNAMIC_DRAW);
    }
    frame.glyphInstanceBuffer->BindBase(static_cast<uint32_t>(
        NoMoreDay::RenderConstants::Binding::SSBO_GLYPH_INSTANCE));

    // B4/H-01: MSDF glyph branch — the adapter's MSDF templates carry MSDF
    // atlas UVs, so the draw must bind glyph_msdf.frag + the GPUTextSystem-
    // owned atlas when the mode is active. glyphMsdfEngineReady mirrors the
    // Engine resource state (s_glyphMsdfShader loaded AND GPUTextSystem
    // initialized); the adapter gates the MSDF path on it, so this branch is
    // the only shader/atlas pair that can be active with MSDF UVs. The shader
    // pointer/id checks below are defensive redundancy.
    const bool msdfReady =
        frame.glyphMsdfEnabled && frame.glyphMsdfEngineReady &&
        frame.glyphMsdfShader != nullptr && frame.glyphMsdfShader->id != 0;

    bool drawGlyphs = true;
    if (msdfReady) {
      // MSDF path: median-decoded distance field from the GPUTextSystem atlas
      // (still texture unit 3 / slot 3, unchanged), with the per-frame
      // screen-space pixel range the adapter derived from the label font size.
      BeginShaderMode(*frame.glyphMsdfShader);
      const Matrix mvp =
          MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
      SetShaderValueMatrix(*frame.glyphMsdfShader, frame.glyphMsdfMvpLoc, mvp);

      rlActiveTextureSlot(3);
      rlEnableTexture(
          NoMoreDay::render::GPUTextSystem::Get().GetAtlasTexture().id);
      if (frame.glyphMsdfTexLoc != -1) {
        int texUnit = 3;
        rlSetUniform(frame.glyphMsdfTexLoc, &texUnit, RL_SHADER_UNIFORM_INT, 1);
      }
      if (frame.glyphMsdfPxRangeLoc != -1) {
        rlSetUniform(frame.glyphMsdfPxRangeLoc, &frame.glyphMsdfPxRange,
                     RL_SHADER_UNIFORM_FLOAT, 1);
      }
    } else if (frame.glyphMsdfEnabled) {
      // H-01 defensive guard: the adapter only enables the MSDF path when the
      // Engine broadcasts glyphMsdfEngineReady, so this branch is theoretically
      // unreachable. Kept as a safety net — sampling the bitmap atlas with
      // MSDF UVs would render wrong texels, so skip the glyph draw instead of
      // emitting garbage. One-time log keeps the defensive path visible.
      drawGlyphs = false;
      static bool s_loggedMsdfSkip = false;
      if (!s_loggedMsdfSkip) {
        LOG_WARN("RenderSystem: MSDF glyph mode without Engine MSDF resources "
                 "(defensive, unreachable); glyph draw skipped.");
        s_loggedMsdfSkip = true;
      }
    } else {
      // Legacy bitmap path: frame.font atlas + glyph.frag.
      BeginShaderMode(*frame.glyphShader);
      const Matrix mvp =
          MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
      SetShaderValueMatrix(*frame.glyphShader, frame.glyphMvpLoc, mvp);

      rlActiveTextureSlot(3);
      rlEnableTexture(frame.font.texture.id);
      if (frame.glyphTexLoc != -1) {
        int texUnit = 3;
        rlSetUniform(frame.glyphTexLoc, &texUnit, RL_SHADER_UNIFORM_INT, 1);
      }
    }

    if (drawGlyphs) {
      rlEnableVertexArray(quadMesh.vaoId);
      rlDrawVertexArrayInstanced(0, 6,
                                 static_cast<int>(frame.glyphBuffer->size()));
      rlDisableVertexArray();
      EndShaderMode();
      rlActiveTextureSlot(0);
    }
  }

  rlDrawRenderBatchActive();
  rlSetBlendMode(RL_BLEND_ALPHA);
  EndMode2D();
}

void ExecuteCompositePass() {
  rlDrawRenderBatchActive();
  rlSetBlendMode(RL_BLEND_ALPHA);
}

void ExecuteCompositePass(
    const NoMoreDay::render::resources::FramebufferHandle *hdrBuffer,
    const CompositeTargetState &targetState) {
  if (hdrBuffer == nullptr || !hdrBuffer->IsValid()) {
    ExecuteCompositePass();
    return;
  }

  constexpr uint32_t kGLFramebuffer = 0x8D40;
  rlDrawRenderBatchActive();
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, targetState.framebuffer);
  NoMoreDay::utils::GPUUtils::Viewport(targetState.viewportX, targetState.viewportY,
                                       targetState.viewportWidth,
                                       targetState.viewportHeight);

  // Switch to default 2D projection matrix for full-screen quad drawing so camera transforms don't corrupt composite
  rlMatrixMode(RL_PROJECTION);
  rlPushMatrix();
  rlLoadIdentity();
  rlOrtho(0.0, targetState.viewportWidth, targetState.viewportHeight, 0.0, 0.0, 1.0);
  rlMatrixMode(RL_MODELVIEW);
  rlPushMatrix();
  rlLoadIdentity();

  Texture2D hdrTexture = {};
  hdrTexture.id = hdrBuffer->colorTexture;
  hdrTexture.width = hdrBuffer->width;
  hdrTexture.height = hdrBuffer->height;
  hdrTexture.mipmaps = 1;
  hdrTexture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;

  const Rectangle source = {0.0f, 0.0f, static_cast<float>(hdrTexture.width),
                            -static_cast<float>(hdrTexture.height)};
  const Rectangle target = {0.0f, 0.0f,
                            static_cast<float>(targetState.viewportWidth),
                            static_cast<float>(targetState.viewportHeight)};
  DrawTexturePro(hdrTexture, source, target, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
  rlDrawRenderBatchActive();

  rlMatrixMode(RL_PROJECTION);
  rlPopMatrix();
  rlMatrixMode(RL_MODELVIEW);
  rlPopMatrix();
}

} // namespace

RenderTargetExtent RenderSystem::GetRenderTargetExtent(int nativeWidth,
                                                       int nativeHeight) {
  const float scale = GetRenderScale();
  RenderTargetExtent extent = {};
  extent.scale = scale;
  extent.width = std::max(1, static_cast<int>(std::lround(
                                 static_cast<float>(nativeWidth) * scale)));
  extent.height = std::max(1, static_cast<int>(std::lround(
                                  static_cast<float>(nativeHeight) * scale)));
  return extent;
}

float RenderSystem::GetRenderScale() {
  return g_adaptiveQualityController.GetCurrentScale();
}

void RenderSystem::NotifyRenderTargetResize() {
  if (g_adaptiveQualityConfigured) {
    g_adaptiveQualityController.Reset(GetTime());
  }
}

void RenderSystem::AddDistortionSource(float worldX, float worldY, float radius,
                                       float strength) {
  if (g_distortionPass == nullptr) {
    return;
  }

  const auto &config =
      NoMoreDay::render::core::QualityTierManager::Get().GetConfig();
  if (!config.distortionEnabled) {
    return;
  }

  g_distortionPass->AddDistortionSource(worldX, worldY, radius, strength);
}

RenderSystem::ScopedTargetStateGuard::ScopedTargetStateGuard() {
  target = CaptureCompositeTargetState();
}

RenderSystem::ScopedTargetStateGuard::~ScopedTargetStateGuard() {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_43)
  constexpr uint32_t kGLFramebuffer = 0x8D40;
  constexpr uint32_t kGLScissorTest = 0x0C11;
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, target.framebuffer);
  NoMoreDay::utils::GPUUtils::Viewport(target.viewportX, target.viewportY,
                                       target.viewportWidth, target.viewportHeight);
  if (target.scissorEnabled) {
    glEnable(kGLScissorTest);
    glScissor(target.scissorX, target.scissorY, target.scissorWidth,
              target.scissorHeight);
  } else {
    glDisable(kGLScissorTest);
  }
#endif
}

bool RenderSystem::Initialize() {
  if (s_initialized) {
    return true;
  }

  if (!NoMoreDay::render::abi::ValidateGeneratedShaderABI(
          NoMoreDay::render::abi::GetGeneratedShaderABIManifest(), false)) {
    LOG_CRITICAL("RenderSystem::Initialize: GPU ABI validation failed!");
    Shutdown();
    return false;
  }

  auto &qualityManager = NoMoreDay::render::core::QualityTierManager::Get();
  qualityManager.Initialize("settings.json");
  if (!qualityManager.IsInitialized()) {
    LOG_CRITICAL("RenderSystem::Initialize: QualityTierManager failed to initialize!");
    Shutdown();
    return false;
  }
  ConfigureAdaptiveQualityController(qualityManager.GetConfig(),
                                     qualityManager.GetTier());
  qualityManager.SetV3ToggleCallback(
      [](bool enabled) { HandleV3RuntimeToggle(enabled); });

  auto &materialManager = NoMoreDay::render::MaterialManager::Get();
  materialManager.Initialize();
  materialManager.LoadFromJson("assets/data/materials_vfx.json");
  if (!materialManager.IsInitialized()) {
    LOG_CRITICAL("RenderSystem::Initialize: MaterialManager failed to initialize!");
    Shutdown();
    return false;
  }

  auto &textureArrayManager = NoMoreDay::render::TextureArrayManager::Get();
  textureArrayManager.Initialize(64, 128);
  if (!textureArrayManager.IsInitialized()) {
    LOG_CRITICAL("RenderSystem::Initialize: TextureArrayManager failed to initialize!");
    Shutdown();
    return false;
  }

  auto &lightManager = NoMoreDay::render::lighting::LightManager::Get();
  lightManager.Initialize();
  if (!lightManager.IsInitialized()) {
    LOG_CRITICAL("RenderSystem::Initialize: LightManager failed to initialize!");
    Shutdown();
    return false;
  }

  // Phase F (RG-4): probe the device capability matrix once at init and fail
  // closed when production-critical features (GL 4.3 core, compute, SSBO,
  // image load/store, glMemoryBarrier) are missing. No silent degradation.
  auto &capMatrix = NoMoreDay::render::core::DeviceCapabilityMatrix::Get();
  const auto capabilityReport = capMatrix.ProbeCapabilities();
  const auto capabilityCheck =
      NoMoreDay::render::core::DeviceCapabilityMatrix::CheckProductionRequirements(
          capabilityReport);
  if (!capabilityCheck.passed) {
    std::string failureReason;
    for (size_t i = 0; i < capabilityCheck.missingRequirements.size(); ++i) {
      if (i > 0) failureReason += ", ";
      failureReason += capabilityCheck.missingRequirements[i];
    }
    LOG_CRITICAL("RenderSystem::Initialize: GPU production requirements not met: {}", failureReason);
    LOG_ERROR("RenderSystem: production capability gate FAILED - missing required feature(s):");
    for (const auto &missing : capabilityCheck.missingRequirements) {
      LOG_ERROR("  - {}", missing);
    }
    LOG_ERROR("{}", capabilityReport.DumpReport());
    Shutdown();
    return false;
  }

  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    // P0 S3: install the resident production GL debug callback so driver
    // errors surface through the log instead of silently. Diagnostic-only.
    NoMoreDay::render::debug::GLDebugCallback::Get().Install();

    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    if (screenWidth > 0 && screenHeight > 0) {
      s_hdrSceneBuffer = NoMoreDay::render::resources::FramebufferManager::Create(
          screenWidth, screenHeight, 0x881A, false); // GL_RGBA16F
    }
  }

  g_postProcessPass = std::make_shared<NoMoreDay::render::passes::PostProcessPass>();
  g_postProcessPass->Initialize();
  g_lightCullingPass =
      std::make_shared<NoMoreDay::render::passes::LightCullingPass>();
  g_lightingPass = std::make_shared<NoMoreDay::render::passes::LightingPass>();
  g_heightShadowPass =
      std::make_shared<NoMoreDay::render::passes::HeightShadowPass>();
  g_occluderExtractPass =
      std::make_shared<NoMoreDay::render::passes::OccluderExtractPass>();
  g_jfaPass = std::make_shared<NoMoreDay::render::passes::JFAPass>();
  g_jfaPass->SetOccluderExtractPass(g_occluderExtractPass.get());
  g_radianceCascadesPass =
      std::make_shared<NoMoreDay::render::passes::RadianceCascadesPass>();
  g_giCompositePass = std::make_shared<NoMoreDay::render::passes::GICompositePass>();
  g_giCompositePass->SetOccluderExtractPass(g_occluderExtractPass.get());
  g_giCompositePass->SetRadianceCascadesPass(g_radianceCascadesPass.get());
  g_fluidSimulationPass =
      std::make_shared<NoMoreDay::render::passes::FluidSimulationPass>();
  g_fluidSimulationPass->SetOccluderExtractPass(g_occluderExtractPass.get());
  g_fluidSimulationPass->SetRadiancePass(g_radianceCascadesPass.get());
  g_lightingPass->Initialize();
  g_shadowPreparePass =
      std::make_shared<NoMoreDay::render::passes::ShadowPreparePass>();
  g_shadowBuildPass = std::make_shared<NoMoreDay::render::passes::ShadowBuildPass>();
  g_shadowResolvePass =
      std::make_shared<NoMoreDay::render::passes::ShadowResolvePass>();
  g_shadowBuildPass->SetPreparePass(g_shadowPreparePass.get());
  g_shadowResolvePass->SetBuildPass(g_shadowBuildPass.get());
  g_lightingPass->SetShadowResolvePass(g_shadowResolvePass.get());
  g_lightingPass->SetLightCullingPass(g_lightCullingPass.get());
  g_volumetricPass =
      std::make_shared<NoMoreDay::render::passes::VolumetricLightPass>();
  g_distortionPass = std::make_shared<NoMoreDay::render::passes::DistortionPass>();
  g_distortionPass->Initialize();
  if (s_hdrSceneBuffer.IsValid() && qualityManager.GetConfig().giEnabled) {
    g_occluderExtractPass->OnResize(s_hdrSceneBuffer.width, s_hdrSceneBuffer.height);
    g_jfaPass->OnResize(s_hdrSceneBuffer.width, s_hdrSceneBuffer.height);
    g_radianceCascadesPass->OnResize(s_hdrSceneBuffer.width, s_hdrSceneBuffer.height);
    g_giCompositePass->OnResize(s_hdrSceneBuffer.width, s_hdrSceneBuffer.height);
  }
  if (s_hdrSceneBuffer.IsValid() && qualityManager.GetConfig().fluidEnabled &&
      g_fluidSimulationPass != nullptr) {
    g_fluidSimulationPass->OnResize(s_hdrSceneBuffer.width, s_hdrSceneBuffer.height);
  }
  g_renderProfiler = std::make_unique<NoMoreDay::render::debug::RenderProfiler>();

  s_labelShader = NoMoreDay::utils::GPUUtils::LoadShaderLabeled(
      "assets/shaders/ui/label_instanced.vert",
      "assets/shaders/ui/label_instanced.frag");

  if (s_labelShader.id != 0) {
    s_labelMvpLoc = GetShaderLocation(s_labelShader, "mvp");
  }

  using NoMoreDay::RenderConstants::Binding;
  s_labelInstanceBuffer = std::make_unique<NoMoreDay::core::ComputeBuffer>();
  s_labelInstanceBuffer->Create(
      1000 * sizeof(NoMoreDay::components::GPULabelInstance), nullptr,
      RL_DYNAMIC_DRAW);
  s_labelInstanceBuffer->BindBase(
      static_cast<uint32_t>(Binding::SSBO_LABEL_INSTANCE));

  s_beamShader = NoMoreDay::utils::GPUUtils::LoadShaderLabeled(
      "assets/shaders/vfx/beam_instanced.vert",
      "assets/shaders/vfx/beam_instanced.frag");
  if (s_beamShader.id != 0) {
    s_beamMvpLoc = GetShaderLocation(s_beamShader, "mvp");
  }

  s_beamInstanceBuffer = std::make_unique<NoMoreDay::core::ComputeBuffer>();
  s_beamInstanceBuffer->Create(500 * sizeof(NoMoreDay::render::GPUBeamInstance), nullptr,
                               RL_DYNAMIC_DRAW);

  s_glyphShader = NoMoreDay::utils::GPUUtils::LoadShaderLabeled(
      "assets/shaders/ui/glyph.vert", "assets/shaders/ui/glyph.frag");
  if (s_glyphShader.id != 0) {
    s_glyphMvpLoc = GetShaderLocation(s_glyphShader, "mvp");
    s_glyphTexLoc = GetShaderLocation(s_glyphShader, "uFontAtlas");
  }

  // B4: MSDF glyph variant. Shares glyph.vert; the fragment shader decodes the
  // MSDF median with a screen-space pixel range uniform. Locations are queried
  // against this program's own id (GL does not guarantee cross-program
  // location equality). On load failure the shader id stays 0 and the glyph
  // draw falls back to the bitmap path.
  s_glyphMsdfShader = NoMoreDay::utils::GPUUtils::LoadShaderLabeled(
      "assets/shaders/ui/glyph.vert", "assets/shaders/ui/glyph_msdf.frag");
  if (s_glyphMsdfShader.id != 0) {
    s_glyphMsdfMvpLoc = GetShaderLocation(s_glyphMsdfShader, "mvp");
    s_glyphMsdfTexLoc = GetShaderLocation(s_glyphMsdfShader, "uFontAtlas");
    s_glyphMsdfPxRangeLoc =
        rlGetLocationUniform(s_glyphMsdfShader.id, "uScreenPxRange");
  } else {
    LOG_WARN("RenderSystem: glyph_msdf.frag failed to load; MSDF glyph "
             "rendering disabled (bitmap path).");
  }

  s_glyphInstanceBuffer = std::make_unique<NoMoreDay::core::ComputeBuffer>();
  s_glyphInstanceBuffer->Create(
      NoMoreDay::RenderConstants::GPU::MAX_GLYPHS *
          sizeof(NoMoreDay::components::GPUGlyphInstance),
      nullptr, RL_DYNAMIC_DRAW);

  NoMoreDay::render::GPULootSystem::Get().Init();

  s_initialized = true;
  return true;
}

void RenderSystem::Shutdown() {
  s_initialized = false;
  g_adaptiveQualityConfigured = false;
  g_adaptiveQualityController.Configure({});
  if (s_labelShader.id != 0) {
    UnloadShader(s_labelShader);
    s_labelShader.id = 0;
  }
  s_labelMvpLoc = -1;
  s_labelInstanceBuffer = nullptr;

  if (s_beamShader.id != 0) {
    UnloadShader(s_beamShader);
    s_beamShader.id = 0;
  }
  s_beamMvpLoc = -1;
  s_beamInstanceBuffer = nullptr;

  if (s_glyphShader.id != 0) {
    UnloadShader(s_glyphShader);
    s_glyphShader.id = 0;
  }
  s_glyphMvpLoc = -1;
  s_glyphTexLoc = -1;
  s_glyphInstanceBuffer = nullptr;
  if (s_glyphMsdfShader.id != 0) {
    UnloadShader(s_glyphMsdfShader);
    s_glyphMsdfShader.id = 0;
    s_glyphMsdfMvpLoc = -1;
    s_glyphMsdfTexLoc = -1;
    s_glyphMsdfPxRangeLoc = -1;
  }
  s_labelBuffer.clear();
  s_glyphBuffer.clear();

  NoMoreDay::render::GPULootSystem::Get().Shutdown();

  if (s_hdrSceneBuffer.IsValid()) {
    NoMoreDay::render::resources::FramebufferManager::Destroy(s_hdrSceneBuffer);
    s_hdrSceneBuffer = {};
  }
  NoMoreDay::render::MaterialManager::Get().Shutdown();
  NoMoreDay::render::TextureArrayManager::Get().Shutdown();
  NoMoreDay::render::lighting::LightManager::Get().Shutdown();
  NoMoreDay::render::lighting::ClusteredLightingState::Get().Shutdown();
  if (g_lightCullingPass) {
    g_lightCullingPass.reset();
  }
  if (g_lightingPass) {
    g_lightingPass->Shutdown();
    g_lightingPass.reset();
  }
  if (g_heightShadowPass) {
    g_heightShadowPass->Shutdown();
    g_heightShadowPass.reset();
  }
  if (g_jfaPass) {
    g_jfaPass->Shutdown();
    g_jfaPass.reset();
  }
  if (g_radianceCascadesPass) {
    g_radianceCascadesPass->Shutdown();
    g_radianceCascadesPass.reset();
  }
  if (g_giCompositePass) {
    g_giCompositePass->Shutdown();
    g_giCompositePass.reset();
  }
  if (g_fluidSimulationPass) {
    g_fluidSimulationPass->Shutdown();
    g_fluidSimulationPass.reset();
  }
  if (g_occluderExtractPass) {
    g_occluderExtractPass->Shutdown();
    g_occluderExtractPass.reset();
  }
  if (g_volumetricPass) {
    g_volumetricPass->Shutdown();
    g_volumetricPass.reset();
  }
  if (g_postProcessPass) {
    g_postProcessPass->Shutdown();
    g_postProcessPass.reset();
  }
  if (g_distortionPass) {
    g_distortionPass->Shutdown();
    g_distortionPass.reset();
  }
  if (g_shadowResolvePass) {
    g_shadowResolvePass->Shutdown();
    g_shadowResolvePass.reset();
  }
  if (g_shadowBuildPass) {
    g_shadowBuildPass->Shutdown();
    g_shadowBuildPass.reset();
  }
  if (g_shadowPreparePass) {
    g_shadowPreparePass.reset();
  }
  g_renderProfiler.reset();
  NoMoreDay::render::GPUTrailRenderer::Get().Shutdown();
  NoMoreDay::render::resources::FullscreenQuad::Shutdown();
  g_transientPool.Shutdown();
  // B2 (P2 AD-8): teardown drain — releases any still-pending retire entries
  // (fence deletion + resource destruction) and pooled resources owned by the
  // GPUTexturePool at engine shutdown.
  NoMoreDay::render::resources::GPUTexturePool::Get().Shutdown();
  // P0 S3: restore the pre-init GL debug callback / GL_DEBUG_OUTPUT state.
  NoMoreDay::render::debug::GLDebugCallback::Get().Shutdown();
}

void RenderSystem::render(entt::registry &registry,
                          const NoMoreDay::render::RenderFrameInput &context,
                          const Camera2D &camera,
                          NoMoreDay::render::GameplayRenderHooks *gameplayHooks) {
  RenderSystem::ScopedTargetStateGuard targetGuard;
  const OffscreenTargetDescriptor &compositeTarget = targetGuard.target;

  RenderFrameData frame{registry, context, camera};
  frame.labelBuffer = &s_labelBuffer;
  frame.glyphBuffer = &s_glyphBuffer;
  frame.labelShader = &s_labelShader;
  frame.glyphShader = &s_glyphShader;
  frame.labelMvpLoc = s_labelMvpLoc;
  frame.glyphMvpLoc = s_glyphMvpLoc;
  frame.glyphTexLoc = s_glyphTexLoc;
  // B4/H-01: MSDF glyph variant resources wired alongside the bitmap glyph
  // shader; the Engine broadcasts its MSDF resource readiness (shader loaded
  // AND GPUTextSystem initialized) so the adapter can gate the MSDF path on it.
  frame.glyphMsdfShader = &s_glyphMsdfShader;
  frame.glyphMsdfMvpLoc = s_glyphMsdfMvpLoc;
  frame.glyphMsdfTexLoc = s_glyphMsdfTexLoc;
  frame.glyphMsdfPxRangeLoc = s_glyphMsdfPxRangeLoc;
  frame.glyphMsdfEngineReady =
      (s_glyphMsdfShader.id != 0 &&
       NoMoreDay::render::GPUTextSystem::Get().IsInitialized());
  frame.labelInstanceBuffer = s_labelInstanceBuffer.get();
  frame.glyphInstanceBuffer = s_glyphInstanceBuffer.get();
  if (gameplayHooks != nullptr) {
    // Game frame-data derivation (camera zoom/font/player/fog state) moved to
    // the adapter. Engine keeps the glyph atlas out-field for the instanced draw.
    NoMoreDay::render::GameplayRenderFrame hooksFrame = frame.ToHooksFrame();
    gameplayHooks->onFrameData(hooksFrame);
    frame.font = hooksFrame.font;
    // Occluder projection: the Game adapter projects the ECS occluders into the
    // Engine-owned staging buffer + FNV signature stats consumed by
    // OccluderExtractPass/ShadowBuildPass through graph::RenderContext.
    gameplayHooks->onOccluders(hooksFrame);
    frame.occluderStaticCount = hooksFrame.occluderStaticCount;
    frame.occluderDynamicCount = hooksFrame.occluderDynamicCount;
    frame.occluderStaticSignature = hooksFrame.occluderStaticSignature;
    frame.occluderDynamicSignature = hooksFrame.occluderDynamicSignature;
    // Height-field projection: the Game adapter projects the ECS terrain/casters/
    // sprites into the Engine-owned stamp buffer + world semantics consumed by
    // HeightShadowPass through graph::RenderContext.
    gameplayHooks->onHeightField(hooksFrame);
    frame.worldWidth = hooksFrame.worldWidth;
    frame.worldHeight = hooksFrame.worldHeight;
    frame.tileWorldSize = hooksFrame.tileWorldSize;
    // Loot projection: the Game adapter projects the ECS dropped loot into the
    // Engine-owned staging buffer via the shared GPULootAdapter; the Engine
    // only uploads the DTO span to the instance SSBO below.
    gameplayHooks->onLoot(hooksFrame);
    // Emissive projection: the Game adapter projects the ECS emissive
    // materials into the Engine-owned stamp buffer via the shared
    // EmissiveStampAdapter; RadianceCascadesPass consumes the DTO span for its
    // per-stamp compute dispatch through graph::RenderContext.
    gameplayHooks->onEmissive(hooksFrame);
  } else {
    // No gameplay adapter (gate/harness): never feed stale occluders from a
    // previous game frame into the graph context.
    s_occluderBuffer.clear();
    frame.occluderStaticCount = 0u;
    frame.occluderDynamicCount = 0u;
    frame.occluderStaticSignature = 0u;
    frame.occluderDynamicSignature = 0u;
    s_heightFieldBuffer.clear();
    frame.worldWidth = 0.0f;
    frame.worldHeight = 0.0f;
    frame.tileWorldSize = 0.0f;
    s_lootInstanceBuffer.clear();
    s_emissiveStampBuffer.clear();
  }

  g_transientPool.BeginFrame();
  // B2 (P2 AD-8): drive the GPUTexturePool retire/eviction pipeline every frame.
  // BeginFrame at frame start drains the retire queue (destroying or recycling
  // GL resources whose retire fence has signaled, instead of leaking them);
  // the matching EndFrame (stale-pool eviction) runs next to
  // g_transientPool.EndFrame() at the end of render(). The frame index comes
  // from GPUResourceRegistry, which advances once per render() call.
  NoMoreDay::render::resources::GPUTexturePool::Get().BeginFrame(
      NoMoreDay::render::resources::GPUResourceRegistry::Get().GetFrameIndex());
  const auto &renderConfig =
      NoMoreDay::render::core::QualityTierManager::Get().GetConfig();
  NoMoreDay::render::GPULootSystem::Get().UploadInstances(s_lootInstanceBuffer);
  const bool gpuTextRuntimeReady =
      NoMoreDay::render::GPUTextSystem::Get().IsInitialized();
  const bool gpuLootRuntimeReady =
      NoMoreDay::render::GPULootSystem::Get().IsInitialized();
  frame.gpuTextEnabled = renderConfig.gpuTextEnabled && gpuTextRuntimeReady;
  frame.gpuLootEnabled = renderConfig.gpuLootEnabled && gpuLootRuntimeReady;
  frame.gpuLootGlowEnabled =
      renderConfig.gpuLootGlowEnabled && gpuLootRuntimeReady;
  static bool s_loggedGpuTextFallback = false;
  static bool s_loggedGpuLootFallback = false;
  if (renderConfig.gpuTextEnabled && !gpuTextRuntimeReady && !s_loggedGpuTextFallback) {
    LOG_WARN("RenderSystem: gpuText enabled in config but GPUTextSystem is not ready; "
             "falling back to CPU popup rendering.");
    s_loggedGpuTextFallback = true;
  }
  if (renderConfig.gpuLootEnabled && !gpuLootRuntimeReady && !s_loggedGpuLootFallback) {
    LOG_WARN("RenderSystem: gpuLoot enabled in config but GPULootSystem is not ready; "
             "falling back to CPU loot label rendering.");
    s_loggedGpuLootFallback = true;
  }
  HandleV3RuntimeToggle(renderConfig.v3Enabled);
  static bool s_prevGiEnabled = false;
  if (renderConfig.giEnabled != s_prevGiEnabled && g_giCompositePass != nullptr) {
    // S7a: temporal history must be invalidated on BOTH transitions so no stale
    // history from the other leg is reused after a GI toggle.
    g_giCompositePass->InvalidateHistory();
    LOG_INFO("RenderSystem: GI enabled {} -> {}, GICompositePass history invalidated",
             s_prevGiEnabled ? 1 : 0, renderConfig.giEnabled ? 1 : 0);
  }
  s_prevGiEnabled = renderConfig.giEnabled;
  if (renderConfig.materialSystemEnabled) {
    NoMoreDay::render::MaterialManager::Get().TryHotReload();
    NoMoreDay::render::MaterialManager::Get().SyncToGPU();
    NoMoreDay::render::MaterialManager::Get().BindSSBO(
        NoMoreDay::RenderConstants::Binding::SSBO_MATERIAL_DATA);
  }
  // Render path split:
  // - default backbuffer path
  // - gameplay offscreen composite path (BeginTextureMode target)
  // Offscreen composite uses framebuffer blit to avoid camera-transform
  // contamination when called inside BeginMode2D.
  const bool hdrPipelineRequested = IsHdrScenePipelineRequested(renderConfig);
  const bool isOffscreenCompositeTarget = (compositeTarget.framebuffer != 0u);
  bool useHdrSceneBuffer = hdrPipelineRequested;
  // GPU Production HDR/GI Closure: Offscreen target (m_sceneRT) runs full HDR/GI pass matrix.
  const bool offscreenV3SafeMode = false;
  if (isOffscreenCompositeTarget) {
    LOG_LIMITED_INFO(
        3.0f,
        "RenderSystem: gameplay offscreen target active, running full HDR/GI pass matrix");
  }
  const bool useDistortionPass =
      useHdrSceneBuffer && !offscreenV3SafeMode && renderConfig.distortionEnabled &&
      (g_postProcessPass != nullptr) && (g_distortionPass != nullptr);
  const bool useVolumetricPass =
      useHdrSceneBuffer && !offscreenV3SafeMode &&
      renderConfig.volumetricLightEnabled &&
      (g_volumetricPass != nullptr);
  if (!useDistortionPass && g_distortionPass != nullptr) {
    g_distortionPass->ResetSources();
  }
  // NOTE:
  // Avoid heavy runtime Shutdown() calls during frame/render-path toggles.
  // They can cause visible hitch spikes when leaving gameplay or switching
  // between offscreen/backbuffer paths. Pass execution is already gated; keep
  // resources alive and release them in RenderSystem::Shutdown().

  // Phase D (D1/D5): unified resize dispatch. RenderGraph::OnResize fans out to
  // every registered node.pass->OnResize once after the frame's passes are
  // added, replacing the manual per-pass fan-out list below. The flags record
  // when a unified OnResize must run this frame (HDR buffer create/resize or
  // same-resolution GI re-enable). The TextureArrayManager rebuild stays at its
  // original site; pass backing create/resize/reclassify behavior is unchanged.
  bool hdrPassesNeedResize = false;
  int resizeWidth = 0;
  int resizeHeight = 0;
  if (useHdrSceneBuffer && NoMoreDay::utils::GPUUtils::IsInitialized()) {
    const int targetWidth = std::max(1, (isOffscreenCompositeTarget && compositeTarget.renderExtentWidth > 0)
                                            ? compositeTarget.renderExtentWidth
                                            : GetScreenWidth());
    const int targetHeight = std::max(1, (isOffscreenCompositeTarget && compositeTarget.renderExtentHeight > 0)
                                             ? compositeTarget.renderExtentHeight
                                             : GetScreenHeight());
    if (!s_hdrSceneBuffer.IsValid()) {
      s_hdrSceneBuffer = NoMoreDay::render::resources::FramebufferManager::Create(
          targetWidth, targetHeight, 0x881A, false); // GL_RGBA16F
      if (s_hdrSceneBuffer.IsValid()) {
        const double approxMb =
            (static_cast<double>(targetWidth) * static_cast<double>(targetHeight) *
             8.0) /
            (1024.0 * 1024.0);
        LOG_INFO("RenderSystem: created HDR scene buffer {}x{} (~{:.2f} MB)",
                 targetWidth, targetHeight, approxMb);
        hdrPassesNeedResize = true;
        resizeWidth = targetWidth;
        resizeHeight = targetHeight;
      } else {
        LOG_LIMITED_WARN(
            3.0f,
            "RenderSystem: failed to allocate HDR scene buffer {}x{}, falling back to direct scene path",
            targetWidth, targetHeight);
        useHdrSceneBuffer = false;
      }
      NoMoreDay::render::TextureArrayManager::Get().RebuildForResize(
          targetWidth, targetHeight);
    } else if (s_hdrSceneBuffer.width != targetWidth ||
               s_hdrSceneBuffer.height != targetHeight) {
      NoMoreDay::render::resources::FramebufferManager::Resize(
          s_hdrSceneBuffer, targetWidth, targetHeight);
      const double approxMb =
          (static_cast<double>(targetWidth) * static_cast<double>(targetHeight) *
           8.0) /
          (1024.0 * 1024.0);
      LOG_INFO("RenderSystem: resized HDR scene buffer {}x{} (~{:.2f} MB)",
               targetWidth, targetHeight, approxMb);
      hdrPassesNeedResize = true;
      resizeWidth = targetWidth;
      resizeHeight = targetHeight;
      NoMoreDay::render::TextureArrayManager::Get().RebuildForResize(
          targetWidth, targetHeight);
    }
  }

  // S7a: same-resolution GI re-enable. When the HDR buffer already exists at the
  // current resolution the create/resize chain above is not hit, so explicitly
  // re-drive the unified resize path (graph.OnResize) to the current HDR scene
  // buffer dimensions; the graph fans out to the four GI pass nodes.
  static bool s_giPassesSized = false;
  if (useHdrSceneBuffer && s_hdrSceneBuffer.IsValid() && renderConfig.giEnabled &&
      !s_giPassesSized) {
    hdrPassesNeedResize = true;
    resizeWidth = s_hdrSceneBuffer.width;
    resizeHeight = s_hdrSceneBuffer.height;
    s_giPassesSized = true;
  }
  if (!renderConfig.giEnabled) {
    s_giPassesSized = false;
  }

  // Gameplay offscreen path renders level/tilemap before RenderSystem::render().
  // Seed HDR scene buffer from current composite target so V3 passes operate on
  // full scene content instead of a blank background.
  if (useHdrSceneBuffer && isOffscreenCompositeTarget && s_hdrSceneBuffer.IsValid()) {
    constexpr uint32_t kGLReadFramebuffer = 0x8CA8;
    constexpr uint32_t kGLDrawFramebuffer = 0x8CA9;
    constexpr uint32_t kGLColorBufferBit = 0x00004000;
    rlDrawRenderBatchActive();
    NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLReadFramebuffer,
                                                compositeTarget.framebuffer);
    NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLDrawFramebuffer,
                                                s_hdrSceneBuffer.fbo);
    rlBlitFramebuffer(compositeTarget.viewportX, compositeTarget.viewportY,
                      compositeTarget.viewportX + compositeTarget.viewportWidth,
                      compositeTarget.viewportY + compositeTarget.viewportHeight, 0,
                      0, s_hdrSceneBuffer.width, s_hdrSceneBuffer.height,
                      kGLColorBufferBit);
  }

  if (useHdrSceneBuffer && !offscreenV3SafeMode &&
      renderConfig.dynamicLightingEnabled && g_lightingPass) {
    // Light projection: the Game adapter projects the ECS lights into the
    // Engine-owned staging buffer via the shared LightAdapter; the Engine only
    // consumes the DTO span (view cull / sort / transient / budget / upload).
    if (gameplayHooks != nullptr) {
      NoMoreDay::render::GameplayRenderFrame hooksFrame = frame.ToHooksFrame();
      gameplayHooks->onLights(hooksFrame);
      frame.ecsLights = hooksFrame.ecsLights;
    } else {
      // No gameplay adapter (gate/harness): never feed stale light candidates
      // from a previous game frame into the LightManager.
      s_lightCandidateBuffer.clear();
      frame.ecsLights = 0;
    }
    NoMoreDay::render::lighting::LightManager::Get().UpdateCandidates(
        s_lightCandidateBuffer, camera, renderConfig.maxLights,
        frame.ecsLights);

    static double s_lastLightingDiagLogTime = 0.0;
    const double now = GetTime();
    if ((now - s_lastLightingDiagLogTime) >= 10.0) {
      const auto &stats = NoMoreDay::render::lighting::LightManager::Get().GetDebugStats();
      const double hdrApproxMb =
          (s_hdrSceneBuffer.IsValid()
               ? (static_cast<double>(s_hdrSceneBuffer.width) *
                  static_cast<double>(s_hdrSceneBuffer.height) * 8.0)
               : 0.0) /
          (1024.0 * 1024.0); // RGBA16F ~= 8 bytes/pixel
      LOG_INFO(
          "RenderSystem: LightingDiag tier={} allowed={} ecs={} transient={} "
          "candidates={} selected={} dropped={} hdr={}x{} (~{:.2f} MB)",
          static_cast<int>(NoMoreDay::render::core::QualityTierManager::Get().GetTier()),
          stats.allowedLights, stats.ecsLights, stats.transientLights,
          stats.candidatesAfterCull, stats.selectedLights, stats.droppedByBudget,
          s_hdrSceneBuffer.width, s_hdrSceneBuffer.height, hdrApproxMb);
      s_lastLightingDiagLogTime = now;
    }
  }

  using NoMoreDay::render::graph::RenderOwnerTag;
  using NoMoreDay::render::graph::RenderResourceTag;

  NoMoreDay::render::graph::RenderGraph graph;
  // P2 AD-6 (M1): feed the live dynamic-resolution scale into the compilation
  // key so adaptive-resolution changes invalidate the cached compiled plan
  // even at an unchanged screen size.
  graph.SetDynamicResolutionScale(RenderSystem::GetRenderScale());
  graph.AddPass(std::make_shared<NoMoreDay::render::passes::ScenePass>(
      [&frame, gameplayHooks, useHdrSceneBuffer, isOffscreenCompositeTarget](
          NoMoreDay::render::graph::RenderContext &context) {
        NoMoreDay::render::core::ScopedGLState scopedState;
        if (useHdrSceneBuffer && context.hdrSceneBuffer.IsValid()) {
          constexpr uint32_t kGLFramebuffer = 0x8D40;
          NoMoreDay::utils::GPUUtils::BindFramebuffer(
              kGLFramebuffer, context.hdrSceneBuffer.fbo);
          NoMoreDay::utils::GPUUtils::Viewport(0, 0,
                                               context.hdrSceneBuffer.width,
                                               context.hdrSceneBuffer.height);
          if (!isOffscreenCompositeTarget) {
            ClearBackground(BLANK);
          }
        }
        ExecuteScenePass(frame, gameplayHooks);
      }));

  if (renderConfig.v3Enabled && useHdrSceneBuffer) {
    if (g_shadowPreparePass != nullptr) {
      graph.AddPass(g_shadowPreparePass);
    }
    if (g_shadowBuildPass != nullptr) {
      graph.AddPass(g_shadowBuildPass);
    }
    if (g_shadowResolvePass != nullptr) {
      graph.AddPass(g_shadowResolvePass);
    }
    if (g_lightCullingPass != nullptr) {
      graph.AddPass(g_lightCullingPass);
    }
  } else if (renderConfig.v3Enabled && !useHdrSceneBuffer) {
    LOG_LIMITED_WARN(
        3.0f,
        "RenderSystem: skip V3 shadow/cluster passes because HDR scene buffer is disabled "
        "(compositeFbo={})",
        compositeTarget.framebuffer);
  }

  if (useHdrSceneBuffer && !offscreenV3SafeMode &&
      renderConfig.dynamicLightingEnabled &&
      g_lightingPass != nullptr) {
    graph.AddPass(g_lightingPass);
  }
  if (useHdrSceneBuffer && !offscreenV3SafeMode &&
      renderConfig.heightShadowEnabled &&
      g_heightShadowPass != nullptr) {
    graph.AddPass(g_heightShadowPass);
  }
  if (useHdrSceneBuffer && !offscreenV3SafeMode && renderConfig.giEnabled &&
      g_occluderExtractPass != nullptr && g_jfaPass != nullptr &&
      g_radianceCascadesPass != nullptr && g_giCompositePass != nullptr) {
    graph.AddPass(g_occluderExtractPass);
    graph.AddPass(g_jfaPass);
    graph.AddPass(std::make_shared<NoMoreDay::render::passes::VFXEmissionSnapshotPass>(
        [](NoMoreDay::render::graph::RenderContext &context) {
          if (g_radianceCascadesPass != nullptr) {
            g_radianceCascadesPass->PrepareVfxEmissionSnapshot(context);
          }
        }));
    graph.AddPass(g_radianceCascadesPass);
    graph.AddPass(g_giCompositePass);
  }
  if (useHdrSceneBuffer && !offscreenV3SafeMode && renderConfig.fluidEnabled &&
      g_fluidSimulationPass != nullptr) {
    graph.AddPass(g_fluidSimulationPass);
  }
  if (useVolumetricPass && g_volumetricPass != nullptr) {
    graph.AddPass(g_volumetricPass);
  }
  graph.AddPass(std::make_shared<NoMoreDay::render::passes::VFXPass>(
      [&frame, gameplayHooks](NoMoreDay::render::graph::RenderContext &) {
        NoMoreDay::render::core::ScopedGLState scopedState;
        ExecuteVFXPass(frame, gameplayHooks);
      }));

  if (frame.gpuTextEnabled) {
    graph.AddPass(std::make_shared<NoMoreDay::render::passes::GPUTextPass>(
        [&frame](NoMoreDay::render::graph::RenderContext &) {
          NoMoreDay::render::core::ScopedGLState scopedState;
          ExecuteGPUTextPass(frame);
        }));
  }

  if (frame.gpuLootEnabled) {
    graph.AddPass(std::make_shared<NoMoreDay::render::passes::GPULootPass>(
        [&frame](NoMoreDay::render::graph::RenderContext &) {
          NoMoreDay::render::core::ScopedGLState scopedState;
          ExecuteGPULootPass(frame);
        }));
  }

  graph.AddPass(std::make_shared<NoMoreDay::render::passes::UIWorldPass>(
      [&frame, gameplayHooks](NoMoreDay::render::graph::RenderContext &) {
        NoMoreDay::render::core::ScopedGLState scopedState;
        ExecuteUIWorldPass(frame, gameplayHooks);
      }));

  if (useHdrSceneBuffer && !offscreenV3SafeMode && g_postProcessPass != nullptr) {
    graph.AddPass(g_postProcessPass);
  }
  if (useDistortionPass && g_distortionPass != nullptr &&
      g_postProcessPass != nullptr) {
    graph.AddPass(g_distortionPass);
  }

  // Phase D (D2/D3): derive the composite input from the graph instead of
  // manual owner tracking. CollectPassDeclarations() runs each registered
  // pass's Setup (declaration collection) without compiling the plan; the last
  // writer of each LDR resource decides which producer feeds composite,
  // preserving the previous distortion > postprocess > hdr priority.
  // P2 AD-6 (H1): this replaces the former Build() here. Building twice per
  // frame was pure overhead -- the second Build() after AddPass(CompositePass)
  // invalidated this result anyway -- and declaration collection is all
  // FindLastWriterOwner needs.
  graph.CollectPassDeclarations();
  const RenderOwnerTag sceneHdrOwner = graph.FindLastWriterOwner(
      RenderResourceTag::SceneHdrColor);
  const RenderOwnerTag postProcessOwner = graph.FindLastWriterOwner(
      RenderResourceTag::PostProcessLdrColor);
  const RenderOwnerTag distortionOwner = graph.FindLastWriterOwner(
      RenderResourceTag::DistortionLdrColor);
  RenderResourceTag compositeInputResource = RenderResourceTag::SceneHdrColor;
  RenderOwnerTag compositeInputOwner = sceneHdrOwner;
  if (distortionOwner == RenderOwnerTag::Distortion) {
    compositeInputResource = RenderResourceTag::DistortionLdrColor;
    compositeInputOwner = RenderOwnerTag::Distortion;
  } else if (postProcessOwner == RenderOwnerTag::PostProcess) {
    compositeInputResource = RenderResourceTag::PostProcessLdrColor;
    compositeInputOwner = RenderOwnerTag::PostProcess;
  }

  graph.AddPass(std::make_shared<NoMoreDay::render::passes::CompositePass>(
      compositeInputResource, compositeInputOwner,
      [compositeInputOwner, useHdrSceneBuffer, compositeTarget](
          NoMoreDay::render::graph::RenderContext &context) {
        NoMoreDay::render::core::ScopedGLState scopedState;
        if (compositeInputOwner == RenderOwnerTag::Distortion &&
            g_distortionPass != nullptr &&
            g_distortionPass->GetOutputBuffer().IsValid()) {
          ExecuteCompositePass(&g_distortionPass->GetOutputBuffer(),
                               compositeTarget);
        } else if (compositeInputOwner == RenderOwnerTag::PostProcess &&
                   g_postProcessPass != nullptr &&
                   g_postProcessPass->GetOutputBuffer().IsValid()) {
          ExecuteCompositePass(&g_postProcessPass->GetOutputBuffer(),
                               compositeTarget);
        } else if (useHdrSceneBuffer && context.hdrSceneBuffer.IsValid()) {
          ExecuteCompositePass(&context.hdrSceneBuffer, compositeTarget);
        } else {
          ExecuteCompositePass();
        }
      }));

  // Phase D (D1): unified resize dispatch once every node (including composite)
  // is registered; the graph fans out to each node.pass->OnResize exactly once.
  if (hdrPassesNeedResize && resizeWidth > 0 && resizeHeight > 0) {
    graph.OnResize(resizeWidth, resizeHeight);
  }

  NoMoreDay::render::graph::RenderContext graphContext = {};
  graphContext.registry = &registry;
  graphContext.resources = frame.context.resources;
  graphContext.camera = &camera;
  graphContext.transientPool = &g_transientPool;
  graphContext.qualityManager = &NoMoreDay::render::core::QualityTierManager::Get();
  graphContext.renderProfiler =
      (g_renderProfiler != nullptr) ? g_renderProfiler.get() : nullptr;
  graphContext.hdrSceneBuffer =
      useHdrSceneBuffer ? s_hdrSceneBuffer
                        : NoMoreDay::render::resources::FramebufferHandle{};
  graphContext.occluders =
      s_occluderBuffer.empty() ? nullptr : s_occluderBuffer.data();
  graphContext.occluderCount = static_cast<uint32_t>(s_occluderBuffer.size());
  graphContext.occluderStaticCount = frame.occluderStaticCount;
  graphContext.occluderDynamicCount = frame.occluderDynamicCount;
  graphContext.occluderStaticSignature = frame.occluderStaticSignature;
  graphContext.occluderDynamicSignature = frame.occluderDynamicSignature;
  graphContext.heightFieldStamps =
      s_heightFieldBuffer.empty() ? nullptr : s_heightFieldBuffer.data();
  graphContext.heightFieldStampCount =
      static_cast<uint32_t>(s_heightFieldBuffer.size());
  graphContext.emissiveStamps =
      s_emissiveStampBuffer.empty() ? nullptr : s_emissiveStampBuffer.data();
  graphContext.emissiveStampCount =
      static_cast<uint32_t>(s_emissiveStampBuffer.size());
  graphContext.worldWidth = frame.worldWidth;
  graphContext.worldHeight = frame.worldHeight;
  graphContext.tileWorldSize = frame.tileWorldSize;

  // AD-3: expose the height field to the GI composite for surface-normal
  // extraction. The producer is HeightShadowPass (GlobalHeightField), which
  // executes earlier in this graph when heightShadowEnabled; the handle is
  // captured pre-execution, so it resolves to a valid texture from the second
  // frame onward (the height field is lazily initialized on its first Execute).
  graphContext.heightFieldTexture =
      (g_heightShadowPass != nullptr) ? g_heightShadowPass->GetHeightFieldTexture()
                                      : 0u;

  // Ensure lazy-backed shadow/cluster resources exist before the
  // imported-backing snapshot below is captured. On the first offscreen-HDR
  // frame these handles are still zero (they are allocated inside pass
  // Execute), which makes the graph reject ShadowBuild/LightCulling bindings
  // and skip lighting for that frame.
  if (useHdrSceneBuffer && !offscreenV3SafeMode && s_hdrSceneBuffer.IsValid()) {
    const auto &backingConfig =
        NoMoreDay::render::core::QualityTierManager::Get().GetConfig();
    const int backingWidth = s_hdrSceneBuffer.width;
    const int backingHeight = s_hdrSceneBuffer.height;
    if (g_shadowBuildPass != nullptr) {
      g_shadowBuildPass->EnsureBackingResources(
          s_occluderBuffer.empty() ? nullptr : s_occluderBuffer.data(),
          static_cast<uint32_t>(s_occluderBuffer.size()), backingWidth,
          backingHeight,
          (backingConfig.shadowMode ==
           NoMoreDay::render::core::ShadowMode::Hybrid)
              ? static_cast<int>(backingConfig.shadowAtlasSize)
              : 0);
    }
    const auto backingClusterGrid =
        NoMoreDay::render::lighting::ClusteredLightingState::
            ComputeClusterGridDimensions(
                static_cast<uint32_t>(std::max(0, backingWidth)),
                static_cast<uint32_t>(std::max(0, backingHeight)),
                backingConfig.clusterTileSize, backingConfig.clusterZSliceCount);
    NoMoreDay::render::lighting::ClusteredLightingState::Get()
        .EnsureBuffersAllocated(
            backingClusterGrid.clusterCount,
            static_cast<uint32_t>(
                NoMoreDay::render::lighting::LightManager::Get()
                    .GetActiveLightRecordsCpu()
                    .size()));
  }

  const auto addImportedBacking = [&graphContext](
      NoMoreDay::render::graph::RenderResourceTag tag, uint32_t buffer,
      uint32_t texture, uint32_t framebuffer) {
    graphContext.importedBackings.push_back({tag, buffer, texture, framebuffer});
  };
  if (g_shadowBuildPass != nullptr) {
    addImportedBacking(NoMoreDay::render::graph::RenderResourceTag::ShadowAtlas,
                       0u, g_shadowBuildPass->GetShadowAtlasTexture(),
                       g_shadowBuildPass->GetShadowAtlasFramebuffer());
    addImportedBacking(
        NoMoreDay::render::graph::RenderResourceTag::ShadowDistanceField, 0u,
        g_shadowBuildPass->GetSdfImageTexture(), 0u);
    addImportedBacking(
        NoMoreDay::render::graph::RenderResourceTag::ShadowOccluderSSBO,
        g_shadowBuildPass->GetOccluderBufferId(), 0u, 0u);
  }
  if (g_shadowResolvePass != nullptr) {
    addImportedBacking(NoMoreDay::render::graph::RenderResourceTag::ShadowMask,
                       0u, g_shadowResolvePass->GetShadowMaskTexture(),
                       g_shadowResolvePass->GetShadowMaskFramebuffer());
  }
  const auto &clusterState =
      NoMoreDay::render::lighting::ClusteredLightingState::Get();
  addImportedBacking(
      NoMoreDay::render::graph::RenderResourceTag::ClusterHeaderSSBO,
      clusterState.GetClusterHeaderBufferId(), 0u, 0u);
  addImportedBacking(
      NoMoreDay::render::graph::RenderResourceTag::ClusterLightIndexSSBO,
      clusterState.GetClusterLightIndexBufferId(), 0u, 0u);
  addImportedBacking(
      NoMoreDay::render::graph::RenderResourceTag::ClusterPackedLightSSBO,
      clusterState.GetClusterPackedLightBufferId(), 0u, 0u);
  addImportedBacking(
      NoMoreDay::render::graph::RenderResourceTag::ClusterCounterSSBO,
      clusterState.GetCounterBufferId(), 0u, 0u);
  addImportedBacking(
      NoMoreDay::render::graph::RenderResourceTag::LightBoundsSSBO,
      clusterState.GetLightBoundsBufferId(), 0u, 0u);
  addImportedBacking(
      NoMoreDay::render::graph::RenderResourceTag::LightBufferSSBO,
      NoMoreDay::render::lighting::LightManager::Get().GetLightBufferId(), 0u,
      0u);
  if (g_jfaPass != nullptr && g_jfaPass->HasDistanceField()) {
    graphContext.giDistanceFieldTexture = g_jfaPass->GetDistanceFieldTexture();
    graphContext.giDistanceFieldWidth = g_jfaPass->GetDistanceFieldWidth();
    graphContext.giDistanceFieldHeight = g_jfaPass->GetDistanceFieldHeight();
  } else {
    graphContext.giDistanceFieldTexture = 0u;
    graphContext.giDistanceFieldWidth = 0;
    graphContext.giDistanceFieldHeight = 0;
  }
  if (g_radianceCascadesPass != nullptr && g_radianceCascadesPass->GetEmissiveTexture() != 0u) {
    graphContext.giEmissiveTexture = g_radianceCascadesPass->GetEmissiveTexture();
    graphContext.giEmissiveWidth = g_radianceCascadesPass->GetEmissiveWidth();
    graphContext.giEmissiveHeight = g_radianceCascadesPass->GetEmissiveHeight();
  } else {
    graphContext.giEmissiveTexture = 0u;
    graphContext.giEmissiveWidth = 0;
    graphContext.giEmissiveHeight = 0;
  }
  if (g_radianceCascadesPass != nullptr && g_radianceCascadesPass->HasRadianceMap()) {
    graphContext.giRadianceTexture = g_radianceCascadesPass->GetRadianceTexture();
    graphContext.giRadianceWidth = g_radianceCascadesPass->GetRadianceWidth();
    graphContext.giRadianceHeight = g_radianceCascadesPass->GetRadianceHeight();
  } else {
    graphContext.giRadianceTexture = 0u;
    graphContext.giRadianceWidth = 0;
    graphContext.giRadianceHeight = 0;
  }

  if (graphContext.renderProfiler != nullptr) {
    graphContext.renderProfiler->BeginFrame();
  }
  graph.Build();
  // W6 (M0-C): capture the real executed pass order for the hardware gate's
  // pass-trace evidence (actual graph, actual passes, actual order).
  s_lastExecutedPassOrder = graph.GetCompiledPlan().passOrder;
  graph.Execute(graphContext);
  // W5.6 (RG-3 contract): exact-one frame advancement. Advance the registry
  // immediately after a successful graph execute - never per pass and never
  // before a failed/aborted execute. Downstream snapshot consumers (profiler
  // HUD, adaptive policy, gate quiescence) read the just-advanced epoch.
  NoMoreDay::render::resources::GPUResourceRegistry::Get().AdvanceFrame();
  if (graphContext.renderProfiler != nullptr) {
    // S1b: single Poll point of the render path. Must precede every
    // DRS/adaptive-policy read so HUD/summary/DRS consume backfilled stats.
    graphContext.renderProfiler->FlushRingToProfiler();
    graphContext.renderProfiler->UpdateStats();
  }
  const double policyNow = GetTime();
  UpdateAdaptiveQualityPolicy(policyNow);
  UpdateAutoDegradePolicy(policyNow);
  if (graphContext.renderProfiler != nullptr) {
    graphContext.renderProfiler->EndFrame();

    const auto &passStats = graphContext.renderProfiler->GetAllStats();

    if (renderConfig.profilerHudEnabled) {
      static double s_lastProfilerLog = 0.0;
      const double now = GetTime();
      if ((now - s_lastProfilerLog) >= 5.0) {
        for (size_t i = 0; i < passStats.size(); ++i) {
          const auto passId = static_cast<NoMoreDay::render::debug::RenderPassId>(i);
          const auto &stats = passStats[i];
          const bool gpuValid =
              (stats.gpuState == NoMoreDay::render::debug::QueryState::Valid);
          const float overPct =
              (gpuValid && stats.budgetMs > 0.0f)
                  ? std::max(0.0f, (stats.gpuMeanMs - stats.budgetMs) /
                                       stats.budgetMs * 100.0f)
                  : 0.0f;
          LOG_INFO("RenderProfiler[{}]: CPU(mean={:.3f},p95={:.3f}) GPU(mean={:.3f},p95={:.3f},state={}) budget={:.3f} over={:.1f}%",
                   NoMoreDay::render::debug::RenderProfiler::ToString(passId),
                   stats.cpuMeanMs, stats.cpuP95Ms, stats.gpuMeanMs, stats.gpuP95Ms,
                   NoMoreDay::render::debug::ToQueryStateName(stats.gpuState),
                   stats.budgetMs, overPct);
        }
        s_lastProfilerLog = now;
      }

      NoMoreDay::render::debug::DrawProfilerHud(*graphContext.renderProfiler, 14.0f,
                                                 14.0f);
      const auto drsScale = g_adaptiveQualityController.GetCurrentScale();
      const auto drsSettings = g_adaptiveQualityController.GetSettings();
      const auto frameState =
          NoMoreDay::render::debug::GPUTimerQueryRing::Get().GetFrameResult();
      DrawText(TextFormat("DRS: scale=%.3f enabled=%d locked=%d autoExp=%d "
                          "GPUstate=%s p95=%.3f",
                          drsScale, drsSettings.dynamicResolutionEnabled ? 1 : 0,
                          drsSettings.renderScaleLocked ? 1 : 0,
                          drsSettings.autoExposureEnabled ? 1 : 0,
                          NoMoreDay::render::debug::ToQueryStateName(frameState.state),
                          NoMoreDay::render::debug::GPUTimerQueryRing::Get()
                              .GetValidFrameP95Ms()),
               390, static_cast<int>(14.0f) + 18, 14, Color{180, 220, 255, 255});
    }
  }

  g_transientPool.EndFrame();
  // B2 (P2 AD-8): frame-end counterpart of the BeginFrame wired at the top of
  // render(); evicts stale pooled entries (idle past m_frameRetention).
  NoMoreDay::render::resources::GPUTexturePool::Get().EndFrame();
}

// W6 (M0-C): hardware-gate evidence accessors (see RenderSystem.hpp).
const std::vector<std::string> &RenderSystem::GetLastExecutedPassOrder() {
  return s_lastExecutedPassOrder;
}

RenderSystem::JfaDiagnostics RenderSystem::GetJfaDiagnostics() {
  JfaDiagnostics diagnostics;
  if (g_jfaPass == nullptr) {
    return diagnostics;
  }

  const auto &report = g_jfaPass->GetLastReport();
  switch (report.mode) {
  case NoMoreDay::render::gi::JFAUpdateMode::Skip:
    diagnostics.mode = "skip";
    break;
  case NoMoreDay::render::gi::JFAUpdateMode::Incremental:
    diagnostics.mode = "incremental";
    break;
  case NoMoreDay::render::gi::JFAUpdateMode::Revert:
    diagnostics.mode = "revert";
    break;
  case NoMoreDay::render::gi::JFAUpdateMode::Full:
  default:
    diagnostics.mode = "full";
    break;
  }
  diagnostics.dispatchTexelCount = report.dispatchTexelCount;
  diagnostics.dirtyRectArea = report.dirtyRect.Area();
  diagnostics.expandedRectArea = report.expandedRect.Area();
  diagnostics.plus2Recovery = g_jfaPass->UsedFallbackPlus2ThisFrame();
  diagnostics.verificationAttempted = report.verificationAttempted;
  diagnostics.verificationPassed = report.verificationPassed;
  diagnostics.verificationRecovery = report.verificationFallback;
  diagnostics.verificationResult = report.verificationResult;
  return diagnostics;
}

RenderSystem::GiDistanceFieldInfo RenderSystem::GetGiDistanceField() {
  GiDistanceFieldInfo info;
  if (g_jfaPass != nullptr && g_jfaPass->HasDistanceField()) {
    info.texture = g_jfaPass->GetDistanceFieldTexture();
    info.width = g_jfaPass->GetDistanceFieldWidth();
    info.height = g_jfaPass->GetDistanceFieldHeight();
  }
  return info;
}

// M0-A R3: occupancy history evidence (GICompositePass R8 ping-pong). Returns
// the current read history texture + its extent plus the temporal-rejection
// instrumentation. The hardware gate reads this to prove the REAL occupancy
// history exists and was reset at least once; the diagnostic path (no
// RenderSystem::Initialize) leaves the info defaulted and the gate fails
// closed on the missing texture.
RenderSystem::GiOccupancyInfo RenderSystem::GetGiOccupancy() {
  GiOccupancyInfo info;
  if (g_giCompositePass != nullptr && g_giCompositePass->HasOccupancyHistory()) {
    info.historyValid = true;
    info.texture = g_giCompositePass->GetOccupancyHistoryTexture();
    info.width = g_giCompositePass->GetOccupancyHistoryWidth();
    info.height = g_giCompositePass->GetOccupancyHistoryHeight();
    info.historyResetCount = g_giCompositePass->GetHistoryResetCount();
    info.lastResetReason = g_giCompositePass->GetLastResetReason();
  }
  return info;
}
