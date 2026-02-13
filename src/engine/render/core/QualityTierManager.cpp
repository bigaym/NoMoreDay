#include "engine/render/core/QualityTierManager.hpp"

#include "GLFW/glfw3.h"
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace NoMoreDay::render::core {
namespace {

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool Contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
}

bool ParseTierString(std::string value, QualityTier &outTier) {
  value = ToLower(std::move(value));
  if (value == "low") {
    outTier = QualityTier::Low;
    return true;
  }
  if (value == "medium") {
    outTier = QualityTier::Medium;
    return true;
  }
  if (value == "high") {
    outTier = QualityTier::High;
    return true;
  }
  if (value == "ultra") {
    outTier = QualityTier::Ultra;
    return true;
  }
  return false;
}

} // namespace

QualityTierManager &QualityTierManager::Get() {
  static QualityTierManager manager;
  return manager;
}

void QualityTierManager::Initialize(const std::string &settingsPath,
                                    bool forceRedetect) {
  if (m_initialized && !forceRedetect) {
    return;
  }

  m_rendererString = QueryRendererString();
  m_fromSettings = false;

  QualityTier chosenTier = DetectTierFromRenderer(m_rendererString);
  QualityTier overrideTier = QualityTier::Medium;
  if (TryLoadTierFromSettings(settingsPath, overrideTier)) {
    chosenTier = overrideTier;
    m_fromSettings = true;
  }

  m_tier = chosenTier;
  UpdateConfigForTier(chosenTier);
  m_initialized = true;

  LOG_INFO("QualityTierManager: Tier={} (source={}, renderer='{}')",
           ToString(m_tier), m_fromSettings ? "settings.json" : "GPU detection",
           m_rendererString);
}

void QualityTierManager::ForceTier(QualityTier tier) {
  const QualityTier previous = m_tier;
  m_tier = tier;
  m_fromSettings = false;
  m_initialized = true;
  UpdateConfigForTier(tier);
  LOG_INFO("QualityTierManager: ForceTier {} -> {}", ToString(previous),
           ToString(m_tier));
}

