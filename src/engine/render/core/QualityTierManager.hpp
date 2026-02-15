#pragma once

#include "engine/render/core/RenderConstants.hpp"
#include <array>
#include <string>
#include <string_view>

namespace NoMoreDay::render::core {

class QualityTierManager {
public:
  struct AutoDegradeBudgetThresholds {
    float degradeTriggerMs = 0.0f;
    float recoverTriggerMs = 0.0f;
    float sustainSeconds = 3.0f;
    float cooldownSeconds = 3.0f;
  };

  struct CapabilitySnapshot {
    int maxShaderStorageBufferBindings = 0;
    int maxComputeWorkGroupInvocations = 0;
    std::array<int, 3> maxComputeWorkGroupSize = {0, 0, 0};
    int maxTextureSize = 0;
    int maxArrayTextureLayers = 0;
    int maxImageUnits = 0;
    bool valid = false;
  };

  enum class TierSelectionSource : uint8_t {
    CapabilityAndBenchmark = 0,
    SettingsOverride = 1,
  };

  struct TierSelectionMetadata {
    int version = 1;
    TierSelectionSource source = TierSelectionSource::CapabilityAndBenchmark;
    QualityTier capabilityTier = QualityTier::Medium;
    QualityTier benchmarkTier = QualityTier::Medium;
    QualityTier selectedTier = QualityTier::Medium;
    float benchmarkScore = 0.0f;
    std::string reasonCode;
  };

  static QualityTierManager &Get();
  static AutoDegradeBudgetThresholds
  GetAutoDegradeBudgetThresholds(QualityTier tier);

  void Initialize(const std::string &settingsPath = "settings.json",
                  bool forceRedetect = false);

  QualityTier GetTier() const { return m_tier; }
  const RenderConfig &GetConfig() const { return m_config; }
  const std::string &GetRendererString() const { return m_rendererString; }
  bool IsInitialized() const { return m_initialized; }
  bool IsTierOverriddenBySettings() const { return m_fromSettings; }
  const CapabilitySnapshot &GetCapabilitySnapshot() const {
    return m_capabilitySnapshot;
  }
  const TierSelectionMetadata &GetSelectionMetadata() const {
    return m_selectionMetadata;
  }
  int GetAutoDegradeLevel() const { return m_autoDegradeLevel; }

  void ForceTier(QualityTier tier);
  bool IncreaseAutoDegradeLevel(std::string_view reasonCode,
                                float observedFrameMs, float budgetMs);
  bool DecreaseAutoDegradeLevel(std::string_view reasonCode,
                                float observedFrameMs, float budgetMs);
  void ResetAutoDegrade(std::string_view reasonCode);

private:
  static constexpr int kSelectionMetadataVersion = 1;
  static constexpr int kAutoDegradeMaxLevel = 5;

  QualityTierManager() = default;

  bool TryLoadTierFromSettings(const std::string &settingsPath,
                               QualityTier &outTier) const;
  void PersistSelectionMetadata(const std::string &settingsPath) const;
  CapabilitySnapshot ProbeCapabilities() const;
  QualityTier DetectTierFromCapabilities(
      const CapabilitySnapshot &snapshot) const;
  QualityTier RunBaselineBenchmark(const CapabilitySnapshot &snapshot,
                                   float &outScore) const;
  QualityTier DetectTierFromRenderer(std::string_view renderer) const;
  void UpdateConfigForTier(QualityTier tier);
  void ApplyAutoDegradeLevel();
  std::string QueryRendererString() const;

  bool m_initialized = false;
  bool m_fromSettings = false;
  QualityTier m_tier = QualityTier::Medium;
  int m_autoDegradeLevel = 0;
  RenderConfig m_baseConfig = {};
  RenderConfig m_config = {};
  std::string m_rendererString;
  CapabilitySnapshot m_capabilitySnapshot = {};
  TierSelectionMetadata m_selectionMetadata = {};
};

} // namespace NoMoreDay::render::core
