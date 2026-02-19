#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>

#include <nlohmann/json.hpp>

namespace {

nlohmann::json ReadJsonFile(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.is_open());
  nlohmann::json value = nlohmann::json::object();
  in >> value;
  return value;
}

const nlohmann::json *FindCheckById(const nlohmann::json &matrix,
                                    const std::string &checkId) {
  for (const auto &layer : matrix["layers"]) {
    for (const auto &check : layer["checks"]) {
      if (check.value("id", "") == checkId) {
        return &check;
      }
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE(
    "[Unit] ReleaseGate - Performance metric regex contracts parse benchmark output") {
  const auto matrix = ReadJsonFile("conductor/validation/v3_gate_matrix.json");

  struct ParseCase {
    std::string checkId;
    std::string sampleLine;
  };

  const ParseCase parseCases[] = {
      {"perf_baseline_270", "RELEASE_GATE_METRIC baseline_270_fps=883.992"},
      {"perf_combat_180", "RELEASE_GATE_METRIC combat_180_fps=192308.0"},
      {"perf_stress_144", "RELEASE_GATE_METRIC stress_144_fps=10787.5"},
      {"perf_clustered_uplift",
       "RELEASE_GATE_METRIC clustered_128_improvement_pct=-2.26261"},
  };

  for (const auto &parseCase : parseCases) {
    const auto *check = FindCheckById(matrix, parseCase.checkId);
    REQUIRE(check != nullptr);
    REQUIRE(check->contains("metric"));
    const std::string pattern =
        (*check)["metric"].value("pattern", std::string{});
    REQUIRE_FALSE(pattern.empty());

    const std::regex expr(pattern);
    std::smatch match;
    const bool parsed = std::regex_search(parseCase.sampleLine, match, expr);
    CHECK(parsed);
    REQUIRE(match.size() >= 2);
    CHECK_FALSE(match[1].str().empty());
    CHECK_NOTHROW(static_cast<void>(std::stod(match[1].str())));
  }
}

TEST_CASE("[Unit] ReleaseGate - Benchmark context output contract is parseable") {
  const std::regex stressExpr(
      R"(RELEASE_GATE_CONTEXT\s+stress_seed=(\d+)\s+warmup_frames=(\d+)\s+measure_frames=(\d+))");
  const std::regex clusteredExpr(
      R"(RELEASE_GATE_CONTEXT\s+clustered_seed=(\d+)\s+lights=(\d+)\s+warmup_frames=(\d+)\s+measure_frames=(\d+)\s+trials=(\d+))");

  std::smatch stressMatch;
  const std::string stressLine =
      "RELEASE_GATE_CONTEXT stress_seed=1313686611 warmup_frames=120 "
      "measure_frames=800";
  REQUIRE(std::regex_search(stressLine, stressMatch, stressExpr));
  REQUIRE(stressMatch.size() == 4);
  const int stressWarmup = std::stoi(stressMatch[2].str());
  const int stressMeasure = std::stoi(stressMatch[3].str());
  CHECK(stressWarmup > 0);
  CHECK(stressMeasure > stressWarmup);

  std::smatch clusteredMatch;
  const std::string clusteredLine =
      "RELEASE_GATE_CONTEXT clustered_seed=1313686595 lights=128 "
      "warmup_frames=20 measure_frames=120 trials=3";
  REQUIRE(std::regex_search(clusteredLine, clusteredMatch, clusteredExpr));
  REQUIRE(clusteredMatch.size() == 6);
  const int clusteredWarmup = std::stoi(clusteredMatch[3].str());
  const int clusteredMeasure = std::stoi(clusteredMatch[4].str());
  const int clusteredTrials = std::stoi(clusteredMatch[5].str());
  CHECK(clusteredWarmup > 0);
  CHECK(clusteredMeasure > clusteredWarmup);
  CHECK(clusteredTrials >= 3);
}
