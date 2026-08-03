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

// M0-B: external target contract. The gate captures the REAL state of the
// external composite target (owned by the fixture driver/harness, never by the
// gate) with legal GL 4.3 pnames - glGetFramebufferAttachmentParameteriv
// (OBJECT_TYPE/OBJECT_NAME/COLOR_ENCODING/COMPONENT_TYPE/*_SIZE) plus
// texture-level / renderbuffer parameter queries for extent and internal
// format. The bind/viewport/scissor snapshot is captured alongside the
// attachment identity so a later reader can verify what was actually rendered
// into. Fail-closed: a missing entry point is "unavailable", an invalid FBO /
// absent attachment / contract mismatch is "failed"; only a fully verified
// capture yields status "passed". Nothing is default-filled.
struct TargetAttachmentState {
  bool captured{false};
  std::string status;                  // "passed" | "failed" | "unavailable"
  std::string reason;                  // fail-closed reason (empty when passed)
  uint32_t expectedInternalFormat{0};  // contract format the target must have
  uint32_t framebufferBinding{0};      // GL_FRAMEBUFFER_BINDING at capture time
  int viewportX{0};
  int viewportY{0};
  int viewportWidth{0};
  int viewportHeight{0};
  bool scissorTestEnabled{false};
  int scissorX{0};
  int scissorY{0};
  int scissorWidth{0};
  int scissorHeight{0};
  uint32_t attachmentObjectType{0};    // GL_NONE/GL_TEXTURE/GL_RENDERBUFFER
  uint32_t attachmentObjectName{0};    // texture/renderbuffer name (0 if none)
  int attachmentWidth{0};
  int attachmentHeight{0};
  uint32_t attachmentInternalFormat{0};
  uint32_t colorEncoding{0};
  uint32_t componentType{0};
  int redSize{0};
  int greenSize{0};
  int blueSize{0};
  int alphaSize{0};
  int depthSize{0};
  int stencilSize{0};
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
  // M0-B: the external composite target state captured for this matrix cell
  // (attachment identity/extent/format + bind/viewport/scissor). Cells that
  // never ran a target keep this defaulted (absent from the JSON artifact).
  TargetAttachmentState targetState;
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

// Leak-candidate detail record (diagnostics). Captured at gate end for every
// live resource whose (handle, kind) was not part of the pre-pressure
// baseline. The artifact serializes these so a NO_GO caused by the leak
// watchdog carries the exact (kind/name/bytes/owner) evidence instead of an
// opaque count.
struct LeakCandidateRecord {
  uint32_t handle{0};
  graph::ResourceKind kind{graph::ResourceKind::Texture2D};
  graph::RenderOwnerTag ownerTag{graph::RenderOwnerTag::Unknown};
  size_t sizeBytes{0};
  std::string name;
  uint64_t creationFrame{0};
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

// W6 (M0-C) occupancy evidence probe (M0-A R3). The REAL GI composite occupancy
// history (R8 ping-pong) is read back with glGetTexImage (GL_RED/GL_FLOAT) and
// classified by the pure-CPU ClassifyOccupancyProbe: every texel finite, sane
// min/max/mean, and a 5-point probe (4 corners + center) sitting within epsilon
// of {0,1} (occupancy is a 0/1 mask). No synthetic proxy is ever used.
struct OccupancyProbeResult {
  bool texturePresent{false}; // readback succeeded (resource existed)
  bool maskValid{false};      // 0/1-mask classification passed
  float minValue{0.0f};
  float maxValue{0.0f};
  float meanValue{0.0f};
  std::vector<float> probeSamples; // 5: corners + center of the mask channel
  std::string reason;
};

// W6 (M0-C) occupancy evidence verdict (fail-closed). "present" requires: a
// real history texture exposed by the GI composite pass, a valid 0/1 mask
// probe, and a positive history reset count (proving temporal rejection
// actually occurred). Anything else yields status "failed" with blocksGo=true.
struct OccupancyEvidenceResult {
  std::string status; // "present" | "failed"
  std::string reason;
  bool blocksGo{true};
  bool texturePresent{false};
  int width{0};
  int height{0};
  OccupancyProbeResult probe;
  uint64_t historyResetCount{0};
  std::string lastResetReason;
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
  // W6 (M0-C) Blocker-1: occupancy/disocclusion probes (M0-A R3). Status is
  // "present" only when the real history texture was probed, classified as a
  // valid 0/1 mask, and a positive history reset count was observed; anything
  // else stays fail-closed (can never GO). Detail fields serialize under
  // occupancy.* in the artifact.
  std::string occupancyStatus;
  std::string occupancyReason;
  bool occupancyTexturePresent{false};
  int occupancyProbeWidth{0};
  int occupancyProbeHeight{0};
  float occupancyMinValue{0.0f};
  float occupancyMaxValue{0.0f};
  float occupancyMeanValue{0.0f};
  std::vector<float> occupancyProbePoints;
  uint64_t occupancyHistoryResetCount{0};
  std::string occupancyLastResetReason;
  std::vector<FixtureExecutionResult> matrixResults;
  std::vector<PairedGiDeltaResult> pairedGiDeltas;
  StressTestReport stressReport;
  uint64_t totalTrackedBytes{0};
  uint64_t peakTrackedBytes{0};
  size_t activeResourceCount{0};
  size_t leakCandidateCount{0};
  // S4 (M0-C R5.2): per-resource leak-candidate evidence (handle/kind/owner/
  // bytes/name/creation frame). Serialized under resources.leak_candidates.
  std::vector<LeakCandidateRecord> leakCandidates;
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

  // M0-B: captures the real state of the external composite target (owned by
  // the fixture driver) with glGetFramebufferAttachmentParameteriv +
  // texture-level/renderbuffer parameter queries. Records bind/viewport/scissor
  // plus attachment identity/extent/format. Fail-closed: missing entry points,
  // an invalid FBO, or a contract mismatch (extent/internalFormat) yield
  // "unavailable"/"failed" and are never default-filled. expectedInternalFormat
  // defaults to the RGBA16F (0x881A) external-target contract.
  [[nodiscard]] static TargetAttachmentState CaptureTargetState(
      uint32_t framebuffer, int expectedWidth, int expectedHeight,
      uint32_t expectedInternalFormat = 0x881A);

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

  // W6 (M0-C) occupancy evidence (M0-A R3). Pure-CPU classifier over the raw
  // occupancy mask texels (GL_RED/GL_FLOAT readback of the R8 history):
  // dimension check, every texel finite, min/max/mean, and a 5-point probe
  // (4 corners + center) within epsilon of {0,1}. GPU-free so the 0/1-mask
  // contract is unit-testable; the GPU readback path delegates here.
  [[nodiscard]] static OccupancyProbeResult ClassifyOccupancyProbe(
      const float *texels, size_t texelCount, int width, int height);

  // W6 (M0-C) occupancy evidence verdict (fail-closed). "present" requires a
  // real texture + a valid 0/1 mask probe + a positive history reset count
  // (proof temporal rejection occurred); otherwise "failed" and blocksGo=true.
  [[nodiscard]] static OccupancyEvidenceResult EvaluateOccupancyEvidence(
      uint32_t texture, int width, int height, const OccupancyProbeResult &probe,
      uint64_t historyResetCount, const std::string &lastResetReason);

  [[nodiscard]] static GateReport RunGate(const std::string &revision = "HEAD",
                                          int sampleFramesPerFixture = 120,
                                          bool stressTest1Min = true,
                                          int toggleLoops = 100,
                                          FixtureRenderDriver *driver = nullptr);
};

} // namespace NoMoreDay::render::validation
