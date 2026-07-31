#include "doctest.h"

#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/GPUUtils.hpp"
#include "GameplayRuntimeHarness.hpp"

#include "raylib.h"

#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

// S8 (M0-C R6): the Python runner forwards --samples/--toggle-loops/
// --stress-test-1min to the C++ side by injecting NMD_GATE_* environment
// variables. Read them here (falling back to the previous hardcoded defaults)
// so the runner CLI is actually wired into RunGate instead of being dead.
int GateEnvIntOr(const char* name, int fallback) {
  const char* raw = std::getenv(name);
  if (raw == nullptr || *raw == '\0') {
    return fallback;
  }
  char* end = nullptr;
  const long value = std::strtol(raw, &end, 10);
  if (end == raw || *end != '\0') {
    return fallback;
  }
  return static_cast<int>(value);
}

bool GateEnvBoolOr(const char* name, bool fallback) {
  const char* raw = std::getenv(name);
  if (raw == nullptr || *raw == '\0') {
    return fallback;
  }
  return std::strcmp(raw, "1") == 0 || std::strcmp(raw, "true") == 0 ||
         std::strcmp(raw, "TRUE") == 0;
}

}  // namespace

static bool CreateMinimalGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "GPU Hardware Validation Gate Test Window");
  if (!IsWindowReady()) {
    std::cerr << "ERROR: Failed to create hidden GLFW window for GPU context\n";
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  return NoMoreDay::utils::GPUUtils::IsInitialized();
}

static void DestroyMinimalGpuContext() {
  if (IsWindowReady()) {
    CloseWindow();
  }
}

TEST_CASE("[Integration] GPU Hardware Validation Gate - QueryCapabilities and Fixtures") {
  using namespace NoMoreDay::render::validation;

  if (!CreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping QueryCapabilities test");
  }

  const auto caps = GPUHardwareValidationGate::QueryCapabilities();
  const auto fixtures = GPUHardwareValidationGate::GetStandardFixtures();

  REQUIRE(fixtures.size() == 3);
  CHECK(fixtures[0].name == "cave_color_bleed");
  CHECK(fixtures[1].name == "dynamic_combat_emissive");
  CHECK(fixtures[2].name == "outdoor_light_pressure");

  for (const auto &fix : fixtures) {
    CHECK(fix.roiWidth > 0);
    CHECK(fix.roiHeight > 0);
    CHECK(fix.warmupFrames >= 10);
    CHECK(fix.sampleFrames >= 120);
  }

  CHECK(caps.meetsPreflightPrerequisites);
  CHECK(caps.computeShaderSupported);
  CHECK(caps.rgba16fSupported);
}

