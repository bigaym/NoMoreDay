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
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <utility>

namespace NoMoreDay::render::core {
namespace {

constexpr uint32_t kGLMaxShaderStorageBufferBindings = 0x90DD;
constexpr uint32_t kGLMaxComputeWorkGroupInvocations = 0x90EB;
constexpr uint32_t kGLMaxComputeWorkGroupSize = 0x91BF;
constexpr uint32_t kGLMaxTextureSize = 0x0D33;
constexpr uint32_t kGLMaxArrayTextureLayers = 0x88FF;
constexpr uint32_t kGLMaxImageUnits = 0x8F38;
constexpr const char *kRenderKey = "render";
constexpr const char *kRenderV3Key = "v3";
constexpr const char *kRenderV3FlatEnabledKey = "render.v3.enabled";
constexpr std::array<QualityTierManager::AutoDegradeStep, 6>
    kV3AutoDegradeSequence = {
        QualityTierManager::AutoDegradeStep::ReduceBloom,
        QualityTierManager::AutoDegradeStep::DisableDistortion,
        QualityTierManager::AutoDegradeStep::LimitDynamicLights,
        QualityTierManager::AutoDegradeStep::ReduceClusteredPressure,
        QualityTierManager::AutoDegradeStep::HybridShadowToSDF,
        QualityTierManager::AutoDegradeStep::DisableHighMaterialBranch,
    };
constexpr uint32_t kClusteredDegradedTileSize = 64;
constexpr uint32_t kClusteredDegradedMaxZSlices = 2;

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool Contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
}

const char *ToString(ShadowMode mode) {
  switch (mode) {
  case ShadowMode::Off:
    return "off";
  case ShadowMode::SDF:
    return "sdf";
  case ShadowMode::Hybrid:
    return "hybrid";
  }
  return "off";
}

bool ParseShadowMode(std::string value, ShadowMode &outMode) {
  value = ToLower(std::move(value));
  if (value == "off") {
    outMode = ShadowMode::Off;
    return true;
  }
  if (value == "sdf") {
    outMode = ShadowMode::SDF;
    return true;
  }
  if (value == "hybrid") {
    outMode = ShadowMode::Hybrid;
    return true;
  }
  return false;
}

void WriteV3ConfigToJson(nlohmann::json &jsonSettings, const RenderConfig &config) {
  nlohmann::json v3 = nlohmann::json::object();
  v3["enabled"] = config.v3Enabled;
  v3["shadowEnabled"] = config.shadowEnabled;
  v3["shadowMode"] = ToString(config.shadowMode);
  v3["maxShadowedLights"] = config.maxShadowedLights;
  v3["shadowAtlasSize"] = config.shadowAtlasSize;
  v3["shadowSoftness"] = config.shadowSoftness;
  v3["clusteredLightingEnabled"] = config.clusteredLightingEnabled;
  v3["clusterTileSize"] = config.clusterTileSize;
  v3["clusterZSliceCount"] = config.clusterZSliceCount;
  v3["normalLightingEnabled"] = config.normalLightingEnabled;
  v3["specularEnabled"] = config.specularEnabled;
  v3["materialQualityLevel"] = config.materialQualityLevel;

  jsonSettings[kRenderV3FlatEnabledKey] = config.v3Enabled;
  jsonSettings[kRenderKey][kRenderV3Key] = std::move(v3);
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

void ApplyTierShadowPolicy(RenderConfig &config, QualityTier tier) {
  if (!config.v3Enabled) {
    config.shadowEnabled = false;
    config.shadowMode = ShadowMode::Off;
    return;
  }

  switch (tier) {
  case QualityTier::Low:
  case QualityTier::Medium:
    config.shadowEnabled = false;
    config.shadowMode = ShadowMode::Off;
    break;
  case QualityTier::High:
    config.shadowEnabled = true;
    config.shadowMode = ShadowMode::SDF;
    config.maxShadowedLights = std::max(config.maxShadowedLights, 4u);
    break;
  case QualityTier::Ultra:
    config.shadowEnabled = true;
    config.shadowMode = ShadowMode::Hybrid;
    config.maxShadowedLights = std::max(config.maxShadowedLights, 8u);
    config.shadowAtlasSize = std::max(config.shadowAtlasSize, 2048u);
    break;
  }
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

const std::array<QualityTierManager::AutoDegradeStep, 6> &
QualityTierManager::GetV3AutoDegradeSequence() {
  return kV3AutoDegradeSequence;
}

QualityTierManager::V3CapabilityMatrixEntry
QualityTierManager::GetV3CapabilityMatrix(QualityTier tier) {
  switch (tier) {
  case QualityTier::Low:
    return {.shadowMode = ShadowMode::Off,
            .clusteredLighting = V3FeatureLevel::Off,
            .materialHighBranch = V3FeatureLevel::Off,
            .volumetricQuality = V3FeatureLevel::Off,
            .distortion = V3FeatureLevel::Off};
  case QualityTier::Medium:
    return {.shadowMode = ShadowMode::Off,
            .clusteredLighting = V3FeatureLevel::Optional,
            .materialHighBranch = V3FeatureLevel::Off,
            .volumetricQuality = V3FeatureLevel::Basic,
            .distortion = V3FeatureLevel::Basic};
  case QualityTier::High:
    return {.shadowMode = ShadowMode::SDF,
            .clusteredLighting = V3FeatureLevel::On,
            .materialHighBranch = V3FeatureLevel::Partial,
            .volumetricQuality = V3FeatureLevel::On,
            .distortion = V3FeatureLevel::On};
  case QualityTier::Ultra:
    return {.shadowMode = ShadowMode::Hybrid,
            .clusteredLighting = V3FeatureLevel::On,
            .materialHighBranch = V3FeatureLevel::Full,
            .volumetricQuality = V3FeatureLevel::Full,
            .distortion = V3FeatureLevel::On};
  }
  return {.shadowMode = ShadowMode::Off,
          .clusteredLighting = V3FeatureLevel::Off,
          .materialHighBranch = V3FeatureLevel::Off,
          .volumetricQuality = V3FeatureLevel::Off,
          .distortion = V3FeatureLevel::Off};
}

void QualityTierManager::Initialize(const std::string &settingsPath,
                                    bool forceRedetect) {
  if (m_initialized && !forceRedetect) {
    return;
  }

  m_rendererString = QueryRendererString();
  m_fromSettings = false;
  m_autoDegradeLevel = 0;
  m_v3Config = {};

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
  TryLoadV3ConfigFromSettings(settingsPath, m_v3Config);
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

bool QualityTierManager::SetV3Enabled(bool enabled,
                                      const std::string &settingsPath) {
  const bool previous = m_v3Config.v3Enabled;
  const bool changed = (previous != enabled);
  m_v3Config.v3Enabled = enabled;
  ApplyV3ConfigOverrides(m_baseConfig);
  ApplyV3ConfigOverrides(m_config);

  if (changed) {
    LOG_INFO("QualityTierManager: render.v3.enabled {} -> {}", previous ? 1 : 0,
             enabled ? 1 : 0);
    if (m_v3ToggleCallback) {
      m_v3ToggleCallback(enabled);
    }
  }

  PersistSelectionMetadata(settingsPath);
  return changed;
}

bool QualityTierManager::SetClusteredLightingEnabled(
    bool enabled, const std::string &settingsPath) {
  const bool previous = m_v3Config.clusteredLightingEnabled;
  const bool changed = (previous != enabled);
  m_v3Config.clusteredLightingEnabled = enabled;
  ApplyV3ConfigOverrides(m_baseConfig);
  ApplyAutoDegradeLevel();

  if (changed) {
    LOG_INFO("QualityTierManager: render.v3.clusteredLightingEnabled {} -> {}",
             previous ? 1 : 0, enabled ? 1 : 0);
  }

  PersistSelectionMetadata(settingsPath);
  return changed;
}

bool QualityTierManager::SetNormalLightingEnabled(
    bool enabled, const std::string &settingsPath) {
  const bool previous = m_v3Config.normalLightingEnabled;
  const bool changed = (previous != enabled);
  m_v3Config.normalLightingEnabled = enabled;
  ApplyV3ConfigOverrides(m_baseConfig);
  ApplyAutoDegradeLevel();

  if (changed) {
    LOG_INFO("QualityTierManager: render.v3.normalLightingEnabled {} -> {}",
             previous ? 1 : 0, enabled ? 1 : 0);
  }

  PersistSelectionMetadata(settingsPath);
  return changed;
}

bool QualityTierManager::SetSpecularEnabled(
    bool enabled, const std::string &settingsPath) {
  const bool previous = m_v3Config.specularEnabled;
  const bool changed = (previous != enabled);
  m_v3Config.specularEnabled = enabled;
  ApplyV3ConfigOverrides(m_baseConfig);
  ApplyAutoDegradeLevel();

  if (changed) {
    LOG_INFO("QualityTierManager: render.v3.specularEnabled {} -> {}", previous ? 1 : 0,
             enabled ? 1 : 0);
  }

  PersistSelectionMetadata(settingsPath);
  return changed;
}

void QualityTierManager::SetV3ToggleCallback(V3ToggleCallback callback) {
  m_v3ToggleCallback = std::move(callback);
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

bool QualityTierManager::TryLoadV3ConfigFromSettings(
    const std::string &settingsPath, RenderConfig &outConfig) const {
  if (!std::filesystem::exists(settingsPath)) {
    LOG_WARN("QualityTierManager: {} missing render.v3 settings, defaults applied",
             settingsPath);
    return false;
  }

  nlohmann::json jsonSettings;
  try {
    std::ifstream file(settingsPath);
    if (!file.is_open()) {
      LOG_WARN("QualityTierManager: failed to open {} for V3 config", settingsPath);
      return false;
    }
    file >> jsonSettings;
  } catch (...) {
    LOG_WARN("QualityTierManager: failed to parse {} for V3 config", settingsPath);
    return false;
  }

  const nlohmann::json *v3Node = nullptr;
  if (jsonSettings.contains(kRenderKey) && jsonSettings[kRenderKey].is_object()) {
    const auto &renderNode = jsonSettings[kRenderKey];
    if (renderNode.contains(kRenderV3Key) && renderNode[kRenderV3Key].is_object()) {
      v3Node = &renderNode[kRenderV3Key];
    } else if (renderNode.contains(kRenderV3Key)) {
      LOG_WARN(
          "QualityTierManager: {} has invalid render.v3 section, defaults applied",
          settingsPath);
    }
  } else if (jsonSettings.contains(kRenderKey)) {
    LOG_WARN("QualityTierManager: {} has invalid render section, defaults applied",
             settingsPath);
  }

  bool hasInvalidValue = false;
  auto readBool = [&](const char *key, bool &target) {
    if (v3Node == nullptr || !v3Node->contains(key)) {
      LOG_WARN("QualityTierManager: {} missing render.v3.{}, default used",
               settingsPath, key);
      return;
    }
    const auto &value = (*v3Node)[key];
    if (!value.is_boolean()) {
      LOG_WARN("QualityTierManager: {} invalid render.v3.{} (expected bool), "
               "default used",
               settingsPath, key);
      hasInvalidValue = true;
      return;
    }
    target = value.get<bool>();
  };

  auto readUInt32 = [&](const char *key, uint32_t &target) {
    if (v3Node == nullptr || !v3Node->contains(key)) {
      LOG_WARN("QualityTierManager: {} missing render.v3.{}, default used",
               settingsPath, key);
      return;
    }
    const auto &value = (*v3Node)[key];
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
      LOG_WARN("QualityTierManager: {} invalid render.v3.{} (expected uint32), "
               "default used",
               settingsPath, key);
      hasInvalidValue = true;
      return;
    }

    if (value.is_number_unsigned()) {
      const uint64_t parsedUnsigned = value.get<uint64_t>();
      if (parsedUnsigned > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
        LOG_WARN("QualityTierManager: {} out-of-range render.v3.{}={}, default used",
                 settingsPath, key, parsedUnsigned);
        hasInvalidValue = true;
        return;
      }
      target = static_cast<uint32_t>(parsedUnsigned);
      return;
    }

    const int64_t parsedSigned = value.get<int64_t>();
    if (parsedSigned < 0 ||
        parsedSigned > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
      LOG_WARN("QualityTierManager: {} out-of-range render.v3.{}={}, default used",
               settingsPath, key, parsedSigned);
      hasInvalidValue = true;
      return;
    }
    target = static_cast<uint32_t>(parsedSigned);
  };

  auto readNonNegativeFloat = [&](const char *key, float &target) {
    if (v3Node == nullptr || !v3Node->contains(key)) {
      LOG_WARN("QualityTierManager: {} missing render.v3.{}, default used",
               settingsPath, key);
      return;
    }
    const auto &value = (*v3Node)[key];
    if (!value.is_number()) {
      LOG_WARN("QualityTierManager: {} invalid render.v3.{} (expected number), "
               "default used",
               settingsPath, key);
      hasInvalidValue = true;
      return;
    }

    const float parsed = value.get<float>();
    if (!std::isfinite(parsed) || parsed < 0.0f) {
      LOG_WARN("QualityTierManager: {} out-of-range render.v3.{}={:.3f}, "
               "default used",
               settingsPath, key, parsed);
      hasInvalidValue = true;
      return;
    }
    target = parsed;
  };

  if (jsonSettings.contains(kRenderV3FlatEnabledKey)) {
    const auto &value = jsonSettings[kRenderV3FlatEnabledKey];
    if (!value.is_boolean()) {
      LOG_WARN("QualityTierManager: {} invalid {} (expected bool), default used",
               settingsPath, kRenderV3FlatEnabledKey);
      hasInvalidValue = true;
    } else {
      outConfig.v3Enabled = value.get<bool>();
    }
  } else {
    readBool("enabled", outConfig.v3Enabled);
  }

  if (v3Node == nullptr) {
    LOG_WARN("QualityTierManager: {} missing render.v3 object, V3 defaults used",
             settingsPath);
    return !hasInvalidValue;
  }

  readBool("shadowEnabled", outConfig.shadowEnabled);

  if (v3Node->contains("shadowMode")) {
    const auto &shadowModeValue = (*v3Node)["shadowMode"];
    if (shadowModeValue.is_string()) {
      ShadowMode parsedMode = outConfig.shadowMode;
      if (ParseShadowMode(shadowModeValue.get<std::string>(), parsedMode)) {
        outConfig.shadowMode = parsedMode;
      } else {
        LOG_WARN("QualityTierManager: {} invalid render.v3.shadowMode, default used",
                 settingsPath);
        hasInvalidValue = true;
      }
    } else if (shadowModeValue.is_number_integer()) {
      const int modeValue = shadowModeValue.get<int>();
      if (modeValue >= static_cast<int>(ShadowMode::Off) &&
          modeValue <= static_cast<int>(ShadowMode::Hybrid)) {
        outConfig.shadowMode = static_cast<ShadowMode>(modeValue);
      } else {
        LOG_WARN(
            "QualityTierManager: {} out-of-range render.v3.shadowMode={}, default "
            "used",
            settingsPath, modeValue);
        hasInvalidValue = true;
      }
    } else {
      LOG_WARN("QualityTierManager: {} invalid render.v3.shadowMode type, "
               "default used",
               settingsPath);
      hasInvalidValue = true;
    }
  } else {
    LOG_WARN("QualityTierManager: {} missing render.v3.shadowMode, default used",
             settingsPath);
  }

  readUInt32("maxShadowedLights", outConfig.maxShadowedLights);
  readUInt32("shadowAtlasSize", outConfig.shadowAtlasSize);
  readNonNegativeFloat("shadowSoftness", outConfig.shadowSoftness);
  readBool("clusteredLightingEnabled", outConfig.clusteredLightingEnabled);
  readUInt32("clusterTileSize", outConfig.clusterTileSize);
  readUInt32("clusterZSliceCount", outConfig.clusterZSliceCount);
  readBool("normalLightingEnabled", outConfig.normalLightingEnabled);
  readBool("specularEnabled", outConfig.specularEnabled);
  readUInt32("materialQualityLevel", outConfig.materialQualityLevel);

  return !hasInvalidValue;
}

void QualityTierManager::ApplyV3ConfigOverrides(RenderConfig &config) const {
  config.shadowEnabled = m_v3Config.shadowEnabled;
  config.shadowMode = m_v3Config.shadowMode;
  config.maxShadowedLights = m_v3Config.maxShadowedLights;
  config.shadowAtlasSize = m_v3Config.shadowAtlasSize;
  config.shadowSoftness = m_v3Config.shadowSoftness;
  config.clusteredLightingEnabled = m_v3Config.clusteredLightingEnabled;
  config.clusterTileSize = m_v3Config.clusterTileSize;
  config.clusterZSliceCount = m_v3Config.clusterZSliceCount;
  config.normalLightingEnabled = m_v3Config.normalLightingEnabled;
  config.specularEnabled = m_v3Config.specularEnabled;
  config.materialQualityLevel = m_v3Config.materialQualityLevel;
  config.v3Enabled = m_v3Config.v3Enabled;
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
  WriteV3ConfigToJson(jsonSettings, m_v3Config);

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

  ApplyV3ConfigOverrides(m_baseConfig);
  ApplyTierShadowPolicy(m_baseConfig, tier);
  ApplyAutoDegradeLevel();
}

void QualityTierManager::ApplyAutoDegradeLevel() {
  m_config = m_baseConfig;
  const int level = std::clamp(m_autoDegradeLevel, 0, kAutoDegradeMaxLevel);

  // 1) Reduce bloom level.
  if (level >= static_cast<int>(AutoDegradeStep::ReduceBloom) &&
      m_config.bloomEnabled) {
    m_config.bloomMipLevels = std::max(1, m_config.bloomMipLevels - 2);
  }

  // 2) Disable distortion.
  if (level >= static_cast<int>(AutoDegradeStep::DisableDistortion)) {
    m_config.distortionEnabled = false;
  }

  // 3) Limit dynamic lights.
  if (level >= static_cast<int>(AutoDegradeStep::LimitDynamicLights) &&
      m_config.maxLights > 0) {
    m_config.maxLights = std::max(4, m_config.maxLights / 2);
  }

  // 4) Reduce clustered high-pressure parameters.
  if (level >= static_cast<int>(AutoDegradeStep::ReduceClusteredPressure) &&
      m_config.clusteredLightingEnabled) {
    m_config.clusterTileSize =
        std::max(m_config.clusterTileSize, kClusteredDegradedTileSize);
    m_config.clusterZSliceCount =
        std::min(m_config.clusterZSliceCount, kClusteredDegradedMaxZSlices);
  }

  // 5) Hybrid shadow degrades to SDF.
  if (level >= static_cast<int>(AutoDegradeStep::HybridShadowToSDF) &&
      m_config.shadowMode == ShadowMode::Hybrid) {
    m_config.shadowMode = ShadowMode::SDF;
  }

  // 6) Disable high-end material branches.
  if (level >= static_cast<int>(AutoDegradeStep::DisableHighMaterialBranch)) {
    m_config.normalLightingEnabled = false;
    m_config.specularEnabled = false;
    m_config.materialQualityLevel = 0;
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
