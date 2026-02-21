#include "doctest.h"

#include "engine/render/core/QualityTierManager.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

using namespace NoMoreDay;

namespace {

std::filesystem::path MakeTempSettingsPath(const std::string &name) {
  const std::filesystem::path dir = std::filesystem::path("bin") / "tmp_quality_tier";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / name;
}

void WriteJson(const std::filesystem::path &path, const nlohmann::json &json) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out << json.dump(2);
}

nlohmann::json ReadJson(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.is_open());
  nlohmann::json parsed = nlohmann::json::object();
  in >> parsed;
  return parsed;
}

float HysteresisRatio(
    const render::core::QualityTierManager::AutoDegradeBudgetThresholds &thresholds) {
  if (thresholds.degradeTriggerMs <= 0.0f) {
    return 0.0f;
  }
  return (thresholds.degradeTriggerMs - thresholds.recoverTriggerMs) /
         thresholds.degradeTriggerMs;
}

} // namespace

TEST_CASE("[Unit] QualityTierManager - Render V3 Config Roundtrip") {
  const auto settingsPath = MakeTempSettingsPath("render_v3_roundtrip.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"render",
              {{"v3",
                {{"enabled", true},
                 {"shadowEnabled", true},
                 {"shadowMode", "hybrid"},
                 {"maxShadowedLights", 7},
                 {"shadowAtlasSize", 4096},
                 {"shadowSoftness", 1.75f},
                 {"clusteredLightingEnabled", true},
                 {"clusterTileSize", 48},
                 {"clusterZSliceCount", 6},
                 {"normalLightingEnabled", true},
                 {"specularEnabled", true},
                 {"materialQualityLevel", 2}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  const auto &cfg = manager.GetConfig();
  CHECK(cfg.v3Enabled == true);
  CHECK(cfg.shadowEnabled == true);
  CHECK(cfg.shadowMode == render::core::ShadowMode::SDF);
  CHECK(cfg.maxShadowedLights == 7);
  CHECK(cfg.shadowAtlasSize == 4096);
  CHECK(cfg.shadowSoftness == doctest::Approx(1.75f));
  CHECK(cfg.clusteredLightingEnabled == true);
  CHECK(cfg.clusterTileSize == 48);
  CHECK(cfg.clusterZSliceCount == 6);
  CHECK(cfg.normalLightingEnabled == true);
  CHECK(cfg.specularEnabled == true);
  CHECK(cfg.materialQualityLevel == 2);

  const nlohmann::json saved = ReadJson(settingsPath);
  REQUIRE(saved.contains("render"));
  REQUIRE(saved["render"].contains("v3"));
  REQUIRE(saved["render"]["v3"].contains("enabled"));
  CHECK(saved["render"]["v3"]["enabled"].get<bool>() == true);
  CHECK(saved["render"]["v3"]["shadowMode"].get<std::string>() == "hybrid");
  REQUIRE(saved.contains("render.v3.enabled"));
  CHECK(saved["render.v3.enabled"].get<bool>() == true);

  manager.Initialize(settingsPath.string(), true);
  const auto &reloaded = manager.GetConfig();
  CHECK(reloaded.v3Enabled == true);
  CHECK(reloaded.shadowMode == render::core::ShadowMode::SDF);
}

TEST_CASE("[Unit] QualityTierManager - Render V3 Missing Fields Use Defaults") {
  const auto settingsPath = MakeTempSettingsPath("render_v3_missing_fields.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "Medium"},
             {"render",
              {{"v3",
                {
                    {"enabled", true},
                }}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  const auto &cfg = manager.GetConfig();
  CHECK(cfg.v3Enabled == true);
  CHECK(cfg.shadowEnabled == false);
  CHECK(cfg.shadowMode == render::core::ShadowMode::Off);
  CHECK(cfg.maxShadowedLights == 4);
  CHECK(cfg.shadowAtlasSize == 2048);
  CHECK(cfg.shadowSoftness == doctest::Approx(1.0f));
  CHECK(cfg.clusteredLightingEnabled == false);
  CHECK(cfg.clusterTileSize == 32);
  CHECK(cfg.clusterZSliceCount == 8);
  CHECK(cfg.normalLightingEnabled == false);
  CHECK(cfg.specularEnabled == false);
  CHECK(cfg.materialQualityLevel == 0);
}

TEST_CASE("[Unit] QualityTierManager - Render V3 Invalid Values Rejected") {
  const auto settingsPath = MakeTempSettingsPath("render_v3_invalid_values.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "Medium"},
             {"render.v3.enabled", "not_bool"},
             {"render",
              {{"v3",
                {{"enabled", "bad"},
                 {"shadowEnabled", 1},
                 {"shadowMode", "invalid"},
                 {"maxShadowedLights", -1},
                 {"shadowAtlasSize", "big"},
                 {"shadowSoftness", -0.5f},
                 {"clusteredLightingEnabled", "bad"},
                 {"clusterTileSize", -32},
                 {"clusterZSliceCount", "NaN"},
                 {"normalLightingEnabled", 42},
                 {"specularEnabled", 99},
                 {"materialQualityLevel", -3}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  const auto &cfg = manager.GetConfig();
  CHECK(cfg.v3Enabled == false);
  CHECK(cfg.shadowEnabled == false);
  CHECK(cfg.shadowMode == render::core::ShadowMode::Off);
  CHECK(cfg.maxShadowedLights == 4);
  CHECK(cfg.shadowAtlasSize == 2048);
  CHECK(cfg.shadowSoftness == doctest::Approx(1.0f));
  CHECK(cfg.clusteredLightingEnabled == false);
  CHECK(cfg.clusterTileSize == 32);
  CHECK(cfg.clusterZSliceCount == 8);
  CHECK(cfg.normalLightingEnabled == false);
  CHECK(cfg.specularEnabled == false);
  CHECK(cfg.materialQualityLevel == 0);
}

TEST_CASE("[Unit] QualityTierManager - GPUText Tier Matrix Policy") {
  const auto settingsPath = MakeTempSettingsPath("gpu_text_tier_matrix.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"render", {{"v3", {{"enabled", false}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  manager.ForceTier(render::core::QualityTier::Low);
  CHECK(manager.GetConfig().gpuTextEnabled == false);
  CHECK(manager.GetConfig().gpuTextAdvancedAnimation == false);

  manager.ForceTier(render::core::QualityTier::Medium);
  CHECK(manager.GetConfig().gpuTextEnabled == true);
  CHECK(manager.GetConfig().gpuTextAdvancedAnimation == false);

  manager.ForceTier(render::core::QualityTier::High);
  CHECK(manager.GetConfig().gpuTextEnabled == true);
  CHECK(manager.GetConfig().gpuTextAdvancedAnimation == true);

  manager.ForceTier(render::core::QualityTier::Ultra);
  CHECK(manager.GetConfig().gpuTextEnabled == true);
  CHECK(manager.GetConfig().gpuTextAdvancedAnimation == true);
}

TEST_CASE("[Unit] QualityTierManager - GPUText Feature Flag Route Switch") {
  const auto settingsPath = MakeTempSettingsPath("gpu_text_feature_flag_switch.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"render", {{"gpuText", {{"enabled", true}}}, {"v3", {{"enabled", false}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().gpuTextEnabled == true);
  CHECK(manager.GetConfig().gpuTextAdvancedAnimation == true);

  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"render", {{"gpuText", {{"enabled", false}}}, {"v3", {{"enabled", false}}}}}});
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().gpuTextEnabled == false);
  CHECK(manager.GetConfig().gpuTextAdvancedAnimation == false);

  WriteJson(settingsPath,
            {{"renderQualityTier", "Medium"},
             {"render.gpuText.enabled", true},
             {"render", {{"v3", {{"enabled", false}}}}}});
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().gpuTextEnabled == true);
  CHECK(manager.GetConfig().gpuTextAdvancedAnimation == false);
}

TEST_CASE("[Unit] QualityTierManager - GPULoot Tier Matrix Policy") {
  const auto settingsPath = MakeTempSettingsPath("gpu_loot_tier_matrix.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"render", {{"v3", {{"enabled", false}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  manager.ForceTier(render::core::QualityTier::Low);
  CHECK(manager.GetConfig().gpuLootEnabled == false);
  CHECK(manager.GetConfig().gpuLootGlowEnabled == false);

  manager.ForceTier(render::core::QualityTier::Medium);
  CHECK(manager.GetConfig().gpuLootEnabled == false);
  CHECK(manager.GetConfig().gpuLootGlowEnabled == false);

  manager.ForceTier(render::core::QualityTier::High);
  CHECK(manager.GetConfig().gpuLootEnabled == true);
  CHECK(manager.GetConfig().gpuLootGlowEnabled == false);

  manager.ForceTier(render::core::QualityTier::Ultra);
  CHECK(manager.GetConfig().gpuLootEnabled == true);
  CHECK(manager.GetConfig().gpuLootGlowEnabled == true);
}

TEST_CASE("[Unit] QualityTierManager - GPULoot Feature Flag Route Switch") {
  const auto settingsPath = MakeTempSettingsPath("gpu_loot_feature_flag_switch.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"render", {{"gpuLoot", {{"enabled", true}}}, {"v3", {{"enabled", false}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().gpuLootEnabled == true);
  CHECK(manager.GetConfig().gpuLootGlowEnabled == false);

  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"render", {{"gpuLoot", {{"enabled", false}}}, {"v3", {{"enabled", false}}}}}});
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().gpuLootEnabled == false);
  CHECK(manager.GetConfig().gpuLootGlowEnabled == false);

  WriteJson(settingsPath,
            {{"renderQualityTier", "Ultra"},
             {"render.gpuLoot.enabled", true},
             {"render", {{"v3", {{"enabled", false}}}}}});
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().gpuLootEnabled == true);
  CHECK(manager.GetConfig().gpuLootGlowEnabled == true);
}

TEST_CASE("[Unit] QualityTierManager - GI Tier Matrix Policy") {
  const auto settingsPath = MakeTempSettingsPath("gi_tier_matrix.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"render", {{"v3", {{"enabled", false}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  manager.ForceTier(render::core::QualityTier::Low);
  CHECK(manager.GetConfig().giEnabled == false);
  CHECK(manager.GetConfig().giCascadeLevels == 0u);

  manager.ForceTier(render::core::QualityTier::Medium);
  CHECK(manager.GetConfig().giEnabled == false);
  CHECK(manager.GetConfig().giCascadeLevels == 0u);

  manager.ForceTier(render::core::QualityTier::High);
  CHECK(manager.GetConfig().giEnabled == true);
  CHECK(manager.GetConfig().giCascadeLevels == 4u);
  CHECK(manager.GetConfig().giHalfResolution == true);
  CHECK(manager.GetConfig().giSdfUpdateInterval == 2u);

  manager.ForceTier(render::core::QualityTier::Ultra);
  CHECK(manager.GetConfig().giEnabled == true);
  CHECK(manager.GetConfig().giCascadeLevels == 6u);
  CHECK(manager.GetConfig().giHalfResolution == false);
  CHECK(manager.GetConfig().giSdfUpdateInterval == 1u);
}

TEST_CASE("[Unit] QualityTierManager - Shadow Tier Policy Linkage") {
  const auto settingsPath = MakeTempSettingsPath("render_v3_tier_shadow_policy.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"render",
              {{"v3",
                {{"enabled", true},
                 {"shadowEnabled", false},
                 {"shadowMode", "hybrid"},
                 {"maxShadowedLights", 3},
                 {"shadowAtlasSize", 1024}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  manager.ForceTier(render::core::QualityTier::Medium);
  CHECK(manager.GetConfig().shadowEnabled == false);
  CHECK(manager.GetConfig().shadowMode == render::core::ShadowMode::Off);

  manager.ForceTier(render::core::QualityTier::High);
  CHECK(manager.GetConfig().shadowEnabled == true);
  CHECK(manager.GetConfig().shadowMode == render::core::ShadowMode::SDF);
  CHECK(manager.GetConfig().maxShadowedLights >= 4u);

  manager.ForceTier(render::core::QualityTier::Ultra);
  CHECK(manager.GetConfig().shadowEnabled == true);
  CHECK(manager.GetConfig().shadowMode == render::core::ShadowMode::Hybrid);
  CHECK(manager.GetConfig().maxShadowedLights >= 8u);
  CHECK(manager.GetConfig().shadowAtlasSize >= 2048u);
}

TEST_CASE("[Unit] QualityTierManager - Settings Override Precedence") {
  const auto settingsPath = MakeTempSettingsPath("override_precedence.json");
  WriteJson(settingsPath, {{"renderQualityTier", "Low"}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  CHECK(manager.GetTier() == render::core::QualityTier::Low);
  CHECK(manager.IsTierOverriddenBySettings() == true);
  CHECK(manager.GetSelectionMetadata().source ==
        render::core::QualityTierManager::TierSelectionSource::SettingsOverride);
}

TEST_CASE("[Unit] QualityTierManager - Detection Metadata Persisted") {
  const auto settingsPath = MakeTempSettingsPath("metadata_persist.json");
  WriteJson(settingsPath, nlohmann::json::object());

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  const nlohmann::json saved = ReadJson(settingsPath);
  REQUIRE(saved.contains("renderQualityAutoDetect"));
  const auto &meta = saved["renderQualityAutoDetect"];
  CHECK(meta["version"].get<int>() == 1);
  CHECK(meta.contains("capability"));
  CHECK(meta.contains("source"));
  CHECK(meta.contains("selectedTier"));
}

TEST_CASE("[Unit] QualityTierManager - AutoDegrade Threshold Contract") {
  using Tier = render::core::QualityTier;
  constexpr Tier kTiers[4] = {Tier::Low, Tier::Medium, Tier::High, Tier::Ultra};

  for (const Tier tier : kTiers) {
    const auto thresholds =
        render::core::QualityTierManager::GetAutoDegradeBudgetThresholds(tier);
    CHECK(thresholds.degradeTriggerMs > thresholds.recoverTriggerMs);
    CHECK(HysteresisRatio(thresholds) >= 0.20f);
    CHECK(thresholds.cooldownSeconds >= 3.0f);
    CHECK(thresholds.sustainSeconds >= 3.0f);
  }
}

TEST_CASE("[Unit] QualityTierManager - V4 Budget Constants Contract") {
  CHECK(render::core::kBudgetLightCulling_Normal == doctest::Approx(0.15f));
  CHECK(render::core::kBudgetLightCulling_High == doctest::Approx(0.30f));
  CHECK(render::core::kBudgetLightCulling_Extreme == doctest::Approx(0.60f));
  CHECK(render::core::kBudgetShadow_Normal == doctest::Approx(0.40f));
  CHECK(render::core::kBudgetShadow_High == doctest::Approx(0.90f));
  CHECK(render::core::kBudgetShadow_Extreme == doctest::Approx(1.30f));
  CHECK(render::core::kBudgetLighting_Normal == doctest::Approx(0.60f));
  CHECK(render::core::kBudgetLighting_High == doctest::Approx(1.00f));
  CHECK(render::core::kBudgetLighting_Extreme == doctest::Approx(1.40f));
  CHECK(render::core::kBudgetHeightShadow_Normal == doctest::Approx(0.30f));
  CHECK(render::core::kBudgetHeightShadow_High == doctest::Approx(0.60f));
  CHECK(render::core::kBudgetHeightShadow_Extreme == doctest::Approx(0.90f));
}

TEST_CASE("[Unit] QualityTierManager - V3 Capability Matrix Contract") {
  using Tier = render::core::QualityTier;
  using FeatureLevel = render::core::QualityTierManager::V3FeatureLevel;

  const auto low = render::core::QualityTierManager::GetV3CapabilityMatrix(Tier::Low);
  CHECK(low.shadowMode == render::core::ShadowMode::Off);
  CHECK(low.clusteredLighting == FeatureLevel::Off);
  CHECK(low.materialHighBranch == FeatureLevel::Off);
  CHECK(low.volumetricQuality == FeatureLevel::Off);
  CHECK(low.distortion == FeatureLevel::Off);

  const auto medium =
      render::core::QualityTierManager::GetV3CapabilityMatrix(Tier::Medium);
  CHECK(medium.shadowMode == render::core::ShadowMode::Off);
  CHECK(medium.clusteredLighting == FeatureLevel::Optional);
  CHECK(medium.materialHighBranch == FeatureLevel::Off);
  CHECK(medium.volumetricQuality == FeatureLevel::Basic);
  CHECK(medium.distortion == FeatureLevel::Basic);

  const auto high = render::core::QualityTierManager::GetV3CapabilityMatrix(Tier::High);
  CHECK(high.shadowMode == render::core::ShadowMode::SDF);
  CHECK(high.clusteredLighting == FeatureLevel::On);
  CHECK(high.materialHighBranch == FeatureLevel::Partial);
  CHECK(high.volumetricQuality == FeatureLevel::On);
  CHECK(high.distortion == FeatureLevel::On);

  const auto ultra =
      render::core::QualityTierManager::GetV3CapabilityMatrix(Tier::Ultra);
  CHECK(ultra.shadowMode == render::core::ShadowMode::Hybrid);
  CHECK(ultra.clusteredLighting == FeatureLevel::On);
  CHECK(ultra.materialHighBranch == FeatureLevel::Full);
  CHECK(ultra.volumetricQuality == FeatureLevel::Full);
  CHECK(ultra.distortion == FeatureLevel::On);
}

TEST_CASE("[Unit] QualityTierManager - V3 AutoDegrade Sequence Contract") {
  using Step = render::core::QualityTierManager::AutoDegradeStep;

  const auto &sequence = render::core::QualityTierManager::GetV3AutoDegradeSequence();
  CHECK(sequence.size() == 6);
  CHECK(sequence[0] == Step::ReduceBloom);
  CHECK(sequence[1] == Step::DisableDistortion);
  CHECK(sequence[2] == Step::LimitDynamicLights);
  CHECK(sequence[3] == Step::ReduceClusteredPressure);
  CHECK(sequence[4] == Step::HybridShadowToSDF);
  CHECK(sequence[5] == Step::DisableHighMaterialBranch);
}

TEST_CASE("[Unit] QualityTierManager - Legacy Metadata Version Migration") {
  const auto settingsPath = MakeTempSettingsPath("metadata_migration.json");
  WriteJson(settingsPath,
            {{"renderQualityAutoDetect",
              {{"version", 0}, {"selectedTier", "Low"}, {"reason", "legacy"}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  const nlohmann::json saved = ReadJson(settingsPath);
  REQUIRE(saved.contains("renderQualityAutoDetect"));
  const auto &meta = saved["renderQualityAutoDetect"];
  CHECK(meta["version"].get<int>() == 1);
  CHECK(meta.contains("source"));
  CHECK(meta.contains("capability"));
  CHECK(meta.contains("updatedAtUtc"));
}

TEST_CASE("[Unit] QualityTierManager - AutoDegrade Sequence And Recovery") {
  const auto settingsPath = MakeTempSettingsPath("v3_autodegrade_sequence.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "Ultra"},
             {"render",
              {{"v3",
                {{"enabled", true},
                 {"shadowEnabled", true},
                 {"shadowMode", "hybrid"},
                 {"maxShadowedLights", 8},
                 {"shadowAtlasSize", 4096},
                 {"shadowSoftness", 1.0f},
                 {"clusteredLightingEnabled", true},
                 {"clusterTileSize", 32},
                 {"clusterZSliceCount", 6},
                 {"normalLightingEnabled", true},
                 {"specularEnabled", true},
                 {"materialQualityLevel", 2}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetAutoDegradeLevel() == 0);
  CHECK(manager.GetConfig().shadowMode == render::core::ShadowMode::Hybrid);
  CHECK(manager.GetConfig().clusterTileSize == 32);
  CHECK(manager.GetConfig().clusterZSliceCount == 6);
  CHECK(manager.GetConfig().normalLightingEnabled == true);
  CHECK(manager.GetConfig().specularEnabled == true);
  CHECK(manager.GetConfig().materialQualityLevel == 2);
  CHECK(manager.GetConfig().distortionEnabled == true);
  CHECK(manager.GetConfig().giEnabled == true);
  CHECK(manager.GetConfig().giCascadeLevels == 6u);
  CHECK(manager.GetConfig().giHalfResolution == false);
  CHECK(manager.GetConfig().giSdfUpdateInterval == 1u);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 1);
  CHECK(manager.GetConfig().bloomMipLevels == 5);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 2);
  CHECK(manager.GetConfig().distortionEnabled == false);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 3);
  CHECK(manager.GetConfig().maxLights == 1024);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 4);
  CHECK(manager.GetConfig().clusterTileSize == 64);
  CHECK(manager.GetConfig().clusterZSliceCount == 2);
  CHECK(manager.GetConfig().giEnabled == true);
  CHECK(manager.GetConfig().giHalfResolution == true);
  CHECK(manager.GetConfig().giCascadeLevels == 4u);
  CHECK(manager.GetConfig().giSdfUpdateInterval >= 2u);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 5);
  CHECK(manager.GetConfig().shadowMode == render::core::ShadowMode::SDF);
  CHECK(manager.GetConfig().giEnabled == true);
  CHECK(manager.GetConfig().giSdfUpdateInterval >= 4u);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 6);
  CHECK(manager.GetConfig().normalLightingEnabled == false);
  CHECK(manager.GetConfig().specularEnabled == false);
  CHECK(manager.GetConfig().materialQualityLevel == 0);
  CHECK(manager.GetConfig().giEnabled == false);
  CHECK(manager.GetConfig().giCascadeLevels == 0u);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == false);
  CHECK(manager.GetAutoDegradeLevel() == 6);

  CHECK(manager.DecreaseAutoDegradeLevel("unit_test", 8.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 5);
  CHECK(manager.GetConfig().shadowMode == render::core::ShadowMode::SDF);
  CHECK(manager.GetConfig().normalLightingEnabled == true);
  CHECK(manager.GetConfig().specularEnabled == true);
  CHECK(manager.GetConfig().materialQualityLevel == 2);
  CHECK(manager.GetConfig().giEnabled == true);
  CHECK(manager.GetConfig().giCascadeLevels == 4u);
  CHECK(manager.GetConfig().giHalfResolution == true);
  CHECK(manager.GetConfig().giSdfUpdateInterval >= 4u);

  manager.ResetAutoDegrade("unit_test");
  CHECK(manager.GetAutoDegradeLevel() == 0);
  CHECK(manager.GetConfig().shadowMode == render::core::ShadowMode::Hybrid);
  CHECK(manager.GetConfig().clusterTileSize == 32);
  CHECK(manager.GetConfig().clusterZSliceCount == 6);
  CHECK(manager.GetConfig().distortionEnabled == true);
  CHECK(manager.GetConfig().normalLightingEnabled == true);
  CHECK(manager.GetConfig().specularEnabled == true);
  CHECK(manager.GetConfig().materialQualityLevel == 2);
  CHECK(manager.GetConfig().giEnabled == true);
  CHECK(manager.GetConfig().giCascadeLevels == 6u);
  CHECK(manager.GetConfig().giHalfResolution == false);
  CHECK(manager.GetConfig().giSdfUpdateInterval == 1u);
}
