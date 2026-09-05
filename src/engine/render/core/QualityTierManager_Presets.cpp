#include "engine/render/core/QualityTierManagerInternal.hpp"

namespace NoMoreDay::render::core {
using namespace NoMoreDay::render::core::detail;

QualityTier QualityTierManager::DetectTierFromCapabilities(
    const CapabilitySnapshot &snapshot) const {
  const int wgX = snapshot.maxComputeWorkGroupSize[0];
  if (snapshot.maxShaderStorageBufferBindings < 8 ||
      snapshot.maxComputeWorkGroupInvocations < 128 || wgX < 128 ||
      snapshot.maxTextureSize < 2048 || snapshot.maxArrayTextureLayers < 128 ||
      snapshot.maxImageUnits < 4) {
    return QualityTier::Low;
  }
  if (snapshot.maxShaderStorageBufferBindings < 16 ||
      snapshot.maxComputeWorkGroupInvocations < 256 || wgX < 256 ||
      snapshot.maxTextureSize < 4096 || snapshot.maxArrayTextureLayers < 256 ||
      snapshot.maxImageUnits < 6) {
    return QualityTier::Medium;
  }
  if (snapshot.maxShaderStorageBufferBindings < 24 ||
      snapshot.maxComputeWorkGroupInvocations < 512 || wgX < 512 ||
      snapshot.maxTextureSize < 8192 || snapshot.maxArrayTextureLayers < 512 ||
      snapshot.maxImageUnits < 8) {
    return QualityTier::High;
  }
  return QualityTier::Ultra;
}

QualityTier QualityTierManager::RunBaselineBenchmark(
    const CapabilitySnapshot &snapshot, float &outScore) const {
  const float ssboScore = std::min(snapshot.maxShaderStorageBufferBindings / 32.0f, 1.0f) * 40.0f;
  const float invocScore =
      std::min(snapshot.maxComputeWorkGroupInvocations / 1024.0f, 1.0f) * 30.0f;
  const float wgScore =
      std::min(snapshot.maxComputeWorkGroupSize[0] / 1024.0f, 1.0f) * 15.0f;
  const float texScore = std::min(snapshot.maxTextureSize / 16384.0f, 1.0f) * 15.0f;
  const float layerScore =
      std::min(snapshot.maxArrayTextureLayers / 1024.0f, 1.0f) * 10.0f;
  const float imageScore = std::min(snapshot.maxImageUnits / 16.0f, 1.0f) * 10.0f;

  const auto benchStart = std::chrono::high_resolution_clock::now();
  volatile float sink = 0.0f;
  for (int i = 0; i < 50000; ++i) {
    sink += std::sin(static_cast<float>(i) * 0.0019f);
  }
  const auto benchEnd = std::chrono::high_resolution_clock::now();
  const float benchMs =
      std::chrono::duration<float, std::milli>(benchEnd - benchStart).count();
  (void)sink;
  const float benchScore =
      std::clamp(2.5f - benchMs, 0.0f, 2.5f) * 4.0f; // 0..10

  outScore =
      ssboScore + invocScore + wgScore + texScore + layerScore + imageScore + benchScore;
  if (outScore >= 100.0f) {
    return QualityTier::Ultra;
  }
  if (outScore >= 72.0f) {
    return QualityTier::High;
  }
  if (outScore >= 45.0f) {
    return QualityTier::Medium;
  }
  return QualityTier::Low;
}

QualityTier QualityTierManager::DetectTierFromRenderer(
    std::string_view renderer) const {
  const std::string lowered = ToLower(std::string(renderer));
  const std::string_view rendererView = lowered;

  if (Contains(rendererView, "llvmpipe") ||
      Contains(rendererView, "swiftshader") ||
      Contains(rendererView, "software")) {
    return QualityTier::Low;
  }

  if (Contains(rendererView, "rtx") || Contains(rendererView, "rx 79") ||
      Contains(rendererView, "rx 78") || Contains(rendererView, "rx 69")) {
    return QualityTier::Ultra;
  }

  if (Contains(rendererView, "radeon") || Contains(rendererView, "gtx") ||
      Contains(rendererView, "arc") || Contains(rendererView, "nvidia")) {
    return QualityTier::High;
  }

  if (Contains(rendererView, "iris") || Contains(rendererView, "uhd") ||
      Contains(rendererView, "vega") || Contains(rendererView, "intel")) {
    return QualityTier::Medium;
  }

  return QualityTier::Medium;
}

void QualityTierManager::UpdateConfigForTier(QualityTier tier) {
#if defined(NDEBUG)
  constexpr bool kHotReloadEnabled = false;
#else
  constexpr bool kHotReloadEnabled = true;
#endif

  m_baseConfig = {};
  switch (tier) {
  case QualityTier::Low:
    m_baseConfig.bloomEnabled = false;
    m_baseConfig.dynamicLightingEnabled = false;
    m_baseConfig.maxParticles = 30000;
    m_baseConfig.maxLights = 0;
    m_baseConfig.ambientIntensity = 0.5f;
    m_baseConfig.ambientColorR = 0.15f;
    m_baseConfig.ambientColorG = 0.15f;
    m_baseConfig.ambientColorB = 0.2f;
    m_baseConfig.shadowResolution = 0;
    m_baseConfig.bloomMipLevels = 0;
    m_baseConfig.bloomThreshold = 1.0f;
    m_baseConfig.bloomIntensity = 0.0f;
    m_baseConfig.bloomKnee = 0.1f;
    m_baseConfig.fxaaEnabled = false;
    m_baseConfig.vignetteEnabled = false;
    m_baseConfig.vignetteIntensity = 0.0f;
    m_baseConfig.vignetteRadius = 0.75f;
    m_baseConfig.particleTexturesEnabled = false;
    m_baseConfig.subEmitterEnabled = false;
    m_baseConfig.forceFieldEnabled = false;
    m_baseConfig.maxForceFields = 0;
    m_baseConfig.trailEnabled = false;
    m_baseConfig.trailMaxPoints = 0;
    m_baseConfig.maxTrails = 0;
    m_baseConfig.distortionEnabled = false;
    m_baseConfig.maxMaterials = 32;
    m_baseConfig.materialSystemEnabled = true;
    m_baseConfig.vfxSequenceDetail = 0;
    m_baseConfig.hotReloadEnabled = kHotReloadEnabled;
    m_baseConfig.colorGradingEnabled = false;
    m_baseConfig.colorGradingLutSize = 0;
    m_baseConfig.colorGradingIntensity = 1.0f;
    m_baseConfig.volumetricLightEnabled = false;
    m_baseConfig.volumetricSampleCount = 0;
    m_baseConfig.volumetricScattering = 0.0f;
    m_baseConfig.volumetricDecay = 0.0f;
    m_baseConfig.profilerHudEnabled = false;
    m_baseConfig.shaderHotReloadEnabled = false;
    m_baseConfig.gpuTextEnabled = false;
    m_baseConfig.gpuTextAdvancedAnimation = false;
    m_baseConfig.gpuLootEnabled = false;
    m_baseConfig.gpuLootGlowEnabled = false;
    m_baseConfig.giEnabled = false;
    m_baseConfig.giCascadeLevels = 0;
    m_baseConfig.giHalfResolution = false;
    m_baseConfig.giTemporalWeight = 0.0f;
    m_baseConfig.giSdfUpdateInterval = 0;
    m_baseConfig.giIntensity = 0.0f;
    m_baseConfig.giHolographicEnabled = false;
    m_baseConfig.fluidEnabled = false;
    m_baseConfig.fluidMaxParticles = 0;
    m_baseConfig.clusteredLightingEnabled = false;
    m_baseConfig.clusteredLightingV4Enabled = false;
    m_baseConfig.heightShadowEnabled = false;
    m_baseConfig.heightShadowSteps = 0;
    m_baseConfig.selfShadowEnabled = false;
    m_baseConfig.selfShadowSteps = 0;
    m_baseConfig.pomEnabled = false;
    m_baseConfig.pomLayers = 0;
    break;
  case QualityTier::Medium:
    m_baseConfig.bloomEnabled = true;
    m_baseConfig.dynamicLightingEnabled = true;
    m_baseConfig.maxParticles = 60000;
    m_baseConfig.maxLights = 256;
    m_baseConfig.ambientIntensity = 0.32f;
    m_baseConfig.ambientColorR = 0.15f;
    m_baseConfig.ambientColorG = 0.15f;
    m_baseConfig.ambientColorB = 0.2f;
    m_baseConfig.shadowResolution = 512;
    m_baseConfig.bloomMipLevels = 3;
    m_baseConfig.bloomThreshold = 1.2f;
    m_baseConfig.bloomIntensity = 0.6f;
    m_baseConfig.bloomKnee = 0.1f;
    m_baseConfig.fxaaEnabled = true;
    m_baseConfig.vignetteEnabled = true;
    m_baseConfig.vignetteIntensity = 0.01f;
    m_baseConfig.vignetteRadius = 0.75f;
    m_baseConfig.particleTexturesEnabled = true;
    m_baseConfig.subEmitterEnabled = false;
    m_baseConfig.forceFieldEnabled = false;
    m_baseConfig.maxForceFields = 0;
    m_baseConfig.trailEnabled = true;
    m_baseConfig.trailMaxPoints = 32;
    m_baseConfig.maxTrails = 128;
    m_baseConfig.distortionEnabled = false;
    m_baseConfig.maxMaterials = 64;
    m_baseConfig.materialSystemEnabled = true;
    m_baseConfig.vfxSequenceDetail = 1;
    m_baseConfig.hotReloadEnabled = kHotReloadEnabled;
    m_baseConfig.colorGradingEnabled = false;
    m_baseConfig.colorGradingLutSize = 0;
    m_baseConfig.colorGradingIntensity = 0.03f;
    m_baseConfig.volumetricLightEnabled = false;
    m_baseConfig.volumetricSampleCount = 0;
    m_baseConfig.volumetricScattering = 0.0f;
    m_baseConfig.volumetricDecay = 0.0f;
    m_baseConfig.profilerHudEnabled = false;
    m_baseConfig.shaderHotReloadEnabled = false;
    m_baseConfig.gpuTextEnabled = true;
    m_baseConfig.gpuTextAdvancedAnimation = false;
    m_baseConfig.gpuLootEnabled = false;
    m_baseConfig.gpuLootGlowEnabled = false;
    m_baseConfig.giEnabled = false;
    m_baseConfig.giCascadeLevels = 0;
    m_baseConfig.giHalfResolution = false;
    m_baseConfig.giTemporalWeight = 0.0f;
    m_baseConfig.giSdfUpdateInterval = 0;
    m_baseConfig.giIntensity = 0.0f;
    m_baseConfig.giHolographicEnabled = false;
    m_baseConfig.fluidEnabled = false;
    m_baseConfig.fluidMaxParticles = 0;
    m_baseConfig.clusteredLightingEnabled = true;
    m_baseConfig.clusteredLightingV4Enabled = false;
    m_baseConfig.heightShadowEnabled = false;
    m_baseConfig.heightShadowSteps = 0;
    m_baseConfig.selfShadowEnabled = false;
    m_baseConfig.selfShadowSteps = 0;
    m_baseConfig.pomEnabled = false;
    m_baseConfig.pomLayers = 0;
    break;
  case QualityTier::High:
    m_baseConfig.bloomEnabled = true;
    m_baseConfig.dynamicLightingEnabled = true;
    m_baseConfig.maxParticles = 120000;
    m_baseConfig.maxLights = 1024;
    m_baseConfig.ambientIntensity = 0.36f;
    m_baseConfig.ambientColorR = 0.15f;
    m_baseConfig.ambientColorG = 0.15f;
    m_baseConfig.ambientColorB = 0.2f;
    m_baseConfig.shadowResolution = 1024;
    m_baseConfig.bloomMipLevels = 5;
    m_baseConfig.bloomThreshold = 1.0f;
    m_baseConfig.bloomIntensity = 0.8f;
    m_baseConfig.bloomKnee = 0.1f;
    m_baseConfig.fxaaEnabled = true;
    m_baseConfig.vignetteEnabled = true;
    m_baseConfig.vignetteIntensity = 0.02f;
    m_baseConfig.vignetteRadius = 0.75f;
    m_baseConfig.particleTexturesEnabled = true;
    m_baseConfig.subEmitterEnabled = true;
    m_baseConfig.forceFieldEnabled = true;
    m_baseConfig.maxForceFields = 8;
    m_baseConfig.trailEnabled = true;
    m_baseConfig.trailMaxPoints = 48;
    m_baseConfig.maxTrails = 256;
    m_baseConfig.distortionEnabled = true;
    m_baseConfig.maxMaterials = 128;
    m_baseConfig.materialSystemEnabled = true;
    m_baseConfig.vfxSequenceDetail = 2;
    m_baseConfig.hotReloadEnabled = kHotReloadEnabled;
    m_baseConfig.colorGradingEnabled = true;
    m_baseConfig.colorGradingLutSize = 16;
    m_baseConfig.colorGradingIntensity = 0.06f;
    m_baseConfig.volumetricLightEnabled = false;
    m_baseConfig.volumetricSampleCount = 0;
    m_baseConfig.volumetricScattering = 0.0f;
    m_baseConfig.volumetricDecay = 0.0f;
    m_baseConfig.profilerHudEnabled = false;
    m_baseConfig.shaderHotReloadEnabled = kHotReloadEnabled;
    m_baseConfig.gpuTextEnabled = true;
    m_baseConfig.gpuTextAdvancedAnimation = true;
    m_baseConfig.gpuLootEnabled = true;
    m_baseConfig.gpuLootGlowEnabled = false;
    m_baseConfig.giEnabled = true;
    m_baseConfig.giCascadeLevels = 4;
    m_baseConfig.giHalfResolution = true;
    m_baseConfig.giTemporalWeight = 0.92f;
    m_baseConfig.giSdfUpdateInterval = 2;
    m_baseConfig.giIntensity = 1.0f;
    m_baseConfig.giHolographicEnabled = false;
    m_baseConfig.fluidEnabled = false;
    m_baseConfig.fluidMaxParticles = 0;
    m_baseConfig.clusteredLightingEnabled = true;
    m_baseConfig.clusteredLightingV4Enabled = true;
    m_baseConfig.heightShadowEnabled = true;
    m_baseConfig.heightShadowSteps = 16;
    m_baseConfig.selfShadowEnabled = true;
    m_baseConfig.selfShadowSteps = 4;
    m_baseConfig.pomEnabled = false;
    m_baseConfig.pomLayers = 0;
    break;
  case QualityTier::Ultra:
    m_baseConfig.bloomEnabled = true;
    m_baseConfig.dynamicLightingEnabled = true;
    m_baseConfig.maxParticles = 200000;
    m_baseConfig.maxLights = 4096;
    m_baseConfig.ambientIntensity = 0.4f;
    m_baseConfig.ambientColorR = 0.15f;
    m_baseConfig.ambientColorG = 0.15f;
    m_baseConfig.ambientColorB = 0.2f;
    m_baseConfig.shadowResolution = 2048;
    m_baseConfig.bloomMipLevels = 7;
    m_baseConfig.bloomThreshold = 0.8f;
    m_baseConfig.bloomIntensity = 1.0f;
    m_baseConfig.bloomKnee = 0.1f;
    m_baseConfig.fxaaEnabled = true;
    m_baseConfig.vignetteEnabled = true;
    m_baseConfig.vignetteIntensity = 0.03f;
    m_baseConfig.vignetteRadius = 0.75f;
    m_baseConfig.particleTexturesEnabled = true;
    m_baseConfig.subEmitterEnabled = true;
    m_baseConfig.forceFieldEnabled = true;
    m_baseConfig.maxForceFields = 16;
    m_baseConfig.trailEnabled = true;
    m_baseConfig.trailMaxPoints = 64;
    m_baseConfig.maxTrails = 512;
    m_baseConfig.distortionEnabled = true;
    m_baseConfig.maxMaterials = 256;
    m_baseConfig.materialSystemEnabled = true;
    m_baseConfig.vfxSequenceDetail = 2;
    m_baseConfig.hotReloadEnabled = kHotReloadEnabled;
    m_baseConfig.colorGradingEnabled = true;
    m_baseConfig.colorGradingLutSize = 32;
    m_baseConfig.colorGradingIntensity = 0.09f;
    m_baseConfig.volumetricLightEnabled = true;
    m_baseConfig.volumetricSampleCount = 48;
    m_baseConfig.volumetricScattering = 0.16f;
    m_baseConfig.volumetricDecay = 0.95f;
    m_baseConfig.profilerHudEnabled = false;
    m_baseConfig.shaderHotReloadEnabled = kHotReloadEnabled;
    m_baseConfig.gpuTextEnabled = true;
    m_baseConfig.gpuTextAdvancedAnimation = true;
    m_baseConfig.gpuLootEnabled = true;
    m_baseConfig.gpuLootGlowEnabled = true;
    m_baseConfig.giEnabled = true;
    m_baseConfig.giCascadeLevels = 6;
    m_baseConfig.giHalfResolution = false;
    m_baseConfig.giTemporalWeight = 0.88f;
    m_baseConfig.giSdfUpdateInterval = 1;
    m_baseConfig.giIntensity = 1.0f;
    m_baseConfig.giHolographicEnabled = false;
    m_baseConfig.fluidEnabled = false;
    m_baseConfig.fluidMaxParticles = 0;
    m_baseConfig.clusteredLightingEnabled = true;
    m_baseConfig.clusteredLightingV4Enabled = true;
    m_baseConfig.heightShadowEnabled = true;
    m_baseConfig.heightShadowSteps = 64;
    m_baseConfig.selfShadowEnabled = true;
    m_baseConfig.selfShadowSteps = 8;
    m_baseConfig.pomEnabled = true;
    m_baseConfig.pomLayers = 16;
    break;
  }

  if (m_gpuTextEnabledOverride.has_value() && !m_gpuTextEnabledOverride.value()) {
    m_baseConfig.gpuTextEnabled = false;
    m_baseConfig.gpuTextAdvancedAnimation = false;
  }
  if (m_gpuLootEnabledOverride.has_value()) {
    m_baseConfig.gpuLootEnabled = m_gpuLootEnabledOverride.value();
    if (!m_baseConfig.gpuLootEnabled || tier != QualityTier::Ultra) {
      m_baseConfig.gpuLootGlowEnabled = false;
    }
  }
  if (m_giEnabledOverride.has_value()) {
    m_baseConfig.giEnabled = m_giEnabledOverride.value();
    if (!m_baseConfig.giEnabled) {
      m_baseConfig.giCascadeLevels = 0;
      m_baseConfig.giIntensity = 0.0f;
    } else {
      if (m_baseConfig.giCascadeLevels == 0) {
        m_baseConfig.giCascadeLevels =
            (tier == QualityTier::Ultra) ? 6u : 4u;
      }
      if (m_baseConfig.giIntensity <= 0.0f) {
        m_baseConfig.giIntensity = 1.0f;
      }
    }
  }
#if defined(NDEBUG)
  // Shipped Release builds strictly enforce fluidEnabled=false
  m_baseConfig.fluidEnabled = false;
  m_baseConfig.fluidMaxParticles = 0;
#else
  if (m_fluidEnabledOverride.has_value()) {
    m_baseConfig.fluidEnabled = m_fluidEnabledOverride.value();
    if (!m_baseConfig.fluidEnabled) {
      m_baseConfig.fluidMaxParticles = 0;
    } else if (m_baseConfig.fluidMaxParticles == 0) {
      m_baseConfig.fluidMaxParticles = (tier == QualityTier::Ultra) ? 10000u : 5000u;
    }
  }
#endif

  m_baseConfig.linearPipeline = true;
  if (m_linearPipelineEnabledOverride.has_value()) {
    m_baseConfig.linearPipeline = m_linearPipelineEnabledOverride.value();
  }

  ApplyV3ConfigOverrides(m_baseConfig);
  ApplyTierShadowPolicy(m_baseConfig, tier);
  ApplyAutoDegradeLevel();
}


} // namespace NoMoreDay::render::core
