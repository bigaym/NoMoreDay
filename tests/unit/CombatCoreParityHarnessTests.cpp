#include "TestCommon.hpp"
#include "TestJsonArtifact.hpp"

#include <filesystem>
#include <string>

namespace NoMoreDay::tests {

TEST_CASE("[Unit] CombatCoreParityHarness - Baseline harness artifact is wired") {
  const std::filesystem::path harnessBaselinePath =
      "docs/reports/combat-core-vnext/baseline/parity-harness-baseline.json";
  const std::filesystem::path scenarioManifestPath =
      "docs/reports/combat-core-vnext/baseline/scenario-manifest.json";

  INFO("P0-B baseline contract: harness and scenario artifacts must parse and expose required keys.");

  const nlohmann::json harnessBaseline = LoadJsonArtifact(harnessBaselinePath);
  REQUIRE(harnessBaseline.is_object());
  REQUIRE(harnessBaseline.contains("schema_version"));
  CHECK(harnessBaseline["schema_version"].is_string());
  REQUIRE(harnessBaseline.contains("parity_tolerance"));
  REQUIRE(harnessBaseline["parity_tolerance"].is_object());

  const auto &tolerance = harnessBaseline["parity_tolerance"];
  REQUIRE(tolerance.contains("exact_match"));
  REQUIRE(tolerance["exact_match"].is_object());
  CHECK(tolerance["exact_match"].contains("abs_delta"));
  CHECK(tolerance["exact_match"]["abs_delta"].is_number());
  CHECK(tolerance["exact_match"]["abs_delta"].get<double>() == doctest::Approx(0.0));

  REQUIRE(tolerance.contains("hit_float"));
  CHECK(tolerance["hit_float"].contains("abs_delta"));
  CHECK(tolerance["hit_float"]["abs_delta"].is_number());
  CHECK(tolerance["hit_float"].contains("rel_delta_pct"));
  CHECK(tolerance["hit_float"]["rel_delta_pct"].is_number());

  REQUIRE(tolerance.contains("dot_aggregate"));
  CHECK(tolerance["dot_aggregate"].contains("abs_delta"));
  CHECK(tolerance["dot_aggregate"]["abs_delta"].is_number());
  CHECK(tolerance["dot_aggregate"].contains("rel_delta_pct"));
  CHECK(tolerance["dot_aggregate"]["rel_delta_pct"].is_number());

  REQUIRE(tolerance.contains("status_duration"));
  CHECK(tolerance["status_duration"].contains("abs_delta_seconds"));
  CHECK(tolerance["status_duration"]["abs_delta_seconds"].is_number());

  for (const char *scenarioClass :
       {"exact_match", "hit_float", "dot_aggregate", "status_duration"}) {
    INFO("Missing parity class contract: " << scenarioClass);
    CHECK(tolerance.contains(scenarioClass));
  }

  REQUIRE(harnessBaseline.contains("fixtures"));
  REQUIRE(harnessBaseline["fixtures"].is_array());
  REQUIRE_FALSE(harnessBaseline["fixtures"].empty());
  const auto &firstFixture = harnessBaseline["fixtures"].front();
  CHECK(firstFixture.contains("scenario_id"));
  CHECK(firstFixture["scenario_id"].is_string());
  CHECK(firstFixture.contains("scenario_class"));
  CHECK(firstFixture["scenario_class"].is_string());
  CHECK(firstFixture.contains("seed"));
  CHECK(firstFixture["seed"].is_number_unsigned());
  CHECK(firstFixture.contains("expected_legacy_final"));
  CHECK(firstFixture["expected_legacy_final"].is_number());
  CHECK(firstFixture.contains("expected_trace_hash"));
  CHECK(firstFixture["expected_trace_hash"].is_string());

  if (firstFixture.contains("expected_trace_hash") &&
      firstFixture["expected_trace_hash"].is_string()) {
    const std::string traceHash = firstFixture["expected_trace_hash"].get<std::string>();
    CHECK(traceHash.rfind("sha256:", 0) == 0);
    CHECK(traceHash.find("placeholder") == std::string::npos);
    CHECK(traceHash.size() > 20);
  }

  const nlohmann::json scenarioManifest = LoadJsonArtifact(scenarioManifestPath);
  REQUIRE(scenarioManifest.is_object());
  REQUIRE(scenarioManifest.contains("schema_version"));
  CHECK(scenarioManifest["schema_version"].is_string());
  REQUIRE(scenarioManifest.contains("scenarios"));
  REQUIRE(scenarioManifest["scenarios"].is_array());
  REQUIRE_FALSE(scenarioManifest["scenarios"].empty());
  const auto &firstScenario = scenarioManifest["scenarios"].front();
  CHECK(firstScenario.contains("scenario_id"));
  CHECK(firstScenario.contains("scenario_class"));
  CHECK(firstScenario.contains("seed"));

  if (firstFixture.contains("scenario_id") && firstFixture["scenario_id"].is_string() &&
      firstScenario.contains("scenario_id") && firstScenario["scenario_id"].is_string()) {
    CHECK(firstFixture["scenario_id"].get<std::string>() ==
          firstScenario["scenario_id"].get<std::string>());
  }
  if (firstFixture.contains("scenario_class") &&
      firstFixture["scenario_class"].is_string() &&
      firstScenario.contains("scenario_class") &&
      firstScenario["scenario_class"].is_string()) {
    CHECK(firstFixture["scenario_class"].get<std::string>() ==
          firstScenario["scenario_class"].get<std::string>());
  }
  if (firstFixture.contains("seed") && firstFixture["seed"].is_number_unsigned() &&
      firstScenario.contains("seed") && firstScenario["seed"].is_number_unsigned()) {
    CHECK(firstFixture["seed"].get<uint32_t>() == firstScenario["seed"].get<uint32_t>());
  }
}

} // namespace NoMoreDay::tests
