#include "engine/render/core/QualityTierManagerInternal.hpp"

namespace NoMoreDay::render::core {
using namespace NoMoreDay::render::core::detail;

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
  readBool("clusteredLightingV4Enabled", outConfig.clusteredLightingV4Enabled);
  readUInt32("clusterTileSize", outConfig.clusterTileSize);
  readUInt32("clusterZSliceCount", outConfig.clusterZSliceCount);
  readBool("normalLightingEnabled", outConfig.normalLightingEnabled);
  readBool("specularEnabled", outConfig.specularEnabled);
  readUInt32("materialQualityLevel", outConfig.materialQualityLevel);
  readBool("heightShadowEnabled", outConfig.heightShadowEnabled);
  readUInt32("heightShadowSteps", outConfig.heightShadowSteps);
  readBool("selfShadowEnabled", outConfig.selfShadowEnabled);
  readUInt32("selfShadowSteps", outConfig.selfShadowSteps);
  readBool("pomEnabled", outConfig.pomEnabled);
  readUInt32("pomLayers", outConfig.pomLayers);

  return !hasInvalidValue;
}

bool QualityTierManager::TryLoadAdaptiveQualityConfigFromSettings(
    const std::string &settingsPath,
    AdaptiveQualitySettings &outSettings) const {
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
    LOG_WARN("QualityTierManager: failed to parse {} for adaptive quality config",
             settingsPath);
    return false;
  }

  if (!jsonSettings.contains(kRenderKey) ||
      !jsonSettings[kRenderKey].is_object() ||
      !jsonSettings[kRenderKey].contains(kRenderAdaptiveQualityKey) ||
      !jsonSettings[kRenderKey][kRenderAdaptiveQualityKey].is_object()) {
    return false;
  }

  const auto &node = jsonSettings[kRenderKey][kRenderAdaptiveQualityKey];
  bool hasInvalidValue = false;
  auto readBool = [&](const char *key, bool &target) {
    if (!node.contains(key)) {
      return;
    }
    const auto &value = node[key];
    if (!value.is_boolean()) {
      LOG_WARN("QualityTierManager: {} invalid render.adaptiveQuality.{} (expected bool)",
               settingsPath, key);
      hasInvalidValue = true;
      return;
    }
    target = value.get<bool>();
  };
  auto readFloat = [&](const char *key, float &target, float minValue,
                       float maxValue) {
    if (!node.contains(key)) {
      return;
    }
    const auto &value = node[key];
    if (!value.is_number()) {
      LOG_WARN(
          "QualityTierManager: {} invalid render.adaptiveQuality.{} (expected number)",
          settingsPath, key);
      hasInvalidValue = true;
      return;
    }
    const float parsed = value.get<float>();
    if (!std::isfinite(parsed) || parsed < minValue || parsed > maxValue) {
      LOG_WARN("QualityTierManager: {} out-of-range render.adaptiveQuality.{}={:.3f}",
               settingsPath, key, parsed);
      hasInvalidValue = true;
      return;
    }
    target = parsed;
  };

  readBool("dynamicResolutionEnabled", outSettings.dynamicResolutionEnabled);
  readBool("renderScaleLocked", outSettings.renderScaleLocked);
  readFloat("renderScale", outSettings.renderScale, 0.1f, 1.0f);
  readFloat("minRenderScale", outSettings.minRenderScale, 0.1f, 1.0f);
  readFloat("maxRenderScale", outSettings.maxRenderScale, 0.1f, 1.0f);
  readFloat("renderScaleStep", outSettings.renderScaleStep, 0.001f, 1.0f);
  readFloat("downThresholdMs", outSettings.downThresholdMs, 0.0f, 1000.0f);
  readFloat("upThresholdMs", outSettings.upThresholdMs, 0.0f, 1000.0f);
  readFloat("sustainSeconds", outSettings.sustainSeconds, 0.0f, 600.0f);
  readFloat("cooldownSeconds", outSettings.cooldownSeconds, 0.0f, 3600.0f);
  readBool("autoExposureEnabled", outSettings.autoExposureEnabled);
  readFloat("exposure", outSettings.exposure, 0.001f, 32.0f);
  readFloat("minExposure", outSettings.minExposure, 0.001f, 32.0f);
  readFloat("maxExposure", outSettings.maxExposure, 0.001f, 32.0f);
  readFloat("brightenRate", outSettings.brightenRate, 0.0f, 100.0f);
  readFloat("darkenRate", outSettings.darkenRate, 0.0f, 100.0f);

  if (outSettings.minRenderScale > outSettings.maxRenderScale) {
    LOG_WARN("QualityTierManager: {} has inverted adaptive render scale bounds",
             settingsPath);
    outSettings = AdaptiveQualitySettings{};
    hasInvalidValue = true;
  } else if (outSettings.exposure < outSettings.minExposure ||
             outSettings.exposure > outSettings.maxExposure ||
             outSettings.minExposure > outSettings.maxExposure) {
    LOG_WARN("QualityTierManager: {} has invalid adaptive exposure bounds",
             settingsPath);
    outSettings = AdaptiveQualitySettings{};
    hasInvalidValue = true;
  }

  outSettings.renderScale = std::clamp(outSettings.renderScale,
                                       outSettings.minRenderScale,
                                       outSettings.maxRenderScale);
  return !hasInvalidValue;
}

