#pragma once

#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace NoMoreDay::render::validation {

enum class GateStatus {
  Go,
  NoGo,
  NotRun
};

struct HardwareCapabilityReport {
  std::string vendor;
  std::string renderer;
  std::string driverVersion;
  std::string glVersion;
  bool computeShaderSupported{false};
  bool ssboSupported{false};
  bool persistentMappingSupported{false};
  bool indirectDrawSupported{false};
  bool timerQuerySupported{false};
  bool textureArraySupported{false};
  bool rgba16fSupported{false};
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
  std::vector<std::string> failureReasons;
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
};

struct GateReport {
  std::string revision;
  std::string timestamp;
  HardwareCapabilityReport capabilities;
  std::vector<FixtureExecutionResult> matrixResults;
  StressTestReport stressReport;
  uint64_t totalTrackedBytes{0};
  uint64_t peakTrackedBytes{0};
  size_t activeResourceCount{0};
  size_t leakCandidateCount{0};
  int debugMessageCount{0};
  int severeGlErrorCount{0};
  GateStatus status{GateStatus::NotRun};
  std::vector<std::string> globalFailures;

  [[nodiscard]] std::string ToJsonString() const;
};

class GPUHardwareValidationGate {
public:
  [[nodiscard]] static HardwareCapabilityReport QueryCapabilities();
  [[nodiscard]] static std::vector<FixtureConfig> GetStandardFixtures();

  [[nodiscard]] static GateReport RunGate(const std::string &revision = "HEAD",
                                          int sampleFramesPerFixture = 120,
                                          bool stressTest1Min = false,
                                          int toggleLoops = 100);
};

} // namespace NoMoreDay::render::validation