TEST_CASE("[Integration] GPU Hardware Validation Gate - RunGate Offscreen Matrix & Preflight Output") {
  using namespace NoMoreDay::render::validation;

  if (!CreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping RunGate test");
  }

  // S6 (M0-C R1.2): the gate must be driven by a real gameplay fixture harness
  // (registry + minimal SharedContext + owned RGBA16F composite target).
  NoMoreDay::render::validation::GameplayRuntimeHarness harness;
  // S8 (M0-C R6): sample count, stress loop and toggle loops are wired from the
  // runner CLI through NMD_GATE_* env vars (defaults match previous hardcoded
  // values), so the C++ gate honors the runner parameters.
  const int sampleFrames = GateEnvIntOr("NMD_GATE_SAMPLES", 120);
  const int toggleLoops = GateEnvIntOr("NMD_GATE_TOGGLE_LOOPS", 100);
  const bool stressTest1Min = GateEnvBoolOr("NMD_GATE_STRESS", true);
  const auto report =
      GPUHardwareValidationGate::RunGate("TEST_REV_123", sampleFrames,
                                         stressTest1Min, toggleLoops, &harness);

  const std::string jsonStr = report.ToJsonString();
  CHECK_FALSE(jsonStr.empty());

  const nlohmann::json parsed = nlohmann::json::parse(jsonStr);
  CHECK(parsed.contains("revision"));
  CHECK(parsed["revision"] == "TEST_REV_123");
  CHECK(parsed.contains("timestamp"));
  CHECK(parsed.contains("capabilities"));
  CHECK(parsed.contains("gate_status"));
  CHECK(parsed.contains("resources"));
  CHECK(parsed.contains("stress_test"));
  CHECK(parsed.contains("matrix_results"));

  CHECK(parsed["capabilities"].contains("debug_callback"));
  CHECK(parsed["capabilities"].contains("debug_output_installed"));
  CHECK(parsed["capabilities"].contains("debug_output_enabled"));
  CHECK(parsed["gl_diagnostics"].is_object());
  CHECK(parsed["gl_diagnostics"].contains("debug_message_count"));
  CHECK(parsed["gl_diagnostics"].contains("severe_error_count"));
  CHECK(parsed["gl_diagnostics"].contains("messages"));
  CHECK(parsed["gl_diagnostics"]["messages"].is_array());

  const std::string statusStr = parsed["gate_status"];
  CHECK((statusStr == "GO" || statusStr == "NO_GO" || statusStr == "NOT_RUN"));

  // S4 (M0-C R5.2): the stress report carries the five-second snapshot schema.
  CHECK(parsed["stress_test"].contains("resource_snapshots"));
  CHECK(parsed["stress_test"]["resource_snapshots"].is_array());
  const auto &snapshots = parsed["stress_test"]["resource_snapshots"];
  if (statusStr != "NOT_RUN") {
    CHECK(snapshots.size() >= 1);
    for (const auto &snap : snapshots) {
      CHECK(snap.contains("frame_index"));
      CHECK(snap.contains("timestamp_ms"));
      CHECK(snap.contains("active_resource_count"));
      CHECK(snap.contains("current_total_bytes"));
      CHECK(snap.contains("peak_total_bytes"));
      CHECK(snap.contains("total_created_count"));
      CHECK(snap.contains("total_destroyed_count"));
      CHECK(snap.contains("live_reference_count"));
      CHECK(snap.contains("pending_reference_count"));
      CHECK(snap.contains("pending_query_overage_count"));
      CHECK(snap.contains("bytes_net_growth"));
      CHECK(snap.contains("count_net_growth"));
      CHECK(snap.contains("net_growth_violation"));
      CHECK(snap.contains("pending_overage_violation"));
    }
  }

  if (statusStr == "GO") {
    CHECK(parsed["matrix_results"].size() >= 3);
    const auto &firstFixture = parsed["matrix_results"][0];
    CHECK(firstFixture.contains("non_black_roi_passed"));
    CHECK(firstFixture.contains("roi_mean_brightness"));
    CHECK(firstFixture.contains("timings"));

    for (const auto &fix : parsed["matrix_results"]) {
      CHECK(fix["pass_trace_valid"].get<bool>());
    }

    CHECK(parsed["stress_test"]["stress_1min_passed"].get<bool>());
    CHECK(parsed["stress_test"]["toggle_100_loops_passed"].get<bool>());
    CHECK(parsed["resources"]["leak_candidate_count"].get<size_t>() == 0);
  }

  // S6 (T6.5): the harness drove the matrix - every matrix cell records the
  // deterministic scene input hash, recipe version and provenance.
  if (!parsed["matrix_results"].empty()) {
    for (const auto &fix : parsed["matrix_results"]) {
      CHECK(fix.contains("scene_input_hash"));
      CHECK(fix.contains("fixture_version"));
      CHECK(fix.contains("scene_source"));
      CHECK_FALSE(fix["scene_input_hash"].get<std::string>().empty());
      CHECK_FALSE(fix["fixture_version"].get<std::string>().empty());
      CHECK_FALSE(fix["scene_source"].get<std::string>().empty());
    }
  }

  std::cout << "GPU_HARDWARE_GATE_RESULT status=" << statusStr << "\n";
  std::cout << "GL_VENDOR: " << parsed["capabilities"]["vendor"].get<std::string>() << "\n";
  std::cout << "GL_RENDERER: " << parsed["capabilities"]["renderer"].get<std::string>() << "\n";
  std::cout << "GL_VERSION: " << parsed["capabilities"]["gl_version"].get<std::string>() << "\n";

  std::cout << "GPU_HARDWARE_GATE_REPORT_BEGIN\n";
  std::cout << jsonStr << "\n";
  // S8 (M0-C R6): flush after the END marker so the full report reaches the
  // pipe atomically. Without this, log output from test cases that run after
  // this one can interleave into the still-buffered tail of the JSON and
  // corrupt the payload the runner extracts.
  std::cout << "GPU_HARDWARE_GATE_REPORT_END\n" << std::flush;
}

