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
}
