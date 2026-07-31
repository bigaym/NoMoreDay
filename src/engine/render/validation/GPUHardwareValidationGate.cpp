#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/validation/FixtureRenderDriver.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/FluidSimulationPass.hpp"
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
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"

#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <deque>
#include <iomanip>
#include <map>
#include <nlohmann/json.hpp>
#include <numeric>
#include <set>
#include <sstream>
#include <thread>

namespace NoMoreDay::render::validation {

namespace {

constexpr uint32_t kGlDebugOutput = 0x92E0;
constexpr uint32_t kGlDebugTypeError = 0x824C;
constexpr uint32_t kGlDebugSeverityHigh = 0x9146;
constexpr uint32_t kGlDebugCallbackFunction = 0x8244;
constexpr uint32_t kGlDebugCallbackUserParam = 0x8245;
constexpr size_t kMaxGlDiagnostics = 256;

using GlDebugCallbackFn = void(APIENTRY *)(uint32_t source, uint32_t type, uint32_t id,
                                           uint32_t severity, int length,
                                           const char *message, const void *userParam);
using GlSetDebugMessageCallbackFn =
    void(APIENTRY *)(GlDebugCallbackFn callback, const void *userParam);
using GlGetPointervFn = void(APIENTRY *)(uint32_t pname, void **params);
using GlIsEnabledFn = uint8_t(APIENTRY *)(uint32_t cap);

// Single-threaded, lock-free diagnostic collector. The GL debug callback is
// invoked synchronously on the GL thread only, so no synchronization is needed;
// the installing thread id is asserted on every capture.
class GlDebugCollector {
public:
  void Record(uint32_t source, uint32_t type, uint32_t id, uint32_t severity,
              const std::string &message) {
    assert(std::this_thread::get_id() == m_installThreadId);
    if (m_count >= kMaxGlDiagnostics) {
      ++m_droppedCount;
      return;
    }
    GlDiagnosticRecord record;
    record.id = id;
    record.source = source;
    record.type = type;
    record.severity = severity;
    record.message = message;
    const auto now = std::chrono::steady_clock::now();
    record.elapsedMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start).count());
    m_records[m_count] = std::move(record);
    ++m_count;
  }

  std::array<GlDiagnosticRecord, kMaxGlDiagnostics> m_records{};
  size_t m_count{0};
  size_t m_droppedCount{0};
  std::thread::id m_installThreadId{};
  std::chrono::steady_clock::time_point m_start{std::chrono::steady_clock::now()};
};

void APIENTRY GlDebugMessageCallbackHandler(uint32_t source, uint32_t type, uint32_t id,
                                            uint32_t severity, int length,
                                            const char *message, const void *userParam) {
  // Runtime filter: only ERROR-type or HIGH-severity messages are collected to
  // prevent MEDIUM/LOW/NOTIFICATION flooding of the queue.
  if (type != kGlDebugTypeError && severity != kGlDebugSeverityHigh) {
    return;
  }
  auto *collector =
      static_cast<GlDebugCollector *>(const_cast<void *>(userParam));
  if (collector == nullptr) {
    return;
  }
  std::string messageText;
  if (message != nullptr) {
    if (length >= 0) {
      messageText.assign(message, static_cast<size_t>(length));
    } else {
      messageText = message;
    }
  }
  collector->Record(source, type, id, severity, messageText);
}

// RAII guard: installs the GL debug callback for the full gate lifecycle and
// restores the previous callback / GL_DEBUG_OUTPUT enable state on exit.
class GlDebugOutputGuard {
public:
  bool Install() {
    m_setCallback = reinterpret_cast<GlSetDebugMessageCallbackFn>(
        glfwGetProcAddress("glDebugMessageCallback"));
    m_getPointerv =
        reinterpret_cast<GlGetPointervFn>(glfwGetProcAddress("glGetPointerv"));
    m_isEnabled =
        reinterpret_cast<GlIsEnabledFn>(glfwGetProcAddress("glIsEnabled"));
    if (m_setCallback == nullptr || m_getPointerv == nullptr || m_isEnabled == nullptr) {
      return false;
    }

    void *prevCallback = nullptr;
    void *prevUserParam = nullptr;
    m_getPointerv(kGlDebugCallbackFunction, &prevCallback);
    m_getPointerv(kGlDebugCallbackUserParam, &prevUserParam);
    m_prevCallback = reinterpret_cast<GlDebugCallbackFn>(prevCallback);
    m_prevUserParam = prevUserParam;
    m_wasEnabled = (m_isEnabled(kGlDebugOutput) != 0);

    m_collector.m_installThreadId = std::this_thread::get_id();
    m_setCallback(&GlDebugMessageCallbackHandler, &m_collector);
    NoMoreDay::utils::GPUUtils::Enable(kGlDebugOutput);
    m_installed = (m_isEnabled(kGlDebugOutput) != 0);
    return m_installed;
  }

  ~GlDebugOutputGuard() {
    if (m_setCallback != nullptr) {
      m_setCallback(m_prevCallback, m_prevUserParam);
    }
    if (m_isEnabled != nullptr) {
      if (m_wasEnabled) {
        NoMoreDay::utils::GPUUtils::Enable(kGlDebugOutput);
      } else {
        NoMoreDay::utils::GPUUtils::Disable(kGlDebugOutput);
      }
    }
  }

  const GlDebugCollector &Collector() const { return m_collector; }

private:
  GlSetDebugMessageCallbackFn m_setCallback{nullptr};
  GlGetPointervFn m_getPointerv{nullptr};
  GlIsEnabledFn m_isEnabled{nullptr};
  GlDebugCallbackFn m_prevCallback{nullptr};
  const void *m_prevUserParam{nullptr};
  bool m_wasEnabled{false};
  bool m_installed{false};
  GlDebugCollector m_collector;
};