TEST_CASE("[Integration] GPU Hardware Validation Gate - GL diagnostics JSON schema") {
  using namespace NoMoreDay::render::validation;

  GateReport report;
  report.revision = "TEST_REV_GLDIAG";
  report.capabilities.vendor = "TestVendor";
  report.capabilities.renderer = "TestRenderer";
  report.capabilities.debugCallbackSupported = true;
  report.capabilities.debugOutputInstalled = true;
  report.capabilities.debugOutputEnabled = true;
  report.debugOutputInstalled = true;
  report.debugOutputEnabled = true;

  GlDiagnosticRecord record;
  record.id = 1282;
  record.source = 0x824A;   // GL_DEBUG_SOURCE_APPLICATION
  record.type = 0x824C;     // GL_DEBUG_TYPE_ERROR
  record.severity = 0x9146; // GL_DEBUG_SEVERITY_HIGH
  record.elapsedMs = 42;
  record.timeUtc = "2026-08-01T00:00:00Z";
  record.message = "synthetic severe diagnostic";
  report.glDiagnostics.push_back(record);
  report.debugMessageCount = 1;
  report.severeGlErrorCount = 1;
  report.status = GateStatus::NoGo;

  const nlohmann::json parsed = nlohmann::json::parse(report.ToJsonString());
  CHECK(parsed["gate_status"] == "NO_GO");
  CHECK(parsed["capabilities"]["debug_callback"].get<bool>());
  CHECK(parsed["gl_diagnostics"]["debug_message_count"].get<int>() == 1);
  CHECK(parsed["gl_diagnostics"]["severe_error_count"].get<int>() == 1);
  CHECK(parsed["gl_diagnostics"]["messages"].size() == 1);
  const auto &message = parsed["gl_diagnostics"]["messages"][0];
  CHECK(message["id"].get<uint32_t>() == 1282);
  CHECK(message["message"].get<std::string>() == "synthetic severe diagnostic");
  CHECK(message.contains("severity"));
  CHECK(message.contains("type"));
  CHECK(message.contains("source"));
  CHECK(message.contains("time"));

  // S4 (M0-C R5.2): synthetic reports still serialize the snapshot schema as an
  // array (empty when the stress loop did not run).
  CHECK(parsed["stress_test"]["resource_snapshots"].is_array());
  CHECK(parsed["stress_test"]["resource_snapshots"].empty());

  GateReport missingCapabilityReport;
  missingCapabilityReport.revision = "TEST_REV_NOCAP";
  missingCapabilityReport.capabilities.debugCallbackSupported = false;
  missingCapabilityReport.capabilities.debugOutputInstalled = false;
  missingCapabilityReport.capabilities.debugOutputEnabled = false;
  missingCapabilityReport.status = GateStatus::NotRun;
  missingCapabilityReport.globalFailures.push_back(
      "GL debug callback unsupported (glDebugMessageCallback missing); fail-closed NOT_RUN");

  const nlohmann::json notRunJson =
      nlohmann::json::parse(missingCapabilityReport.ToJsonString());
  CHECK(notRunJson["gate_status"] == "NOT_RUN");
  CHECK_FALSE(notRunJson["capabilities"]["debug_callback"].get<bool>());
  CHECK_FALSE(notRunJson["capabilities"]["debug_output_installed"].get<bool>());
  CHECK(notRunJson["gl_diagnostics"]["messages"].is_array());
  CHECK(notRunJson["gl_diagnostics"]["messages"].empty());
}

TEST_CASE("[Integration] GPU Hardware Validation Gate - Missing driver fails closed NOT_RUN") {
  using namespace NoMoreDay::render::validation;

  if (!CreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping missing-driver test");
  }

  // S6 (T6.1/T6.2): without a FixtureRenderDriver the gate must NOT run on an
  // empty registry/SharedContext - it fails closed with NOT_RUN.
  const auto report = GPUHardwareValidationGate::RunGate("TEST_REV_NODRIVER", 120, false, 0);

  CHECK(report.status == GateStatus::NotRun);
  CHECK(report.matrixResults.empty());
  CHECK_FALSE(report.globalFailures.empty());
  // S6 (T6.2): the single fail-closed failure must reference the required
  // FixtureRenderDriver (matches GPUHardwareValidationGate.cpp message).
  CHECK(report.globalFailures.size() == 1);
  CHECK(report.globalFailures.front().find("FixtureRenderDriver") != std::string::npos);

  const nlohmann::json parsed = nlohmann::json::parse(report.ToJsonString());
  CHECK(parsed["gate_status"] == "NOT_RUN");
  CHECK(parsed["matrix_results"].empty());
}
