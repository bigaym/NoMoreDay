#include "doctest.h"

#include "engine/render/core/QualityTierManager.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
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

// Reads the file as raw bytes so a test can prove a save was skipped entirely
// (byte-identical output) instead of merely re-serialized to equal JSON.
std::string ReadRaw(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.is_open());
  std::ostringstream contents;
  contents << in.rdbuf();
  return contents.str();
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
  // S1b repair: Initialize's metadata refresh no longer writes the V3 domain
  // back into the settings file, so it must not synthesize the flat
  // render.v3.enabled key either. The user's nested v3 object is preserved.
  CHECK_FALSE(saved.contains("render.v3.enabled"));
  CHECK(saved.contains("renderQualityAutoDetect"));

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

TEST_CASE("[Unit] QualityTierManager - Adaptive Quality Config Roundtrip") {
  const auto settingsPath = MakeTempSettingsPath("adaptive_quality_roundtrip.json");
  WriteJson(settingsPath,
            { {"renderQualityTier", "High"},
              {"render",
               { {"adaptiveQuality",
                  { {"dynamicResolutionEnabled", true},
                    {"renderScaleLocked", false},
                    {"renderScale", 0.85f},
                    {"minRenderScale", 0.7f},
                    {"maxRenderScale", 1.0f},
                    {"renderScaleStep", 0.05f},
                    {"downThresholdMs", 14.0f},
                    {"upThresholdMs", 10.0f},
                    {"sustainSeconds", 1.25f},
                    {"cooldownSeconds", 20.0f},
                    {"autoExposureEnabled", false},
                    {"exposure", 1.25f},
                    {"minExposure", 0.5f},
                    {"maxExposure", 2.0f},
                    {"brightenRate", 1.0f},
                    {"darkenRate", 2.0f} } } } } });

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  const auto &adaptive = manager.GetConfig().adaptiveQuality;
  CHECK(adaptive.dynamicResolutionEnabled == true);
  CHECK(adaptive.renderScaleLocked == false);
  CHECK(adaptive.renderScale == doctest::Approx(0.85f));
  CHECK(adaptive.minRenderScale == doctest::Approx(0.7f));
  CHECK(adaptive.cooldownSeconds == doctest::Approx(20.0f));
  CHECK(adaptive.exposure == doctest::Approx(1.25f));

  const auto saved = ReadJson(settingsPath);
  REQUIRE(saved["render"].contains("adaptiveQuality"));
  CHECK(saved["render"]["adaptiveQuality"]["renderScale"].get<float>() ==
        doctest::Approx(0.85f));
  CHECK(saved["render"]["adaptiveQuality"]["exposure"].get<float>() ==
        doctest::Approx(1.25f));
}

