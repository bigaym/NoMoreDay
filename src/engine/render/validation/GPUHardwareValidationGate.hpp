#pragma once

#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace NoMoreDay::render::validation {

class FixtureRenderDriver;

enum class GateStatus {
  Go,
  NoGo,
  NotRun
};

struct GlDiagnosticRecord {
  uint32_t id{0};
  uint32_t source{0};
  uint32_t type{0};
  uint32_t severity{0};
  uint64_t elapsedMs{0};
  std::string timeUtc;
  std::string message;
};

struct HardwareCapabilityReport {
  std::string vendor;
  std::string renderer;
  bool rendererIsHardware{false};
  std::string driverVersion;
  std::string glVersion;
  bool computeShaderSupported{false};
  bool ssboSupported{false};
  bool persistentMappingSupported{false};
  bool indirectDrawSupported{false};
  bool timerQuerySupported{false};
  bool textureArraySupported{false};
  bool rgba16fSupported{false};
  bool debugCallbackSupported{false};
  bool debugOutputInstalled{false};
  bool debugOutputEnabled{false};
  bool meetsPreflightPrerequisites{false};
  std::string preflightFailureReason;
};

struct FixtureConfig {
  std::string name;
  std::string description;
  uint32_t sceneSeed{0};
  float cameraX{0.0f};
  float cameraY{0.0f};
  float cameraZoom{1.0f};
  int roiX{400};
  int roiY{200};
  int roiWidth{480};
  int roiHeight{320};
  int width{1280};
  int height{720};
  int warmupFrames{10};
  int sampleFrames{120};
};

struct PassTimingReport {
  std::string passName;
  uint32_t validSampleCount{0};
  double meanMs{0.0};
  double p95Ms{0.0};
  uint32_t pendingCount{0};
  uint32_t unavailableCount{0};
  uint32_t cpuFallbackCount{0};
  double budgetMs{0.0};
  bool passed{false};
};

struct FixtureExecutionResult {
  std::string fixtureName;
  std::string qualityTier;
  bool giEnabled{true};
  int width{1280};
  int height{720};
  // W6 (M0-C): camera and ROI recorded per matrix cell so the artifact pins the
  // exact reproducible input (fixed seed/ROI/camera for the hardware gate).
  float cameraX{0.0f};
  float cameraY{0.0f};
  float cameraZoom{1.0f};
  int roiX{0};
  int roiY{0};
  int roiWidth{0};
  int roiHeight{0};
  bool passTraceValid{false};
  // W6 (M0-C): the pass trace is the REAL executed pass order of the last
  // RenderSystem::render frame (RenderGraph::CompiledRenderPlan.passOrder
  // captured inside render()) - never a synthetic test graph. The source is
  // pinned in the artifact for provenance.
  std::string passTraceSource;
  std::vector<std::string> executedPassOrder;
  bool nonBlackRoiPassed{false};
  float roiMeanBrightness{0.0f};
  bool giIndirectPassed{false};
  // W6 (M0-C): per-cell paired GI delta capture (GI runtime override ON vs OFF
  // legs on the real fixture scene) is part of the cell verdict.
  float giPairedDelta{0.0f};
  bool giPairedPassed{false};
  // W6 (M0-C): real SDF evidence. sdfReadbackStatus is "passed" (real JFAPass
  // distance field read back and spatially valid), "not_applicable" (GI-off
  // cell: no JFA pass runs), "missing" (GI-on but no distance field produced)
  // or "failed" (readback/probe failed). GI-on cells must be "passed".
  std::string sdfReadbackStatus;
  std::string sdfEvidenceSource;
  float sdfMinValue{0.0f};
  float sdfMaxValue{0.0f};
  float sdfMeanValue{0.0f};
  // Five sign-probe samples of the distance channel: 4 corners + center.
  std::vector<float> sdfProbeSamples;
  bool sdfReadbackPassed{false};
  std::vector<PassTimingReport> passTimings;
  uint64_t trackedBytes{0};
  uint64_t peakTrackedBytes{0};
  bool overallPassed{false};
  // S6 (T6.5): artifact/version contract - fixture input hash, recipe version
  // and provenance are recorded alongside the gate output for reproducibility.
  uint64_t sceneInputHash{0};
  std::string fixtureVersion;
  std::string sceneSource;
  std::vector<std::string> failureReasons;
};

// S4 (M0-C R5.2): one quiescence snapshot taken at a frame boundary during the
// pressure loop. `bytesNetGrowth` / `countNetGrowth` are the sliding-window-mean
// deltas versus the baseline mean; violations fail the stress test fail-closed.
struct StressResourceSnapshot {
  uint64_t frameIndex{0};
  uint64_t timestampMs{0};
  size_t activeResourceCount{0};
  size_t currentTotalBytes{0};
  size_t peakTotalBytes{0};
  size_t totalCreatedCount{0};
  size_t totalDestroyedCount{0};
  size_t liveReferenceCount{0};
  size_t pendingReferenceCount{0};
  uint32_t pendingQueryOverageCount{0};
  int64_t bytesNetGrowth{0};
  int64_t countNetGrowth{0};
  bool netGrowthViolation{false};
  bool pendingOverageViolation{false};
};

