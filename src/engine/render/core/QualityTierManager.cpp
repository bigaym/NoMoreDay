#include "engine/render/core/QualityTierManagerInternal.hpp"

namespace NoMoreDay::render::core {
using namespace NoMoreDay::render::core::detail;


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
  m_adaptiveQualitySettings = {};
  m_gpuTextEnabledOverride = std::nullopt;
  m_gpuLootEnabledOverride = std::nullopt;
  m_giEnabledOverride = std::nullopt;
  m_fluidEnabledOverride = std::nullopt;
  m_giRuntimeOverride = std::nullopt;
  m_giOverrideActive = false;
  m_giOverrideThread = std::thread::id{};

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
  m_gpuTextEnabledOverride = TryLoadGpuTextEnabledOverride(settingsPath);
  m_gpuLootEnabledOverride = TryLoadGpuLootEnabledOverride(settingsPath);
  m_giEnabledOverride = TryLoadGiEnabledOverride(settingsPath);
  m_fluidEnabledOverride = TryLoadFluidEnabledOverride(settingsPath);
  m_linearPipelineEnabledOverride =
      TryLoadLinearPipelineEnabledOverride(settingsPath);
  TryLoadAdaptiveQualityConfigFromSettings(settingsPath, m_adaptiveQualitySettings);
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

bool QualityTierManager::SetGiEnabledOverride(bool enabled) {
  if (m_giOverrideActive &&
      std::this_thread::get_id() != m_giOverrideThread) {
    LOG_WARN(
        "QualityTierManager: SetGiEnabledOverride({}) rejected: active runtime "
        "override owned by a different thread",
        enabled ? 1 : 0);
    return false;
  }
  const bool changed =
      !m_giOverrideActive || m_giRuntimeOverride.value_or(!enabled) != enabled;
  SetGiOverrideInternal(std::optional<bool>(enabled));
  ReapplyConfigAfterGiOverride();
  if (changed) {
    LOG_INFO("QualityTierManager: runtime GI override -> {}",
             enabled ? 1 : 0);
  }
  return true;
}

bool QualityTierManager::ClearGiEnabledOverride() {
  if (!m_giOverrideActive) {
    return true;
  }
  if (std::this_thread::get_id() != m_giOverrideThread) {
    LOG_WARN(
        "QualityTierManager: ClearGiEnabledOverride rejected: active runtime "
        "override owned by a different thread");
    return false;
  }
  SetGiOverrideInternal(std::nullopt);
  ReapplyConfigAfterGiOverride();
  LOG_INFO("QualityTierManager: runtime GI override cleared");
  return true;
}

std::optional<bool> QualityTierManager::EffectiveGiEnabled() const {
  if (m_giOverrideActive && m_giRuntimeOverride.has_value()) {
    return m_giRuntimeOverride;
  }
  if (m_giEnabledOverride.has_value()) {
    return m_giEnabledOverride;
  }
  return std::nullopt;
}

QualityTierManager::GiEnabledOverrideGuard::GiEnabledOverrideGuard(bool enabled)
    : m_manager(QualityTierManager::Get()), m_owned(false) {
  m_wasActive = m_manager.m_giOverrideActive;
  m_priorValue = m_manager.m_giRuntimeOverride.value_or(false);
  m_owned = m_manager.SetGiEnabledOverride(enabled);
}

QualityTierManager::GiEnabledOverrideGuard::~GiEnabledOverrideGuard() {
  if (!m_owned) {
    return;
  }
  m_manager.SetGiOverrideInternal(m_wasActive
                                      ? std::optional<bool>(m_priorValue)
                                      : std::nullopt);
  m_manager.ReapplyConfigAfterGiOverride();
  m_owned = false;
}

bool QualityTierManager::SetLinearPipelineEnabled(
    bool enabled, const std::string &settingsPath) {
  const bool previous = m_config.linearPipeline;
  const bool changed = (previous != enabled);
  m_linearPipelineEnabledOverride = enabled;
  m_baseConfig.linearPipeline = enabled;
  m_config.linearPipeline = enabled;

  if (changed) {
    LOG_INFO("QualityTierManager: render.color.linearPipeline {} -> {}",
             previous ? 1 : 0, enabled ? 1 : 0);
  }

  if (settingsPath.empty()) {
    return changed;
  }

  nlohmann::json jsonSettings = nlohmann::json::object();
  if (std::filesystem::exists(settingsPath)) {
    try {
      std::ifstream file(settingsPath);
      if (file.is_open()) {
        file >> jsonSettings;
      }
    } catch (...) {
      LOG_WARN("QualityTierManager: failed to parse {}, linearPipeline save skipped",
               settingsPath);
      return false;
    }

    if (!jsonSettings.is_object()) {
      LOG_WARN("QualityTierManager: {} has non-object root, linearPipeline save skipped",
               settingsPath);
      return false;
    }
  }

  if (!jsonSettings.contains(kRenderKey) || !jsonSettings[kRenderKey].is_object()) {
    jsonSettings[kRenderKey] = nlohmann::json::object();
  }
  if (!jsonSettings[kRenderKey].contains(kRenderColorKey) ||
      !jsonSettings[kRenderKey][kRenderColorKey].is_object()) {
    jsonSettings[kRenderKey][kRenderColorKey] = nlohmann::json::object();
  }
  jsonSettings[kRenderKey][kRenderColorKey]["linearPipeline"] = enabled;
  jsonSettings[kRenderColorLinearPipelineFlatEnabledKey] = enabled;

  const bool persisted =
      WriteJsonAtomically(settingsPath, jsonSettings, "linearPipeline config");
  return persisted;
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

  const bool persisted = WriteV3ConfigToFile(settingsPath, m_v3Config);
  return changed && persisted;
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

  const bool persisted = WriteV3ConfigToFile(settingsPath, m_v3Config);
  return changed && persisted;
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

  const bool persisted = WriteV3ConfigToFile(settingsPath, m_v3Config);
  return changed && persisted;
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

  const bool persisted = WriteV3ConfigToFile(settingsPath, m_v3Config);
  return changed && persisted;
}

void QualityTierManager::SetV3ToggleCallback(V3ToggleCallback callback) {
  m_v3ToggleCallback = std::move(callback);
}


void QualityTierManager::ApplyGiRuntimeOverrideToConfig(
    RenderConfig &config) const {
  if (!m_giOverrideActive || !m_giRuntimeOverride.has_value()) {
    return;
  }
  config.giEnabled = m_giRuntimeOverride.value();
  if (!config.giEnabled) {
    config.giCascadeLevels = 0;
    config.giIntensity = 0.0f;
  } else {
    if (config.giCascadeLevels == 0) {
      config.giCascadeLevels = (m_tier == QualityTier::Ultra) ? 6u : 4u;
    }
    if (config.giIntensity <= 0.0f) {
      config.giIntensity = 1.0f;
    }
  }
}

void QualityTierManager::SetGiOverrideInternal(std::optional<bool> value) {
  m_giRuntimeOverride = value;
  m_giOverrideActive = value.has_value();
  m_giOverrideThread =
      value.has_value() ? std::this_thread::get_id() : std::thread::id{};
}

void QualityTierManager::ReapplyConfigAfterGiOverride() {
  if (m_initialized) {
    ApplyAutoDegradeLevel();
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