TEST_CASE("[Unit] QualityTierManager - metadata refresh preserves unrelated preference domains") {
  const auto settingsPath = MakeTempSettingsPath("metadata_preserves_domains.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"unknownRootKey", "keep-me"},
             {"render",
              {{"v3", {{"enabled", true}, {"futureV3Key", 7}}},
               {"gi", {{"enabled", true}, {"intensity", 2.5f}}},
               {"gpuText", {{"enabled", true}}},
               {"gpuLoot", {{"enabled", true}}},
               {"fluid", {{"enabled", true}}},
               {"adaptiveQuality", {{"renderScale", 0.85f}}},
               {"unknownRenderSubtree", {{"keep", 1}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  CHECK(manager.GetConfig().giEnabled == true);
  CHECK(manager.EffectiveGiEnabled().has_value());
  CHECK(manager.EffectiveGiEnabled().value() == true);

  // The metadata refresh wrote its own subtree; every unrelated domain and
  // unknown key survived.
  const nlohmann::json saved = ReadJson(settingsPath);
  REQUIRE(saved.contains("renderQualityAutoDetect"));
  CHECK(saved["unknownRootKey"].get<std::string>() == "keep-me");
  CHECK(saved["render"]["v3"]["enabled"].get<bool>() == true);
  CHECK(saved["render"]["v3"]["futureV3Key"].get<int>() == 7);
  CHECK(saved["render"]["gi"]["enabled"].get<bool>() == true);
  CHECK(saved["render"]["gi"]["intensity"].get<float>() == doctest::Approx(2.5f));
  CHECK(saved["render"]["gpuText"]["enabled"].get<bool>() == true);
  CHECK(saved["render"]["gpuLoot"]["enabled"].get<bool>() == true);
  CHECK(saved["render"]["fluid"]["enabled"].get<bool>() == true);
  CHECK(saved["render"]["adaptiveQuality"]["renderScale"].get<float>() ==
        doctest::Approx(0.85f));
  CHECK(saved["render"]["unknownRenderSubtree"]["keep"].get<int>() == 1);

  // Reinitialize from the persisted file: the GI preference still drives the
  // effective config under the documented precedence.
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().giEnabled == true);
}

TEST_CASE("[Unit] QualityTierManager - runtime override and auto-degrade never persist") {
  const auto settingsPath = MakeTempSettingsPath("runtime_not_persisted.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"render", {{"v3", {{"enabled", false}}}, {"gi", {{"enabled", true}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().giEnabled == true);

  // A transient runtime override flips the effective config only; the
  // serialized user preference is untouched.
  CHECK(manager.SetGiEnabledOverride(false));
  CHECK(manager.GetConfig().giEnabled == false);
  {
    const nlohmann::json saved = ReadJson(settingsPath);
    CHECK(saved["render"]["gi"]["enabled"].get<bool>() == true);
  }
  CHECK(manager.ClearGiEnabledOverride());
  CHECK(manager.GetConfig().giEnabled == true);
  {
    const nlohmann::json saved = ReadJson(settingsPath);
    CHECK(saved["render"]["gi"]["enabled"].get<bool>() == true);
  }

  // Auto-degrade to the level that disables GI in the effective config; the
  // serialized preference stays true (m_config is never a persistence source).
  for (int i = 0; i < 6; ++i) {
    manager.IncreaseAutoDegradeLevel("unit_test", 30.0f, 16.0f);
  }
  CHECK(manager.GetConfig().giEnabled == false);
  {
    const nlohmann::json saved = ReadJson(settingsPath);
    CHECK(saved["render"]["gi"]["enabled"].get<bool>() == true);
  }

  // Reinitialize restores the persisted preference and resets degrade state.
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().giEnabled == true);
}

TEST_CASE("[Unit] QualityTierManager - V3 save updates V3 domain only") {
  const auto settingsPath = MakeTempSettingsPath("v3_save_v3_only.json");
  WriteJson(settingsPath,
            {{"renderQualityTier", "High"},
             {"unknownRootKey", "keep-me"},
             {"render",
              {{"v3", {{"enabled", true}, {"futureV3Key", 7}}},
               {"gi", {{"enabled", true}}},
               {"gpuText", {{"enabled", true}}}}}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().v3Enabled == true);

  CHECK(manager.SetV3Enabled(false, settingsPath.string()));
  const nlohmann::json saved = ReadJson(settingsPath);
  CHECK(saved["render"]["v3"]["enabled"].get<bool>() == false);
  // Unknown V3 child key survives the in-place merge.
  CHECK(saved["render"]["v3"]["futureV3Key"].get<int>() == 7);
  // Unrelated domains are untouched by the V3-only save.
  CHECK(saved["render"]["gi"]["enabled"].get<bool>() == true);
  CHECK(saved["render"]["gpuText"]["enabled"].get<bool>() == true);
  CHECK(saved["unknownRootKey"].get<std::string>() == "keep-me");
  // The explicit V3 save path writes the compatibility flat key.
  CHECK(saved.contains("render.v3.enabled"));
  CHECK(saved["render.v3.enabled"].get<bool>() == false);
}

TEST_CASE("[Unit] QualityTierManager - non-object settings structures fail closed") {
  auto &manager = render::core::QualityTierManager::Get();

  SUBCASE("root is a JSON array") {
    const auto settingsPath = MakeTempSettingsPath("fail_closed_array.json");
    WriteJson(settingsPath, nlohmann::json::array({1, 2, 3}));
    const std::string before = ReadRaw(settingsPath);
    // Metadata refresh and the V3 save must both leave the file untouched.
    manager.Initialize(settingsPath.string(), true);
    CHECK(ReadRaw(settingsPath) == before);
    manager.SetV3Enabled(true, settingsPath.string());
    CHECK(ReadRaw(settingsPath) == before);
  }

  SUBCASE("render is not an object") {
    const auto settingsPath = MakeTempSettingsPath("fail_closed_render.json");
    WriteJson(settingsPath,
              {{"render", "not-an-object"}, {"renderQualityTier", "High"}});
    manager.Initialize(settingsPath.string(), true);
    const std::string afterInit = ReadRaw(settingsPath);
    manager.SetV3Enabled(true, settingsPath.string());
    CHECK(ReadRaw(settingsPath) == afterInit);
  }

  SUBCASE("render.v3 is not an object") {
    const auto settingsPath = MakeTempSettingsPath("fail_closed_v3.json");
    WriteJson(settingsPath,
              {{"render", {{"v3", "not-an-object"}}}, {"renderQualityTier", "High"}});
    manager.Initialize(settingsPath.string(), true);
    const std::string afterInit = ReadRaw(settingsPath);
    manager.SetV3Enabled(true, settingsPath.string());
    CHECK(ReadRaw(settingsPath) == afterInit);
  }

  SUBCASE("existing unparseable file is preserved byte-for-byte") {
    const auto settingsPath = MakeTempSettingsPath("fail_closed_badjson.json");
    {
      std::ofstream out(settingsPath, std::ios::binary | std::ios::trunc);
      REQUIRE(out.is_open());
      out << "{ this is not json ";
    }
    const std::string before = ReadRaw(settingsPath);
    manager.Initialize(settingsPath.string(), true);
    CHECK(ReadRaw(settingsPath) == before);
    manager.SetV3Enabled(true, settingsPath.string());
    CHECK(ReadRaw(settingsPath) == before);
  }
}
