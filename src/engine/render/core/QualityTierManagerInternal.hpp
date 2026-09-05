#pragma once

#include "engine/render/core/QualityTierManager.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

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
#include <thread>
#include <utility>

namespace NoMoreDay::render::core::detail {

constexpr uint32_t kGLMaxShaderStorageBufferBindings = 0x90DD;
constexpr uint32_t kGLMaxComputeWorkGroupInvocations = 0x90EB;
constexpr uint32_t kGLMaxComputeWorkGroupSize = 0x91BF;
constexpr uint32_t kGLMaxTextureSize = 0x0D33;
constexpr uint32_t kGLMaxArrayTextureLayers = 0x88FF;
constexpr uint32_t kGLMaxImageUnits = 0x8F38;
constexpr const char *kRenderKey = "render";
constexpr const char *kRenderV3Key = "v3";
constexpr const char *kRenderGpuTextKey = "gpuText";
constexpr const char *kRenderGpuLootKey = "gpuLoot";
constexpr const char *kRenderGiKey = "gi";
constexpr const char *kRenderFluidKey = "fluid";
constexpr const char *kRenderAdaptiveQualityKey = "adaptiveQuality";
constexpr const char *kRenderV3FlatEnabledKey = "render.v3.enabled";
constexpr const char *kRenderGpuTextFlatEnabledKey = "render.gpuText.enabled";
constexpr const char *kRenderGpuLootFlatEnabledKey = "render.gpuLoot.enabled";
constexpr const char *kRenderGiFlatEnabledKey = "render.gi.enabled";
constexpr const char *kRenderFluidFlatEnabledKey = "render.fluid.enabled";
constexpr const char *kRenderColorKey = "color";
constexpr const char *kRenderColorLinearPipelineFlatEnabledKey =
    "render.color.linearPipeline";
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

inline std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

inline bool Contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
}

