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
  auto &manager = render::core::QualityTierManager::Get();
  manager.ForceTier(render::core::QualityTier::Ultra);
  CHECK(manager.GetAutoDegradeLevel() == 0);
  CHECK(manager.GetConfig().distortionEnabled == true);
  CHECK(manager.GetConfig().volumetricLightEnabled == true);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 1);
  CHECK(manager.GetConfig().bloomMipLevels == 5);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 2);
  CHECK(manager.GetConfig().distortionEnabled == false);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 3);
  CHECK(manager.GetConfig().maxLights == 128);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 4);
  CHECK(manager.GetConfig().maxParticles == 100000);
  CHECK(manager.GetConfig().subEmitterEnabled == false);

  CHECK(manager.IncreaseAutoDegradeLevel("unit_test", 20.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 5);
  CHECK(manager.GetConfig().volumetricLightEnabled == false);
  CHECK(manager.GetConfig().volumetricSampleCount == 0);

  CHECK(manager.DecreaseAutoDegradeLevel("unit_test", 8.0f, 16.0f) == true);
  CHECK(manager.GetAutoDegradeLevel() == 4);
  CHECK(manager.GetConfig().volumetricLightEnabled == true);

  manager.ResetAutoDegrade("unit_test");
  CHECK(manager.GetAutoDegradeLevel() == 0);
  CHECK(manager.GetConfig().distortionEnabled == true);
  CHECK(manager.GetConfig().volumetricLightEnabled == true);
}
