#include "doctest.h"

#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/GICompositePass.hpp"
#include "engine/render/passes/HeightShadowPass.hpp"
#include "engine/render/passes/JFAPass.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/PostProcessPass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/UIWorldPass.hpp"
#include "engine/render/passes/VFXPass.hpp"
#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/GPUUtils.hpp"
#include "GameplayRuntimeHarness.hpp"

#include "raylib.h"

#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace NoMoreDay;
using NoMoreDay::render::core::QualityTier;
using NoMoreDay::render::core::QualityTierManager;
using NoMoreDay::render::graph::RenderOwnerTag;
using NoMoreDay::render::graph::RenderResourceTag;
using NoMoreDay::render::passes::CompositePass;
using NoMoreDay::render::passes::GICompositePass;
using NoMoreDay::render::passes::HeightShadowPass;
using NoMoreDay::render::passes::JFAPass;
using NoMoreDay::render::passes::LightingPass;
using NoMoreDay::render::passes::OccluderExtractPass;
using NoMoreDay::render::passes::PostProcessPass;
using NoMoreDay::render::passes::RadianceCascadesPass;
using NoMoreDay::render::passes::ScenePass;
using NoMoreDay::render::passes::UIWorldPass;
using NoMoreDay::render::passes::VFXPass;

namespace {

std::filesystem::path S7MakeTempSettingsPath(const std::string &name) {
  const std::filesystem::path dir = std::filesystem::path("bin") / "tmp_s7_paired_gi";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / name;
}

void S7WriteJson(const std::filesystem::path &path, const nlohmann::json &json) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out << json.dump(2);
}

nlohmann::json S7ReadJson(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.is_open());
  nlohmann::json parsed = nlohmann::json::object();
  in >> parsed;
  return parsed;
}

// S7b: reuses the same hidden-window GL context convention as the existing gate
// integration tests. The context is created once and left alive for the whole
// test binary (other gate tests rely on it too).
bool S7CreateMinimalGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "S7 Paired GI Delta Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  return NoMoreDay::utils::GPUUtils::IsInitialized();
}

std::string S7Join(const std::vector<std::string> &parts) {
  std::ostringstream out;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i != 0) {
      out << ",";
    }
    out << parts[i];
  }
  return out.str();
}

// S7a: the graph wiring mirrors the gate's synthetic pass trace contract and
// branches on the EFFECTIVE config from the manager, exactly like the real
// RenderSystem graph construction does. Two builds with different effective
// configs produce two different compiled pass/resource traces.
std::shared_ptr<NoMoreDay::render::graph::RenderGraph> S7BuildTraceGraph() {
  auto graph = std::make_shared<NoMoreDay::render::graph::RenderGraph>();
  graph->AddPass(std::make_shared<ScenePass>());
  graph->AddPass(std::make_shared<LightingPass>());
  graph->AddPass(std::make_shared<HeightShadowPass>());
  graph->AddPass(std::make_shared<OccluderExtractPass>());
  if (QualityTierManager::Get().GetConfig().giEnabled) {
    graph->AddPass(std::make_shared<JFAPass>());
    graph->AddPass(std::make_shared<RadianceCascadesPass>());
    graph->AddPass(std::make_shared<GICompositePass>());
  }
  graph->AddPass(std::make_shared<VFXPass>());
  graph->AddPass(std::make_shared<UIWorldPass>());
  graph->AddPass(std::make_shared<PostProcessPass>());
  graph->AddPass(std::make_shared<CompositePass>(
      NoMoreDay::render::graph::RenderResourceTag::SceneHdrColor,
      NoMoreDay::render::graph::RenderOwnerTag::UIWorld));
  graph->Build();
  return graph;
}

} // namespace

// ---------------------------------------------------------------------------
// S7a unit tests: runtime GI override lifecycle, priority contract and
// exception/thread safety. No GL context required.
// ---------------------------------------------------------------------------

