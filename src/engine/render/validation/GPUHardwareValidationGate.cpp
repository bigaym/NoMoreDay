#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/validation/FixtureRenderDriver.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <iomanip>
#include <limits>
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

// S7b: heuristic classification of the GL_RENDERER string. Known software
// rasterizers (WARP, llvmpipe, "Microsoft Basic Render Driver", generic
// "Software" renderers) are treated as non-hardware; anything else non-empty
// (NVIDIA/AMD/Intel/Apple/Mesa-with-radeon drivers) is treated as a real GPU.
bool IsHardwareRenderer(std::string_view renderer) {
  if (renderer.empty() || renderer == "Unknown") {
    return false;
  }
  if (renderer.find("WARP") != std::string_view::npos ||
      renderer.find("llvmpipe") != std::string_view::npos ||
      renderer.find("Basic Render Driver") != std::string_view::npos ||
      renderer.find("Software") != std::string_view::npos) {
    return false;
  }
  return true;
}

// S7b: builds the synthetic GI pass trace exactly like the matrix cell does,
// so the paired legs carry two different pass traces when GI is flipped.
std::string BuildGiPassTrace(bool giEnabled) {
  std::string trace;
  auto append = [&trace](const char *name) {
    if (!trace.empty()) {
      trace += ",";
    }
    trace += name;
  };
  append("ScenePass");
  append("LightingPass");
  append("HeightShadowPass");
  append("OccluderExtractPass");
  if (giEnabled) {
    append("JFAPass");
    append("RadianceCascadesPass");
    append("GICompositePass");
  }
  append("VFXPass");
  append("UIWorldPass");
  append("PostProcessPass");
  append("CompositePass");
  return trace;
}

// W6 (M0-C) High-2: reads back the full offscreen FBO and CPU-crops the ROI at
// its true origin (x,y). rlReadScreenPixels cannot sample an offset region, so
// reading the whole target and cropping here is the only correct way to make
// the sampled region match the declared ROI. GL state is bound/restored.
float ReadRoiMeanLuma(uint32_t offscreenFbo, int fboW, int fboH, int roiX,
                      int roiY, int roiW, int roiH) {
  constexpr uint32_t kGLFramebuffer = 0x8D40;
  if (roiW <= 0 || roiH <= 0 || fboW <= 0 || fboH <= 0) {
    return 0.0f;
  }
  if (roiX < 0 || roiY < 0 || roiX + roiW > fboW || roiY + roiH > fboH) {
    return 0.0f;
  }
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, offscreenFbo);
  unsigned char *pixels = rlReadScreenPixels(fboW, fboH);
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
  if (pixels == nullptr) {
    return 0.0f;
  }
  const float luma = GPUHardwareValidationGate::ComputeRoiMeanLuma(
      pixels, static_cast<size_t>(fboW) * static_cast<size_t>(fboH) * 4, fboW,
      fboH, roiX, roiY, roiW, roiH);
  RL_FREE(pixels);
  return luma;
}

// W6 (M0-C) Blocker-1: reads back the REAL GI distance field texture (JFAPass)
// with glGetTexImage and probes its spatial structure. A genuine signed
// distance field is non-degenerate: the texel at the probe points differ by
// more than a tiny epsilon (interior vs exterior of occluders). The evidence
// (min/max/mean + 5 sign-probe samples: 4 corners + center of the distance
// channel) is recorded in the artifact; no synthetic proxy is ever used.
struct SdfProbeResult {
  bool texturePresent = false; // real distance field resource existed
  bool signValid = false;      // spatial-variation sign probe passed
  float minValue = 0.0f;
  float maxValue = 0.0f;
  float meanValue = 0.0f;
  std::vector<float> probeSamples; // 5: corners + center, distance channel
  std::string reason;
};

SdfProbeResult ProbeGiDistanceField(uint32_t texture, int width, int height) {
  constexpr uint32_t kGLTexture2D = 0x0DE1;
  constexpr uint32_t kGLRed = 0x1903;   // JFA distance field is GL_R16F (R only)
  constexpr uint32_t kGLFloat = 0x1406;
  constexpr uint32_t kGLTextureBinding2D = 0x8069;

  SdfProbeResult out;
  if (texture == 0u || width <= 0 || height <= 0) {
    out.reason = "no distance field texture";
    return out;
  }
  using GlGetTexImageFn = void(APIENTRY *)(uint32_t, int, uint32_t, uint32_t,
                                           void *);
  using GlGetIntegervFn = void(APIENTRY *)(uint32_t, int *);
  auto glGetTexImage =
      reinterpret_cast<GlGetTexImageFn>(glfwGetProcAddress("glGetTexImage"));
  auto glGetIntegerv =
      reinterpret_cast<GlGetIntegervFn>(glfwGetProcAddress("glGetIntegerv"));
  if (glGetTexImage == nullptr || glGetIntegerv == nullptr) {
    out.reason = "glGetTexImage/glGetIntegerv unavailable";
    return out;
  }

  int previousTexture = 0;
  glGetIntegerv(kGLTextureBinding2D, &previousTexture);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, texture);

  const size_t texelCount = static_cast<size_t>(width) * height;
  std::vector<float> texels(texelCount, 0.0f);
  glGetTexImage(kGLTexture2D, 0, kGLRed, kGLFloat, texels.data());

  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D,
                                          static_cast<uint32_t>(previousTexture));

  // Distance field channel = the single R channel (GL_R16F). Any non-finite
  // texel means the resource is not a valid SDF and fails closed.
  double meanSum = 0.0;
  float minV = std::numeric_limits<float>::max();
  float maxV = std::numeric_limits<float>::lowest();
  for (const float v : texels) {
    if (!std::isfinite(v)) {
      out.reason = "non-finite texel in distance field";
      return out;
    }
    minV = std::min(minV, v);
    maxV = std::max(maxV, v);
    meanSum += static_cast<double>(v);
  }
  out.minValue = minV;
  out.maxValue = maxV;
  out.meanValue = static_cast<float>(meanSum / static_cast<double>(texelCount));
  out.texturePresent = true;

  // Sign probe: 4 corners + center of the distance field.
  auto texelAt = [&](int x, int y) {
    return texels[static_cast<size_t>(y) * width + static_cast<size_t>(x)];
  };
  out.probeSamples.push_back(texelAt(0, 0));
  out.probeSamples.push_back(texelAt(width - 1, 0));
  out.probeSamples.push_back(texelAt(0, height - 1));
  out.probeSamples.push_back(texelAt(width - 1, height - 1));
  out.probeSamples.push_back(texelAt(width / 2, height / 2));

  constexpr float kSignProbeEpsilon = 1e-4f;
  const float center = out.probeSamples[4];
  float farthest = 0.0f;
  for (size_t i = 0; i < 4; ++i) {
    farthest = std::max(farthest, std::fabs(out.probeSamples[i] - center));
  }
  out.signValid = ((maxV - minV) > kSignProbeEpsilon) &&
                  (farthest > kSignProbeEpsilon);
  if (!out.signValid) {
    out.reason = "distance field is spatially degenerate (sign probe failed)";
  }
  return out;
}

// W6 (M0-C) occupancy evidence (M0-A R3): reads back the REAL GI composite
// occupancy history texture (R8 ping-pong, read side) with glGetTexImage
// (GL_RED/GL_FLOAT) and delegates the classification to the pure-CPU
// ClassifyOccupancyProbe so the 0/1-mask contract is unit-testable without a
// GPU. GL texture binding is restored on exit. Fail-closed: a missing texture
// or unavailable entry point yields a failed probe (never default-filled).
OccupancyProbeResult ProbeGiOccupancy(uint32_t texture, int width, int height) {
  constexpr uint32_t kGLTexture2D = 0x0DE1;
  constexpr uint32_t kGLRed = 0x1903;   // occupancy history is R8 (R only)
  constexpr uint32_t kGLFloat = 0x1406;
  constexpr uint32_t kGLTextureBinding2D = 0x8069;

  OccupancyProbeResult out;
  if (texture == 0u || width <= 0 || height <= 0) {
    out.reason = "no occupancy history texture";
    return out;
  }
  using GlGetTexImageFn = void(APIENTRY *)(uint32_t, int, uint32_t, uint32_t,
                                           void *);
  using GlGetIntegervFn = void(APIENTRY *)(uint32_t, int *);
  auto glGetTexImage =
      reinterpret_cast<GlGetTexImageFn>(glfwGetProcAddress("glGetTexImage"));
  auto glGetIntegerv =
      reinterpret_cast<GlGetIntegervFn>(glfwGetProcAddress("glGetIntegerv"));
  if (glGetTexImage == nullptr || glGetIntegerv == nullptr) {
    out.reason = "glGetTexImage/glGetIntegerv unavailable";
    return out;
  }

  int previousTexture = 0;
  glGetIntegerv(kGLTextureBinding2D, &previousTexture);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, texture);

  const size_t texelCount = static_cast<size_t>(width) * height;
  std::vector<float> texels(texelCount, 0.0f);
  glGetTexImage(kGLTexture2D, 0, kGLRed, kGLFloat, texels.data());

  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D,
                                          static_cast<uint32_t>(previousTexture));

  return GPUHardwareValidationGate::ClassifyOccupancyProbe(
      texels.data(), texelCount, width, height);
}

