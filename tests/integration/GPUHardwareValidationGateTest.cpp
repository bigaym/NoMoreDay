#include "doctest.h"

#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/GPUUtils.hpp"

#include "raylib.h"

#include <nlohmann/json.hpp>
#include <iostream>

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

  const auto report = GPUHardwareValidationGate::RunGate("TEST_REV_123", 120, true, 100);

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

  std::cout << "GPU_HARDWARE_GATE_RESULT status=" << statusStr << "\n";
  std::cout << "GL_VENDOR: " << parsed["capabilities"]["vendor"].get<std::string>() << "\n";
  std::cout << "GL_RENDERER: " << parsed["capabilities"]["renderer"].get<std::string>() << "\n";
  std::cout << "GL_VERSION: " << parsed["capabilities"]["gl_version"].get<std::string>() << "\n";

  std::cout << "GPU_HARDWARE_GATE_REPORT_BEGIN\n";
  std::cout << jsonStr << "\n";
  std::cout << "GPU_HARDWARE_GATE_REPORT_END\n";
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