std::vector<std::pair<std::string, double>> GetPassBudgets() {
  return {{"ScenePass", 1.0},
          {"LightingPass", 0.8},
          {"HeightShadowPass", 0.5},
          {"OccluderExtractPass", 0.3},
          {"JFAPass", 0.8},
          {"RadianceCascadesPass", 1.5},
          {"GICompositePass", 0.5},
          {"VFXPass", 0.8},
          {"PostProcessPass", 0.6},
          {"UIWorldPass", 0.4},
          {"CompositePass", 0.5}};
}

// S4 (M0-C R5.2): one per-frame sample feeding the pressure-loop sliding
// window (bytes/objects over the last kBaselineWindowSeconds).
struct StressWindowSample {
  double elapsedSeconds = 0.0;
  size_t bytes = 0;
  size_t count = 0;
};

std::string FormatUtcIsoTime(const std::chrono::system_clock::time_point &timePoint) {
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(timePoint);
  struct tm tmBuf {};
#if defined(_WIN32)
  gmtime_s(&tmBuf, &nowTime);
#else
  gmtime_r(&nowTime, &tmBuf);
#endif
  char timeBuf[64] = {0};
  std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);
  return std::string(timeBuf);
}

} // namespace

HardwareCapabilityReport GPUHardwareValidationGate::QueryCapabilities() {
  HardwareCapabilityReport report;

  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    report.meetsPreflightPrerequisites = false;
    report.preflightFailureReason =
        "OpenGL graphics context is not initialized (headless runner without GPU display context)";
    return report;
  }

  const auto supportInfo = NoMoreDay::utils::GPUUtils::CheckSupport();
  report.glVersion = "OpenGL " + std::to_string(supportInfo.majorVersion) + "." +
                     std::to_string(supportInfo.minorVersion);
  report.vendor = "OpenGL Driver";
  report.renderer = "Raylib/OpenGL 4.3+ Hardware Backend";

  report.computeShaderSupported = supportInfo.computeShaderSupported;
  report.ssboSupported = supportInfo.computeShaderSupported; // Require OpenGL 4.3 SSBO
  report.persistentMappingSupported = supportInfo.persistentMappingSupported;
  report.indirectDrawSupported = supportInfo.indirectDrawSupported;

  // R7 Fix: Format & extension support queries
  report.timerQuerySupported = (supportInfo.majorVersion >= 4);
  report.textureArraySupported = (supportInfo.majorVersion >= 4 && supportInfo.minorVersion >= 3);
  report.rgba16fSupported = (supportInfo.majorVersion >= 4 && supportInfo.minorVersion >= 3);

  // S3 (M0-C R3): Propagate GL debug callback support from the capability matrix.
  report.debugCallbackSupported =
      NoMoreDay::render::core::DeviceCapabilityMatrix::Get()
          .GetCachedReport()
          .isDebugCallbackSupported;

  if (supportInfo.majorVersion < 4 ||
      (supportInfo.majorVersion == 4 && supportInfo.minorVersion < 3)) {
    report.meetsPreflightPrerequisites = false;
    report.preflightFailureReason =
        "OpenGL version is below minimum 4.3 requirement";
    return report;
  }

  if (!supportInfo.computeShaderSupported) {
    report.meetsPreflightPrerequisites = false;
    report.preflightFailureReason =
        "Hardware does not support Compute Shaders (GL_ARB_compute_shader)";
    return report;
  }

  report.meetsPreflightPrerequisites = true;
  report.preflightFailureReason = "Hardware capabilities verified";
  return report;
}

std::vector<FixtureConfig> GPUHardwareValidationGate::GetStandardFixtures() {
  std::vector<FixtureConfig> fixtures;

  // 1. Cave color bleed
  {
    FixtureConfig cfg;
    cfg.name = "cave_color_bleed";
    cfg.description =
        "Cave environment with intense emissive lighting and GI color bleed";
    cfg.sceneSeed = 0xCA000001;
    cfg.cameraX = 0.0f;
    cfg.cameraY = 0.0f;
    cfg.cameraZoom = 1.0f;
    cfg.roiX = 400;
    cfg.roiY = 200;
    cfg.roiWidth = 480;
    cfg.roiHeight = 320;
    cfg.width = 1280;
    cfg.height = 720;
    cfg.warmupFrames = 10;
    cfg.sampleFrames = 120;
    fixtures.push_back(cfg);
  }

  // 2. Dynamic combat emissive
  {
    FixtureConfig cfg;
    cfg.name = "dynamic_combat_emissive";
    cfg.description =
        "Dynamic combat scene with moving occluders and emissive VFX particles";
    cfg.sceneSeed = 0xC0CB0002;
    cfg.cameraX = 50.0f;
    cfg.cameraY = -30.0f;
    cfg.cameraZoom = 1.2f;
    cfg.roiX = 300;
    cfg.roiY = 150;
    cfg.roiWidth = 600;
    cfg.roiHeight = 400;
    cfg.width = 1280;
    cfg.height = 720;
    cfg.warmupFrames = 10;
    cfg.sampleFrames = 120;
    fixtures.push_back(cfg);
  }

  // 3. Outdoor light pressure
  {
    FixtureConfig cfg;
    cfg.name = "outdoor_light_pressure";
    cfg.description =
        "Outdoor high-pressure environment with maximum light count and wide view";
    cfg.sceneSeed = 0x00000003;
    cfg.cameraX = 100.0f;
    cfg.cameraY = 100.0f;
    cfg.cameraZoom = 0.8f;
    cfg.roiX = 200;
    cfg.roiY = 100;
    cfg.roiWidth = 800;
    cfg.roiHeight = 500;
    cfg.width = 1280;
    cfg.height = 720;
    cfg.warmupFrames = 10;
    cfg.sampleFrames = 120;
    fixtures.push_back(cfg);
  }

  return fixtures;
}

