#include "engine/render/core/QualityTierManager.hpp"

#include "GLFW/glfw3.h"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

namespace NoMoreDay::render::core {
namespace {

constexpr uint32_t kGLMaxShaderStorageBufferBindings = 0x90DD;
constexpr uint32_t kGLMaxComputeWorkGroupInvocations = 0x90EB;
constexpr uint32_t kGLMaxComputeWorkGroupSize = 0x91BF;
constexpr uint32_t kGLMaxTextureSize = 0x0D33;
constexpr uint32_t kGLMaxArrayTextureLayers = 0x88FF;
constexpr uint32_t kGLMaxImageUnits = 0x8F38;

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

QualityTier MinTier(QualityTier lhs, QualityTier rhs) {
  return (static_cast<int>(lhs) < static_cast<int>(rhs)) ? lhs : rhs;
}

const char *ToString(QualityTierManager::TierSelectionSource source) {
  switch (source) {
  case QualityTierManager::TierSelectionSource::CapabilityAndBenchmark:
    return "capability+benchmark";
  case QualityTierManager::TierSelectionSource::SettingsOverride:
    return "settings_override";
  }
  return "unknown";
}

std::string MakeUtcTimestamp() {
  using Clock = std::chrono::system_clock;
  const auto now = Clock::now();
  const std::time_t nowTime = Clock::to_time_t(now);
  std::tm tmUtc = {};
#if defined(_WIN32)
  gmtime_s(&tmUtc, &nowTime);
#else
  gmtime_r(&nowTime, &tmUtc);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tmUtc, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

} // namespace

QualityTierManager &QualityTierManager::Get() {
  static QualityTierManager manager;
  return manager;
}

QualityTierManager::AutoDegradeBudgetThresholds
QualityTierManager::GetAutoDegradeBudgetThresholds(QualityTier tier) {
  switch (tier) {
  case QualityTier::Low:
    return {.degradeTriggerMs = 7.2f, .recoverTriggerMs = 5.6f};
  case QualityTier::Medium:
    return {.degradeTriggerMs = 10.5f, .recoverTriggerMs = 8.2f};
  case QualityTier::High:
    return {.degradeTriggerMs = 13.5f, .recoverTriggerMs = 10.5f};
  case QualityTier::Ultra:
    return {.degradeTriggerMs = 16.0f, .recoverTriggerMs = 12.5f};
  }
  return {.degradeTriggerMs = 10.5f, .recoverTriggerMs = 8.2f};
}

void QualityTierManager::Initialize(const std::string &settingsPath,
                                    bool forceRedetect) {
  if (m_initialized && !forceRedetect) {
    return;
  }

  m_rendererString = QueryRendererString();
  m_fromSettings = false;
  m_autoDegradeLevel = 0;

  m_capabilitySnapshot = ProbeCapabilities();
  const QualityTier capabilityTier = DetectTierFromCapabilities(m_capabilitySnapshot);
  float benchmarkScore = 0.0f;
  const QualityTier benchmarkTier =
      RunBaselineBenchmark(m_capabilitySnapshot, benchmarkScore);

  QualityTier chosenTier = MinTier(capabilityTier, benchmarkTier);
  QualityTier overrideTier = QualityTier::Medium;
  TierSelectionSource source = TierSelectionSource::CapabilityAndBenchmark;
  std::string reasonCode = "capability_and_benchmark";

  if (TryLoadTierFromSettings(settingsPath, overrideTier)) {
    chosenTier = overrideTier;
    source = TierSelectionSource::SettingsOverride;
    reasonCode = "settings_override";
    m_fromSettings = true;
  }

  m_tier = chosenTier;
  m_selectionMetadata.version = kSelectionMetadataVersion;
  m_selectionMetadata.source = source;
  m_selectionMetadata.capabilityTier = capabilityTier;
  m_selectionMetadata.benchmarkTier = benchmarkTier;
  m_selectionMetadata.selectedTier = chosenTier;
  m_selectionMetadata.benchmarkScore = benchmarkScore;
  m_selectionMetadata.reasonCode = reasonCode;
  UpdateConfigForTier(chosenTier);
  m_initialized = true;

  PersistSelectionMetadata(settingsPath);

  LOG_INFO(
      "QualityTierManager: CapabilityProbe ssbo={}, invocations={}, wg=({},{},{}), "
      "maxTex={}, maxLayers={}, imageUnits={}, valid={}",
      m_capabilitySnapshot.maxShaderStorageBufferBindings,
      m_capabilitySnapshot.maxComputeWorkGroupInvocations,
      m_capabilitySnapshot.maxComputeWorkGroupSize[0],
      m_capabilitySnapshot.maxComputeWorkGroupSize[1],
      m_capabilitySnapshot.maxComputeWorkGroupSize[2],
      m_capabilitySnapshot.maxTextureSize, m_capabilitySnapshot.maxArrayTextureLayers,
      m_capabilitySnapshot.maxImageUnits, m_capabilitySnapshot.valid ? 1 : 0);
  LOG_INFO(
      "QualityTierManager: Tier={} (source={}, capabilityTier={}, benchmarkTier={}, "
      "benchmarkScore={:.2f}, degradeLevel={}, renderer='{}')",
      ToString(m_tier), ToString(m_selectionMetadata.source),
      ToString(m_selectionMetadata.capabilityTier),
      ToString(m_selectionMetadata.benchmarkTier), m_selectionMetadata.benchmarkScore,
      m_autoDegradeLevel, m_rendererString);
}

void QualityTierManager::ForceTier(QualityTier tier) {
  const QualityTier previous = m_tier;
  m_tier = tier;
  m_fromSettings = false;
  m_autoDegradeLevel = 0;
  m_initialized = true;

  m_selectionMetadata.version = kSelectionMetadataVersion;
  m_selectionMetadata.source = TierSelectionSource::SettingsOverride;
  m_selectionMetadata.capabilityTier = tier;
  m_selectionMetadata.benchmarkTier = tier;
  m_selectionMetadata.selectedTier = tier;
  m_selectionMetadata.benchmarkScore = 0.0f;
  m_selectionMetadata.reasonCode = "force_tier_runtime";

  UpdateConfigForTier(tier);
  LOG_INFO("QualityTierManager: ForceTier {} -> {} (degradeLevel={})",
           ToString(previous), ToString(m_tier), m_autoDegradeLevel);
}

bool QualityTierManager::IncreaseAutoDegradeLevel(std::string_view reasonCode,
                                                   float observedFrameMs,
                                                   float budgetMs) {
  if (!m_initialized || m_autoDegradeLevel >= kAutoDegradeMaxLevel) {
    return false;
  }

  const int previous = m_autoDegradeLevel;
  ++m_autoDegradeLevel;
  ApplyAutoDegradeLevel();
  LOG_WARN(
      "QualityTierManager: AutoDegrade level {} -> {} reason={} frameMs={:.3f} "
      "budgetMs={:.3f}",
      previous, m_autoDegradeLevel, reasonCode, observedFrameMs, budgetMs);
  return true;
}

bool QualityTierManager::DecreaseAutoDegradeLevel(std::string_view reasonCode,
                                                   float observedFrameMs,
                                                   float budgetMs) {
  if (!m_initialized || m_autoDegradeLevel <= 0) {
    return false;
  }

  const int previous = m_autoDegradeLevel;
  --m_autoDegradeLevel;
  ApplyAutoDegradeLevel();
  LOG_INFO(
      "QualityTierManager: AutoDegrade recovery {} -> {} reason={} frameMs={:.3f} "
      "budgetMs={:.3f}",
      previous, m_autoDegradeLevel, reasonCode, observedFrameMs, budgetMs);
  return true;
}

void QualityTierManager::ResetAutoDegrade(std::string_view reasonCode) {
  if (m_autoDegradeLevel == 0) {
    return;
  }
  const int previous = m_autoDegradeLevel;
  m_autoDegradeLevel = 0;
  ApplyAutoDegradeLevel();
  LOG_INFO("QualityTierManager: AutoDegrade reset {} -> 0 reason={}", previous,
           reasonCode);
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

void QualityTierManager::PersistSelectionMetadata(
    const std::string &settingsPath) const {
  if (settingsPath.empty()) {
    return;
  }

  nlohmann::json jsonSettings = nlohmann::json::object();
  if (std::filesystem::exists(settingsPath)) {
    try {
      std::ifstream file(settingsPath);
      if (file.is_open()) {
        file >> jsonSettings;
      }
    } catch (...) {
      LOG_WARN("QualityTierManager: failed to parse {}, metadata overwrite", settingsPath);
      jsonSettings = nlohmann::json::object();
    }
  }

  if (jsonSettings.contains("renderQualityAutoDetect")) {
    const auto &existing = jsonSettings["renderQualityAutoDetect"];
    if (!existing.is_object()) {
      LOG_WARN("QualityTierManager: invalid metadata format in {}, overwrite",
               settingsPath);
    } else {
      const int existingVersion = existing.value("version", -1);
      if (existingVersion != kSelectionMetadataVersion) {
        LOG_WARN(
            "QualityTierManager: metadata version migration {} -> {} in {}",
            existingVersion, kSelectionMetadataVersion, settingsPath);
      }
    }
  }

  const auto &caps = m_capabilitySnapshot;
  const auto &meta = m_selectionMetadata;
  nlohmann::json detail = nlohmann::json::object();
  detail["version"] = meta.version;
  detail["selectedTier"] = ToString(meta.selectedTier);
  detail["source"] = ToString(meta.source);
  detail["reason"] = meta.reasonCode;
  detail["capabilityTier"] = ToString(meta.capabilityTier);
  detail["benchmarkTier"] = ToString(meta.benchmarkTier);
  detail["benchmarkScore"] = meta.benchmarkScore;
  detail["degradeLevel"] = m_autoDegradeLevel;
  detail["renderer"] = m_rendererString;
  detail["updatedAtUtc"] = MakeUtcTimestamp();
  detail["capability"] = {{"maxShaderStorageBufferBindings",
                            caps.maxShaderStorageBufferBindings},
                           {"maxComputeWorkGroupInvocations",
                            caps.maxComputeWorkGroupInvocations},
                           {"maxComputeWorkGroupSize",
                            {caps.maxComputeWorkGroupSize[0],
                             caps.maxComputeWorkGroupSize[1],
                             caps.maxComputeWorkGroupSize[2]}},
                           {"maxTextureSize", caps.maxTextureSize},
                           {"maxArrayTextureLayers", caps.maxArrayTextureLayers},
                           {"maxImageUnits", caps.maxImageUnits},
                           {"valid", caps.valid}};
  jsonSettings["renderQualityAutoDetect"] = std::move(detail);

  try {
    std::ofstream out(settingsPath, std::ios::trunc);
    if (!out.is_open()) {
      LOG_WARN("QualityTierManager: failed to write {}", settingsPath);
      return;
    }
    out << jsonSettings.dump(4);
  } catch (...) {
    LOG_WARN("QualityTierManager: failed to persist metadata into {}", settingsPath);
  }
}

QualityTierManager::CapabilitySnapshot
QualityTierManager::ProbeCapabilities() const {
  CapabilitySnapshot snapshot = {};

  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    // Conservative fallback when no GPU context is available.
    snapshot.maxShaderStorageBufferBindings = 16;
    snapshot.maxComputeWorkGroupInvocations = 256;
    snapshot.maxComputeWorkGroupSize = {256, 1, 1};
    snapshot.maxTextureSize = 4096;
    snapshot.maxArrayTextureLayers = 256;
    snapshot.maxImageUnits = 8;
    snapshot.valid = false;
    return snapshot;
  }

  auto queryIntegerv = [](uint32_t token, int fallback) -> int {
    int value = fallback;
    glGetIntegerv(token, &value);
    return std::max(0, value);
  };

  snapshot.maxShaderStorageBufferBindings =
      queryIntegerv(kGLMaxShaderStorageBufferBindings, 16);
  snapshot.maxComputeWorkGroupInvocations =
      queryIntegerv(kGLMaxComputeWorkGroupInvocations, 256);
  snapshot.maxTextureSize = queryIntegerv(kGLMaxTextureSize, 4096);
  snapshot.maxArrayTextureLayers = queryIntegerv(kGLMaxArrayTextureLayers, 256);
  snapshot.maxImageUnits = queryIntegerv(kGLMaxImageUnits, 8);

  using GetIntegeriVFn = void (*)(uint32_t, uint32_t, int *);
  auto getIntegeriV = reinterpret_cast<GetIntegeriVFn>(
      glfwGetProcAddress("glGetIntegeri_v"));
  if (getIntegeriV != nullptr) {
    for (uint32_t i = 0; i < 3; ++i) {
      int value = 1;
      getIntegeriV(kGLMaxComputeWorkGroupSize, i, &value);
      snapshot.maxComputeWorkGroupSize[static_cast<size_t>(i)] = std::max(1, value);
    }
  } else {
    snapshot.maxComputeWorkGroupSize = {256, 1, 1};
  }

  snapshot.valid = true;
  return snapshot;
}

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
    break;
  case QualityTier::Medium:
    m_baseConfig.bloomEnabled = true;
    m_baseConfig.dynamicLightingEnabled = true;
    m_baseConfig.maxParticles = 60000;
    m_baseConfig.maxLights = 32;
    m_baseConfig.ambientIntensity = 0.3f;
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
    m_baseConfig.vignetteIntensity = 0.2f;
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
    m_baseConfig.colorGradingIntensity = 1.0f;
    m_baseConfig.volumetricLightEnabled = false;
    m_baseConfig.volumetricSampleCount = 0;
    m_baseConfig.volumetricScattering = 0.0f;
    m_baseConfig.volumetricDecay = 0.0f;
    m_baseConfig.profilerHudEnabled = false;
    m_baseConfig.shaderHotReloadEnabled = false;
    break;
  case QualityTier::High:
    m_baseConfig.bloomEnabled = true;
    m_baseConfig.dynamicLightingEnabled = true;
    m_baseConfig.maxParticles = 120000;
    m_baseConfig.maxLights = 128;
    m_baseConfig.ambientIntensity = 0.25f;
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
    m_baseConfig.vignetteIntensity = 0.3f;
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
    m_baseConfig.colorGradingIntensity = 1.0f;
    m_baseConfig.volumetricLightEnabled = false;
    m_baseConfig.volumetricSampleCount = 0;
    m_baseConfig.volumetricScattering = 0.0f;
    m_baseConfig.volumetricDecay = 0.0f;
    m_baseConfig.profilerHudEnabled = kHotReloadEnabled;
    m_baseConfig.shaderHotReloadEnabled = kHotReloadEnabled;
    break;
  case QualityTier::Ultra:
    m_baseConfig.bloomEnabled = true;
    m_baseConfig.dynamicLightingEnabled = true;
    m_baseConfig.maxParticles = 200000;
    m_baseConfig.maxLights = 256;
    m_baseConfig.ambientIntensity = 0.2f;
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
    m_baseConfig.vignetteIntensity = 0.35f;
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
    m_baseConfig.colorGradingIntensity = 1.0f;
    m_baseConfig.volumetricLightEnabled = true;
    m_baseConfig.volumetricSampleCount = 48;
    m_baseConfig.volumetricScattering = 0.16f;
    m_baseConfig.volumetricDecay = 0.95f;
    m_baseConfig.profilerHudEnabled = kHotReloadEnabled;
    m_baseConfig.shaderHotReloadEnabled = kHotReloadEnabled;
    break;
  }