TEST_CASE("[Unit] S7a QualityTierManager - runtime GI override beats settings.json override") {
  const auto settingsPath = S7MakeTempSettingsPath("s7_precedence.json");
  S7WriteJson(settingsPath,
              {{"renderQualityTier", "High"},
               {"render",
                {{"gi", {{"enabled", false}}},
                 {"v3", {{"enabled", false}}}}}});

  auto &manager = QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  // Settings override says GI off.
  CHECK_FALSE(manager.GetConfig().giEnabled);
  CHECK(manager.EffectiveGiEnabled().has_value());
  CHECK_FALSE(manager.EffectiveGiEnabled().value());

  // S7a priority contract: the runtime setter wins over the settings override.
  CHECK(manager.SetGiEnabledOverride(true));
  CHECK(manager.IsGiOverrideActive());
  CHECK(manager.GetConfig().giEnabled);
  CHECK(manager.EffectiveGiEnabled().has_value());
  CHECK(manager.EffectiveGiEnabled().value());

  // Clearing restores the settings override (GI off again), not the tier default.
  CHECK(manager.ClearGiEnabledOverride());
  CHECK_FALSE(manager.IsGiOverrideActive());
  CHECK_FALSE(manager.GetConfig().giEnabled);
  CHECK(manager.EffectiveGiEnabled().has_value());
  CHECK_FALSE(manager.EffectiveGiEnabled().value());
}

TEST_CASE("[Unit] S7a QualityTierManager - runtime GI override true -> false -> true") {
  const auto settingsPath = S7MakeTempSettingsPath("s7_toggle_cycle.json");
  S7WriteJson(settingsPath,
              {{"renderQualityTier", "High"},
               {"render", {{"v3", {{"enabled", false}}}}}});

  auto &manager = QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().giEnabled);

  CHECK(manager.SetGiEnabledOverride(true));
  CHECK(manager.GetConfig().giEnabled);
  CHECK(manager.GetConfig().giCascadeLevels == 4u);

  CHECK(manager.SetGiEnabledOverride(false));
  CHECK_FALSE(manager.GetConfig().giEnabled);
  CHECK(manager.GetConfig().giCascadeLevels == 0u);
  CHECK(manager.GetConfig().giIntensity == 0.0f);

  CHECK(manager.SetGiEnabledOverride(true));
  CHECK(manager.GetConfig().giEnabled);
  CHECK(manager.GetConfig().giCascadeLevels == 4u);
  CHECK(manager.GetConfig().giIntensity == 1.0f);

  CHECK(manager.ClearGiEnabledOverride());
  CHECK(manager.GetConfig().giEnabled);
}

TEST_CASE("[Unit] S7a QualityTierManager - runtime GI override false -> true restores GI params") {
  const auto settingsPath = S7MakeTempSettingsPath("s7_false_to_true.json");
  S7WriteJson(settingsPath,
              {{"renderQualityTier", "High"},
               {"render", {{"v3", {{"enabled", false}}}}}});

  auto &manager = QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().giEnabled);

  // Turn GI off via the runtime override, then re-enable. The re-enable must
  // restore a valid cascade count and a positive intensity (sizing/warmup
  // prerequisites), matching the settings-override contract.
  CHECK(manager.SetGiEnabledOverride(false));
  CHECK_FALSE(manager.GetConfig().giEnabled);
  CHECK(manager.GetConfig().giCascadeLevels == 0u);

  CHECK(manager.SetGiEnabledOverride(true));
  CHECK(manager.GetConfig().giEnabled);
  CHECK(manager.GetConfig().giCascadeLevels == 4u);
  CHECK(manager.GetConfig().giIntensity == 1.0f);
  CHECK(manager.GetConfig().giHalfResolution == true);

  CHECK(manager.ClearGiEnabledOverride());
}

TEST_CASE("[Unit] S7a QualityTierManager - exception exit restores override via guard") {
  const auto settingsPath = S7MakeTempSettingsPath("s7_exception_recovery.json");
  S7WriteJson(settingsPath,
              {{"renderQualityTier", "High"},
               {"render", {{"v3", {{"enabled", false}}}}}});

  auto &manager = QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  const bool configBefore = manager.GetConfig().giEnabled;
  CHECK(configBefore);
  CHECK_FALSE(manager.IsGiOverrideActive());

  // S7a apply-restore lifecycle: the guard applies an override and restores the
  // prior state when unwinding through an exception.
  try {
    QualityTierManager::GiEnabledOverrideGuard guard(true);
    CHECK(guard.IsOwned());
    CHECK(manager.IsGiOverrideActive());
    CHECK(manager.GetConfig().giEnabled);
    throw std::runtime_error("simulated failure mid-paired-capture");
  } catch (...) {
    // expected: exception unwound through the guard
  }

  CHECK_FALSE(manager.IsGiOverrideActive());
  CHECK(manager.GetConfig().giEnabled == configBefore);
  CHECK(manager.GetConfig().giEnabled);
}