bool QualityTierManager::TryLoadTierFromSettings(
    const std::string &settingsPath, QualityTier &outTier) const {
  if (!std::filesystem::exists(settingsPath)) {
    return false;
  }

  nlohmann::json jsonSettings;
  try {
    std::ifstream file(settingsPath);
    if (!file.is_open()) {
      return false;
    }
    file >> jsonSettings;
  } catch (...) {
    return false;
  }

  static constexpr const char *kTierKeys[] = {
      "renderQualityTier", "renderQuality", "qualityTier", "quality"};

  for (const char *key : kTierKeys) {
    if (!jsonSettings.contains(key)) {
      continue;
    }

    const auto &value = jsonSettings[key];
    if (value.is_string()) {
      if (ParseTierString(value.get<std::string>(), outTier)) {
        return true;
      }
    } else if (value.is_number_integer()) {
      const int tierIndex = value.get<int>();
      if (tierIndex >= static_cast<int>(QualityTier::Low) &&
          tierIndex <= static_cast<int>(QualityTier::Ultra)) {
        outTier = static_cast<QualityTier>(tierIndex);
        return true;
      }
    }
  }

  return false;
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

  switch (tier) {
  case QualityTier::Low:
    m_config.bloomEnabled = false;
    m_config.dynamicLightingEnabled = false;
    m_config.maxParticles = 30000;
    m_config.maxLights = 0;
    m_config.ambientIntensity = 0.5f;
    m_config.ambientColorR = 0.15f;
    m_config.ambientColorG = 0.15f;
    m_config.ambientColorB = 0.2f;
    m_config.shadowResolution = 0;
    m_config.bloomMipLevels = 0;
    m_config.bloomThreshold = 1.0f;
    m_config.bloomIntensity = 0.0f;
    m_config.bloomKnee = 0.1f;
    m_config.fxaaEnabled = false;
    m_config.vignetteEnabled = false;
    m_config.vignetteIntensity = 0.0f;
    m_config.vignetteRadius = 0.75f;
    m_config.particleTexturesEnabled = false;
    m_config.subEmitterEnabled = false;
    m_config.forceFieldEnabled = false;
    m_config.maxForceFields = 0;
    m_config.trailEnabled = false;
    m_config.trailMaxPoints = 0;
    m_config.maxTrails = 0;
    m_config.distortionEnabled = false;
    m_config.maxMaterials = 32;
    m_config.materialSystemEnabled = true;
    m_config.vfxSequenceDetail = 0;
    m_config.hotReloadEnabled = kHotReloadEnabled;
    m_config.colorGradingEnabled = false;
    m_config.colorGradingLutSize = 0;
    m_config.colorGradingIntensity = 1.0f;
    m_config.volumetricLightEnabled = false;
    m_config.volumetricSampleCount = 0;
    m_config.volumetricScattering = 0.0f;
    m_config.volumetricDecay = 0.0f;
    m_config.profilerHudEnabled = false;
    m_config.shaderHotReloadEnabled = false;
    break;
  case QualityTier::Medium:
    m_config.bloomEnabled = true;
    m_config.dynamicLightingEnabled = true;
    m_config.maxParticles = 60000;
    m_config.maxLights = 32;
    m_config.ambientIntensity = 0.3f;
    m_config.ambientColorR = 0.15f;
    m_config.ambientColorG = 0.15f;
    m_config.ambientColorB = 0.2f;
    m_config.shadowResolution = 512;
    m_config.bloomMipLevels = 3;
    m_config.bloomThreshold = 1.2f;
    m_config.bloomIntensity = 0.6f;
    m_config.bloomKnee = 0.1f;
    m_config.fxaaEnabled = true;
    m_config.vignetteEnabled = true;
    m_config.vignetteIntensity = 0.2f;
    m_config.vignetteRadius = 0.75f;
    m_config.particleTexturesEnabled = true;
    m_config.subEmitterEnabled = false;
    m_config.forceFieldEnabled = false;
    m_config.maxForceFields = 0;
    m_config.trailEnabled = true;
    m_config.trailMaxPoints = 32;
    m_config.maxTrails = 128;
    m_config.distortionEnabled = false;
    m_config.maxMaterials = 64;
    m_config.materialSystemEnabled = true;
    m_config.vfxSequenceDetail = 1;
    m_config.hotReloadEnabled = kHotReloadEnabled;
    m_config.colorGradingEnabled = false;
    m_config.colorGradingLutSize = 0;
    m_config.colorGradingIntensity = 1.0f;
    m_config.volumetricLightEnabled = false;
    m_config.volumetricSampleCount = 0;
    m_config.volumetricScattering = 0.0f;
    m_config.volumetricDecay = 0.0f;
    m_config.profilerHudEnabled = false;
    m_config.shaderHotReloadEnabled = false;
    break;
  case QualityTier::High:
    m_config.bloomEnabled = true;
    m_config.dynamicLightingEnabled = true;
    m_config.maxParticles = 120000;
    m_config.maxLights = 128;
    m_config.ambientIntensity = 0.25f;
    m_config.ambientColorR = 0.15f;
    m_config.ambientColorG = 0.15f;
    m_config.ambientColorB = 0.2f;
    m_config.shadowResolution = 1024;
    m_config.bloomMipLevels = 5;
    m_config.bloomThreshold = 1.0f;
    m_config.bloomIntensity = 0.8f;
    m_config.bloomKnee = 0.1f;
    m_config.fxaaEnabled = true;
    m_config.vignetteEnabled = true;
    m_config.vignetteIntensity = 0.3f;
    m_config.vignetteRadius = 0.75f;
    m_config.particleTexturesEnabled = true;
    m_config.subEmitterEnabled = true;
    m_config.forceFieldEnabled = true;
    m_config.maxForceFields = 8;
    m_config.trailEnabled = true;
    m_config.trailMaxPoints = 48;
    m_config.maxTrails = 256;
    m_config.distortionEnabled = true;
    m_config.maxMaterials = 128;
    m_config.materialSystemEnabled = true;
    m_config.vfxSequenceDetail = 2;
    m_config.hotReloadEnabled = kHotReloadEnabled;
    m_config.colorGradingEnabled = true;
    m_config.colorGradingLutSize = 16;
    m_config.colorGradingIntensity = 1.0f;
    m_config.volumetricLightEnabled = false;
    m_config.volumetricSampleCount = 0;
    m_config.volumetricScattering = 0.0f;
    m_config.volumetricDecay = 0.0f;
    m_config.profilerHudEnabled = kHotReloadEnabled;
    m_config.shaderHotReloadEnabled = kHotReloadEnabled;
    break;
  case QualityTier::Ultra:
    m_config.bloomEnabled = true;
    m_config.dynamicLightingEnabled = true;
    m_config.maxParticles = 200000;
    m_config.maxLights = 256;
    m_config.ambientIntensity = 0.2f;
    m_config.ambientColorR = 0.15f;
    m_config.ambientColorG = 0.15f;
    m_config.ambientColorB = 0.2f;
    m_config.shadowResolution = 2048;
    m_config.bloomMipLevels = 7;
    m_config.bloomThreshold = 0.8f;
    m_config.bloomIntensity = 1.0f;
    m_config.bloomKnee = 0.1f;
    m_config.fxaaEnabled = true;
    m_config.vignetteEnabled = true;
    m_config.vignetteIntensity = 0.35f;
    m_config.vignetteRadius = 0.75f;
    m_config.particleTexturesEnabled = true;
    m_config.subEmitterEnabled = true;
    m_config.forceFieldEnabled = true;
    m_config.maxForceFields = 16;
    m_config.trailEnabled = true;
    m_config.trailMaxPoints = 64;
    m_config.maxTrails = 512;
    m_config.distortionEnabled = true;
    m_config.maxMaterials = 256;
    m_config.materialSystemEnabled = true;
    m_config.vfxSequenceDetail = 2;
    m_config.hotReloadEnabled = kHotReloadEnabled;
    m_config.colorGradingEnabled = true;
    m_config.colorGradingLutSize = 32;
    m_config.colorGradingIntensity = 1.0f;
    m_config.volumetricLightEnabled = true;
    m_config.volumetricSampleCount = 48;
    m_config.volumetricScattering = 0.16f;
    m_config.volumetricDecay = 0.95f;
    m_config.profilerHudEnabled = kHotReloadEnabled;
    m_config.shaderHotReloadEnabled = kHotReloadEnabled;
    break;
  }
}

std::string QualityTierManager::QueryRendererString() const {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_43)
  const unsigned char *renderer = glGetString(GL_RENDERER);
  if (renderer != nullptr) {
    return reinterpret_cast<const char *>(renderer);
  }
#endif
  return "Unknown";
}

} // namespace NoMoreDay::render::core
