#pragma once

#include <cstdint>

namespace NoMoreDay::render::core {

enum class QualityTier : uint8_t {
  Low = 0,
  Medium = 1,
  High = 2,
  Ultra = 3,
};

enum class ShadowMode : uint8_t {
  Off = 0,
  SDF = 1,
  Hybrid = 2,
};

// V3 pass budget contract (ms), aligned with GPU_Rendering_System_3.md §15.2.
inline constexpr float kBudgetLightCulling_Normal = 0.15f;
inline constexpr float kBudgetLightCulling_High = 0.30f;
inline constexpr float kBudgetLightCulling_Extreme = 0.60f;

inline constexpr float kBudgetShadow_Normal = 0.40f;
inline constexpr float kBudgetShadow_High = 0.90f;
inline constexpr float kBudgetShadow_Extreme = 1.30f;

inline constexpr float kBudgetLighting_Normal = 0.60f;
inline constexpr float kBudgetLighting_High = 1.00f;
inline constexpr float kBudgetLighting_Extreme = 1.40f;

inline constexpr float kBudgetHeightShadow_Normal = 0.30f;
inline constexpr float kBudgetHeightShadow_High = 0.60f;
inline constexpr float kBudgetHeightShadow_Extreme = 0.90f;

inline constexpr uint32_t kDefaultClusterTileSize = 32u;
inline constexpr uint32_t kDefaultClusterZSliceCount = 8u;
inline constexpr uint32_t kMaxLightsPerCluster = 64u;
inline constexpr uint32_t kMaxTotalClusteredLights = 4096u;

struct AdaptiveQualitySettings {
  bool dynamicResolutionEnabled = false;
  bool renderScaleLocked = true;
  float renderScale = 1.0f;
  float minRenderScale = 0.70f;
  float maxRenderScale = 1.0f;
  float renderScaleStep = 0.05f;
  float downThresholdMs = 0.0f; // 0 uses the selected tier budget.
  float upThresholdMs = 0.0f;   // 0 uses the selected tier recovery budget.
  float sustainSeconds = 0.75f;
  float cooldownSeconds = 30.0f;

  // Auto exposure remains opt-in until the HDR histogram path has hardware evidence.
  bool autoExposureEnabled = false;
  float exposure = 1.0f;
  float minExposure = 0.25f;
  float maxExposure = 4.0f;
  float brightenRate = 1.5f;
  float darkenRate = 3.0f;
};

struct RenderConfig {
  bool bloomEnabled = false;
  bool dynamicLightingEnabled = false;
  int maxParticles = 20000;
  int shadowResolution = 0;
  int maxLights = 0;
  float ambientIntensity = 0.3f;
  float ambientColorR = 0.15f;
  float ambientColorG = 0.15f;
  float ambientColorB = 0.2f;

  int bloomMipLevels = 0;
  float bloomThreshold = 1.0f;
  float bloomIntensity = 0.8f;
  float bloomKnee = 0.1f;

  bool fxaaEnabled = false;
  bool vignetteEnabled = false;
  float vignetteIntensity = 0.3f;
  float vignetteRadius = 0.75f;

  bool particleTexturesEnabled = false;
  bool subEmitterEnabled = false;
  bool forceFieldEnabled = false;
  int maxForceFields = 0;

  bool trailEnabled = false;
  int trailMaxPoints = 0;
  int maxTrails = 0;

  // Phase 4
  bool distortionEnabled = false;
  int maxMaterials = 0;
  bool materialSystemEnabled = false;
  int vfxSequenceDetail = 0; // 0=minimal, 1=reduced, 2=full
  bool hotReloadEnabled = false;

  // Phase 5 - Color Grading
  bool colorGradingEnabled = false;
  int colorGradingLutSize = 0; // 0=off, 16 or 32
  float colorGradingIntensity = 1.0f;

  // Phase 5 - Volumetric Light
  bool volumetricLightEnabled = false;
  int volumetricSampleCount = 0; // 0=off, Ultra defaults 48
  float volumetricScattering = 0.0f;
  float volumetricDecay = 0.0f;

  // Phase 5 - Debug/Dev
  bool profilerHudEnabled = false;
  bool shaderHotReloadEnabled = false;

  // V3 Baseline Contracts (Step A)
  bool shadowEnabled = false;
  ShadowMode shadowMode = ShadowMode::Off;
  uint32_t maxShadowedLights = 4;
  uint32_t shadowAtlasSize = 2048;
  float shadowSoftness = 1.0f;
  bool clusteredLightingEnabled = false;
  uint32_t clusterTileSize = kDefaultClusterTileSize;
  uint32_t clusterZSliceCount = kDefaultClusterZSliceCount;
  bool normalLightingEnabled = false;
  bool specularEnabled = false;
  uint32_t materialQualityLevel = 0;
  bool v3Enabled = false;

  // V4 advanced lighting controls.
  bool clusteredLightingV4Enabled = false;
  bool heightShadowEnabled = false;
  uint32_t heightShadowSteps = 0;
  bool selfShadowEnabled = false;
  uint32_t selfShadowSteps = 0;
  bool pomEnabled = false;
  uint32_t pomLayers = 0;

  // V4 GPU text feature routing.
  bool gpuTextEnabled = false;
  bool gpuTextAdvancedAnimation = false;

  // V4 GPU loot feature routing.
  bool gpuLootEnabled = false;
  bool gpuLootGlowEnabled = false;

  // V5 GI feature routing.
  bool giEnabled = false;
  uint32_t giCascadeLevels = 0;
  bool giHalfResolution = false;
  float giTemporalWeight = 0.9f;
  uint32_t giSdfUpdateInterval = 1;
  float giIntensity = 1.0f;
  bool giHolographicEnabled = false;

  // V5 fluid feature routing.
  bool fluidEnabled = false;
  uint32_t fluidMaxParticles = 0;

  AdaptiveQualitySettings adaptiveQuality = {};
};

inline const char *ToString(QualityTier tier) {
  switch (tier) {
  case QualityTier::Low:
    return "Low";
  case QualityTier::Medium:
    return "Medium";
  case QualityTier::High:
    return "High";
  case QualityTier::Ultra:
    return "Ultra";
  default:
    return "Unknown";
  }
}

} // namespace NoMoreDay::render::core