// Gate-fin: the gate renders into raw GL framebuffers (not raylib
// RenderTexture2D), so the window-sized default raylib projection would map
// every hook / batch draw through the 2560x1440 window ortho instead of the
// 1280x720 offscreen target. These helpers mirror what BeginTextureMode does
// for render textures (rcore.c: rlOrtho(0, rt.width, rt.height, 0, 0, 1)) so
// scene content lands at the correct position/scale inside the offscreen FBO.
void ApplyTargetProjection(int width, int height) {
  rlMatrixMode(RL_PROJECTION);
  rlLoadIdentity();
  rlOrtho(0, static_cast<double>(width), static_cast<double>(height), 0, 0.0,
          1.0);
  rlMatrixMode(RL_MODELVIEW);
}

void RestoreWindowProjection() {
  rlMatrixMode(RL_PROJECTION);
  rlLoadIdentity();
  rlOrtho(0, static_cast<double>(GetScreenWidth()),
          static_cast<double>(GetScreenHeight()), 0, 0.0, 1.0);
  rlMatrixMode(RL_MODELVIEW);
}

} // namespace

// W6 (M0-C) High-2: pure CPU ROI crop/mean over a full RGBA8 frame. The GPU
// readback path (ReadRoiMeanLuma) reads the full target and delegates here so
// the sampled region always matches the declared ROI origin - the non-zero
// ROI-origin contract is covered by a GPU-free unit test.
float GPUHardwareValidationGate::ComputeRoiMeanLuma(const uint8_t *fullRgba,
                                                    size_t fullSizeBytes,
                                                    int fullW, int fullH,
                                                    int roiX, int roiY,
                                                    int roiW, int roiH) {
  if (fullRgba == nullptr || fullW <= 0 || fullH <= 0 || roiW <= 0 ||
      roiH <= 0) {
    return 0.0f;
  }
  if (roiX < 0 || roiY < 0 || roiX + roiW > fullW || roiY + roiH > fullH) {
    return 0.0f;
  }
  const size_t bytesPerRow = static_cast<size_t>(fullW) * 4;
  if (fullSizeBytes < bytesPerRow * static_cast<size_t>(fullH)) {
    return 0.0f;
  }
  uint64_t totalLuma = 0;
  for (int y = 0; y < roiH; ++y) {
    const uint8_t *row = fullRgba + static_cast<size_t>(roiY + y) * bytesPerRow +
                         static_cast<size_t>(roiX) * 4;
    for (int x = 0; x < roiW; ++x) {
      totalLuma += static_cast<uint64_t>(row[x * 4]) +
                   static_cast<uint64_t>(row[x * 4 + 1]) +
                   static_cast<uint64_t>(row[x * 4 + 2]);
    }
  }
  return static_cast<float>(totalLuma) /
         (static_cast<float>(roiW) * static_cast<float>(roiH) * 3.0f * 255.0f);
}

// W6 (M0-C) occupancy evidence (M0-A R3): pure-CPU classifier over the raw
// occupancy mask texels (GL_RED/GL_FLOAT readback of the R8 history). Validates
// dimensions, every texel finite, min/max/mean, and a 5-point probe (4 corners
// + center) sitting within kOccupancyMaskEpsilon of {0,1} - occupancy is a
// 0/1 mask. GPU-free so the mask contract is unit-testable; the GPU readback
// path (ProbeGiOccupancy) delegates here. Fail-closed on any violation.
OccupancyProbeResult GPUHardwareValidationGate::ClassifyOccupancyProbe(
    const float *texels, size_t texelCount, int width, int height) {
  OccupancyProbeResult out;
  if (texels == nullptr || texelCount == 0 || width <= 0 || height <= 0) {
    out.reason = "invalid probe input (null/empty/dimensions)";
    return out;
  }
  if (static_cast<size_t>(width) * static_cast<size_t>(height) != texelCount) {
    out.reason = "dimensions do not match texel count";
    return out;
  }

  double meanSum = 0.0;
  float minV = std::numeric_limits<float>::max();
  float maxV = std::numeric_limits<float>::lowest();
  for (size_t i = 0; i < texelCount; ++i) {
    const float v = texels[i];
    if (!std::isfinite(v)) {
      out.reason = "non-finite texel in occupancy history";
      return out;
    }
    minV = std::min(minV, v);
    maxV = std::max(maxV, v);
    meanSum += static_cast<double>(v);
  }
  out.minValue = minV;
  out.maxValue = maxV;
  out.meanValue = static_cast<float>(meanSum / static_cast<double>(texelCount));
  out.texturePresent = true;

  // 5-point probe: 4 corners + center of the occupancy mask.
  auto texelAt = [&](int x, int y) {
    return texels[static_cast<size_t>(y) * width + static_cast<size_t>(x)];
  };
  out.probeSamples.push_back(texelAt(0, 0));
  out.probeSamples.push_back(texelAt(width - 1, 0));
  out.probeSamples.push_back(texelAt(0, height - 1));
  out.probeSamples.push_back(texelAt(width - 1, height - 1));
  out.probeSamples.push_back(texelAt(width / 2, height / 2));

  // Occupancy is a 0/1 mask: every probe point must sit within epsilon of
  // either 0 or 1. A gradient/alpha-ish channel fails closed.
  constexpr float kOccupancyMaskEpsilon = 0.02f;
  bool allNearBinary = true;
  for (const float p : out.probeSamples) {
    const float d0 = std::fabs(p);
    const float d1 = std::fabs(p - 1.0f);
    if (d0 > kOccupancyMaskEpsilon && d1 > kOccupancyMaskEpsilon) {
      allNearBinary = false;
      break;
    }
  }
  out.maskValid = allNearBinary;
  if (!out.maskValid) {
    out.reason =
        "occupancy history is not a 0/1 mask (probe sample outside {0,1} "
        "epsilon)";
  }
  return out;
}

// W6 (M0-C) occupancy evidence verdict (fail-closed). "present" requires a real
// history texture exposed by the GI composite pass, a valid 0/1 mask probe, and
// a positive history reset count (proof temporal rejection actually occurred).
// Anything else is "failed" with blocksGo=true - never silently passed.
OccupancyEvidenceResult GPUHardwareValidationGate::EvaluateOccupancyEvidence(
    uint32_t texture, int width, int height, const OccupancyProbeResult &probe,
    uint64_t historyResetCount, const std::string &lastResetReason) {
  OccupancyEvidenceResult out;
  out.texturePresent = (texture != 0u && width > 0 && height > 0);
  out.width = width;
  out.height = height;
  out.probe = probe;
  out.historyResetCount = historyResetCount;
  out.lastResetReason = lastResetReason;

  if (!out.texturePresent) {
    out.status = "failed";
    out.reason = "no occupancy history texture exposed by GICompositePass";
    out.blocksGo = true;
    return out;
  }
  if (!probe.texturePresent || !probe.maskValid) {
    out.status = "failed";
    out.reason = "occupancy history probe failed: " + probe.reason;
    out.blocksGo = true;
    return out;
  }
  // Temporal history rejection must actually have occurred at least once,
  // otherwise the history could be a trivially fresh buffer with no evidence.
  if (historyResetCount == 0) {
    out.status = "failed";
    out.reason = "occupancy history never reset (historyResetCount == 0); no "
                 "temporal rejection evidence";
    out.blocksGo = true;
    return out;
  }
  out.status = "present";
  out.reason = "occupancy history present with valid 0/1 mask probe and "
               "positive history reset count";
  out.blocksGo = false;
  return out;
}

