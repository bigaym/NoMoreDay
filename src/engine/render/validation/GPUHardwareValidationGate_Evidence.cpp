#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/validation/GPUHardwareValidationGateInternal.hpp"
#include "engine/render/validation/FixtureRenderDriver.hpp"

#include "GLFW/glfw3.h"

namespace NoMoreDay::render::validation {

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

PairedGiDeltaResult GPUHardwareValidationGate::RunPairedGiDeltaCapture(
    FixtureRenderDriver &driver, const FixtureConfig &fixture,
    NoMoreDay::render::core::QualityTier qualityTier) {
  PairedGiDeltaResult result;
  result.fixtureName = fixture.name;
  result.sceneSeed = fixture.sceneSeed;
  result.width = fixture.width;
  result.height = fixture.height;
  result.colorSpace = "sRGB";
  result.qualityTier = NoMoreDay::render::core::ToString(qualityTier);
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
  tierMgr.ForceTier(qualityTier);

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

} // namespace NoMoreDay::render::validation
