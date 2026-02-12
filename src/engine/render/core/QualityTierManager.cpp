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
  m_tier = tier;
  m_fromSettings = false;
  m_initialized = true;
  UpdateConfigForTier(tier);
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
  switch (tier) {
  case QualityTier::Low:
    m_config.bloomEnabled = false;
    m_config.dynamicLightingEnabled = false;
    m_config.maxParticles = 30000;
    m_config.shadowResolution = 0;
    break;
  case QualityTier::Medium:
    m_config.bloomEnabled = false;
    m_config.dynamicLightingEnabled = true;
    m_config.maxParticles = 60000;
    m_config.shadowResolution = 512;
    break;
  case QualityTier::High:
    m_config.bloomEnabled = true;
    m_config.dynamicLightingEnabled = true;
    m_config.maxParticles = 120000;
    m_config.shadowResolution = 1024;
    break;
  case QualityTier::Ultra:
    m_config.bloomEnabled = true;
    m_config.dynamicLightingEnabled = true;
    m_config.maxParticles = 200000;
    m_config.shadowResolution = 2048;
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