struct StressTestReport {
  bool stress1MinPassed{false};
  double durationSeconds{0.0};
  uint64_t startTrackedBytes{0};
  uint64_t endTrackedBytes{0};
  uint64_t peakTrackedBytes{0};
  size_t leakCandidateCount{0};
  bool toggleStress100LoopsPassed{false};
  int severeGlErrorCount{0};
  std::vector<StressResourceSnapshot> resourceSnapshots;
};

// S7b: paired GI delta capture. One deterministic scene fixture is captured
// twice - GI runtime override ON vs OFF - under identical seed/camera/frame/
// FBO/color-space/ROI, each leg with its own temporal warmup and independent
// sampling window. The delta is the mean over the sampling window of the
// absolute per-frame ROI mean-brightness difference between the two legs.
struct PairedGiDeltaResult {
  std::string fixtureName;
  uint32_t sceneSeed{0};
  int width{1280};
  int height{720};
  std::string colorSpace;
  int roiX{0};
  int roiY{0};
  int roiWidth{0};
  int roiHeight{0};
  std::string renderer;
  bool rendererIsHardware{false};
  // W6 (M0-C): the quality tier the paired capture actually ran under (each
  // matrix cell captures its own pair at the cell's tier).
  std::string qualityTier;
  int warmupFrames{0};
  int sampleFrames{0};
  float roiMeanOn{0.0f};
  float roiMeanOff{0.0f};
  float pairedDelta{0.0f};
  float threshold{0.001f};
  bool passed{false};
  uint64_t trackedBytesOn{0};
  uint64_t trackedBytesOff{0};
  std::vector<std::string> legPassTraces;
  std::vector<std::string> failureReasons;
};

struct GateReport {
  std::string revision;
  std::string timestamp;
  HardwareCapabilityReport capabilities;
  // W6 (M0-C): hooks binding evidence - true when the driver supplied real
  // gameplay render hooks. Serialized under capabilities.render_hooks_supplied.
  bool hooksSupplied{false};
  // W6 (M0-C) Medium-5: explicit below-floor parameters are honored verbatim
  // (diagnostic runs) and recorded as requested/actual; production defaults
  // stay at the 120/100 floor. A non-exhaustive run can never yield GO.
  int requestedSampleFrames{0};
  int actualSampleFrames{0};
  int requestedToggleLoops{0};
  int actualToggleLoops{0};
  bool nonExhaustive{false};
  // W6 (M0-C) Blocker-1: occupancy/disocclusion probes (M0-A R3) are not
  // implemented; the gate records status "missing_pending_m0a" and is
  // fail-closed (can never GO) until they land.
  std::string occupancyStatus;
  std::string occupancyReason;
  std::vector<FixtureExecutionResult> matrixResults;
  std::vector<PairedGiDeltaResult> pairedGiDeltas;
  StressTestReport stressReport;
  uint64_t totalTrackedBytes{0};
  uint64_t peakTrackedBytes{0};
  size_t activeResourceCount{0};
  size_t leakCandidateCount{0};
  int debugMessageCount{0};
  int severeGlErrorCount{0};
  size_t glDiagnosticsDroppedCount{0};
  bool debugOutputInstalled{false};
  bool debugOutputEnabled{false};
  std::vector<GlDiagnosticRecord> glDiagnostics;
  GateStatus status{GateStatus::NotRun};
  std::vector<std::string> globalFailures;

  [[nodiscard]] std::string ToJsonString() const;
};

class GPUHardwareValidationGate {
public:
  [[nodiscard]] static HardwareCapabilityReport QueryCapabilities();
  [[nodiscard]] static std::vector<FixtureConfig> GetStandardFixtures();

  // W6 (M0-C) High-2: pure CPU ROI crop/mean helper. Crops [roiX,roiY,w,h] out
  // of a full RGBA8 frame and returns the mean normalized RGB brightness. It
  // exists so the non-zero-ROI-origin contract is unit-testable without a GPU;
  // the GPU readback path samples the full FBO and delegates here.
  [[nodiscard]] static float ComputeRoiMeanLuma(const uint8_t *fullRgba,
                                                size_t fullSizeBytes, int fullW,
                                                int fullH, int roiX, int roiY,
                                                int roiW, int roiH);

  // S7b: performs one paired GI delta capture for the given fixture on the
  // driver's real gameplay scene (GI runtime override ON vs OFF legs, each with
  // its own temporal warmup and independent sampling window). `qualityTier`
  // selects the tier the capture runs under (default "High"; matrix cells pass
  // their own tier so the paired evidence is per-cell).
  [[nodiscard]] static PairedGiDeltaResult RunPairedGiDeltaCapture(
      FixtureRenderDriver &driver, const FixtureConfig &fixture,
      const std::string &qualityTier = "High");

  [[nodiscard]] static GateReport RunGate(const std::string &revision = "HEAD",
                                          int sampleFramesPerFixture = 120,
                                          bool stressTest1Min = true,
                                          int toggleLoops = 100,
                                          FixtureRenderDriver *driver = nullptr);
};

} // namespace NoMoreDay::render::validation
