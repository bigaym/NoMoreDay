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
  bool passTraceValid{false};
  bool nonBlackRoiPassed{false};
  float roiMeanBrightness{0.0f};
  bool giIndirectPassed{false};
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

  // S7b: performs one paired GI delta capture for the given fixture on the
  // driver's real gameplay scene (GI runtime override ON vs OFF legs, each with
  // its own temporal warmup and independent sampling window).
  [[nodiscard]] static PairedGiDeltaResult RunPairedGiDeltaCapture(
      FixtureRenderDriver &driver, const FixtureConfig &fixture);

  [[nodiscard]] static GateReport RunGate(const std::string &revision = "HEAD",
                                          int sampleFramesPerFixture = 120,
                                          bool stressTest1Min = true,
                                          int toggleLoops = 100,
                                          FixtureRenderDriver *driver = nullptr);
};

} // namespace NoMoreDay::render::validation
