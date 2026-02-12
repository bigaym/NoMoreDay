#pragma once

#include "engine/render/core/RenderConstants.hpp"
#include <string>
#include <string_view>

namespace NoMoreDay::render::core {

class QualityTierManager {
public:
  static QualityTierManager &Get();

  void Initialize(const std::string &settingsPath = "settings.json",
                  bool forceRedetect = false);

  QualityTier GetTier() const { return m_tier; }
  const RenderConfig &GetConfig() const { return m_config; }
  const std::string &GetRendererString() const { return m_rendererString; }
  bool IsInitialized() const { return m_initialized; }
  bool IsTierOverriddenBySettings() const { return m_fromSettings; }

  void ForceTier(QualityTier tier);

private:
  QualityTierManager() = default;

  bool TryLoadTierFromSettings(const std::string &settingsPath,
                               QualityTier &outTier) const;
  QualityTier DetectTierFromRenderer(std::string_view renderer) const;
  void UpdateConfigForTier(QualityTier tier);
  std::string QueryRendererString() const;

  bool m_initialized = false;
  bool m_fromSettings = false;
  QualityTier m_tier = QualityTier::Medium;
  RenderConfig m_config = {};
  std::string m_rendererString;
};

} // namespace NoMoreDay::render::core
