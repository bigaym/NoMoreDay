#include "doctest.h"

#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/GPUUtils.hpp"
#include "GameplayRuntimeHarness.hpp"

#include "raylib.h"

#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

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
  // W6 (M0-C) High-3: real GPU identity must be reported (no hardcoded label).
  CHECK_FALSE(caps.vendor.empty());
  CHECK_FALSE(caps.driverVersion.empty());
  CHECK_FALSE(caps.renderer.empty());
}

// W6 (M0-C) High-2: the ROI readback must sample at the declared ROI origin,
// not always at (0,0). This is a GPU-free test of the CPU crop/mask path that
// the full-FBO readback delegates to.
TEST_CASE("[Unit] GPU Hardware Validation Gate - ROI origin crop correctness") {
  using namespace NoMoreDay::render::validation;

  // 4x4 RGBA8 frame: a bright 2x2 block at origin (2,2); everywhere else black.
  constexpr int kFullW = 4;
  constexpr int kFullH = 4;
  std::vector<uint8_t> frame(static_cast<size_t>(kFullW * kFullH * 4), 0);
  for (int y = 2; y < 4; ++y) {
    for (int x = 2; x < 4; ++x) {
      const size_t idx =
          (static_cast<size_t>(y) * kFullW + static_cast<size_t>(x)) * 4;
      frame[idx] = 255;
      frame[idx + 1] = 255;
      frame[idx + 2] = 255;
      frame[idx + 3] = 255;
    }
  }

  // ROI at (2,2) 2x2 overlaps the bright block -> mean brightness > 0.
  const float brightRoi = GPUHardwareValidationGate::ComputeRoiMeanLuma(
      frame.data(), frame.size(), kFullW, kFullH, 2, 2, 2, 2);
  CHECK(brightRoi > 0.0f);
  CHECK(brightRoi <= 1.0f);

  // ROI at (0,0) 2x2 covers only black texels -> exactly 0. If the readback
  // ignored the origin and sampled the top-left, this would wrongly be bright.
  const float blackRoi = GPUHardwareValidationGate::ComputeRoiMeanLuma(
      frame.data(), frame.size(), kFullW, kFullH, 0, 0, 2, 2);
  CHECK(blackRoi == 0.0f);

  // Out-of-bounds ROI is rejected (returns 0) instead of reading garbage.
  const float oobRoi = GPUHardwareValidationGate::ComputeRoiMeanLuma(
      frame.data(), frame.size(), kFullW, kFullH, 3, 3, 2, 2);
  CHECK(oobRoi == 0.0f);
}