std::optional<bool>
QualityTierManager::TryLoadGpuTextEnabledOverride(
    const std::string &settingsPath) const {
  if (!std::filesystem::exists(settingsPath)) {
    return std::nullopt;
  }

  nlohmann::json jsonSettings;
  try {
    std::ifstream file(settingsPath);
    if (!file.is_open()) {
      return std::nullopt;
    }
    file >> jsonSettings;
  } catch (...) {
    return std::nullopt;
  }

  if (jsonSettings.contains(kRenderGpuTextFlatEnabledKey)) {
    const auto &value = jsonSettings[kRenderGpuTextFlatEnabledKey];
    if (!value.is_boolean()) {
      LOG_WARN("QualityTierManager: {} invalid {} (expected bool), ignored",
               settingsPath, kRenderGpuTextFlatEnabledKey);
      return std::nullopt;
    }
    return value.get<bool>();
  }

  if (jsonSettings.contains(kRenderKey) && jsonSettings[kRenderKey].is_object()) {
    const auto &renderNode = jsonSettings[kRenderKey];
    if (renderNode.contains(kRenderGpuTextKey) &&
        renderNode[kRenderGpuTextKey].is_object()) {
      const auto &gpuTextNode = renderNode[kRenderGpuTextKey];
      if (gpuTextNode.contains("enabled")) {
        const auto &enabledValue = gpuTextNode["enabled"];
        if (!enabledValue.is_boolean()) {
          LOG_WARN("QualityTierManager: {} invalid render.gpuText.enabled "
                   "(expected bool), ignored",
                   settingsPath);
          return std::nullopt;
        }
        return enabledValue.get<bool>();
      }
    }
  }

  return std::nullopt;
}

std::optional<bool>
QualityTierManager::TryLoadGpuLootEnabledOverride(
    const std::string &settingsPath) const {
  if (!std::filesystem::exists(settingsPath)) {
    return std::nullopt;
  }

  nlohmann::json jsonSettings;
  try {
    std::ifstream file(settingsPath);
    if (!file.is_open()) {
      return std::nullopt;
    }
    file >> jsonSettings;
  } catch (...) {
    return std::nullopt;
  }

  if (jsonSettings.contains(kRenderGpuLootFlatEnabledKey)) {
    const auto &value = jsonSettings[kRenderGpuLootFlatEnabledKey];
    if (!value.is_boolean()) {
      LOG_WARN("QualityTierManager: {} invalid {} (expected bool), ignored",
               settingsPath, kRenderGpuLootFlatEnabledKey);
      return std::nullopt;
    }
    return value.get<bool>();
  }

  if (jsonSettings.contains(kRenderKey) && jsonSettings[kRenderKey].is_object()) {
    const auto &renderNode = jsonSettings[kRenderKey];
    if (renderNode.contains(kRenderGpuLootKey) &&
        renderNode[kRenderGpuLootKey].is_object()) {
      const auto &gpuLootNode = renderNode[kRenderGpuLootKey];
      if (gpuLootNode.contains("enabled")) {
        const auto &enabledValue = gpuLootNode["enabled"];
        if (!enabledValue.is_boolean()) {
          LOG_WARN("QualityTierManager: {} invalid render.gpuLoot.enabled "
                   "(expected bool), ignored",
                   settingsPath);
          return std::nullopt;
        }
        return enabledValue.get<bool>();
      }
    }
  }

  return std::nullopt;
}