TEST_CASE("[Unit] S7a QualityTierManager - guard restores prior runtime override value") {
  const auto settingsPath = S7MakeTempSettingsPath("s7_guard_restore.json");
  S7WriteJson(settingsPath,
              {{"renderQualityTier", "High"},
               {"render", {{"v3", {{"enabled", false}}}}}});

  auto &manager = QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  // Pre-activate a runtime override, then nest a guard that flips it. On guard
  // exit the prior runtime override value must come back.
  CHECK(manager.SetGiEnabledOverride(true));
  {
    QualityTierManager::GiEnabledOverrideGuard guard(false);
    CHECK(guard.IsOwned());
    CHECK_FALSE(manager.GetConfig().giEnabled);
  }
  CHECK(manager.IsGiOverrideActive());
  CHECK(manager.GetConfig().giEnabled);
  CHECK(manager.EffectiveGiEnabled().has_value());
  CHECK(manager.EffectiveGiEnabled().value());
  CHECK(manager.ClearGiEnabledOverride());
}

TEST_CASE("[Unit] S7a QualityTierManager - thread ownership rejects foreign override mutation") {
  const auto settingsPath = S7MakeTempSettingsPath("s7_thread_ownership.json");
  S7WriteJson(settingsPath,
              {{"renderQualityTier", "High"},
               {"render", {{"v3", {{"enabled", false}}}}}});

  auto &manager = QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);

  // Main thread becomes the owner.
  CHECK(manager.SetGiEnabledOverride(true));
  CHECK(manager.GetConfig().giEnabled);

  // A foreign thread must be rejected and must not mutate the effective config.
  std::atomic<bool> setRejected{false};
  std::atomic<bool> clearRejected{false};
  std::thread foreign([&]() {
    setRejected = !manager.SetGiEnabledOverride(false);
    clearRejected = !manager.ClearGiEnabledOverride();
  });
  foreign.join();

  CHECK(setRejected.load());
  CHECK(clearRejected.load());
  CHECK(manager.IsGiOverrideActive());
  CHECK(manager.GetConfig().giEnabled);

  // The owning thread can still mutate.
  CHECK(manager.SetGiEnabledOverride(false));
  CHECK_FALSE(manager.GetConfig().giEnabled);
  CHECK(manager.ClearGiEnabledOverride());
}

TEST_CASE("[Unit] S7a QualityTierManager - Initialize resets runtime override state") {
  const auto settingsPath = S7MakeTempSettingsPath("s7_initialize_reset.json");
  S7WriteJson(settingsPath,
              {{"renderQualityTier", "High"},
               {"render",
                {{"gi", {{"enabled", true}}},
                 {"v3", {{"enabled", false}}}}}});

  auto &manager = QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().giEnabled);

  CHECK(manager.SetGiEnabledOverride(true));
  CHECK(manager.IsGiOverrideActive());

  // Re-initialization (forceRedetect) must clear the runtime override; the
  // config then follows the (re-read) settings override again. The metadata
  // refresh no longer writes the V3 or GI domains back into the settings file,
  // so the persisted render.gi.enabled=true preference survives and the
  // re-read settings override stays on after re-initialization.
  manager.Initialize(settingsPath.string(), true);
  CHECK_FALSE(manager.IsGiOverrideActive());
  CHECK_FALSE(manager.GetGiRuntimeOverride().has_value());
  CHECK(manager.GetConfig().giEnabled);
  CHECK(manager.EffectiveGiEnabled().has_value());
  CHECK(manager.EffectiveGiEnabled().value());

  // The file still carries the user's true GI preference after the
  // metadata-only refresh; the effective config followed it.
  const nlohmann::json persisted = S7ReadJson(settingsPath);
  REQUIRE(persisted.contains("render"));
  REQUIRE(persisted["render"].contains("gi"));
  CHECK(persisted["render"]["gi"]["enabled"].get<bool>() == true);
}