  ApplyAutoDegradeLevel();
}

void QualityTierManager::ApplyAutoDegradeLevel() {
  m_config = m_baseConfig;
  const int level = std::clamp(m_autoDegradeLevel, 0, kAutoDegradeMaxLevel);

  // 1) Reduce bloom mips.
  if (level >= 1 && m_config.bloomEnabled) {
    m_config.bloomMipLevels = std::max(1, m_config.bloomMipLevels - 2);
  }

  // 2) Disable distortion.
  if (level >= 2) {
    m_config.distortionEnabled = false;
  }

  // 3) Reduce max lights.
  if (level >= 3 && m_config.maxLights > 0) {
    m_config.maxLights = std::max(4, m_config.maxLights / 2);
  }

  // 4) Reduce particle/sub-emitter budgets.
  if (level >= 4) {
    m_config.maxParticles = std::max(30000, m_config.maxParticles / 2);
    m_config.subEmitterEnabled = false;
    m_config.maxTrails = std::max(0, m_config.maxTrails / 2);
    m_config.vfxSequenceDetail = std::min(m_config.vfxSequenceDetail, 1);
  }

  // 5) Drop optional volumetric features.
  if (level >= 5) {
    m_config.volumetricLightEnabled = false;
    m_config.volumetricSampleCount = 0;
    m_config.volumetricScattering = 0.0f;
    m_config.volumetricDecay = 0.0f;
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
