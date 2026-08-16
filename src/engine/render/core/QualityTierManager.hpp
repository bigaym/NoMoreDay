#pragma once

#include "engine/render/core/RenderConstants.hpp"
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace NoMoreDay::render::core {

class QualityTierManager {
public:
  using V3ToggleCallback = std::function<void(bool enabled)>;

  /// RAII guard that applies a runtime GI override and restores the prior
  /// override state on destruction (covers normal exit and exception unwind).
  /// The runtime setter has the highest priority in the GI priority contract:
  /// runtime override > settings.json override > tier/degrade default.
  class GiEnabledOverrideGuard {
  public:
    explicit GiEnabledOverrideGuard(bool enabled);
    ~GiEnabledOverrideGuard();
    GiEnabledOverrideGuard(const GiEnabledOverrideGuard &) = delete;
    GiEnabledOverrideGuard &operator=(const GiEnabledOverrideGuard &) = delete;
    bool IsOwned() const { return m_owned; }

  private:
    QualityTierManager &m_manager;
    bool m_wasActive = false;
    bool m_priorValue = false;
    bool m_owned = false;
  };

  struct AutoDegradeBudgetThresholds {
    float degradeTriggerMs = 0.0f;
    float recoverTriggerMs = 0.0f;
    float sustainSeconds = 3.0f;
    float cooldownSeconds = 3.0f;
  };

  enum class V3FeatureLevel : uint8_t {
    Off = 0,
    Basic = 1,
    Optional = 2,
    On = 3,
    Partial = 4,
    Full = 5,
  };

  enum class AutoDegradeStep : uint8_t {
    ReduceBloom = 1,
    DisableDistortion = 2,
    LimitDynamicLights = 3,
    ReduceClusteredPressure = 4,
    HybridShadowToSDF = 5,
    DisableHighMaterialBranch = 6,
  };

  struct V3CapabilityMatrixEntry {
    ShadowMode shadowMode = ShadowMode::Off;
    V3FeatureLevel clusteredLighting = V3FeatureLevel::Off;
    V3FeatureLevel materialHighBranch = V3FeatureLevel::Off;
    V3FeatureLevel volumetricQuality = V3FeatureLevel::Off;
    V3FeatureLevel distortion = V3FeatureLevel::Off;
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
  static const std::array<AutoDegradeStep, 6> &GetV3AutoDegradeSequence();
  static V3CapabilityMatrixEntry GetV3CapabilityMatrix(QualityTier tier);

  void Initialize(const std::string &settingsPath = "settings.json",
                  bool forceRedetect = false);

  QualityTier GetTier() const { return m_tier; }
  const RenderConfig &GetConfig() const { return m_config; }
  const AdaptiveQualitySettings &GetAdaptiveQualitySettings() const {
    return m_config.adaptiveQuality;
  }
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
  /// Applies a runtime GI enabled override. This beats the settings.json
  /// override (render.gi.enabled) and the tier/degrade default. While an
  /// override is active, only the owning thread may modify or clear it.
  /// Returns false when rejected (active override owned by another thread).
  bool SetGiEnabledOverride(bool enabled);
  /// Clears the runtime GI override, restoring the settings/tier chain.
  /// Returns false when rejected (active override owned by another thread).
  bool ClearGiEnabledOverride();
  bool IsGiOverrideActive() const { return m_giOverrideActive; }
  std::optional<bool> GetGiRuntimeOverride() const {
    return m_giRuntimeOverride;
  }
  /// Effective GI override after the priority contract:
  /// runtime override > settings.json override > none.
  std::optional<bool> EffectiveGiEnabled() const;
  bool IsLinearPipelineEnabled() const { return m_config.linearPipeline; }
  bool SetLinearPipelineEnabled(
      bool enabled, const std::string &settingsPath = "settings.json");
  bool SetV3Enabled(bool enabled,
                    const std::string &settingsPath = "settings.json");
  bool SetClusteredLightingEnabled(
      bool enabled, const std::string &settingsPath = "settings.json");
  bool SetNormalLightingEnabled(bool enabled,
                                const std::string &settingsPath = "settings.json");
  bool SetSpecularEnabled(bool enabled,
                          const std::string &settingsPath = "settings.json");
  void SetV3ToggleCallback(V3ToggleCallback callback);
  bool IncreaseAutoDegradeLevel(std::string_view reasonCode,
                                float observedFrameMs, float budgetMs);
  bool DecreaseAutoDegradeLevel(std::string_view reasonCode,
                                float observedFrameMs, float budgetMs);
  void ResetAutoDegrade(std::string_view reasonCode);

private:
  static constexpr int kSelectionMetadataVersion = 1;
  static constexpr int kAutoDegradeMaxLevel = 6;

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
  bool TryLoadV3ConfigFromSettings(const std::string &settingsPath,
                                   RenderConfig &outConfig) const;
  bool TryLoadAdaptiveQualityConfigFromSettings(
      const std::string &settingsPath, AdaptiveQualitySettings &outSettings) const;
  std::optional<bool>
  TryLoadGpuTextEnabledOverride(const std::string &settingsPath) const;
  std::optional<bool>
  TryLoadGpuLootEnabledOverride(const std::string &settingsPath) const;
  std::optional<bool>
  TryLoadGiEnabledOverride(const std::string &settingsPath) const;
  std::optional<bool>
  TryLoadFluidEnabledOverride(const std::string &settingsPath) const;
  std::optional<bool>
  TryLoadLinearPipelineEnabledOverride(const std::string &settingsPath) const;
  void ApplyV3ConfigOverrides(RenderConfig &config) const;
  void ApplyGiRuntimeOverrideToConfig(RenderConfig &config) const;
  void SetGiOverrideInternal(std::optional<bool> value);
  void ReapplyConfigAfterGiOverride();
  std::string QueryRendererString() const;

  bool m_initialized = false;
  bool m_fromSettings = false;
  QualityTier m_tier = QualityTier::Medium;
  int m_autoDegradeLevel = 0;
  RenderConfig m_baseConfig = {};
  RenderConfig m_config = {};
  RenderConfig m_v3Config = {};
  AdaptiveQualitySettings m_adaptiveQualitySettings = {};
  std::optional<bool> m_gpuTextEnabledOverride = std::nullopt;
  std::optional<bool> m_gpuLootEnabledOverride = std::nullopt;
  std::optional<bool> m_giEnabledOverride = std::nullopt;
  std::optional<bool> m_fluidEnabledOverride = std::nullopt;
  std::optional<bool> m_linearPipelineEnabledOverride = std::nullopt;
  std::optional<bool> m_giRuntimeOverride = std::nullopt;
  bool m_giOverrideActive = false;
  std::thread::id m_giOverrideThread = {};
  V3ToggleCallback m_v3ToggleCallback = {};
  std::string m_rendererString;
  CapabilitySnapshot m_capabilitySnapshot = {};
  TierSelectionMetadata m_selectionMetadata = {};
};

} // namespace NoMoreDay::render::core