// ---------------------------------------------------------------------------
// S7a integration: the effective config actually changes the compiled
// pass/resource trace of a graph built exactly like the real render path.
// ---------------------------------------------------------------------------

TEST_CASE("[Integration] S7a - effective config drives two different pass/resource traces") {
  if (!S7CreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping pass/resource trace test");
  }

  const auto settingsPath = S7MakeTempSettingsPath("s7_trace.json");
  S7WriteJson(settingsPath,
              {{"renderQualityTier", "High"},
               {"render",
                {{"gi", {{"enabled", false}}},
                 {"v3", {{"enabled", false}}}}}});

  auto &manager = QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  manager.ForceTier(QualityTier::High);
  CHECK_FALSE(manager.GetConfig().giEnabled);

  // Leg OFF trace: no GI passes.
  const auto offGraph = S7BuildTraceGraph();
  CHECK(offGraph->HasValidationErrors() == false);
  const auto offTrace = S7Join(offGraph->GetCompiledPlan().passOrder);
  const size_t offResourceCount = offGraph->GetCompiledPlan().resources.size();
  CHECK(offTrace.find("JFAPass") == std::string::npos);
  CHECK(offTrace.find("RadianceCascadesPass") == std::string::npos);
  CHECK(offTrace.find("GICompositePass") == std::string::npos);

  // Override ON: the same builder now produces a different trace with GI passes.
  CHECK(manager.SetGiEnabledOverride(true));
  CHECK(manager.GetConfig().giEnabled);
  const auto onGraph = S7BuildTraceGraph();
  CHECK(onGraph->HasValidationErrors() == false);
  const auto onTrace = S7Join(onGraph->GetCompiledPlan().passOrder);
  const size_t onResourceCount = onGraph->GetCompiledPlan().resources.size();
  CHECK(onTrace.find("JFAPass") != std::string::npos);
  CHECK(onTrace.find("RadianceCascadesPass") != std::string::npos);
  CHECK(onTrace.find("GICompositePass") != std::string::npos);

  // S7a requirement: two different pass traces prove the graph actually changed
  // (not just manager state), and GI resources are added alongside the passes.
  CHECK_FALSE(onTrace == offTrace);
  CHECK(onResourceCount > offResourceCount);

  // Clearing returns to the settings-driven trace.
  CHECK(manager.ClearGiEnabledOverride());
  const auto restoredGraph = S7BuildTraceGraph();
  CHECK(S7Join(restoredGraph->GetCompiledPlan().passOrder) == offTrace);
  CHECK_FALSE(manager.IsGiOverrideActive());
}

TEST_CASE("[Integration] S7a - GICompositePass history invalidation accessor contract") {
  if (!S7CreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping history invalidation test");
  }

  GICompositePass pass;
  CHECK_FALSE(pass.IsHistoryValid());
  // InvalidateHistory is idempotent and the fresh pass has no valid history.
  pass.InvalidateHistory();
  CHECK_FALSE(pass.IsHistoryValid());
  pass.InvalidateHistory();
  CHECK_FALSE(pass.IsHistoryValid());
}

// ---------------------------------------------------------------------------
// S7b integration: paired GI delta capture mechanism. Runs the same offscreen
// rendering path as the gate. NOTE: the test binary never initializes
// RenderSystem, so no GI/Lighting passes execute and the measured delta is
// expected to be ~0. This environment records WARP/test-env evidence only; the
// numeric threshold verdict is deferred to the RTX 4070 Super DOD-2 capture.
// ---------------------------------------------------------------------------