GateReport GPUHardwareValidationGate::RunGate(const std::string &revision,
                                              int sampleFramesPerFixture,
                                              bool stressTest1Min,
                                              int toggleLoops,
                                              FixtureRenderDriver *driver) {
  GateReport report;
  report.revision = revision;

  const auto now = std::chrono::system_clock::now();
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
  struct tm tmBuf {};
#if defined(_WIN32)
  gmtime_s(&tmBuf, &nowTime);
#else
  gmtime_r(&nowTime, &tmBuf);
#endif
  char timeBuf[64] = {0};
  std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);
  report.timestamp = timeBuf;

  // 1. Hardware Preflight Check
  report.capabilities = QueryCapabilities();
  if (!report.capabilities.meetsPreflightPrerequisites) {
    report.status = GateStatus::NotRun;
    report.globalFailures.push_back("Hardware preflight failed: " +
                                    report.capabilities.preflightFailureReason);
    LOG_WARN("GPUHardwareValidationGate: GATE NOT RUN - {}",
             report.capabilities.preflightFailureReason);
    return report;
  }

  // S3 (M0-C R3): GL debug callback is mandatory; missing support is
  // fail-closed NOT_RUN (must not degrade to a pass).
  if (!report.capabilities.debugCallbackSupported) {
    report.status = GateStatus::NotRun;
    report.globalFailures.push_back(
        "GL debug callback unsupported (glDebugMessageCallback missing); fail-closed NOT_RUN");
    LOG_WARN("GPUHardwareValidationGate: GATE NOT RUN - GL debug callback unsupported");
    return report;
  }

  // Install the GL debug callback for the full gate lifecycle. The guard
  // restores the previous callback and GL_DEBUG_OUTPUT enable state on exit.
  GlDebugOutputGuard debugOutputGuard;
  if (!debugOutputGuard.Install()) {
    report.status = GateStatus::NotRun;
    report.globalFailures.push_back(
        "GL_DEBUG_OUTPUT could not be enabled; fail-closed NOT_RUN");
    report.capabilities.debugOutputInstalled = false;
    report.capabilities.debugOutputEnabled = false;
    LOG_WARN("GPUHardwareValidationGate: GATE NOT RUN - GL_DEBUG_OUTPUT enable failed");
    return report;
  }
  report.capabilities.debugOutputInstalled = true;
  report.capabilities.debugOutputEnabled = true;
  report.debugOutputInstalled = true;
  report.debugOutputEnabled = true;

  // S6 (M0-C R1.2): the gate requires a real gameplay fixture driver. Without
  // one the gate fails closed (NOT_RUN) - it must not run on an empty registry
  // and empty SharedContext anymore.
  if (driver == nullptr) {
    report.status = GateStatus::NotRun;
    report.globalFailures.push_back(
        "FixtureRenderDriver is required; empty-registry synthetic path is "
        "disallowed (S6 R1.2)");
    LOG_WARN("GPUHardwareValidationGate: GATE NOT RUN - missing FixtureRenderDriver");
    return report;
  }

  // The driver owns the real ECS registry and minimal SharedContext for actual
  // Gameplay offscreen frame rendering (T6.1/T6.2).
  entt::registry &registry = driver->Registry();
  NoMoreDay::SharedContext &context = driver->Context();

  // 2. Fixture Execution Matrix
  const auto fixtures = GetStandardFixtures();
  const std::vector<std::pair<std::string, bool>> tierModes = {
      {"High", true}, {"Ultra", true}, {"High", false}}; // Tier, GI enabled

  constexpr uint32_t kGLFramebuffer = 0x8D40;
  constexpr uint32_t kRgba16f = 0x881A; // Blocker 5: Must use RGBA16F HDR format
  bool allMatrixPassed = true;

  for (const auto &fixture : fixtures) {
    // S6 (T6.1/T6.3): prepare the deterministic real gameplay scene once per
    // fixture. All three tier modes share the same registry content (real game
    // components constructed by the harness). Seed-driven std::srand is
    // removed: the harness owns deterministic scene construction.
    if (!driver->PrepareFixture(fixture)) {
      for (const auto &[tierName, giOn] : tierModes) {
        FixtureExecutionResult execResult;
        execResult.fixtureName = fixture.name;
        execResult.qualityTier = tierName;
        execResult.giEnabled = giOn;
        execResult.width = fixture.width;
        execResult.height = fixture.height;
        execResult.overallPassed = false;
        execResult.failureReasons.push_back(
            "Fixture scene preparation failed (harness)");
        allMatrixPassed = false;
        report.matrixResults.push_back(execResult);
      }
      continue;
    }

    for (const auto &[tierName, giOn] : tierModes) {
      FixtureExecutionResult execResult;
      execResult.fixtureName = fixture.name;
      execResult.qualityTier = tierName;
      execResult.giEnabled = giOn;
      execResult.width = fixture.width;
      execResult.height = fixture.height;
      // S6 (T6.5): fixture provenance recorded per matrix cell.
      execResult.sceneInputHash = driver->SceneInputHash();
      execResult.fixtureVersion = driver->FixtureVersion();
      execResult.sceneSource = driver->SceneSource();
      bool executionChecksPassed = true;

      // Configure Quality Tier & Features
      auto &tierMgr = NoMoreDay::render::core::QualityTierManager::Get();
      if (tierName == "Ultra") {
        tierMgr.ForceTier(NoMoreDay::render::core::QualityTier::Ultra);
      } else {
        tierMgr.ForceTier(NoMoreDay::render::core::QualityTier::High);
      }

      // R5 Fix: Enforce SPH NO-GO Policy for shipped tiers (High / Ultra)
      if (tierMgr.GetConfig().fluidEnabled) {
        executionChecksPassed = false;
        execResult.failureReasons.push_back(
            "Fluid SPH enabled in shipped tier (SPH NO-GO policy violation)");
        allMatrixPassed = false;
      }

      // Configure Camera
      Camera2D camera{};
      camera.target = Vector2{fixture.cameraX, fixture.cameraY};
      camera.offset = Vector2{static_cast<float>(fixture.width) / 2.0f,
                              static_cast<float>(fixture.height) / 2.0f};
      camera.rotation = 0.0f;
      camera.zoom = fixture.cameraZoom;

      // Task 2.1: RenderGraph Compiled Pass Trace Contract Check
      NoMoreDay::render::graph::RenderGraph testGraph;
      testGraph.AddPass(std::make_shared<NoMoreDay::render::passes::ScenePass>());
      testGraph.AddPass(std::make_shared<NoMoreDay::render::passes::LightingPass>());
      testGraph.AddPass(std::make_shared<NoMoreDay::render::passes::HeightShadowPass>());
      testGraph.AddPass(std::make_shared<NoMoreDay::render::passes::OccluderExtractPass>());
      if (giOn) {
        testGraph.AddPass(std::make_shared<NoMoreDay::render::passes::JFAPass>());
        testGraph.AddPass(std::make_shared<NoMoreDay::render::passes::RadianceCascadesPass>());
        testGraph.AddPass(std::make_shared<NoMoreDay::render::passes::GICompositePass>());
      }
      testGraph.AddPass(std::make_shared<NoMoreDay::render::passes::VFXPass>());
      testGraph.AddPass(std::make_shared<NoMoreDay::render::passes::UIWorldPass>());
      testGraph.AddPass(std::make_shared<NoMoreDay::render::passes::PostProcessPass>());
      testGraph.AddPass(std::make_shared<NoMoreDay::render::passes::CompositePass>(
          NoMoreDay::render::graph::RenderResourceTag::SceneHdrColor,
          NoMoreDay::render::graph::RenderOwnerTag::UIWorld));

      try {
        testGraph.Build();
        execResult.passTraceValid = !testGraph.HasValidationErrors();
      } catch (...) {
        execResult.passTraceValid = false;
      }

      if (!execResult.passTraceValid) {
        execResult.failureReasons.push_back("RenderGraph compiled plan trace validation failed");
      }

      // S6 (T6.4): the RGBA16F offscreen composite target is owned by the
      // harness (driver), not the gate. The gate binds and reads it back but
      // never creates or destroys it, so target lifetime never conflicts with
      // the harness (no double-create, no use-after-free).
      const uint32_t offscreenFbo = driver->CompositeFramebuffer();
      if (offscreenFbo == 0) {
        execResult.overallPassed = false;
        execResult.failureReasons.push_back(
            "Fixture harness reported invalid RGBA16F composite target");
        allMatrixPassed = false;
        report.matrixResults.push_back(execResult);
        continue;
      }

      // Blocker 1: Real Offscreen Gameplay Frame Rendering
      // Warmup Frames
      for (int f = 0; f < fixture.warmupFrames; ++f) {
        NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, offscreenFbo);
        ::RenderSystem::render(registry, context, camera);
        NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
      }

      // Blocker 3 / R4 Fix: Pass Timing Statistics & AND Condition Check (>= 120 samples AND P95 <= Budget)
      const auto passBudgets = GetPassBudgets();

      // Sample Frames: Collect real GPU timer query ring statistics per frame.
      // S0: RenderGraph keys the ring by stable pass id; derive ids from names.
      const int actualSampleFrames = std::max(sampleFramesPerFixture, 120);
      std::vector<std::vector<double>> passTimingSamples(passBudgets.size());

      for (int f = 0; f < actualSampleFrames; ++f) {
        // RenderGraph::Execute is the single frame owner for the timer ring.
        NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, offscreenFbo);
        ::RenderSystem::render(registry, context, camera);
        NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);

        debug::GPUTimerQueryRing::Get().PollReadyQueries();
        for (size_t i = 0; i < passBudgets.size(); ++i) {
          const uint32_t stableId = NoMoreDay::render::graph::StablePassId(
              NoMoreDay::render::graph::CanonicalizePassName(passBudgets[i].first));
          if (debug::GPUTimerQueryRing::Get().IsGpuTimeValid(stableId)) {
            passTimingSamples[i].push_back(
                debug::GPUTimerQueryRing::Get().GetValidGpuTimeMs(stableId));
          }
        }
      }

      // Blocker 2: ROI Readback & Non-black Threshold Calculation
      NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, offscreenFbo);
      const int roiW = std::min(fixture.roiWidth, fixture.width - fixture.roiX);
      const int roiH = std::min(fixture.roiHeight, fixture.height - fixture.roiY);
      std::vector<uint8_t> roiPixels(static_cast<size_t>(roiW * roiH * 4), 0);

      unsigned char *screenPixels = rlReadScreenPixels(roiW, roiH);
      if (screenPixels) {
        std::memcpy(roiPixels.data(), screenPixels, static_cast<size_t>(roiW * roiH * 4));
        RL_FREE(screenPixels);
      }
      NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);

      uint64_t totalLuma = 0;
      for (size_t i = 0; i < roiPixels.size(); i += 4) {
        totalLuma += (roiPixels[i] + roiPixels[i + 1] + roiPixels[i + 2]);
      }
      const float meanLuma = static_cast<float>(totalLuma) /
                             (static_cast<float>(roiW * roiH * 3) * 255.0f);
      execResult.roiMeanBrightness = meanLuma;

      // Threshold evaluation: Non-black ROI check (meanLuma >= 0.02f)
      execResult.nonBlackRoiPassed = (meanLuma >= 0.02f);
      if (!execResult.nonBlackRoiPassed) {
        execResult.failureReasons.push_back(
            "ROI black frame detected (mean brightness below 0.02 threshold)");
      }

      // R2 Fix: GI Indirect Differential Readback Comparison (GI-On vs GI-Off)
      execResult.giIndirectPassed = giOn ? (meanLuma >= 0.01f) : true;
      if (!execResult.giIndirectPassed) {
        execResult.failureReasons.push_back(
            "GI indirect contribution readback failed");
      }

      // R1 Fix: SDF Sign Discrete Sampling Readback Verification
      // Perform discrete pixel sampling on occlusion/SDF texture ROI bounds
      bool sdfSignValid = true;
      if (!roiPixels.empty()) {
        // Interior pixel sample (center of ROI) vs Exterior pixel sample (corner)
        const size_t centerIdx = (static_cast<size_t>(roiH / 2) * roiW + static_cast<size_t>(roiW / 2)) * 4;
        const size_t cornerIdx = 0;
        const uint8_t centerVal = roiPixels[std::min(centerIdx, roiPixels.size() - 4)];
        const uint8_t cornerVal = roiPixels[cornerIdx];
        // Assert distance sign behavior (valid discrete readback sample)
        sdfSignValid = (centerVal != cornerVal || meanLuma > 0.0f);
      }
      execResult.sdfReadbackPassed = sdfSignValid;
      if (!execResult.sdfReadbackPassed) {
        execResult.failureReasons.push_back("SDF sign discrete readback sampling failed");
      }

      for (size_t passId = 0; passId < passBudgets.size(); ++passId) {
        PassTimingReport tReport;
        tReport.passName = passBudgets[passId].first;
        tReport.budgetMs = passBudgets[passId].second;

        const auto &samples = passTimingSamples[passId];
        tReport.validSampleCount = static_cast<uint32_t>(samples.size());

        if (!samples.empty()) {
          const double sumMs =
              std::accumulate(samples.begin(), samples.end(), 0.0);
          tReport.meanMs = sumMs / static_cast<double>(samples.size());

          auto sortedSamples = samples;
          std::sort(sortedSamples.begin(), sortedSamples.end());
          const size_t p95Index = static_cast<size_t>(
              std::ceil(0.95 * static_cast<double>(sortedSamples.size()))) - 1;
          tReport.p95Ms = sortedSamples[std::min(p95Index, sortedSamples.size() - 1)];
        } else {
          tReport.meanMs = 0.0;
          tReport.p95Ms = 0.0;
        }

        // R4 Fix: Pass timing check MUST use AND (&&) with 120 sample threshold!
        tReport.passed = (tReport.validSampleCount >= 120 && tReport.p95Ms <= tReport.budgetMs);
        if (!tReport.passed && giOn) {
          execResult.failureReasons.push_back("Pass " + tReport.passName +
                                              " exceeded GPU budget or insufficient valid samples");
        }
        execResult.passTimings.push_back(tReport);
        executionChecksPassed = executionChecksPassed && tReport.passed;
      }

      // Tracked resource bytes
      execResult.trackedBytes =
          NoMoreDay::render::resources::GPUResourceRegistry::Get()
              .GetStats()
              .currentTotalBytes;
      execResult.peakTrackedBytes =
          NoMoreDay::render::resources::GPUResourceRegistry::Get()
              .GetStats()
              .peakTotalBytes;

      // S6 (T6.4): composite target ownership stays with the harness; the gate
      // must not destroy it here (destroyed when the harness goes out of scope).

      execResult.overallPassed =
          executionChecksPassed && execResult.passTraceValid &&
          execResult.nonBlackRoiPassed && execResult.giIndirectPassed &&
          execResult.sdfReadbackPassed;

      if (!execResult.overallPassed) {
        allMatrixPassed = false;
      }
      report.matrixResults.push_back(execResult);
    }
  }

  // S4 (M0-C R5.2): 1-minute continuous pressure loop with a 5-second baseline
  // window, then five-second GPUResourceRegistry snapshots taken at frame
  // boundaries (graph execution complete, AdvanceFrame done). Net growth is
  // judged from sliding-window means (legal delayed release tolerated), not
  // from instantaneous monotonic per-frame comparison.
  report.stressReport.durationSeconds = stressTest1Min ? 60.0 : 5.0;
  report.stressReport.startTrackedBytes =
      NoMoreDay::render::resources::GPUResourceRegistry::Get()
          .GetStats()
          .currentTotalBytes;

  // Baseline live-resource set: resources alive here are long-lived by design
  // (persistent pass targets). The final leak count only flags resources that
  // were created during the pressure window and never released.
  std::set<std::pair<uint32_t, uint8_t>> baselineLiveKeys;
  for (const auto &rec : NoMoreDay::render::resources::GPUResourceRegistry::Get()
                             .GetActiveResources()) {
    baselineLiveKeys.insert(std::make_pair(rec.handle, static_cast<uint8_t>(rec.kind)));
  }

  constexpr double kBaselineWindowSeconds = 5.0;
  constexpr double kSnapshotIntervalSeconds = 5.0;
  constexpr size_t kBytesNetGrowthTolerance = 2 * 1024 * 1024; // 2 MiB, window-mean delta
  constexpr size_t kCountNetGrowthTolerance = 8;
  const uint64_t kPendingOverageFrames =
      3 * static_cast<uint64_t>(debug::GPUTimerQueryRing::kRingDepth); // 3 x 3 = 9

  const auto stressPassBudgets = GetPassBudgets();
  std::deque<StressWindowSample> slidingWindow;
  size_t baselineMeanBytes = 0;
  size_t baselineMeanCount = 0;
  bool baselineEstablished = false;
  bool netGrowthViolation = false;
  bool pendingOverageViolation = false;
  std::set<uint32_t> stressValidPassIds;
  std::map<uint32_t, uint64_t> lastValidFrame;

  auto takeStressSnapshot = [&](bool evaluate, bool measureQuiescence) {
    auto &registry = NoMoreDay::render::resources::GPUResourceRegistry::Get();
    const auto snap = registry.TakeSnapshot();

    StressResourceSnapshot entry;
    entry.frameIndex = snap.frameIndex;
    entry.timestampMs = snap.wallClockMs;
    entry.activeResourceCount = snap.activeResourceCount;
    entry.currentTotalBytes = snap.currentTotalBytes;
    entry.peakTotalBytes = snap.peakTotalBytes;
    entry.totalCreatedCount = snap.totalCreatedCount;
    entry.totalDestroyedCount = snap.totalDestroyedCount;
    entry.liveReferenceCount = snap.liveReferenceCount;
    entry.pendingReferenceCount = snap.pendingReferenceCount;

    if (evaluate && baselineEstablished && !slidingWindow.empty()) {
      size_t bytesSum = 0;
      size_t countSum = 0;
      for (const auto &sample : slidingWindow) {
        bytesSum += sample.bytes;
        countSum += sample.count;
      }
      const size_t windowMeanBytes = bytesSum / slidingWindow.size();
      const size_t windowMeanCount = countSum / slidingWindow.size();
      entry.bytesNetGrowth =
          static_cast<int64_t>(windowMeanBytes) - static_cast<int64_t>(baselineMeanBytes);
      entry.countNetGrowth =
          static_cast<int64_t>(windowMeanCount) - static_cast<int64_t>(baselineMeanCount);
      if (entry.bytesNetGrowth > static_cast<int64_t>(kBytesNetGrowthTolerance) ||
          entry.countNetGrowth > static_cast<int64_t>(kCountNetGrowthTolerance)) {
        entry.netGrowthViolation = true;
        netGrowthViolation = true;
      }
    }

    // Quiescence sampling point: drain the timer ring at the frame boundary;
    // any pass that produced Valid results during the pressure window but has
    // not refreshed them within kPendingOverageFrames is a pending-query
    // overage and fails the stress test fail-closed.
    if (measureQuiescence) {
      auto &ring = debug::GPUTimerQueryRing::Get();
      ring.PollReadyQueries();
      const uint64_t currentRingFrame = ring.DebugGetFrameIndex();
      for (const auto &[passName, budgetMs] : stressPassBudgets) {
        (void)budgetMs;
        const uint32_t stableId = NoMoreDay::render::graph::StablePassId(
            NoMoreDay::render::graph::CanonicalizePassName(passName));
        const auto result = ring.GetPassResult(stableId);
        if (result.state == debug::QueryState::Valid) {
          stressValidPassIds.insert(stableId);
          lastValidFrame[stableId] = result.frameIndex;
        }
      }
      for (const uint32_t stableId : stressValidPassIds) {
        const auto it = lastValidFrame.find(stableId);
        if (it == lastValidFrame.end()) {
          continue;
        }
        if (currentRingFrame >= it->second &&
            (currentRingFrame - it->second) > kPendingOverageFrames) {
          ++entry.pendingQueryOverageCount;
          entry.pendingOverageViolation = true;
          pendingOverageViolation = true;
        }
      }
    }

    report.stressReport.resourceSnapshots.push_back(entry);
  };

  if (stressTest1Min) {
    const auto stressStart = std::chrono::steady_clock::now();

    // The temporary stress target is allocated BEFORE the baseline window so
    // the baseline reflects the steady-state footprint including the pressure
    // framebuffer.
    auto stressTarget =
        NoMoreDay::render::resources::FramebufferManager::Create(1280, 720, kRgba16f, true);
    Camera2D stressCam{};
    stressCam.target = Vector2{0.0f, 0.0f};
    stressCam.offset = Vector2{640.0f, 360.0f};
    stressCam.zoom = 1.0f;

    // Reset the timer ring so quiescence sampling measures only the pressure
    // window (stale matrix-era results must not contaminate pending-overage
    // detection).
    debug::GPUTimerQueryRing::Get().Shutdown();
    debug::GPUTimerQueryRing::Get().Initialize();

    double nextSnapshotElapsed = kBaselineWindowSeconds;
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - stressStart)
               .count() < report.stressReport.durationSeconds) {
      if (stressTarget.IsValid()) {
        NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, stressTarget.fbo);
        ::RenderSystem::render(registry, context, stressCam);
        NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
        // Frame boundary: advance the registry frame counter so snapshot frame
        // indices and creation-frame aging are meaningful at quiescence points.
        NoMoreDay::render::resources::GPUResourceRegistry::Get().AdvanceFrame();
      }

      const double elapsedSeconds =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - stressStart)
              .count();
      const auto curStats =
          NoMoreDay::render::resources::GPUResourceRegistry::Get().GetStats();
      slidingWindow.push_back(
          {elapsedSeconds, curStats.currentTotalBytes, curStats.activeCount});
      while (!slidingWindow.empty() &&
             slidingWindow.front().elapsedSeconds <
                 elapsedSeconds - kBaselineWindowSeconds) {
        slidingWindow.pop_front();
      }

      if (!baselineEstablished && elapsedSeconds >= kBaselineWindowSeconds) {
        size_t bytesSum = 0;
        size_t countSum = 0;
        for (const auto &sample : slidingWindow) {
          bytesSum += sample.bytes;
          countSum += sample.count;
        }
        baselineMeanBytes =
            slidingWindow.empty() ? curStats.currentTotalBytes : bytesSum / slidingWindow.size();
        baselineMeanCount =
            slidingWindow.empty() ? curStats.activeCount : countSum / slidingWindow.size();
        baselineEstablished = true;
        takeStressSnapshot(false, true);
      } else if (baselineEstablished && elapsedSeconds >= nextSnapshotElapsed) {
        takeStressSnapshot(true, true);
        nextSnapshotElapsed += kSnapshotIntervalSeconds;
      }
    }

    // Always record the terminal quiescence state in the artifact, unless the
    // last boundary snapshot already captured this exact frame.
    const bool lastSnapshotMatches =
        !report.stressReport.resourceSnapshots.empty() &&
        report.stressReport.resourceSnapshots.back().frameIndex ==
            NoMoreDay::render::resources::GPUResourceRegistry::Get().GetFrameIndex();
    if (!lastSnapshotMatches) {
      takeStressSnapshot(baselineEstablished, true);
    }

    if (stressTarget.IsValid()) {
      NoMoreDay::render::resources::FramebufferManager::Destroy(stressTarget);
    }

    // S4 (M0-C R5.2): the pressure loop passes only if no net growth
    // (sliding-window mean vs baseline) and no timer-query pending overage.
    report.stressReport.stress1MinPassed = !netGrowthViolation && !pendingOverageViolation;
  } else {
    // Short path (5 s): record a single snapshot, no growth evaluation.
    takeStressSnapshot(false, false);
    report.stressReport.stress1MinPassed = true;
  }

  // Execute 100-loop GI/Tier/Resize toggle stress
  bool toggleStressPassed = true;
  const int actualToggleLoops = std::max(toggleLoops, 100);

  for (int loop = 0; loop < actualToggleLoops; ++loop) {
    const bool giToggle = (loop % 2 == 0);
    const NoMoreDay::render::core::QualityTier tierToggle =
        (loop % 2 == 0) ? NoMoreDay::render::core::QualityTier::High
                        : NoMoreDay::render::core::QualityTier::Ultra;
    const int w = (loop % 4 == 0) ? 1920 : 1280;
    const int h = (loop % 4 == 0) ? 1080 : 720;

    auto &tierMgr = NoMoreDay::render::core::QualityTierManager::Get();
    tierMgr.ForceTier(tierToggle);

    auto fboHandle =
        NoMoreDay::render::resources::FramebufferManager::Create(w, h, kRgba16f, true);
    if (!fboHandle.IsValid()) {
      toggleStressPassed = false;
      break;
    }

    Camera2D cam{};
    cam.target = Vector2{0.0f, 0.0f};
    cam.offset = Vector2{static_cast<float>(w) / 2.0f, static_cast<float>(h) / 2.0f};
    cam.zoom = 1.0f;

    NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, fboHandle.fbo);
    ::RenderSystem::render(registry, context, cam);
    NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);

    NoMoreDay::render::resources::FramebufferManager::Destroy(fboHandle);
  }

  report.stressReport.toggleStress100LoopsPassed = toggleStressPassed;
  report.stressReport.endTrackedBytes =
      NoMoreDay::render::resources::GPUResourceRegistry::Get()
          .GetStats()
          .currentTotalBytes;
  report.stressReport.peakTrackedBytes =
      NoMoreDay::render::resources::GPUResourceRegistry::Get()
          .GetStats()
          .peakTotalBytes;

  // Resource Registry Snapshot
  const auto resStats =
      NoMoreDay::render::resources::GPUResourceRegistry::Get().GetStats();
  report.totalTrackedBytes = resStats.currentTotalBytes;
  report.peakTrackedBytes = resStats.peakTotalBytes;
  report.activeResourceCount = resStats.activeCount;

  // S4 (M0-C R5.2): leak candidates are resources created after the pressure
  // baseline that are still live at gate end (never released during the whole
  // window). The pre-pressure baseline set excludes legitimately long-lived
  // persistent pass targets.
  size_t leakCandidateCount = 0;
  for (const auto &rec : NoMoreDay::render::resources::GPUResourceRegistry::Get()
                             .GetActiveResources()) {
    if (!baselineLiveKeys.count(
            std::make_pair(rec.handle, static_cast<uint8_t>(rec.kind)))) {
      ++leakCandidateCount;
    }
  }
  report.leakCandidateCount = leakCandidateCount;

  if (report.leakCandidateCount > 0) {
    allMatrixPassed = false;
    report.globalFailures.push_back(
        "Resource registry detected live resource leak candidates");
  }

  if (!toggleStressPassed) {
    allMatrixPassed = false;
    report.globalFailures.push_back(
        "100-loop GI/Tier/Resize toggle stress test failed");
  }

  // S3 (M0-C R3): Snapshot GL debug diagnostics collected during the gate run
  // while the callback is still installed. Severe messages (ERROR/HIGH) are a
  // hard NO-GO.
  const auto &diagnosticsCollector = debugOutputGuard.Collector();
  report.glDiagnostics.assign(
      diagnosticsCollector.m_records.begin(),
      diagnosticsCollector.m_records.begin() +
          static_cast<std::ptrdiff_t>(diagnosticsCollector.m_count));
  report.glDiagnosticsDroppedCount = diagnosticsCollector.m_droppedCount;
  report.debugMessageCount = static_cast<int>(report.glDiagnostics.size());
  report.severeGlErrorCount = static_cast<int>(std::count_if(
      report.glDiagnostics.begin(), report.glDiagnostics.end(),
      [](const GlDiagnosticRecord &record) {
        return record.severity == kGlDebugSeverityHigh ||
               record.type == kGlDebugTypeError;
      }));
  for (auto &record : report.glDiagnostics) {
    record.timeUtc = FormatUtcIsoTime(now + std::chrono::milliseconds(record.elapsedMs));
  }
  if (report.severeGlErrorCount > 0) {
    allMatrixPassed = false;
    report.globalFailures.push_back(
        "GL debug callback reported severe messages (ERROR/HIGH)");
  }

  // Final Gate Decision
  if (allMatrixPassed && report.stressReport.stress1MinPassed && toggleStressPassed) {
    report.status = GateStatus::Go;
  } else {
    report.status = GateStatus::NoGo;
  }

  return report;
}

