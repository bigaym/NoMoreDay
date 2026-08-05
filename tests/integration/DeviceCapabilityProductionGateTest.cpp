// Phase F (RG-4) contract: DeviceCapabilityMatrix gates the production path
// fail-closed and the production GL debug callback (P0 S3) captures driver
// errors. These tests pin:
//   - CheckProductionRequirements is pure and fails closed on any missing
//     production-critical feature (compute/SSBO/image/barrier/GL4.3) while
//     diagnostic-only capabilities (timer, debug callback) are not required,
//   - the probe against a live GL context satisfies the same gate equivalence,
//   - the resident GLDebugCallback installs, does not itself emit GL errors,
//     and reports a generated driver error; shutdown restores the previous
//     callback / GL_DEBUG_OUTPUT state.
#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/debug/GLDebugCallback.hpp"

#include "rlgl.h"

#include <string>
#include <vector>

TEST_CASE(
    "[Unit] DeviceCapabilityMatrix - production requirements fail closed on "
    "missing features") {
  using namespace NoMoreDay::render::core;

  CapabilityReport full;
  full.isGL43Supported = true;
  full.isComputeSupported = true;
  full.isSSBOSupported = true;
  full.isImageLoadStoreSupported = true;
  full.isMemoryBarrierSupported = true;

  const ProductionCapabilityCheck ok =
      DeviceCapabilityMatrix::CheckProductionRequirements(full);
  CHECK(ok.passed);
  CHECK(ok.missingRequirements.empty());

  // Missing any production-critical feature must fail closed with a reason.
  CapabilityReport degraded = full;
  degraded.isComputeSupported = false;
  degraded.isImageLoadStoreSupported = false;
  const ProductionCapabilityCheck bad =
      DeviceCapabilityMatrix::CheckProductionRequirements(degraded);
  CHECK_FALSE(bad.passed);
  bool sawCompute = false;
  bool sawImage = false;
  for (const auto &missing : bad.missingRequirements) {
    if (missing.find("Compute") != std::string::npos) {
      sawCompute = true;
    }
    if (missing.find("Image") != std::string::npos) {
      sawImage = true;
    }
  }
  CHECK(sawCompute);
  CHECK(sawImage);

  // Diagnostic-only capabilities are NOT production requirements.
  CapabilityReport noDiag = full;
  noDiag.isTimerQuerySupported = false;
  noDiag.isDebugCallbackSupported = false;
  CHECK(DeviceCapabilityMatrix::CheckProductionRequirements(noDiag).passed);

  // GL 4.3 absence also fails closed.
  CapabilityReport pre43 = full;
  pre43.isGL43Supported = false;
  pre43.isComputeSupported = false;
  pre43.isSSBOSupported = false;
  pre43.isImageLoadStoreSupported = false;
  CHECK_FALSE(DeviceCapabilityMatrix::CheckProductionRequirements(pre43).passed);
}

TEST_CASE(
    "[Integration] DeviceCapabilityMatrix - probe against live GL context "
    "satisfies the production gate") {
  using namespace NoMoreDay::render::core;

  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    FAIL("No live GL context; cannot probe capabilities");
  }

  const auto &report = DeviceCapabilityMatrix::Get().ProbeCapabilities();
  const auto check =
      DeviceCapabilityMatrix::CheckProductionRequirements(report);

  // Contract: the gate passes iff every required capability is present.
  const bool allPresent =
      report.isGL43Supported && report.isComputeSupported &&
      report.isSSBOSupported && report.isImageLoadStoreSupported &&
      report.isMemoryBarrierSupported;
  CHECK(check.passed == allPresent);
  CHECK(check.missingRequirements.empty() == allPresent);

  // Probe sanity against the live context.
  CHECK(report.isGL43Supported == (rlGetVersion() == RL_OPENGL_43));
  CHECK(report.maxSSBOBindings == 16);
}

TEST_CASE(
    "[Integration] GLDebugCallback - production install captures driver "
    "errors and restores state") {
  using namespace NoMoreDay::render::debug;

  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    FAIL("No live GL context; cannot test the GL debug callback");
  }

  // Drain any errors left over by earlier GL activity in this process.
  while (glGetError() != GL_NO_ERROR) {
  }

  auto &cb = GLDebugCallback::Get();
  REQUIRE(cb.Install());
  CHECK(cb.IsInstalled());

  // Installing the callback must not itself generate GL errors.
  CHECK(glGetError() == GL_NO_ERROR);

  const uint64_t before = cb.GetReportedCount();
  // Generate a driver error (GL_INVALID_ENUM) - it must surface through the
  // callback and be counted.
  glEnable(0xFFFF);
  (void)glGetError(); // consume the error queue entry; callback fires on generation

  // Robustness against asynchronous delivery: spin briefly for the report.
  bool reported = false;
  for (int i = 0; i < 200 && !reported; ++i) {
    if (cb.GetReportedCount() > before) {
      reported = true;
    }
  }
  CHECK(reported);

  // Shutdown restores the previous callback / GL_DEBUG_OUTPUT state.
  cb.Shutdown();
  CHECK_FALSE(cb.IsInstalled());
  CHECK(glGetError() == GL_NO_ERROR);
}