inline const char *ToString(ShadowMode mode) {
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

inline bool ParseShadowMode(std::string value, ShadowMode &outMode) {
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

// Writes only the versioned render domain (nested object plus the compatibility
// flat enabled key). Known fields are merged into the existing object so unknown
// child keys survive. This serializer must never touch the GI, GPU text, GPU
// loot, fluid, or adaptive-quality domains; each of
// those domains owns its own subtree and is loaded through its own override
// loader. The caller validates the destination structure; this function also
// fails closed on a non-object root instead of throwing.
inline void WriteV3ConfigToJson(nlohmann::json &jsonSettings, const RenderConfig &config) {
  if (!jsonSettings.is_object()) {
    return;
  }
  // Fail closed rather than overwriting a non-object render subtree.
  if (jsonSettings.contains(kRenderKey) &&
      !jsonSettings[kRenderKey].is_object()) {
    return;
  }
  nlohmann::json &render = jsonSettings[kRenderKey];
  if (!render.is_object()) {
    render = nlohmann::json::object();
  }
  // Fail closed rather than overwriting a non-object versioned subtree.
  if (render.contains(kRenderV3Key) && !render[kRenderV3Key].is_object()) {
    return;
  }
  nlohmann::json &v3 = render[kRenderV3Key];
  if (!v3.is_object()) {
    v3 = nlohmann::json::object();
  }
  v3["enabled"] = config.v3Enabled;
  v3["shadowEnabled"] = config.shadowEnabled;
  v3["shadowMode"] = ToString(config.shadowMode);
  v3["maxShadowedLights"] = config.maxShadowedLights;
  v3["shadowAtlasSize"] = config.shadowAtlasSize;
  v3["shadowSoftness"] = config.shadowSoftness;
  v3["clusteredLightingEnabled"] = config.clusteredLightingEnabled;
  v3["clusteredLightingV4Enabled"] = config.clusteredLightingV4Enabled;
  v3["clusterTileSize"] = config.clusterTileSize;
  v3["clusterZSliceCount"] = config.clusterZSliceCount;
  v3["normalLightingEnabled"] = config.normalLightingEnabled;
  v3["specularEnabled"] = config.specularEnabled;
  v3["materialQualityLevel"] = config.materialQualityLevel;
  v3["heightShadowEnabled"] = config.heightShadowEnabled;
  v3["heightShadowSteps"] = config.heightShadowSteps;
  v3["selfShadowEnabled"] = config.selfShadowEnabled;
  v3["selfShadowSteps"] = config.selfShadowSteps;
  v3["pomEnabled"] = config.pomEnabled;
  v3["pomLayers"] = config.pomLayers;

  jsonSettings[kRenderV3FlatEnabledKey] = config.v3Enabled;
}

// Explicit versioned user-save entry point: read-modify-write of the render domain
// only. Every unrelated subtree and unknown key is preserved. A missing file is
// created with the safe default policy, but an existing file that cannot be
// opened or parsed, or whose root/render/versioned section is not an object, is left
// byte-for-byte untouched (fail closed, no exception, no truncation).
inline bool ReplaceFileAtomically(const std::filesystem::path &temporaryPath,
                           const std::filesystem::path &targetPath) {
#ifdef _WIN32
  return MoveFileExW(temporaryPath.wstring().c_str(), targetPath.wstring().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code error;
  std::filesystem::rename(temporaryPath, targetPath, error);
  return !error;
#endif
}

inline bool WriteJsonAtomically(const std::string &settingsPath,
                         const nlohmann::json &jsonSettings,
                         const char *domain) {
  const std::filesystem::path targetPath(settingsPath);
  const auto stamp = std::chrono::high_resolution_clock::now()
                         .time_since_epoch()
                         .count();
  std::filesystem::path temporaryPath = targetPath;
  temporaryPath += ".tmp." + std::to_string(stamp);

  try {
    {
      std::ofstream out(temporaryPath, std::ios::binary | std::ios::trunc);
      if (!out.is_open()) {
        LOG_WARN("QualityTierManager: failed to create temporary {} file for {}",
                 domain, settingsPath);
        return false;
      }
      out << jsonSettings.dump(4);
      out.flush();
      if (!out.good()) {
        LOG_WARN("QualityTierManager: temporary {} write to {} incomplete", domain,
                 settingsPath);
        return false;
      }
      out.close();
      if (out.fail()) {
        LOG_WARN("QualityTierManager: failed to close temporary {} file for {}",
                 domain, settingsPath);
        return false;
      }
    }

    if (!ReplaceFileAtomically(temporaryPath, targetPath)) {
      LOG_WARN("QualityTierManager: failed to atomically replace {} with {} data",
               settingsPath, domain);
      std::error_code removeError;
      std::filesystem::remove(temporaryPath, removeError);
      return false;
    }
    return true;
  } catch (...) {
    LOG_WARN("QualityTierManager: failed to persist {} into {}", domain,
             settingsPath);
    std::error_code removeError;
    std::filesystem::remove(temporaryPath, removeError);
    return false;
  }
}

inline bool WriteV3ConfigToFile(const std::string &settingsPath,
                         const RenderConfig &config) {
  if (settingsPath.empty()) {
    return true;
  }

  nlohmann::json jsonSettings = nlohmann::json::object();
  if (std::filesystem::exists(settingsPath)) {
    try {
      std::ifstream file(settingsPath);
      if (!file.is_open()) {
        LOG_WARN("QualityTierManager: failed to open {}, versioned save skipped",
                 settingsPath);
        return false;
      }
      file >> jsonSettings;
    } catch (...) {
      LOG_WARN("QualityTierManager: failed to parse {}, versioned save skipped",
               settingsPath);
      return false;
    }

    if (!jsonSettings.is_object()) {
       LOG_WARN("QualityTierManager: {} has non-object root, versioned save skipped",
               settingsPath);
      return false;
    }
    if (jsonSettings.contains(kRenderKey) &&
        !jsonSettings[kRenderKey].is_object()) {
       LOG_WARN("QualityTierManager: {} has non-object render, versioned save skipped",
               settingsPath);
      return false;
    }
    if (jsonSettings.contains(kRenderKey) &&
        jsonSettings[kRenderKey].contains(kRenderV3Key) &&
        !jsonSettings[kRenderKey][kRenderV3Key].is_object()) {
      LOG_WARN(
           "QualityTierManager: {} has non-object versioned section, save skipped",
           settingsPath);
      return false;
    }
  }

  WriteV3ConfigToJson(jsonSettings, config);

  return WriteJsonAtomically(settingsPath, jsonSettings, "versioned render");
}

inline bool ParseTierString(std::string value, QualityTier &outTier) {
  if (const auto parsed = FromStringQualityTier(value)) {
    outTier = *parsed;
    return true;
  }
  return false;
}

inline QualityTier MinTier(QualityTier lhs, QualityTier rhs) {
  return (static_cast<int>(lhs) < static_cast<int>(rhs)) ? lhs : rhs;
}

inline void ApplyTierShadowPolicy(RenderConfig &config, QualityTier tier) {
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

inline const char *ToString(QualityTierManager::TierSelectionSource source) {
  switch (source) {
  case QualityTierManager::TierSelectionSource::CapabilityAndBenchmark:
    return "capability+benchmark";
  case QualityTierManager::TierSelectionSource::SettingsOverride:
    return "settings_override";
  }
  return "unknown";
}

inline std::string MakeUtcTimestamp() {
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

} // namespace NoMoreDay::render::core::detail