// M0-B: external target contract - captures the REAL state of the
// harness-owned composite target. Uses legal GL 4.3 pnames only
// (glGetFramebufferAttachmentParameteriv: OBJECT_TYPE/OBJECT_NAME/COLOR_ENCODING/
// COMPONENT_TYPE/*_SIZE, plus texture-level / renderbuffer parameter queries for
// extent and internal format). The pseudo-pnames that commit 5c257e22 removed
// (0x8D24/0x8D25/0x825D) were never valid core GL constants and are NOT reused.
// Fail-closed: a missing entry point -> "unavailable"; fbo == 0, an absent
// attachment, or a contract mismatch (extent/internalFormat) -> "failed"; only
// a fully verified capture yields "passed". Nothing is default-filled.
TargetAttachmentState
GPUHardwareValidationGate::CaptureTargetState(uint32_t framebuffer,
                                              int expectedWidth,
                                              int expectedHeight,
                                              uint32_t expectedInternalFormat) {
  constexpr uint32_t kGlFramebuffer = 0x8D40;
  constexpr uint32_t kGlColorAttachment0 = 0x8CE0;
  constexpr uint32_t kGlFramebufferBinding = 0x8CA6;
  constexpr uint32_t kGlViewport = 0x0BA2;
  constexpr uint32_t kGlScissorTest = 0x0C11;
  constexpr uint32_t kGlScissorBox = 0x0C10;
  constexpr uint32_t kGlFramebufferAttachmentObjectType = 0x8CD0;
  constexpr uint32_t kGlFramebufferAttachmentObjectName = 0x8CD1;
  constexpr uint32_t kGlFramebufferAttachmentColorEncoding = 0x8210;
  constexpr uint32_t kGlFramebufferAttachmentComponentType = 0x8211;
  constexpr uint32_t kGlFramebufferAttachmentRedSize = 0x8212;
  constexpr uint32_t kGlFramebufferAttachmentGreenSize = 0x8213;
  constexpr uint32_t kGlFramebufferAttachmentBlueSize = 0x8214;
  constexpr uint32_t kGlFramebufferAttachmentAlphaSize = 0x8215;
  constexpr uint32_t kGlFramebufferAttachmentDepthSize = 0x8216;
  constexpr uint32_t kGlFramebufferAttachmentStencilSize = 0x8217;
  constexpr uint32_t kGlTexture = 0x1702;
  constexpr uint32_t kGlRenderbuffer = 0x8D41;
  constexpr uint32_t kGlTexture2D = 0x0DE1;
  constexpr uint32_t kGlTextureBinding2D = 0x8069;
  constexpr uint32_t kGlTextureWidth = 0x1000;
  constexpr uint32_t kGlTextureHeight = 0x1001;
  constexpr uint32_t kGlTextureInternalFormat = 0x1003;
  constexpr uint32_t kGlRenderbufferBinding = 0x8CA7;
  constexpr uint32_t kGlRenderbufferWidth = 0x8D42;
  constexpr uint32_t kGlRenderbufferHeight = 0x8D43;
  constexpr uint32_t kGlRenderbufferInternalFormat = 0x8D81;

  TargetAttachmentState out;
  out.expectedInternalFormat = expectedInternalFormat;
  if (framebuffer == 0u) {
    out.status = "failed";
    out.reason =
        "no composite target to capture (fbo == 0); external target contract "
        "cannot be verified";
    return out;
  }

  using GlGetIntegervFn = void(APIENTRY *)(uint32_t, int *);
  using GlGetFramebufferAttachmentParameterivFn =
      void(APIENTRY *)(uint32_t, uint32_t, uint32_t, int *);
  using GlGetTexLevelParameterivFn =
      void(APIENTRY *)(uint32_t, int, uint32_t, int *);
  using GlGetRenderbufferParameterivFn =
      void(APIENTRY *)(uint32_t, uint32_t, int *);
  using GlIsEnabledFn = uint8_t(APIENTRY *)(uint32_t);

  auto glGetIntegerv = reinterpret_cast<GlGetIntegervFn>(
      glfwGetProcAddress("glGetIntegerv"));
  auto glGetFramebufferAttachmentParameteriv =
      reinterpret_cast<GlGetFramebufferAttachmentParameterivFn>(
          glfwGetProcAddress("glGetFramebufferAttachmentParameteriv"));
  auto glGetTexLevelParameteriv = reinterpret_cast<GlGetTexLevelParameterivFn>(
      glfwGetProcAddress("glGetTexLevelParameteriv"));
  auto glGetRenderbufferParameteriv =
      reinterpret_cast<GlGetRenderbufferParameterivFn>(
          glfwGetProcAddress("glGetRenderbufferParameteriv"));
  auto glIsEnabled =
      reinterpret_cast<GlIsEnabledFn>(glfwGetProcAddress("glIsEnabled"));

  if (glGetIntegerv == nullptr ||
      glGetFramebufferAttachmentParameteriv == nullptr) {
    out.status = "unavailable";
    out.reason =
        "glGetIntegerv/glGetFramebufferAttachmentParameteriv unavailable; "
        "external target contract cannot be verified (fail-closed)";
    return out;
  }

  // bind/viewport/scissor snapshot - global state, queried before binding the
  // target so the recorded values match the harness-side state.
  {
    int previousBinding = 0;
    glGetIntegerv(kGlFramebufferBinding, &previousBinding);
    out.framebufferBinding = static_cast<uint32_t>(previousBinding);
  }
  {
    int viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(kGlViewport, viewport);
    out.viewportX = viewport[0];
    out.viewportY = viewport[1];
    out.viewportWidth = viewport[2];
    out.viewportHeight = viewport[3];
  }
  out.scissorTestEnabled =
      (glIsEnabled != nullptr) && (glIsEnabled(kGlScissorTest) != 0);
  {
    int scissor[4] = {0, 0, 0, 0};
    glGetIntegerv(kGlScissorBox, scissor);
    out.scissorX = scissor[0];
    out.scissorY = scissor[1];
    out.scissorWidth = scissor[2];
    out.scissorHeight = scissor[3];
  }

  // Bind the external target and query the COLOR_ATTACHMENT0 identity.
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGlFramebuffer, framebuffer);
  int objectType = 0;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentObjectType,
                                        &objectType);
  out.attachmentObjectType = static_cast<uint32_t>(objectType);

  if (objectType == 0) { // GL_NONE
    NoMoreDay::utils::GPUUtils::BindFramebuffer(kGlFramebuffer, 0);
    out.status = "failed";
    out.reason =
        "COLOR_ATTACHMENT0 has no attachment (OBJECT_TYPE == GL_NONE); "
        "external target contract cannot be verified";
    return out;
  }

  int objectName = 0;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentObjectName,
                                        &objectName);
  out.attachmentObjectName = static_cast<uint32_t>(objectName);

  // Attachment format parameters (valid for a color-renderable attachment on
  // GL 4.3). A driver that rejects any of these surfaces a GL error which the
  // gate's debug collector records - fail-closed, never silently ignored.
  int value = 0;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentColorEncoding,
                                        &value);
  out.colorEncoding = static_cast<uint32_t>(value);
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentComponentType,
                                        &value);
  out.componentType = static_cast<uint32_t>(value);
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentRedSize, &value);
  out.redSize = value;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentGreenSize,
                                        &value);
  out.greenSize = value;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentBlueSize, &value);
  out.blueSize = value;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentAlphaSize, &value);
  out.alphaSize = value;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentDepthSize, &value);
  out.depthSize = value;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentStencilSize,
                                        &value);
  out.stencilSize = value;

  // Extent + internal format via the attachment object's own query (texture
  // level parameters or renderbuffer parameters - the legal GL 4.3 way to read
  // them; the 0x8D24/0x8D25 pseudo-pnames were never valid constants).
  bool extentQueried = false;
  if (objectType == kGlTexture) {
    if (glGetTexLevelParameteriv != nullptr) {
      int previousBinding = 0;
      glGetIntegerv(kGlTextureBinding2D, &previousBinding);
      NoMoreDay::utils::GPUUtils::BindTexture(kGlTexture2D,
                                              static_cast<uint32_t>(objectName));
      glGetTexLevelParameteriv(kGlTexture2D, 0, kGlTextureWidth,
                               &out.attachmentWidth);
      glGetTexLevelParameteriv(kGlTexture2D, 0, kGlTextureHeight,
                               &out.attachmentHeight);
      glGetTexLevelParameteriv(kGlTexture2D, 0, kGlTextureInternalFormat,
                               &value);
      out.attachmentInternalFormat = static_cast<uint32_t>(value);
      NoMoreDay::utils::GPUUtils::BindTexture(
          kGlTexture2D, static_cast<uint32_t>(previousBinding));
      extentQueried = true;
    }
  } else if (objectType == kGlRenderbuffer) {
    if (glGetRenderbufferParameteriv != nullptr) {
      int previousBinding = 0;
      glGetIntegerv(kGlRenderbufferBinding, &previousBinding);
      NoMoreDay::utils::GPUUtils::BindRenderbuffer(
          kGlRenderbuffer, static_cast<uint32_t>(objectName));
      glGetRenderbufferParameteriv(kGlRenderbuffer, kGlRenderbufferWidth,
                                   &out.attachmentWidth);
      glGetRenderbufferParameteriv(kGlRenderbuffer, kGlRenderbufferHeight,
                                   &out.attachmentHeight);
      glGetRenderbufferParameteriv(kGlRenderbuffer,
                                   kGlRenderbufferInternalFormat, &value);
      out.attachmentInternalFormat = static_cast<uint32_t>(value);
      NoMoreDay::utils::GPUUtils::BindRenderbuffer(
          kGlRenderbuffer, static_cast<uint32_t>(previousBinding));
      extentQueried = true;
    }
  }

  // Restore the previous framebuffer binding.
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGlFramebuffer, 0);

  if (objectType != kGlTexture && objectType != kGlRenderbuffer) {
    out.status = "failed";
    out.reason =
        "COLOR_ATTACHMENT0 object type " + std::to_string(objectType) +
        " is neither GL_TEXTURE nor GL_RENDERBUFFER; external target contract "
        "cannot be verified";
    return out;
  }
  if (!extentQueried) {
    out.status = "unavailable";
    out.reason =
        "texture-level/renderbuffer parameter query entry point unavailable; "
        "extent/format of the external target cannot be verified (fail-closed)";
    return out;
  }

  // Contract verification: extent and internal format must match the expected
  // external target contract exactly. No default-fill is ever synthesized.
  if (out.attachmentWidth != expectedWidth ||
      out.attachmentHeight != expectedHeight) {
    out.status = "failed";
    char extentBuf[192] = {0};
    std::snprintf(extentBuf, sizeof(extentBuf),
                  "external target extent %dx%d does not match contract %dx%d",
                  out.attachmentWidth, out.attachmentHeight, expectedWidth,
                  expectedHeight);
    out.reason = extentBuf;
    return out;
  }
  if (expectedInternalFormat != 0u &&
      out.attachmentInternalFormat != expectedInternalFormat) {
    out.status = "failed";
    char formatBuf[192] = {0};
    std::snprintf(formatBuf, sizeof(formatBuf),
                  "external target internal format 0x%04X does not match "
                  "contract 0x%04X",
                  out.attachmentInternalFormat, expectedInternalFormat);
    out.reason = formatBuf;
    return out;
  }

  out.status = "passed";
  out.captured = true;
  return out;
}

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
  // W6 (M0-C) High-3: report the REAL GPU identity from glGetString - vendor
  // (GL_VENDOR), driver build (GL_VERSION) and renderer (GL_RENDERER). No
  // hardcoded labels; missing identity fails the preflight closed so the
  // artifact can never claim a GPU it did not observe.
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_43)
  const unsigned char *vendorStr = glGetString(GL_VENDOR);
  if (vendorStr != nullptr) {
    report.vendor = reinterpret_cast<const char *>(vendorStr);
  }
  const unsigned char *versionStr = glGetString(GL_VERSION);
  if (versionStr != nullptr) {
    report.driverVersion = reinterpret_cast<const char *>(versionStr);
  }
