#include "engine/render/core/QualityTierManagerInternal.hpp"

namespace NoMoreDay::render::core {
using namespace NoMoreDay::render::core::detail;

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
    if (m_config.maxLights > 1024) {
      m_config.maxLights = 1024;
    } else if (m_config.maxLights > 256) {
      m_config.maxLights = 256;
    } else {
      m_config.maxLights = std::max(4, m_config.maxLights / 2);
    }
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
    m_config.selfShadowEnabled = false;
    m_config.selfShadowSteps = 0;
    m_config.pomEnabled = false;
    m_config.pomLayers = 0;
  }

  // V4: HeightShadow quality chain 64 -> 16 -> Off.
  if (m_config.heightShadowEnabled) {
    if (level >= static_cast<int>(AutoDegradeStep::ReduceClusteredPressure)) {
      m_config.heightShadowSteps = std::min<uint32_t>(m_config.heightShadowSteps, 16u);
    }
    if (level >= static_cast<int>(AutoDegradeStep::HybridShadowToSDF)) {
      m_config.heightShadowEnabled = false;
      m_config.heightShadowSteps = 0;
    }
  }

  // V5: progressively reduce GI quality under pressure, then disable.
  if (m_config.giEnabled) {
    if (level >= static_cast<int>(AutoDegradeStep::ReduceClusteredPressure)) {
      m_config.giHalfResolution = true;
      m_config.giCascadeLevels = std::min<uint32_t>(m_config.giCascadeLevels, 4u);
      m_config.giSdfUpdateInterval = std::max<uint32_t>(m_config.giSdfUpdateInterval, 2u);
    }
    if (level >= static_cast<int>(AutoDegradeStep::HybridShadowToSDF)) {
      m_config.giSdfUpdateInterval = std::max<uint32_t>(m_config.giSdfUpdateInterval, 4u);
    }
    if (level >= static_cast<int>(AutoDegradeStep::DisableHighMaterialBranch)) {
      m_config.giEnabled = false;
      m_config.giCascadeLevels = 0;
      m_config.giIntensity = 0.0f;
    }
  }

  if (level >= static_cast<int>(AutoDegradeStep::DisableHighMaterialBranch)) {
    m_config.fluidEnabled = false;
    m_config.fluidMaxParticles = 0;
  }

  // Runtime override is the top of the GI priority contract and is layered
  // last so it beats the settings.json override and the tier/degrade default.
  ApplyGiRuntimeOverrideToConfig(m_config);
}


} // namespace NoMoreDay::render::core