std::optional<bool>
QualityTierManager::TryLoadGiEnabledOverride(
    const std::string &settingsPath) const {
  if (!std::filesystem::exists(settingsPath)) {
    return std::nullopt;
  }

  nlohmann::json jsonSettings;
  try {
    std::ifstream file(settingsPath);
    if (!file.is_open()) {
      return std::nullopt;
    }
    file >> jsonSettings;
  } catch (...) {
    return std::nullopt;
  }

  if (jsonSettings.contains(kRenderGiFlatEnabledKey)) {
    const auto &value = jsonSettings[kRenderGiFlatEnabledKey];
    if (!value.is_boolean()) {
      LOG_WARN("QualityTierManager: {} invalid {} (expected bool), ignored",
               settingsPath, kRenderGiFlatEnabledKey);
      return std::nullopt;
    }
    return value.get<bool>();
  }

  if (jsonSettings.contains(kRenderKey) && jsonSettings[kRenderKey].is_object()) {
    const auto &renderNode = jsonSettings[kRenderKey];
    if (renderNode.contains(kRenderGiKey) && renderNode[kRenderGiKey].is_object()) {
      const auto &giNode = renderNode[kRenderGiKey];
      if (giNode.contains("enabled")) {
        const auto &enabledValue = giNode["enabled"];
        if (!enabledValue.is_boolean()) {
          LOG_WARN("QualityTierManager: {} invalid render.gi.enabled "
                   "(expected bool), ignored",
                   settingsPath);
          return std::nullopt;
        }
        return enabledValue.get<bool>();
      }
    }
  }

  return std::nullopt;
}

std::optional<bool>
QualityTierManager::TryLoadFluidEnabledOverride(
    const std::string &settingsPath) const {
  if (!std::filesystem::exists(settingsPath)) {
    return std::nullopt;
  }

  nlohmann::json jsonSettings;
  try {
    std::ifstream file(settingsPath);
    if (!file.is_open()) {
      return std::nullopt;
    }
    file >> jsonSettings;
  } catch (...) {
    return std::nullopt;
  }

  if (jsonSettings.contains(kRenderFluidFlatEnabledKey)) {
    const auto &value = jsonSettings[kRenderFluidFlatEnabledKey];
    if (!value.is_boolean()) {
      LOG_WARN("QualityTierManager: {} invalid {} (expected bool), ignored",
               settingsPath, kRenderFluidFlatEnabledKey);
      return std::nullopt;
    }
    return value.get<bool>();
  }

  if (jsonSettings.contains(kRenderKey) && jsonSettings[kRenderKey].is_object()) {
    const auto &renderNode = jsonSettings[kRenderKey];
    if (renderNode.contains(kRenderFluidKey) &&
        renderNode[kRenderFluidKey].is_object()) {
      const auto &fluidNode = renderNode[kRenderFluidKey];
      if (fluidNode.contains("enabled")) {
        const auto &enabledValue = fluidNode["enabled"];
        if (!enabledValue.is_boolean()) {
          LOG_WARN("QualityTierManager: {} invalid render.fluid.enabled "
                   "(expected bool), ignored",
                   settingsPath);
          return std::nullopt;
        }
        return enabledValue.get<bool>();
      }
    }
  }

  return std::nullopt;
}