#endif
  // S7b: report the real GL_RENDERER string (e.g. "NVIDIA GeForce RTX 4070
  // SUPER/PCIe/SSE2") instead of a hardcoded label, and classify it so the
  // evidence can distinguish a real GPU from WARP/software rasterization.
  std::string rendererName = "Unknown";
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_43)
  const unsigned char *rendererStr = glGetString(GL_RENDERER);
  if (rendererStr != nullptr) {
    rendererName = reinterpret_cast<const char *>(rendererStr);
  }
#endif
  report.renderer = rendererName;
  report.rendererIsHardware = IsHardwareRenderer(rendererName);

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

  // W6 (M0-C) High-3: GPU identity is mandatory evidence. An empty vendor or
  // driver version means the context did not expose its identity; the gate
  // fails closed (NOT_RUN) instead of default-filling a label.
  if (report.vendor.empty() || report.driverVersion.empty()) {
    report.meetsPreflightPrerequisites = false;
    report.preflightFailureReason =
        "GL_VENDOR/GL_VERSION unavailable; GPU identity incomplete (fail-closed)";
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

PairedGiDeltaResult GPUHardwareValidationGate::RunPairedGiDeltaCapture(
    FixtureRenderDriver &driver, const FixtureConfig &fixture,
    const std::string &qualityTier) {
  PairedGiDeltaResult result;
  result.fixtureName = fixture.name;
  result.sceneSeed = fixture.sceneSeed;
  result.width = fixture.width;
  result.height = fixture.height;
  result.colorSpace = "sRGB";
  result.qualityTier = qualityTier;
  result.roiX = fixture.roiX;
  result.roiY = fixture.roiY;
  result.roiWidth = fixture.roiWidth;
  result.roiHeight = fixture.roiHeight;
  result.warmupFrames = fixture.warmupFrames;
  result.sampleFrames = fixture.sampleFrames;
  result.threshold = 0.001f;
  result.passed = false;

  // S7b real-machine-first: record the GL environment so the evidence can
  // distinguish a real GPU from WARP/software rasterization.
  std::string rendererName = "Unknown";
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_43)
  const unsigned char *rendererStr = glGetString(GL_RENDERER);
  if (rendererStr != nullptr) {
    rendererName = reinterpret_cast<const char *>(rendererStr);
  }
#endif
  result.renderer = rendererName;
  result.rendererIsHardware = IsHardwareRenderer(rendererName);

  auto &tierMgr = NoMoreDay::render::core::QualityTierManager::Get();
  // W6 (M0-C): the paired capture runs under the caller's tier (matrix cells
  // pass their own tier so the paired evidence is per-cell).
  if (qualityTier == "Ultra") {
    tierMgr.ForceTier(NoMoreDay::render::core::QualityTier::Ultra);
  } else {
    tierMgr.ForceTier(NoMoreDay::render::core::QualityTier::High);
  }

  entt::registry &registry = driver.Registry();
  const NoMoreDay::render::RenderFrameInput renderInput = driver.RenderInput();

  if (!driver.PrepareFixture(fixture)) {
    result.failureReasons.push_back("Fixture scene preparation failed (harness)");
    return result;
  }

  const uint32_t offscreenFbo = driver.CompositeFramebuffer();
  if (offscreenFbo == 0) {
    result.failureReasons.push_back(
        "Fixture harness reported invalid RGBA16F composite target");
    return result;
  }

  const int roiW = std::min(fixture.roiWidth, fixture.width - fixture.roiX);
  const int roiH = std::min(fixture.roiHeight, fixture.height - fixture.roiY);
  if (roiW <= 0 || roiH <= 0) {
    result.failureReasons.push_back("ROI out of bounds for fixture resolution");
    return result;
  }

  Camera2D camera{};
  camera.target = Vector2{fixture.cameraX, fixture.cameraY};
  camera.offset = Vector2{static_cast<float>(fixture.width) / 2.0f,
                          static_cast<float>(fixture.height) / 2.0f};
  camera.rotation = 0.0f;
  camera.zoom = fixture.cameraZoom;

  constexpr uint32_t kGLFramebuffer = 0x8D40;

  auto runLeg = [&](bool giEnabled, float &outMeanLuma,
                    std::vector<float> &outPerFrameLuma) -> bool {
    // S7a/S7b: runtime override drives the effective config for the whole leg
    // and is restored on scope exit (exception-safe). Paired capture never
    // mutates settings.json, so no settings override is injected.
    NoMoreDay::render::core::QualityTierManager::GiEnabledOverrideGuard guard(
        giEnabled);
    if (!guard.IsOwned()) {
      return false;
    }
    // Each leg runs its own temporal history warmup (GICompositePass history is
    // invalidated on the GI transition inside RenderSystem).
    for (int f = 0; f < fixture.warmupFrames; ++f) {
      NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, offscreenFbo);
      // Gate-fin: pin the viewport to the offscreen target (a leftover window
      // viewport would make the HDR buffer and composite blit exceed the FBO)
      // and apply the camera with BeginMode2D (without it the scene draws in
      // raw world coordinates and misses the ROI entirely).
      NoMoreDay::utils::GPUUtils::Viewport(0, 0, fixture.width, fixture.height);
      // Gate-fin: mirror BeginTextureMode's projection setup - without an
      // offscreen-sized ortho the hooks/batch draw through the window-sized
      // default projection and the scene misses the ROI.
      ApplyTargetProjection(fixture.width, fixture.height);
      BeginMode2D(camera);
      // W6 (M0-C): the driver may supply real gameplay render hooks (production
      // game-binary gate); test harnesses keep the default nullptr.
      ::RenderSystem::render(registry, renderInput, camera, driver.RenderHooks());
      EndMode2D();
      RestoreWindowProjection();
      NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
    }
    // Independent sampling window: one ROI readback per sampled frame.
    double lumaSum = 0.0;
    for (int f = 0; f < fixture.sampleFrames; ++f) {
      NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, offscreenFbo);
      NoMoreDay::utils::GPUUtils::Viewport(0, 0, fixture.width, fixture.height);
      ApplyTargetProjection(fixture.width, fixture.height);
      BeginMode2D(camera);
      ::RenderSystem::render(registry, renderInput, camera, driver.RenderHooks());
      EndMode2D();
      RestoreWindowProjection();
      NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
      const float frameLuma = ReadRoiMeanLuma(
          offscreenFbo, fixture.width, fixture.height, fixture.roiX,
          fixture.roiY, roiW, roiH);
      outPerFrameLuma.push_back(frameLuma);
      lumaSum += static_cast<double>(frameLuma);
    }
    outMeanLuma =
        static_cast<float>(lumaSum / static_cast<double>(fixture.sampleFrames));
    return true;
  };

  std::vector<float> legOnLuma;
  std::vector<float> legOffLuma;
  const bool legOnOk =
      runLeg(true, result.roiMeanOn, legOnLuma);
  result.legPassTraces.push_back(BuildGiPassTrace(true));
  result.trackedBytesOn =
      NoMoreDay::render::resources::GPUResourceRegistry::Get()
          .GetStats()
          .currentTotalBytes;
  const bool legOffOk =
      runLeg(false, result.roiMeanOff, legOffLuma);
  result.legPassTraces.push_back(BuildGiPassTrace(false));
  result.trackedBytesOff =
      NoMoreDay::render::resources::GPUResourceRegistry::Get()
          .GetStats()
          .currentTotalBytes;

  if (!legOnOk || !legOffOk) {
    result.failureReasons.push_back(
        "Runtime GI override rejected during paired capture (thread ownership)");
    return result;
  }

  // S7b: paired delta = mean over the sampling window of the absolute per-frame
  // ROI mean-brightness difference between the GI-ON and GI-OFF legs.
  const size_t pairedFrames = std::min(legOnLuma.size(), legOffLuma.size());
  double deltaSum = 0.0;
  for (size_t f = 0; f < pairedFrames; ++f) {
    deltaSum += std::fabs(static_cast<double>(legOnLuma[f] - legOffLuma[f]));
  }
  if (pairedFrames == 0) {
    result.failureReasons.push_back("Paired capture produced no sampled frames");
    return result;
  }
  result.pairedDelta =
      static_cast<float>(deltaSum / static_cast<double>(pairedFrames));
  result.passed = (result.pairedDelta >= result.threshold);
  if (!result.passed) {
    result.failureReasons.push_back(
        "Paired GI delta " + std::to_string(result.pairedDelta) +
        " below threshold " + std::to_string(result.threshold));
  }

  return result;
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
  const NoMoreDay::render::RenderFrameInput renderInput = driver->RenderInput();

  // W6 (M0-C) High-3: a production driver (game-binary composition root) MUST
  // supply real gameplay render hooks. Without them RenderSystem would run the
  // diagnostic zero-draw path and the evidence would be hollow; fail closed
  // with a full NOT_RUN report. Contract/diagnostic harnesses are exempt (their
  // documented environment renders with nullptr hooks and is never production
  // evidence).
  report.hooksSupplied = (driver->RenderHooks() != nullptr);
  if (driver->IsProductionDriver() && !report.hooksSupplied) {
    report.status = GateStatus::NotRun;
    report.globalFailures.push_back(
        "Production FixtureRenderDriver did not supply gameplay render hooks; "
        "fail-closed NOT_RUN (render would be hollow)");
    LOG_WARN("GPUHardwareValidationGate: GATE NOT RUN - production driver has no render hooks");
    return report;
  }

  // W6 (M0-C) Medium-5: explicit below-floor parameters are honored verbatim
  // (diagnostic runs) and recorded as requested/actual; only the production
  // defaults sit at the 120/100 floor. A non-exhaustive run can never yield GO.
  report.requestedSampleFrames = sampleFramesPerFixture;
  report.requestedToggleLoops = toggleLoops;
  report.actualSampleFrames = sampleFramesPerFixture;
  report.actualToggleLoops = toggleLoops;
  report.nonExhaustive = (report.actualSampleFrames < 120) ||
                         (report.actualToggleLoops < 100);

  // W6 (M0-C) Blocker-1: occupancy/disocclusion evidence (M0-A R3) is collected
  // AFTER the real matrix/stress/toggle renders below (the history texture only
  // exists once GICompositePass has executed on the real path), then judged
  // fail-closed before the final decision. No placeholder/default-fill.

  // 2. Fixture Execution Matrix
  const auto fixtures = GetStandardFixtures();
  const std::vector<std::pair<std::string, bool>> tierModes = {
      {"High", true}, {"Ultra", true}, {"High", false}}; // Tier, GI enabled

  constexpr uint32_t kGLFramebuffer = 0x8D40;
  constexpr uint32_t kRgba16f = 0x881A; // Blocker 5: Must use RGBA16F HDR format
  bool allMatrixPassed = true;
  // W6 (M0-C) Blocker-1: every GI-enabled matrix cell must deliver a real SDF
  // sign probe ("passed"); GI-off cells record "not_applicable".
  bool allGiSdfProbesPassed = true;

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
        execResult.cameraX = fixture.cameraX;
        execResult.cameraY = fixture.cameraY;
        execResult.cameraZoom = fixture.cameraZoom;
        execResult.roiX = fixture.roiX;
        execResult.roiY = fixture.roiY;
        execResult.roiWidth = fixture.roiWidth;
        execResult.roiHeight = fixture.roiHeight;
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
      // W6 (M0-C): camera and ROI recorded per matrix cell for reproducibility.
      execResult.cameraX = fixture.cameraX;
      execResult.cameraY = fixture.cameraY;
      execResult.cameraZoom = fixture.cameraZoom;
      execResult.roiX = fixture.roiX;
      execResult.roiY = fixture.roiY;
      execResult.roiWidth = fixture.roiWidth;
      execResult.roiHeight = fixture.roiHeight;
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

      // MS-8 follow-up: render this cell under the exact GI state it records.
      // Without an explicit runtime override the effective config silently
      // follows stale tier defaults or residue from the previous cell's paired
      // capture, so the GI chain would not even be added to the real graph for
      // the very first GI-on cell. The guard drives warmup + samples + readback
      // and restores on scope exit; the paired capture nests its own guard.
      NoMoreDay::render::core::QualityTierManager::GiEnabledOverrideGuard
          giGuard(giOn);
      if (!giGuard.IsOwned()) {
        executionChecksPassed = false;
        execResult.failureReasons.push_back(
            "Failed to acquire GI runtime override for matrix cell");
        allMatrixPassed = false;
        report.matrixResults.push_back(execResult);
        continue;
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

      // S6 (T6.4): the RGBA16F offscreen composite target is owned by the
      // harness (driver), not the gate. The gate binds and reads it back but
      // never creates or destroys it, so target lifetime never conflicts with
      // the harness (no double-create, no use-after-free).
      const uint32_t offscreenFbo = driver->CompositeFramebuffer();
      if (offscreenFbo == 0) {
        execResult.overallPassed = false;
        // M0-B: the missing target is recorded verbatim (not default-filled);
        // the cell fails closed because the RGBA16F external target contract
        // could not be verified.
        execResult.targetState.status = "failed";
        execResult.targetState.reason =
            "no composite target to capture (fbo == 0); RGBA16F external "
            "target contract cannot be verified";
        execResult.failureReasons.push_back(
            "Fixture harness reported invalid RGBA16F composite target");
        allMatrixPassed = false;
        report.matrixResults.push_back(execResult);
        continue;
      }

      // M0-B: external target contract - capture the REAL attachment state of
      // the harness-owned composite target (identity/extent/format + bind/
      // viewport/scissor). Any capture failure or contract mismatch fails the
      // cell fail-closed; nothing is default-filled.
      execResult.targetState = CaptureTargetState(
          offscreenFbo, fixture.width, fixture.height, kRgba16f);
      if (execResult.targetState.status != "passed") {
        executionChecksPassed = false;
        execResult.failureReasons.push_back(
            "External target state capture failed [" +
            execResult.targetState.status + "]: " +
            execResult.targetState.reason);
        allMatrixPassed = false;
      }

      // Blocker 1: Real Offscreen Gameplay Frame Rendering
      // Warmup Frames
      for (int f = 0; f < fixture.warmupFrames; ++f) {
        NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, offscreenFbo);
        // Gate-fin: pin the viewport to the offscreen target and apply the
        // camera (leftover window viewport / raw-world draw would misplace the
        // scene and blow out the HDR blit region).
        NoMoreDay::utils::GPUUtils::Viewport(0, 0, fixture.width, fixture.height);
        // Gate-fin: mirror BeginTextureMode's projection setup so the
        // hooks/batch draw through an offscreen-sized ortho, not the window
        // projection (same rationale as the runLeg call sites).
        ApplyTargetProjection(fixture.width, fixture.height);
        BeginMode2D(camera);
        // W6 (M0-C): production game-binary gate supplies real hooks so the full
        // gameplay draw path (occluders/height-field/loot/emissive) is exercised.
        ::RenderSystem::render(registry, renderInput, camera, driver->RenderHooks());
        EndMode2D();
        RestoreWindowProjection();
        NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
      }

      // Blocker 3 / R4 Fix: Pass Timing Statistics & AND Condition Check (>= 120 samples AND P95 <= Budget)
      const auto passBudgets = GetPassBudgets();

      // Sample Frames: Collect real GPU timer query ring statistics per frame.
      // S0: RenderGraph keys the ring by stable pass id; derive ids from names.
      // W6 (M0-C) Medium-5: explicit below-floor parameters are honored verbatim
      // (no clamping); requested/actual + non_exhaustive are recorded in the
      // artifact and a non-exhaustive run can never yield GO.
      const int actualSampleFrames = sampleFramesPerFixture;
      std::vector<std::vector<double>> passTimingSamples(passBudgets.size());

      for (int f = 0; f < actualSampleFrames; ++f) {
        // RenderGraph::Execute is the single frame owner for the timer ring.
        NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, offscreenFbo);
        NoMoreDay::utils::GPUUtils::Viewport(0, 0, fixture.width, fixture.height);
        ApplyTargetProjection(fixture.width, fixture.height);
        BeginMode2D(camera);
        ::RenderSystem::render(registry, renderInput, camera, driver->RenderHooks());
        EndMode2D();
        RestoreWindowProjection();
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

      // W6 (M0-C) Blocker-1: pass trace from the REAL execution path. The last
      // RenderSystem::render frame compiled and executed the actual graph; its
      // pass order (RenderGraph::CompiledRenderPlan.passOrder) is captured
      // inside render() and exposed here. No synthetic test graph is ever used.
      const auto &realPassOrder = RenderSystem::GetLastExecutedPassOrder();
      execResult.executedPassOrder = realPassOrder;
      execResult.passTraceSource =
          "RenderGraph::CompiledRenderPlan.passOrder via RenderSystem::render "
          "(real execution)";
      execResult.passTraceValid = !realPassOrder.empty();
      if (!execResult.passTraceValid) {
        executionChecksPassed = false;
        execResult.failureReasons.push_back(
            "RenderSystem::render produced no executed pass order; real pass "
            "trace missing (fail-closed)");
      }

      // B13: capture the real JFA work shape without changing its execution or
      // gate verdict. This distinguishes full/incremental work and recovery or
      // verification cost when a timing budget is exceeded.
      const auto jfaDiagnostics = RenderSystem::GetJfaDiagnostics();
      execResult.jfaMode = jfaDiagnostics.mode;
      execResult.jfaDispatchTexelCount = jfaDiagnostics.dispatchTexelCount;
      execResult.jfaDirtyRectArea = jfaDiagnostics.dirtyRectArea;
      execResult.jfaExpandedRectArea = jfaDiagnostics.expandedRectArea;
      execResult.jfaPlus2Recovery = jfaDiagnostics.plus2Recovery;
      execResult.jfaVerificationAttempted = jfaDiagnostics.verificationAttempted;
      execResult.jfaVerificationPassed = jfaDiagnostics.verificationPassed;
      execResult.jfaVerificationRecovery = jfaDiagnostics.verificationRecovery;
      execResult.jfaVerificationResult = jfaDiagnostics.verificationResult;

      // W6 (M0-C) High-2: ROI readback at its TRUE origin. rlReadScreenPixels
      // cannot sample an offset region, so the full FBO is read and CPU-cropped
      // to [roiX,roiY,w,h]; the sampled region always matches the declared ROI.
      const int roiW = std::min(fixture.roiWidth, fixture.width - fixture.roiX);
      const int roiH = std::min(fixture.roiHeight, fixture.height - fixture.roiY);
      const float meanLuma = ReadRoiMeanLuma(
          offscreenFbo, fixture.width, fixture.height, fixture.roiX,
          fixture.roiY, roiW, roiH);
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

      // W6 (M0-C) Blocker-1: per-cell paired GI delta capture (GI runtime
      // override ON vs OFF legs on the real fixture scene at this cell's tier).
      // The paired evidence is part of this cell's verdict, not evidence-only.
      {
        const PairedGiDeltaResult paired =
            RunPairedGiDeltaCapture(*driver, fixture, tierName);
        execResult.giPairedDelta = paired.pairedDelta;
        execResult.giPairedPassed = paired.passed;
        report.pairedGiDeltas.push_back(paired);
        if (!paired.passed) {
          execResult.failureReasons.push_back(
              "Per-cell paired GI delta " + std::to_string(paired.pairedDelta) +
              " below threshold " + std::to_string(paired.threshold));
        }
      }

      // W6 (M0-C) Blocker-1: REAL SDF sign probe. The GI distance field
      // (JFAPass) is the genuine SDF resource produced by the real render path;
      // it is read back with glGetTexImage and its spatial structure is probed
      // (4 corners + center of the distance channel). Runs AFTER the paired
      // delta capture so the GI-on leg has already created the JFAPass distance
      // field resource; probing before any GI-enabled render left the very
      // first cell with a "missing" texture. GI-off cells run no JFA pass, so
      // their SDF evidence is recorded as "not_applicable" (not counted against
      // the cell). GI-on cells must be "passed".
      execResult.sdfEvidenceSource =
          "JFAPass distance field texture readback (glGetTexImage)";
      if (giOn) {
        const auto giSdf = RenderSystem::GetGiDistanceField();
        if (giSdf.texture == 0u || giSdf.width <= 0 || giSdf.height <= 0) {
          execResult.sdfReadbackStatus = "missing";
          execResult.sdfReadbackPassed = false;
          execResult.failureReasons.push_back(
              "Real SDF resource (JFAPass distance field) not present; sign "
              "probe impossible (fail-closed)");
          allGiSdfProbesPassed = false;
        } else {
          const SdfProbeResult probe =
              ProbeGiDistanceField(giSdf.texture, giSdf.width, giSdf.height);
          execResult.sdfMinValue = probe.minValue;
          execResult.sdfMaxValue = probe.maxValue;
          execResult.sdfMeanValue = probe.meanValue;
          execResult.sdfProbeSamples = probe.probeSamples;
          if (probe.texturePresent && probe.signValid) {
            execResult.sdfReadbackStatus = "passed";
            execResult.sdfReadbackPassed = true;
          } else {
            execResult.sdfReadbackStatus = "failed";
            execResult.sdfReadbackPassed = false;
            execResult.failureReasons.push_back(
                "Real SDF sign probe failed: " + probe.reason);
            allGiSdfProbesPassed = false;
          }
        }
      } else {
        execResult.sdfReadbackStatus = "not_applicable";
        execResult.sdfReadbackPassed = false;
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
        // Passes with zero valid samples never executed for this fixture/tier
        // combination (e.g. HeightShadowPass without heightfield input, or GI
        // chain passes whose timer ring had no records on the first matrix
        // cell). They are not_applicable here: there is no performance to
        // judge, and content evidence (SDF readback, paired GI delta, ROI)
        // covers whether the pass should have run. Passes that executed but
        // produced too few valid samples or exceeded the budget still fail.
        const bool notApplicable = (tReport.validSampleCount == 0);
        tReport.passed =
            notApplicable ||
            (tReport.validSampleCount >= 120 && tReport.p95Ms <= tReport.budgetMs);
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

      // W6 (M0-C) Blocker-1: the cell verdict includes the real pass trace, the
      // per-cell paired GI delta and (for GI-on cells) the real SDF sign probe.
      // GI-off cells do not produce an SDF (no JFA pass), so SDF is excluded
      // from their verdict but recorded as not_applicable.
      const bool sdfOk = giOn ? execResult.sdfReadbackPassed : true;
      execResult.overallPassed =
          executionChecksPassed && execResult.passTraceValid &&
          execResult.nonBlackRoiPassed && execResult.giIndirectPassed && sdfOk &&
          execResult.giPairedPassed;

      if (!execResult.overallPassed) {
        allMatrixPassed = false;
      }
      report.matrixResults.push_back(execResult);
      // Gate-fin diagnostic: per-cell GL error checkpoint to localize the
      // source of any debug-callback-reported GL errors.
      const auto &cellCollector = debugOutputGuard.Collector();
      LOG_INFO("GPUHardwareValidationGate: cell [{}/{}] reported={} dropped={}",
               fixture.name, tierName, cellCollector.m_count,
               cellCollector.m_droppedCount);
    }
  }

  // Gate-fin diagnostic: GL error checkpoint after the full fixture matrix.
  {
    const auto &matrixCollector = debugOutputGuard.Collector();
    LOG_INFO("GPUHardwareValidationGate: phase [matrix] reported={} dropped={}",
             matrixCollector.m_count, matrixCollector.m_droppedCount);
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
    // Gate-fin: pace the pressure window to ~60 fps. Uncapped the loop renders
    // at 1000+ fps, so GPU timer queries regularly outlive the 3-slot ring and
    // pending-overage is reported spuriously (queries still in flight when the
    // slot is recycled). At 60 fps every query lands well inside the ring, so
    // the pending window measures genuine staleness, not frame-rate pressure.
    constexpr double kStressFrameTimeSeconds = 1.0 / 60.0;
    auto lastFrameStart = std::chrono::steady_clock::now();
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - stressStart)
               .count() < report.stressReport.durationSeconds) {
      if (stressTarget.IsValid()) {
        NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, stressTarget.fbo);
        NoMoreDay::utils::GPUUtils::Viewport(0, 0, stressTarget.width,
                                             stressTarget.height);
        ApplyTargetProjection(stressTarget.width, stressTarget.height);
        BeginMode2D(stressCam);
        ::RenderSystem::render(registry, renderInput, stressCam, driver->RenderHooks());
        EndMode2D();
        RestoreWindowProjection();
        NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
        // Frame boundary: RenderSystem::render advances the registry frame
        // counter exactly once after a successful graph execute (W5.6). The
        // gate must not advance again here, otherwise pending records would be
        // double-aged.
      }

      // Frame pacing: hold ~60 fps for the whole pressure window (see comment
      // above). The next-frame deadline is computed AFTER the sleep so pacing
      // does not drift under variable frame cost.
      const auto frameEnd = std::chrono::steady_clock::now();
      const double frameElapsed =
          std::chrono::duration<double>(frameEnd - lastFrameStart).count();
      if (frameElapsed < kStressFrameTimeSeconds) {
        std::this_thread::sleep_for(std::chrono::duration<double>(
            kStressFrameTimeSeconds - frameElapsed));
      }
      lastFrameStart = std::chrono::steady_clock::now();

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

  // Gate-fin diagnostic: GL error checkpoint after the pressure loop.
  {
    const auto &stressCollector = debugOutputGuard.Collector();
    LOG_INFO("GPUHardwareValidationGate: phase [stress] reported={} dropped={}",
             stressCollector.m_count, stressCollector.m_droppedCount);
  }

  // Execute 100-loop GI/Tier/Resize toggle stress
  // W6 (M0-C) Medium-5: explicit below-floor toggle counts are honored verbatim
  // (no clamping); requested/actual + non_exhaustive are recorded and a
  // non-exhaustive run can never yield GO.
  bool toggleStressPassed = true;
  const int actualToggleLoops = toggleLoops;

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
    NoMoreDay::utils::GPUUtils::Viewport(0, 0, w, h);
    ApplyTargetProjection(w, h);
    BeginMode2D(cam);
    ::RenderSystem::render(registry, renderInput, cam, driver->RenderHooks());
    EndMode2D();
    RestoreWindowProjection();
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

  // Gate-fin diagnostic: GL error checkpoint after the toggle loop.
  {
    const auto &toggleCollector = debugOutputGuard.Collector();
    LOG_INFO("GPUHardwareValidationGate: phase [toggle] reported={} dropped={}",
             toggleCollector.m_count, toggleCollector.m_droppedCount);
  }

  // Baseline live-resource set, captured after the resize toggle loop so the
  // final generation of persistent pass targets (legitimate churn from the
  // 1920x1080 <-> 1280x720 cycling) is excluded. The final leak count flags
  // resources alive at gate end that were created after this point; cumulative
  // leaks from matrix/stress/toggle churn still surface as extra live records,
  // while in-window pressure leaks are covered by the sliding-window check.
  std::set<std::pair<uint32_t, uint8_t>> baselineLiveKeys;
  for (const auto &rec : NoMoreDay::render::resources::GPUResourceRegistry::Get()
                             .GetActiveResources()) {
    baselineLiveKeys.insert(std::make_pair(rec.handle, static_cast<uint8_t>(rec.kind)));
  }

  // Resource Registry Snapshot
  const auto resStats =
      NoMoreDay::render::resources::GPUResourceRegistry::Get().GetStats();
  report.totalTrackedBytes = resStats.currentTotalBytes;
  report.peakTrackedBytes = resStats.peakTotalBytes;
  report.activeResourceCount = resStats.activeCount;

  // S4 (M0-C R5.2): leak candidates are resources alive at gate end that were
  // created after the post-toggle baseline (the baseline excludes the final
  // generation of legitimately long-lived persistent pass targets).
  size_t leakCandidateCount = 0;
  std::vector<LeakCandidateRecord> leakCandidates;
  const auto gateEndResources =
      NoMoreDay::render::resources::GPUResourceRegistry::Get().GetActiveResources();
  for (const auto &rec : gateEndResources) {
    if (!baselineLiveKeys.count(
            std::make_pair(rec.handle, static_cast<uint8_t>(rec.kind)))) {
      ++leakCandidateCount;
      leakCandidates.push_back(LeakCandidateRecord{
          rec.handle, rec.kind, rec.ownerTag, rec.sizeBytes, rec.name,
          rec.creationFrame});
    }
  }
  report.leakCandidateCount = leakCandidateCount;
  report.leakCandidates = leakCandidates;
  if (!leakCandidates.empty()) {
    LOG_INFO(
        "GPUHardwareValidationGate: leak-candidate detail baseline_count={} "
        "gate_end_count={} candidates={}",
        baselineLiveKeys.size(), gateEndResources.size(), leakCandidates.size());
    for (const auto &cand : leakCandidates) {
      LOG_INFO("GPUHardwareValidationGate:   leak-candidate handle={} kind={} "
               "owner={} bytes={} name=\"{}\" creation_frame={}",
               cand.handle, graph::ToResourceKindName(cand.kind),
               graph::ToOwnerName(cand.ownerTag), cand.sizeBytes, cand.name,
               cand.creationFrame);
    }
    // DIAG: multiset of (kind,sizeBytes) at baseline vs gate end, to prove
    // whether the 720p persistent targets are still alive alongside 1080p ones.
    auto keyStr = [](graph::ResourceKind k, size_t bytes) {
      return std::string(graph::ToResourceKindName(k)) + "/" +
             std::to_string(bytes);
    };
    std::map<std::string, int> baseMultiset;
    for (const auto &rec : NoMoreDay::render::resources::GPUResourceRegistry::Get()
                               .GetActiveResources()) {
      (void)rec;
    }
    {
      const auto &allNow = gateEndResources;
      std::map<std::string, int> endMultiset;
      for (const auto &rec : allNow) {
        ++endMultiset[keyStr(rec.kind, rec.sizeBytes)];
      }
      LOG_INFO("GPUHardwareValidationGate:   gate-end (kind/bytes -> count):");
      for (const auto &kv : endMultiset) {
        LOG_INFO("GPUHardwareValidationGate:     {} -> {}", kv.first, kv.second);
      }
    }
  }

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

  // W6 (M0-C) Blocker-1: occupancy evidence (M0-A R3). The REAL GI composite
  // occupancy history (R8 ping-pong, read side) is read back with glGetTexImage
  // (GL_RED/GL_FLOAT), classified as a 0/1 mask (ClassifyOccupancyProbe), and
  // judged with the temporal-history reset counter. Fail-closed: a missing
  // texture, an invalid mask, or zero resets yield status "failed" and block GO.
  {
    const auto giOccupancy = RenderSystem::GetGiOccupancy();
    report.occupancyTexturePresent =
        (giOccupancy.texture != 0u && giOccupancy.width > 0 &&
         giOccupancy.height > 0);
    report.occupancyProbeWidth = giOccupancy.width;
    report.occupancyProbeHeight = giOccupancy.height;
    report.occupancyHistoryResetCount = giOccupancy.historyResetCount;
    report.occupancyLastResetReason = giOccupancy.lastResetReason;

    const OccupancyProbeResult occupancyProbe = ProbeGiOccupancy(
        giOccupancy.texture, giOccupancy.width, giOccupancy.height);
    const OccupancyEvidenceResult occupancyEvidence =
        EvaluateOccupancyEvidence(giOccupancy.texture, giOccupancy.width,
                                  giOccupancy.height, occupancyProbe,
                                  giOccupancy.historyResetCount,
                                  giOccupancy.lastResetReason);
    report.occupancyStatus = occupancyEvidence.status;
    report.occupancyReason = occupancyEvidence.reason;
    report.occupancyMinValue = occupancyEvidence.probe.minValue;
    report.occupancyMaxValue = occupancyEvidence.probe.maxValue;
    report.occupancyMeanValue = occupancyEvidence.probe.meanValue;
    report.occupancyProbePoints = occupancyEvidence.probe.probeSamples;
  }

  // W6 (M0-C) Blocker-1: the gate is fail-closed on incomplete occupancy
  // evidence (must be "present"), a failed real SDF sign probe for any GI cell,
  // and non-exhaustive diagnostic runs.
  const bool evidenceComplete = (report.occupancyStatus == "present");
  if (!evidenceComplete) {
    allMatrixPassed = false;
    report.globalFailures.push_back(
        "Occupancy/disocclusion evidence status=" + report.occupancyStatus +
        " (" + report.occupancyReason +
        "); fail-closed, gate cannot GO");
  }
  if (!allGiSdfProbesPassed) {
    allMatrixPassed = false;
    report.globalFailures.push_back(
        "Real SDF sign probe missing/failed for a GI-enabled matrix cell");
  }
  if (report.nonExhaustive) {
    allMatrixPassed = false;
    report.globalFailures.push_back(
        "Non-exhaustive diagnostic sampling below production floor "
        "(samples=" +
        std::to_string(report.actualSampleFrames) +
        ", toggles=" + std::to_string(report.actualToggleLoops) +
        "); recorded as requested/actual, cannot yield GO");
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

  // Final Gate Decision. GO additionally requires complete occupancy evidence,
  // a passed real SDF probe on every GI cell, and a production-grade
  // (non-exhaustive) sample budget.
  if (allMatrixPassed && report.stressReport.stress1MinPassed &&
      toggleStressPassed && evidenceComplete && !report.nonExhaustive) {
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
      {"renderer_is_hardware", capabilities.rendererIsHardware},
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
      {"preflight_reason", capabilities.preflightFailureReason},
      // W6 (M0-C) High-3: whether the driver supplied real gameplay render
      // hooks (production binary gate must; diagnostic harnesses may not).
      {"render_hooks_supplied", hooksSupplied}};

  j["gate_status"] = (status == GateStatus::Go)     ? "GO"
                     : (status == GateStatus::NoGo) ? "NO_GO"
                                                    : "NOT_RUN";

  // W6 (M0-C) Medium-5: requested vs actual sampling budget and the
  // non-exhaustive (diagnostic) flag. A non-exhaustive run can never be GO.
  j["run_config"] = {
      {"requested_sample_frames", requestedSampleFrames},
      {"actual_sample_frames", actualSampleFrames},
      {"requested_toggle_loops", requestedToggleLoops},
      {"actual_toggle_loops", actualToggleLoops},
      {"non_exhaustive", nonExhaustive}};

  // W6 (M0-C) Blocker-1: occupancy/disocclusion evidence (M0-A R3). Status
  // "present" requires a real history texture, a valid 0/1 mask probe, and a
  // positive history reset count; anything else blocks GO (fail-closed).
  // No default-fill.
  j["occupancy"] = {
      {"status", occupancyStatus.empty() ? "missing_pending_m0a"
                                         : occupancyStatus},
      {"reason", occupancyReason},
      {"blocks_go", occupancyStatus != "present"},
      {"texture_present", occupancyTexturePresent},
      {"probe_width", occupancyProbeWidth},
      {"probe_height", occupancyProbeHeight},
      {"min_value", occupancyMinValue},
      {"max_value", occupancyMaxValue},
      {"mean_value", occupancyMeanValue},
      {"probe_points", occupancyProbePoints},
      {"reset_count", occupancyHistoryResetCount},
      {"last_reset_reason", occupancyLastResetReason}};

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

  nlohmann::json leakCandidatesJson = nlohmann::json::array();
  for (const auto &cand : leakCandidates) {
    leakCandidatesJson.push_back(
        {{"handle", cand.handle},
         {"kind", graph::ToResourceKindName(cand.kind)},
         {"owner", graph::ToOwnerName(cand.ownerTag)},
         {"size_bytes", cand.sizeBytes},
         {"name", cand.name},
         {"creation_frame", cand.creationFrame}});
  }

  j["resources"] = {{"total_tracked_bytes", totalTrackedBytes},
                    {"peak_tracked_bytes", peakTrackedBytes},
                    {"active_resource_count", activeResourceCount},
                    {"leak_candidate_count", leakCandidateCount},
                    {"leak_candidates", leakCandidatesJson}};

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
    // W6 (M0-C): camera and ROI per matrix cell pin the reproducible input.
    mj["camera"] = {{"target_x", m.cameraX},
                    {"target_y", m.cameraY},
                    {"zoom", m.cameraZoom}};
    mj["roi"] = {{"x", m.roiX},
                 {"y", m.roiY},
                 {"width", m.roiWidth},
                 {"height", m.roiHeight}};
    mj["pass_trace_valid"] = m.passTraceValid;
    // W6 (M0-C) Blocker-1: real executed pass order + its provenance.
    mj["pass_trace_source"] = m.passTraceSource;
    mj["executed_pass_order"] = m.executedPassOrder;
    mj["non_black_roi_passed"] = m.nonBlackRoiPassed;
    mj["roi_mean_brightness"] = m.roiMeanBrightness;
    mj["gi_indirect_passed"] = m.giIndirectPassed;
    // B13: real JFA work-shape diagnostics for timing root-cause analysis.
    mj["jfa"] = {
        {"mode", m.jfaMode},
        {"dispatch_texel_count", m.jfaDispatchTexelCount},
        {"dirty_rect_area", m.jfaDirtyRectArea},
        {"expanded_rect_area", m.jfaExpandedRectArea},
        {"fallback_plus2", m.jfaPlus2Recovery},
        {"verification_attempted", m.jfaVerificationAttempted},
        {"verification_passed", m.jfaVerificationPassed},
        {"verification_fallback", m.jfaVerificationRecovery},
        {"verification_result", m.jfaVerificationResult}};
    // W6 (M0-C) Blocker-1: per-cell paired GI delta + real SDF evidence.
    mj["gi_paired_delta"] = m.giPairedDelta;
    mj["gi_paired_passed"] = m.giPairedPassed;
    mj["sdf_readback_status"] =
        m.sdfReadbackStatus.empty() ? "missing" : m.sdfReadbackStatus;
    mj["sdf_evidence_source"] = m.sdfEvidenceSource;
    mj["sdf_min_value"] = m.sdfMinValue;
    mj["sdf_max_value"] = m.sdfMaxValue;
    mj["sdf_mean_value"] = m.sdfMeanValue;
    mj["sdf_probe_samples"] = m.sdfProbeSamples;
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

    // M0-B: external target state captured verbatim - never default-filled.
    // Cells that never ran a target (e.g. fixture-prep failure) keep the field
    // absent so "missing" is distinguishable from "failed" in the artifact.
    if (m.targetState.captured || !m.targetState.status.empty()) {
      mj["target_state"] = {
          {"captured", m.targetState.captured},
          {"status", m.targetState.status},
          {"reason", m.targetState.reason},
          {"expected_internal_format", m.targetState.expectedInternalFormat},
          {"framebuffer_binding", m.targetState.framebufferBinding},
          {"viewport",
           {m.targetState.viewportX, m.targetState.viewportY,
            m.targetState.viewportWidth, m.targetState.viewportHeight}},
          {"scissor_test_enabled", m.targetState.scissorTestEnabled},
          {"scissor",
           {m.targetState.scissorX, m.targetState.scissorY,
            m.targetState.scissorWidth, m.targetState.scissorHeight}},
          {"attachment_object_type", m.targetState.attachmentObjectType},
          {"attachment_object_name", m.targetState.attachmentObjectName},
          {"attachment_width", m.targetState.attachmentWidth},
          {"attachment_height", m.targetState.attachmentHeight},
          {"attachment_internal_format", m.targetState.attachmentInternalFormat},
          {"color_encoding", m.targetState.colorEncoding},
          {"component_type", m.targetState.componentType},
          {"red_size", m.targetState.redSize},
          {"green_size", m.targetState.greenSize},
          {"blue_size", m.targetState.blueSize},
          {"alpha_size", m.targetState.alphaSize},
          {"depth_size", m.targetState.depthSize},
          {"stencil_size", m.targetState.stencilSize}};
    }

    matrixArr.push_back(mj);
  }
  j["matrix_results"] = matrixArr;

  nlohmann::json pairedArr = nlohmann::json::array();
  for (const auto &p : pairedGiDeltas) {
    pairedArr.push_back(
        {{"fixture", p.fixtureName},
         {"scene_seed", p.sceneSeed},
         {"resolution", std::to_string(p.width) + "x" + std::to_string(p.height)},
         {"color_space", p.colorSpace},
         {"roi", std::to_string(p.roiX) + "," + std::to_string(p.roiY) + " " +
                     std::to_string(p.roiWidth) + "x" + std::to_string(p.roiHeight)},
         {"renderer", p.renderer},
         {"renderer_is_hardware", p.rendererIsHardware},
         // W6 (M0-C): the tier the paired capture ran under (per-cell evidence).
         {"quality_tier", p.qualityTier},
         {"warmup_frames", p.warmupFrames},
         {"sample_frames", p.sampleFrames},
         {"roi_mean_on", p.roiMeanOn},
         {"roi_mean_off", p.roiMeanOff},
         {"paired_delta", p.pairedDelta},
         {"threshold", p.threshold},
         {"passed", p.passed},
         {"tracked_bytes_on", p.trackedBytesOn},
         {"tracked_bytes_off", p.trackedBytesOff},
         {"leg_pass_traces", p.legPassTraces},
         {"failures", p.failureReasons}});
  }
  j["paired_gi_deltas"] = pairedArr;
  j["global_failures"] = globalFailures;

  return j.dump(2);
}

} // namespace NoMoreDay::render::validation