// W6 (M0-C) occupancy evidence (M0-A R3): the pure-CPU 0/1-mask classifier is
// GPU-free, so the mask contract (every texel finite, min/max/mean sane, and
// the 5-point probe sitting within epsilon of {0,1}) is unit-testable without
// touching a GPU. The GPU readback path (ProbeGiOccupancy) delegates here.
TEST_CASE("[Unit] GPU Hardware Validation Gate - ClassifyOccupancyProbe 0/1 mask contract") {
  using namespace NoMoreDay::render::validation;

  // All-zeros mask: a valid 0/1 mask (empty scene).
  {
    std::vector<float> texels(4 * 4, 0.0f);
    const OccupancyProbeResult r = GPUHardwareValidationGate::ClassifyOccupancyProbe(
        texels.data(), texels.size(), 4, 4);
    CAPTURE(r.reason);
    CHECK(r.texturePresent);
    CHECK(r.maskValid);
    CHECK(r.minValue == 0.0f);
    CHECK(r.maxValue == 0.0f);
    CHECK(r.meanValue == 0.0f);
    CHECK(r.probeSamples.size() == 5);
  }

  // All-ones mask: a valid 0/1 mask (fully occluded scene).
  {
    std::vector<float> texels(4 * 4, 1.0f);
    const OccupancyProbeResult r = GPUHardwareValidationGate::ClassifyOccupancyProbe(
        texels.data(), texels.size(), 4, 4);
    CAPTURE(r.reason);
    CHECK(r.texturePresent);
    CHECK(r.maskValid);
    CHECK(r.minValue == 1.0f);
    CHECK(r.maxValue == 1.0f);
    CHECK(r.meanValue == 1.0f);
  }

  // Mixed 0/1 mask with the probe points exactly on {0,1} values.
  {
    std::vector<float> texels(4 * 4, 0.0f);
    // Corners + center of the 4x4 probe are set to 1; interior stays 0.
    texels[0] = 1.0f;              // (0,0)
    texels[3] = 1.0f;              // (3,0)
    texels[4 * 3 + 0] = 1.0f;      // (0,3)
    texels[4 * 3 + 3] = 1.0f;      // (3,3)
    texels[4 * 2 + 2] = 1.0f;      // (2,2)
    const OccupancyProbeResult r = GPUHardwareValidationGate::ClassifyOccupancyProbe(
        texels.data(), texels.size(), 4, 4);
    CAPTURE(r.reason);
    CHECK(r.texturePresent);
    CHECK(r.maskValid);
    CHECK(r.minValue == 0.0f);
    CHECK(r.maxValue == 1.0f);
    CHECK(r.meanValue > 0.0f);
    CHECK(r.meanValue < 1.0f);
    for (const float p : r.probeSamples) {
      CHECK(p == 1.0f);
    }
  }

  // Non-finite texel: never a valid mask - fails closed.
  {
    std::vector<float> texels(4 * 4, 0.0f);
    texels[5] = std::numeric_limits<float>::quiet_NaN();
    const OccupancyProbeResult r = GPUHardwareValidationGate::ClassifyOccupancyProbe(
        texels.data(), texels.size(), 4, 4);
    CAPTURE(r.reason);
    CHECK_FALSE(r.texturePresent);
    CHECK_FALSE(r.maskValid);
    CHECK(r.reason.find("non-finite") != std::string::npos);
  }

  // Inf texel also fails closed.
  {
    std::vector<float> texels(4 * 4, 0.0f);
    texels[7] = std::numeric_limits<float>::infinity();
    const OccupancyProbeResult r = GPUHardwareValidationGate::ClassifyOccupancyProbe(
        texels.data(), texels.size(), 4, 4);
    CAPTURE(r.reason);
    CHECK_FALSE(r.texturePresent);
    CHECK_FALSE(r.maskValid);
  }

  // Dimension mismatch: texel count must equal width * height - fails closed.
  {
    std::vector<float> texels(4 * 5, 0.0f);
    const OccupancyProbeResult r = GPUHardwareValidationGate::ClassifyOccupancyProbe(
        texels.data(), texels.size(), 4, 4);
    CAPTURE(r.reason);
    CHECK_FALSE(r.texturePresent);
    CHECK_FALSE(r.maskValid);
    CHECK(r.reason.find("dimensions") != std::string::npos);
  }

  // Null/empty input fails closed.
  {
    const OccupancyProbeResult r = GPUHardwareValidationGate::ClassifyOccupancyProbe(
        nullptr, 0, 4, 4);
    CAPTURE(r.reason);
    CHECK_FALSE(r.texturePresent);
    CHECK_FALSE(r.maskValid);
  }

  // A gradient-like (non-binary) probe point fails the mask check.
  {
    std::vector<float> texels(4 * 4, 0.0f);
    texels[0] = 0.5f;  // (0,0) sits between 0 and 1 - not a 0/1 mask.
    const OccupancyProbeResult r = GPUHardwareValidationGate::ClassifyOccupancyProbe(
        texels.data(), texels.size(), 4, 4);
    CAPTURE(r.reason);
    CHECK(r.texturePresent);
    CHECK_FALSE(r.maskValid);
    CHECK(r.reason.find("0/1 mask") != std::string::npos);
  }
}