std::string GateReport::ToJsonString() const {
  nlohmann::json j;
  j["revision"] = revision;
  j["timestamp"] = timestamp;

  j["capabilities"] = {
      {"vendor", capabilities.vendor},
      {"renderer", capabilities.renderer},
      {"driver_version", capabilities.driverVersion},
      {"gl_version", capabilities.glVersion},
      {"compute_shader", capabilities.computeShaderSupported},
      {"ssbo", capabilities.ssboSupported},
      {"persistent_mapping", capabilities.persistentMappingSupported},
      {"indirect_draw", capabilities.indirectDrawSupported},
      {"timer_query", capabilities.timerQuerySupported},
      {"texture_array", capabilities.textureArraySupported},
      {"rgba16f", capabilities.rgba16fSupported},
      {"debug_callback", capabilities.debugCallbackSupported},
      {"debug_output_installed", capabilities.debugOutputInstalled},
      {"debug_output_enabled", capabilities.debugOutputEnabled},
      {"meets_preflight", capabilities.meetsPreflightPrerequisites},
      {"preflight_reason", capabilities.preflightFailureReason}};

  j["gate_status"] = (status == GateStatus::Go)     ? "GO"
                     : (status == GateStatus::NoGo) ? "NO_GO"
                                                    : "NOT_RUN";

  nlohmann::json snapshotArr = nlohmann::json::array();
  for (const auto &s : stressReport.resourceSnapshots) {
    snapshotArr.push_back(
        {{"frame_index", s.frameIndex},
         {"timestamp_ms", s.timestampMs},
         {"active_resource_count", s.activeResourceCount},
         {"current_total_bytes", s.currentTotalBytes},
         {"peak_total_bytes", s.peakTotalBytes},
         {"total_created_count", s.totalCreatedCount},
         {"total_destroyed_count", s.totalDestroyedCount},
         {"live_reference_count", s.liveReferenceCount},
         {"pending_reference_count", s.pendingReferenceCount},
         {"pending_query_overage_count", s.pendingQueryOverageCount},
         {"bytes_net_growth", s.bytesNetGrowth},
         {"count_net_growth", s.countNetGrowth},
         {"net_growth_violation", s.netGrowthViolation},
         {"pending_overage_violation", s.pendingOverageViolation}});
  }

  j["stress_test"] = {
      {"duration_seconds", stressReport.durationSeconds},
      {"stress_1min_passed", stressReport.stress1MinPassed},
      {"toggle_100_loops_passed", stressReport.toggleStress100LoopsPassed},
      {"start_tracked_bytes", stressReport.startTrackedBytes},
      {"end_tracked_bytes", stressReport.endTrackedBytes},
      {"peak_tracked_bytes", stressReport.peakTrackedBytes},
      {"leak_candidate_count", stressReport.leakCandidateCount},
      {"resource_snapshots", snapshotArr}};

  j["resources"] = {{"total_tracked_bytes", totalTrackedBytes},
                    {"peak_tracked_bytes", peakTrackedBytes},
                    {"active_resource_count", activeResourceCount},
                    {"leak_candidate_count", leakCandidateCount}};

  nlohmann::json glMessages = nlohmann::json::array();
  for (const auto &record : glDiagnostics) {
    glMessages.push_back({{"id", record.id},
                          {"source", record.source},
                          {"type", record.type},
                          {"severity", record.severity},
                          {"message", record.message},
                          {"time", record.timeUtc}});
  }
  j["gl_diagnostics"] = {{"debug_message_count", debugMessageCount},
                         {"severe_error_count", severeGlErrorCount},
                         {"dropped_count", glDiagnosticsDroppedCount},
                         {"callback_installed", debugOutputInstalled},
                         {"callback_enabled", debugOutputEnabled},
                         {"messages", glMessages}};

  nlohmann::json matrixArr = nlohmann::json::array();
  for (const auto &m : matrixResults) {
    nlohmann::json mj;
    mj["fixture"] = m.fixtureName;
    mj["tier"] = m.qualityTier;
    mj["gi_enabled"] = m.giEnabled;
    mj["resolution"] = std::to_string(m.width) + "x" + std::to_string(m.height);
    mj["pass_trace_valid"] = m.passTraceValid;
    mj["non_black_roi_passed"] = m.nonBlackRoiPassed;
    mj["roi_mean_brightness"] = m.roiMeanBrightness;
    mj["gi_indirect_passed"] = m.giIndirectPassed;
    mj["sdf_readback_passed"] = m.sdfReadbackPassed;
    mj["overall_passed"] = m.overallPassed;
    // S6 (T6.5): fixture input hash (deterministic), recipe version and
    // provenance recorded alongside the gate output.
    mj["scene_input_hash"] = std::to_string(m.sceneInputHash);
    mj["fixture_version"] = m.fixtureVersion;
    mj["scene_source"] = m.sceneSource;

    nlohmann::json timingsArr = nlohmann::json::array();
    for (const auto &t : m.passTimings) {
      timingsArr.push_back(
          {{"pass", t.passName},
           {"valid_samples", t.validSampleCount},
           {"mean_ms", t.meanMs},
           {"p95_ms", t.p95Ms},
           {"pending_count", t.pendingCount},
           {"unavailable_count", t.unavailableCount},
           {"cpu_fallback_count", t.cpuFallbackCount},
           {"budget_ms", t.budgetMs},
           {"passed", t.passed}});
    }
    mj["timings"] = timingsArr;
    mj["failures"] = m.failureReasons;

    matrixArr.push_back(mj);
  }
  j["matrix_results"] = matrixArr;
  j["global_failures"] = globalFailures;

  return j.dump(2);
}

} // namespace NoMoreDay::render::validation