std::optional<bool>
QualityTierManager::TryLoadLinearPipelineEnabledOverride(
    const std::string &settingsPath) const {
  if (!std::filesystem::exists(settingsPath)) {
    return std::nullopt;
  }

  nlohmann::json jsonSettings;
  try {
    std::ifstream file(settingsPath);
    if (!file.is_open()) {
      return std::nullopt;
    }
    file >> jsonSettings;
  } catch (...) {
    return std::nullopt;
  }

  if (jsonSettings.contains(kRenderColorLinearPipelineFlatEnabledKey)) {
    const auto &value = jsonSettings[kRenderColorLinearPipelineFlatEnabledKey];
    if (!value.is_boolean()) {
      LOG_WARN("QualityTierManager: {} invalid {} (expected bool), ignored",
               settingsPath, kRenderColorLinearPipelineFlatEnabledKey);
      return std::nullopt;
    }
    return value.get<bool>();
  }

  if (jsonSettings.contains(kRenderKey) && jsonSettings[kRenderKey].is_object()) {
    const auto &renderNode = jsonSettings[kRenderKey];
    if (renderNode.contains(kRenderColorKey) &&
        renderNode[kRenderColorKey].is_object()) {
      const auto &colorNode = renderNode[kRenderColorKey];
      if (colorNode.contains("linearPipeline")) {
        const auto &enabledValue = colorNode["linearPipeline"];
        if (!enabledValue.is_boolean()) {
          LOG_WARN("QualityTierManager: {} invalid render.color.linearPipeline "
                   "(expected bool), ignored",
                   settingsPath);
          return std::nullopt;
        }
        return enabledValue.get<bool>();
      }
    }
  }

  return std::nullopt;
}

void QualityTierManager::ApplyV3ConfigOverrides(RenderConfig &config) const {
  config.shadowEnabled = m_v3Config.shadowEnabled;
  config.shadowMode = m_v3Config.shadowMode;
  config.maxShadowedLights = m_v3Config.maxShadowedLights;
  config.shadowAtlasSize = m_v3Config.shadowAtlasSize;
  config.shadowSoftness = m_v3Config.shadowSoftness;
  config.clusteredLightingEnabled = m_v3Config.clusteredLightingEnabled;
  config.clusteredLightingV4Enabled = m_v3Config.clusteredLightingV4Enabled;
  config.clusterTileSize = m_v3Config.clusterTileSize;
  config.clusterZSliceCount = m_v3Config.clusterZSliceCount;
  config.normalLightingEnabled = m_v3Config.normalLightingEnabled;
  config.specularEnabled = m_v3Config.specularEnabled;
  config.materialQualityLevel = m_v3Config.materialQualityLevel;
  config.heightShadowEnabled = m_v3Config.heightShadowEnabled;
  config.heightShadowSteps = m_v3Config.heightShadowSteps;
  config.selfShadowEnabled = m_v3Config.selfShadowEnabled;
  config.selfShadowSteps = m_v3Config.selfShadowSteps;
  config.pomEnabled = m_v3Config.pomEnabled;
  config.pomLayers = m_v3Config.pomLayers;
  config.v3Enabled = m_v3Config.v3Enabled;
  config.adaptiveQuality = m_adaptiveQualitySettings;
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
      if (!file.is_open()) {
        // An existing file that cannot be opened is preserved as-is; it must
        // not be mistaken for an empty document and overwritten.
        LOG_WARN("QualityTierManager: failed to open {}, metadata persistence skipped",
                 settingsPath);
        return;
      }
      file >> jsonSettings;
    } catch (...) {
      // A malformed document is preserved as-is rather than being replaced by
      // a metadata-only object, so no unrelated user content is destroyed.
      LOG_WARN("QualityTierManager: failed to parse {}, metadata persistence skipped",
               settingsPath);
      return;
    }

    // Fail closed on a legal but non-object root (e.g. a top-level array or
    // scalar): writing into it would throw or silently replace user content.
    if (!jsonSettings.is_object()) {
      LOG_WARN("QualityTierManager: {} has non-object root, metadata persistence skipped",
               settingsPath);
      return;
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

  (void)WriteJsonAtomically(settingsPath, jsonSettings, "selection metadata");
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


} // namespace NoMoreDay::render::core