// W6 (M0-C) occupancy evidence verdict (M0-A R3): fail-closed judgement. The
// gate may only report "present" when a real texture + a valid 0/1 mask probe
// + a positive history reset count (proof temporal rejection occurred) all hold.
TEST_CASE("[Unit] GPU Hardware Validation Gate - EvaluateOccupancyEvidence fail-closed verdict") {
  using namespace NoMoreDay::render::validation;

  // Helper: a valid probe (as if a real R8 history was read back and classified).
  auto validProbe = [] {
    OccupancyProbeResult p;
    p.texturePresent = true;
    p.maskValid = true;
    p.minValue = 0.0f;
    p.maxValue = 1.0f;
    p.meanValue = 0.5f;
    p.probeSamples = {0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
    return p;
  };

  // No texture exposed -> failed, fail-closed (blocks GO).
  {
    const OccupancyEvidenceResult r = GPUHardwareValidationGate::EvaluateOccupancyEvidence(
        0u, 0, 0, validProbe(), 3, "extent changed");
    CHECK(r.status == "failed");
    CHECK(r.blocksGo);
    CHECK_FALSE(r.texturePresent);
    CHECK(r.reason.find("no occupancy history texture") != std::string::npos);
  }

  // Texture present but the mask probe failed -> failed, fail-closed.
  {
    OccupancyProbeResult badProbe;
    badProbe.texturePresent = false;
    badProbe.reason = "glGetTexImage unavailable";
    const OccupancyEvidenceResult r = GPUHardwareValidationGate::EvaluateOccupancyEvidence(
        42u, 1280, 720, badProbe, 3, "extent changed");
    CHECK(r.status == "failed");
    CHECK(r.blocksGo);
    CHECK(r.texturePresent);
    CHECK(r.reason.find("probe failed") != std::string::npos);
  }

  // Texture + valid mask but reset count == 0 -> failed (no rejection proof).
  {
    const OccupancyEvidenceResult r = GPUHardwareValidationGate::EvaluateOccupancyEvidence(
        42u, 1280, 720, validProbe(), 0, "");
    CHECK(r.status == "failed");
    CHECK(r.blocksGo);
    CHECK(r.texturePresent);
    CHECK(r.reason.find("never reset") != std::string::npos);
  }

  // Texture + valid mask + positive reset count -> present, GO allowed.
  {
    const OccupancyEvidenceResult r = GPUHardwareValidationGate::EvaluateOccupancyEvidence(
        42u, 1280, 720, validProbe(), 4, "extent changed");
    CHECK(r.status == "present");
    CHECK_FALSE(r.blocksGo);
    CHECK(r.texturePresent);
    CHECK(r.width == 1280);
    CHECK(r.height == 720);
    CHECK(r.historyResetCount == 4);
    CHECK(r.lastResetReason == "extent changed");
  }
}

// M0-B: external target contract - CaptureTargetState must read the REAL
// attachment state of the harness-owned RGBA16F composite target through legal
// GL 4.3 pnames (glGetFramebufferAttachmentParameteriv + texture-level
// parameters). The pseudo-pnames 5c257e22 removed (0x8D24/0x8D25/0x825D) are
// never reused. A fully verified capture is "passed"; the identity, extent,
// format and component layout are pinned in the recorded state.
TEST_CASE("[Integration] GPU Hardware Validation Gate - Target state capture verifies composite attachment") {
  using namespace NoMoreDay::render::validation;

  if (!CreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping target state capture test");
  }

  const auto fixtures = GPUHardwareValidationGate::GetStandardFixtures();
  REQUIRE_FALSE(fixtures.empty());

  GameplayRuntimeHarness harness;
  REQUIRE(harness.PrepareFixture(fixtures[0]));

  const uint32_t fbo = harness.CompositeFramebuffer();
  REQUIRE(fbo != 0);

  const TargetAttachmentState state =
      GPUHardwareValidationGate::CaptureTargetState(
          fbo, fixtures[0].width, fixtures[0].height, 0x881A);

  CAPTURE(state.status);
  CAPTURE(state.reason);
  CHECK(state.status == "passed");
  CHECK(state.captured);
  CHECK(state.expectedInternalFormat == 0x881A);
  // The harness attaches a texture (GL_TEXTURE = 0x1702), never a renderbuffer.
  CHECK(state.attachmentObjectType == 0x1702);
  CHECK(state.attachmentObjectName != 0);
  CHECK(state.attachmentWidth == fixtures[0].width);
  CHECK(state.attachmentHeight == fixtures[0].height);
  CHECK(state.attachmentInternalFormat == 0x881A);
  // RGBA16F = GL_FLOAT (0x1406) components, GL_LINEAR (0x2601) encoding, 16-bit
  // RGBA channels.
  CHECK(state.componentType == 0x1406);
  CHECK(state.colorEncoding == 0x2601);
  CHECK(state.redSize == 16);
  CHECK(state.greenSize == 16);
  CHECK(state.blueSize == 16);
  CHECK(state.alphaSize == 16);
  // Bind state must be restored to the pre-capture binding (0 here) so the
  // capture never leaks GL state into the render loop.
  CHECK(state.framebufferBinding == 0);

  // Fail-closed paths: fbo == 0 is rejected verbatim, never default-filled.
  const TargetAttachmentState invalid =
      GPUHardwareValidationGate::CaptureTargetState(0, 1280, 720, 0x881A);
  CHECK(invalid.status == "failed");
  CHECK_FALSE(invalid.captured);
  CHECK_FALSE(invalid.reason.empty());
}

// W6 (M0-C): this test is contract/diagnostic only - it runs the RunGate
// matrix against a 1x1 hidden context + GameplayRuntimeHarness (no real
// Game/App initialization, no RenderSystem::Initialize). It is NOT production
// evidence: production gate = NoMoreDay.exe --gpu-gate. It is excluded from
// broad ci;nonperf and generic integration via the [GPU-Diagnostic] prefix and
// runs under nmd.tests.gpu.diagnostic with a minimal non-exhaustive sample
// budget (NMD_GATE_SAMPLES=3, NMD_GATE_TOGGLE_LOOPS=1, NMD_GATE_STRESS=0).
// doctest success here never means the hardware gate passed.
TEST_CASE("[GPU-Diagnostic] GPU Hardware Validation Gate - RunGate Offscreen Matrix & Preflight Output") {
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
      // W6 (M0-C): each matrix cell pins the reproducible camera and ROI.
      CHECK(fix.contains("camera"));
      CHECK(fix["camera"].contains("target_x"));
      CHECK(fix["camera"].contains("target_y"));
      CHECK(fix["camera"].contains("zoom"));
      CHECK(fix.contains("roi"));
      CHECK(fix["roi"].contains("x"));
      CHECK(fix["roi"].contains("y"));
      CHECK(fix["roi"].contains("width"));
      CHECK(fix["roi"].contains("height"));
      // W6 (M0-C) Blocker-1: real pass trace + per-cell paired GI + SDF evidence
      // are pinned per cell.
      CHECK(fix.contains("pass_trace_source"));
      CHECK_FALSE(fix["pass_trace_source"].get<std::string>().empty());
      CHECK(fix["executed_pass_order"].is_array());
      CHECK(fix.contains("gi_paired_delta"));
      CHECK(fix.contains("gi_paired_passed"));
      CHECK(fix.contains("sdf_readback_status"));
      CHECK(fix.contains("sdf_evidence_source"));
      CHECK_FALSE(fix["sdf_evidence_source"].get<std::string>().empty());
      CHECK(fix.contains("sdf_min_value"));
      CHECK(fix.contains("sdf_max_value"));
      CHECK(fix.contains("sdf_mean_value"));
      CHECK(fix["sdf_probe_samples"].is_array());
      // M0-B: every matrix cell that ran a target captured its external target
      // state; the RGBA16F contract must verify ("passed", fail-closed).
      CHECK(fix.contains("target_state"));
      const auto &ts = fix["target_state"];
      CHECK(ts.contains("captured"));
      CHECK(ts.contains("status"));
      CHECK(ts.contains("reason"));
      CHECK(ts.contains("expected_internal_format"));
      CHECK(ts.contains("framebuffer_binding"));
      CHECK(ts.contains("viewport"));
      CHECK(ts.contains("scissor_test_enabled"));
      CHECK(ts.contains("scissor"));
      CHECK(ts.contains("attachment_object_type"));
      CHECK(ts.contains("attachment_object_name"));
      CHECK(ts.contains("attachment_width"));
      CHECK(ts.contains("attachment_height"));
      CHECK(ts.contains("attachment_internal_format"));
      CHECK(ts.contains("color_encoding"));
      CHECK(ts.contains("component_type"));
      CHECK(ts.contains("red_size"));
      CHECK(ts.contains("green_size"));
      CHECK(ts.contains("blue_size"));
      CHECK(ts.contains("alpha_size"));
      CHECK(ts.contains("depth_size"));
      CHECK(ts.contains("stencil_size"));
      CHECK(ts["status"].get<std::string>() == "passed");
      CHECK(ts["expected_internal_format"].get<uint32_t>() == 0x881A);
      CHECK(ts["attachment_internal_format"].get<uint32_t>() == 0x881A);
      CHECK(ts["attachment_object_type"].get<uint32_t>() == 0x1702);
      CHECK(ts["attachment_width"].get<int>() > 0);
      CHECK(ts["attachment_height"].get<int>() > 0);
    }
  }

  // W6 (M0-C): run_config pins requested vs actual budget + non-exhaustive
  // flag; occupancy records the fail-closed M0-A R3 state.
  CHECK(parsed.contains("run_config"));
  CHECK(parsed["run_config"].contains("requested_sample_frames"));
  CHECK(parsed["run_config"].contains("actual_sample_frames"));
  CHECK(parsed["run_config"].contains("requested_toggle_loops"));
  CHECK(parsed["run_config"].contains("actual_toggle_loops"));
  CHECK(parsed["run_config"].contains("non_exhaustive"));
  // W6 (M0-C) Medium-5: explicit below-floor parameters are honored verbatim
  // (no clamping), so actual must equal the env-driven requested value.
  CHECK(parsed["run_config"]["actual_sample_frames"].get<int>() ==
        parsed["run_config"]["requested_sample_frames"].get<int>());
  CHECK(parsed["run_config"]["actual_toggle_loops"].get<int>() ==
        parsed["run_config"]["requested_toggle_loops"].get<int>());
  CHECK(parsed["run_config"]["non_exhaustive"].is_boolean());
  CHECK(parsed.contains("occupancy"));
  CHECK(parsed["occupancy"].contains("status"));
  CHECK(parsed["occupancy"].contains("reason"));
  CHECK(parsed["occupancy"].contains("blocks_go"));
  // W6 (M0-C) occupancy evidence (M0-A R3): the probe detail schema is always
  // present. The diagnostic harness runs without RenderSystem::Initialize, so
  // no real occupancy history exists and the gate MUST stay fail-closed
  // (never "present", blocks_go stays true) - silent pass-through is forbidden.
  CHECK(parsed["occupancy"].contains("texture_present"));
  CHECK(parsed["occupancy"].contains("probe_width"));
  CHECK(parsed["occupancy"].contains("probe_height"));
  CHECK(parsed["occupancy"].contains("min_value"));
  CHECK(parsed["occupancy"].contains("max_value"));
  CHECK(parsed["occupancy"].contains("mean_value"));
  CHECK(parsed["occupancy"].contains("probe_points"));
  CHECK(parsed["occupancy"].contains("reset_count"));
  CHECK(parsed["occupancy"].contains("last_reset_reason"));
  CHECK(parsed["occupancy"]["status"].get<std::string>() != "present");
  CHECK(parsed["occupancy"]["blocks_go"].get<bool>());
  // W6 (M0-C) High-3: capabilities carry the hooks binding + real identity.
  CHECK(parsed["capabilities"].contains("render_hooks_supplied"));
  CHECK(parsed["capabilities"].contains("driver_version"));

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
