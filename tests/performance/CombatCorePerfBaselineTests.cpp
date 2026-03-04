#include "TestCommon.hpp"
#include "TestJsonArtifact.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace NoMoreDay::tests {

TEST_CASE("[Performance] CombatCorePerfBaseline - Baseline artifact is present") {
  const std::filesystem::path perfBaselinePath =
      "docs/reports/combat-core-vnext/baseline/perf-baseline.json";
  const std::filesystem::path parityBaselinePath =
      "docs/reports/combat-core-vnext/baseline/parity-harness-baseline.json";
  const std::filesystem::path scenarioManifestPath =
      "docs/reports/combat-core-vnext/baseline/scenario-manifest.json";
  const std::filesystem::path hardwareProfilePath =
      "docs/reports/combat-core-vnext/baseline/hardware-profile.md";

  INFO("P0-B baseline contract: perf and hardware artifacts must contain required fields.");

  const nlohmann::json perfBaseline = LoadJsonArtifact(perfBaselinePath);
  REQUIRE(perfBaseline.is_object());
  REQUIRE(perfBaseline.contains("schema_version"));
  CHECK(perfBaseline["schema_version"].is_string());
  REQUIRE(perfBaseline.contains("protocol"));
  REQUIRE(perfBaseline["protocol"].is_object());
  CHECK(perfBaseline["protocol"].contains("warmup_runs"));
  CHECK(perfBaseline["protocol"]["warmup_runs"].is_number_unsigned());
  CHECK(perfBaseline["protocol"]["warmup_runs"].get<uint32_t>() == 1u);
  CHECK(perfBaseline["protocol"].contains("measured_runs"));
  CHECK(perfBaseline["protocol"]["measured_runs"].is_number_unsigned());
  CHECK(perfBaseline["protocol"]["measured_runs"].get<uint32_t>() == 5u);
  CHECK(perfBaseline["protocol"].contains("metric_kind"));
  CHECK(perfBaseline["protocol"]["metric_kind"].is_string());
  CHECK(perfBaseline["protocol"]["metric_kind"].get<std::string>() == "p95_ms");
  CHECK(perfBaseline["protocol"].contains("max_regression_pct"));
  CHECK(perfBaseline["protocol"]["max_regression_pct"].is_number());
  CHECK(perfBaseline["protocol"]["max_regression_pct"].get<double>() ==
        doctest::Approx(5.0));
  REQUIRE(perfBaseline.contains("scenarios"));
  REQUIRE(perfBaseline["scenarios"].is_array());
  REQUIRE_FALSE(perfBaseline["scenarios"].empty());
  const auto &firstScenario = perfBaseline["scenarios"].front();
  CHECK(firstScenario.contains("scenario_id"));
  CHECK(firstScenario["scenario_id"].is_string());
  CHECK(firstScenario.contains("baseline_p95_ms"));
  CHECK(firstScenario["baseline_p95_ms"].is_number());
  CHECK(firstScenario["baseline_p95_ms"].get<double>() > 0.0);
  CHECK(firstScenario.contains("test_case"));
  CHECK(firstScenario["test_case"].is_string());
  CHECK(firstScenario.contains("seed"));
  CHECK(firstScenario["seed"].is_number_unsigned());

  REQUIRE(perfBaseline.contains("hardware_profile_hash"));
  REQUIRE(perfBaseline["hardware_profile_hash"].is_string());
  const std::string perfHash = perfBaseline["hardware_profile_hash"].get<std::string>();
  CHECK(perfHash.rfind("sha256:", 0) == 0);
  CHECK(perfHash.find("placeholder") == std::string::npos);

  const nlohmann::json parityBaseline = LoadJsonArtifact(parityBaselinePath);
  REQUIRE(parityBaseline.contains("fixtures"));
  REQUIRE(parityBaseline["fixtures"].is_array());
  REQUIRE_FALSE(parityBaseline["fixtures"].empty());
  const auto &firstParityFixture = parityBaseline["fixtures"].front();

  const nlohmann::json scenarioManifest = LoadJsonArtifact(scenarioManifestPath);
  REQUIRE(scenarioManifest.contains("scenarios"));
  REQUIRE(scenarioManifest["scenarios"].is_array());
  REQUIRE_FALSE(scenarioManifest["scenarios"].empty());
  const auto &firstManifestScenario = scenarioManifest["scenarios"].front();

  if (firstScenario.contains("scenario_id") && firstScenario["scenario_id"].is_string() &&
      firstParityFixture.contains("scenario_id") &&
      firstParityFixture["scenario_id"].is_string() &&
      firstManifestScenario.contains("scenario_id") &&
      firstManifestScenario["scenario_id"].is_string()) {
    const std::string scenarioId = firstScenario["scenario_id"].get<std::string>();
    CHECK(firstParityFixture["scenario_id"].get<std::string>() == scenarioId);
    CHECK(firstManifestScenario["scenario_id"].get<std::string>() == scenarioId);
  }

  if (firstScenario.contains("seed") && firstScenario["seed"].is_number_unsigned() &&
      firstParityFixture.contains("seed") && firstParityFixture["seed"].is_number_unsigned() &&
      firstManifestScenario.contains("seed") &&
      firstManifestScenario["seed"].is_number_unsigned()) {
    const uint32_t scenarioSeed = firstScenario["seed"].get<uint32_t>();
    CHECK(firstParityFixture["seed"].get<uint32_t>() == scenarioSeed);
    CHECK(firstManifestScenario["seed"].get<uint32_t>() == scenarioSeed);
  }

  INFO("Reading hardware profile artifact: " << hardwareProfilePath.string());
  REQUIRE(std::filesystem::exists(hardwareProfilePath));
  std::ifstream in(hardwareProfilePath, std::ios::binary);
  REQUIRE(in.is_open());
  std::string markdown((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(markdown.find("# Combat Core VNext Hardware Profile") != std::string::npos);
  const std::string hashPrefix = "- profile_hash: ";
  const size_t hashPos = markdown.find(hashPrefix);
  REQUIRE(hashPos != std::string::npos);
  const size_t hashValueStart = hashPos + hashPrefix.size();
  size_t hashValueEnd = markdown.find('\n', hashValueStart);
  if (hashValueEnd == std::string::npos) {
    hashValueEnd = markdown.size();
  }
  const std::string markdownHash = markdown.substr(hashValueStart, hashValueEnd - hashValueStart);
  CHECK(markdownHash.rfind("sha256:", 0) == 0);
  CHECK(markdownHash.find("placeholder") == std::string::npos);
  CHECK(markdownHash == perfHash);
}

} // namespace NoMoreDay::tests