// W6 (M0-C): S7b is contract/diagnostic only - paired GI delta capture against
// a 1x1 hidden context + GameplayRuntimeHarness (no real Game/App
// initialization). Production GI evidence comes from NoMoreDay.exe --gpu-gate.
// Excluded from broad ci;nonperf and generic integration via the
// [GPU-Diagnostic] prefix; runs under nmd.tests.gpu.diagnostic (minimal
// non-exhaustive budget). doctest success here never means the gate passed.
TEST_CASE("[GPU-Diagnostic] S7b - paired GI delta capture emits renderer, leg traces and delta") {
  using NoMoreDay::render::validation::GameplayRuntimeHarness;
  using NoMoreDay::render::validation::GPUHardwareValidationGate;

  if (!S7CreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping paired GI delta capture test");
  }

  GameplayRuntimeHarness harness;
  const auto fixtures = GPUHardwareValidationGate::GetStandardFixtures();
  REQUIRE(fixtures.size() == 3);

  const auto &cave = fixtures[0];
  REQUIRE(cave.name == "cave_color_bleed");

  const auto result = GPUHardwareValidationGate::RunPairedGiDeltaCapture(harness, cave);

  // Renderer environment annotation.
  REQUIRE_FALSE(result.renderer.empty());
  std::cout << "[S7b] paired_delta fixture=" << result.fixtureName
            << " renderer=\"" << result.renderer << "\""
            << " renderer_is_hardware=" << (result.rendererIsHardware ? 1 : 0)
            << " roi_mean_on=" << result.roiMeanOn
            << " roi_mean_off=" << result.roiMeanOff
            << " paired_delta=" << result.pairedDelta << "\n";
  if (!result.rendererIsHardware) {
    std::cout << "[S7b] WARP/test-env: numeric verdict deferred to RTX 4070S "
                 "DOD-2 capture\n";
  }

  // Same seed/camera/frame/FBO/colorspace/ROI, only GI flipped.
  CHECK(result.sceneSeed == cave.sceneSeed);
  CHECK(result.width == cave.width);
  CHECK(result.height == cave.height);
  CHECK(result.roiWidth == cave.roiWidth);
  CHECK(result.roiHeight == cave.roiHeight);
  CHECK(result.colorSpace == "sRGB");
  CHECK(result.warmupFrames == cave.warmupFrames);
  CHECK(result.sampleFrames == cave.sampleFrames);
  CHECK(result.threshold == 0.001f);

  // Two different leg pass traces: the graph wiring actually changed between
  // the GI-ON and GI-OFF legs.
  REQUIRE(result.legPassTraces.size() == 2);
  CHECK_FALSE(result.legPassTraces[0] == result.legPassTraces[1]);
  CHECK(result.legPassTraces[0].find("GICompositePass") != std::string::npos);
  CHECK(result.legPassTraces[1].find("GICompositePass") == std::string::npos);

  // Delta is a finite non-negative number and the mechanism reports verdicts.
  CHECK(std::isfinite(result.pairedDelta));
  CHECK(result.pairedDelta >= 0.0f);
  CHECK_FALSE(result.passed); // test-env: no GI rendered, delta ~0 below threshold

  // JSON schema of the paired delta record.
  const nlohmann::json parsed = nlohmann::json::parse(
      [&]() {
        NoMoreDay::render::validation::GateReport report;
        report.revision = "S7-TEST";
        report.capabilities.renderer = result.renderer;
        report.capabilities.rendererIsHardware = result.rendererIsHardware;
        report.pairedGiDeltas.push_back(result);
        return report.ToJsonString();
      }());
  REQUIRE(parsed.contains("paired_gi_deltas"));
  REQUIRE(parsed["paired_gi_deltas"].is_array());
  REQUIRE(parsed["paired_gi_deltas"].size() == 1);
  const auto &entry = parsed["paired_gi_deltas"][0];
  CHECK(entry["fixture"] == "cave_color_bleed");
  CHECK(entry.contains("scene_seed"));
  CHECK(entry.contains("renderer"));
  CHECK(entry.contains("renderer_is_hardware"));
  CHECK(entry.contains("roi_mean_on"));
  CHECK(entry.contains("roi_mean_off"));
  CHECK(entry.contains("paired_delta"));
  CHECK(entry.contains("threshold"));
  CHECK(entry.contains("leg_pass_traces"));
  CHECK(entry["leg_pass_traces"].size() == 2);
  CHECK(entry.contains("tracked_bytes_on"));
  CHECK(entry.contains("tracked_bytes_off"));
  CHECK(parsed["capabilities"]["renderer_is_hardware"].is_boolean());
}
