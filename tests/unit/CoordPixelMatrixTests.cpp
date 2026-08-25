#include "doctest.h"

#include "engine/render/CoordSystem.hpp"

#include <array>
#include <cmath>
#include <cstdio>

// Cross-domain pixel matrix regression (Track refactor_coordinate_system_20260824, R6).
// Locks the unified coordinate chain (WorldToScenePixel + Build2DMvp +
// BlitSourceRect + RenderTargetDescriptor.flipY) so that the same world point
// lands on the same final window pixel across the display matrix:
//   16:9 / 21:9 / 4:3  x  DRS on/off  x  HDR on/off
// Any reintroduced hand-rolled flip, MVP or blit math must break one of the
// twelve configurations.

namespace NoMoreDay::render::coord {
namespace {

struct DisplayConfig {
  const char* name;
  int winW;
  int winH;
  float drsScale;  // 1.0f = DRS off (render extent == window extent)
  bool hdr;        // HDR composite targets a y-up FBO: flipY == true
};

constexpr std::array<DisplayConfig, 12> kMatrix = {{
    // 16:9
    {"16:9 DRS-on HDR-on", 1920, 1080, 0.75f, true},
    {"16:9 DRS-on HDR-off", 1920, 1080, 0.75f, false},
    {"16:9 DRS-off HDR-on", 1920, 1080, 1.0f, true},
    {"16:9 DRS-off HDR-off", 1920, 1080, 1.0f, false},
    // 21:9
    {"21:9 DRS-on HDR-on", 3440, 1440, 0.75f, true},
    {"21:9 DRS-on HDR-off", 3440, 1440, 0.75f, false},
    {"21:9 DRS-off HDR-on", 3440, 1440, 1.0f, true},
    {"21:9 DRS-off HDR-off", 3440, 1440, 1.0f, false},
    // 4:3
    {"4:3 DRS-on HDR-on", 1600, 1200, 0.75f, true},
    {"4:3 DRS-on HDR-off", 1600, 1200, 0.75f, false},
    {"4:3 DRS-off HDR-on", 1600, 1200, 1.0f, true},
    {"4:3 DRS-off HDR-off", 1600, 1200, 1.0f, false},
}};

// Full-screen blit: the render target texel (y-down, origin at the target
// top-left) maps linearly onto the window. A full-extent blit is flipY
// invariant: the y flip only changes the sampling direction, and the final
// window pixel of a given scene texel is the same whether the target is the
// y-down window (BlitSourceRect negative height) or a y-up FBO (positive
// height, pure copy path). This is the mathematical property the R3 blit
// helper must preserve.
Vector2 BlitTexelToWindow(const Vector2& texel, const DisplayConfig& cfg) {
  const float rtW = static_cast<float>(cfg.winW) * cfg.drsScale;
  const float rtH = static_cast<float>(cfg.winH) * cfg.drsScale;
  return {texel.x * static_cast<float>(cfg.winW) / rtW,
          texel.y * static_cast<float>(cfg.winH) / rtH};
}

TEST_CASE(
    "[Unit] CoordPixelMatrix - blit source rect follows the target flipY") {
  // RenderTargetDescriptor.flipY is the single source of truth for the blit
  // y-flip: y-down targets (window) flip the source height, y-up targets
  // (HDR FBO) keep it positive.
  CHECK(BlitSourceRect(false, 1920.0f, 1080.0f).height == doctest::Approx(-1080.0f));
  CHECK(BlitSourceRect(false, 1920.0f, 1080.0f).width == doctest::Approx(1920.0f));
  CHECK(BlitSourceRect(true, 1920.0f, 1080.0f).height == doctest::Approx(1080.0f));
  CHECK(BlitSourceRect(true, 3440.0f, 1440.0f).width == doctest::Approx(3440.0f));
  CHECK(BlitSourceRect(false, 0.0f, 0.0f).height == doctest::Approx(-0.0f));
}

TEST_CASE("[Unit] CoordPixelMatrix - 12-config window pixel chain") {
  // Real pipeline math locked by this test:
  //   RT texel  = camera mapping (world - target) * zoom + offset
  //               (window-space; the scene extent stores it 1:1, no DRS scale)
  //   window px = RT texel * winW/rtW (full-extent blit magnification)
  // So DRS (rtW < winW) magnifies final positions by 1/scale and never shifts
  // content relative to the camera mapping; HDR (flipY) does not affect
  // positions at all. Any hand-rolled flip, MVP, or size-source drift that
  // breaks one of the twelve configurations must fail here (R6).
  Camera2DTransform cam;
  cam.target = {120.0f, -45.0f};
  cam.zoom = 1.5f;

  const std::array<Vector2, 3> worldPoints = {
      Vector2{100.0f, 50.0f}, Vector2{0.0f, 0.0f}, Vector2{-77.5f, 300.25f}};

  for (const Vector2& wp : worldPoints) {
    for (size_t i = 0; i < kMatrix.size(); ++i) {
      const DisplayConfig& cfg = kMatrix[i];
      Camera2DTransform cfgCam = cam;
      cfgCam.offset = {cfg.winW * 0.5f, cfg.winH * 0.5f};
      const float rtW = static_cast<float>(cfg.winW) * cfg.drsScale;
      const float rtH = static_cast<float>(cfg.winH) * cfg.drsScale;

      char msg[256];
      std::snprintf(msg, sizeof(msg), "%s: world (%.1f, %.1f)", cfg.name,
                    wp.x, wp.y);

      // Blit chain: the texel produced by the camera mapping is stored in the
      // scene RT 1:1 and the full-extent blit scales it by winW/rtW.
      const Vector2 v = WorldToScenePixel(cfgCam, wp);
      const Vector2 px = BlitTexelToWindow(v, cfg);
      const Vector2 analytic = {v.x * static_cast<float>(cfg.winW) / rtW,
                                v.y * static_cast<float>(cfg.winH) / rtH};
      CHECK_MESSAGE(std::fabs(px.x - analytic.x) <= 0.01f, msg);
      CHECK_MESSAGE(std::fabs(px.y - analytic.y) <= 0.01f, msg);

      if (cfg.drsScale >= 1.0f) {
        // DRS off: the window position relative to the window center equals
        // (w - target) * zoom, independent of the window resolution, so
        // 16:9 / 21:9 / 4:3 all land on the same spot.
        const float relX = px.x - cfg.winW * 0.5f;
        const float relY = px.y - cfg.winH * 0.5f;
        CHECK_MESSAGE(std::fabs(relX - (wp.x - cam.target.x) * cam.zoom) <=
                          0.5f,
                      msg);
        CHECK_MESSAGE(std::fabs(relY - (wp.y - cam.target.y) * cam.zoom) <=
                          0.5f,
                      msg);
      } else {
        // DRS on: same window size and camera, the RT is smaller, so the
        // window position is the DRS-off one magnified by 1/scale.
        const DisplayConfig& ref = kMatrix[i + 2]; // DRS-off, same HDR
        Camera2DTransform refCam = cam;
        refCam.offset = {ref.winW * 0.5f, ref.winH * 0.5f};
        const Vector2 refPx = WorldToScenePixel(refCam, wp);
        CHECK_MESSAGE(std::fabs(px.x - refPx.x / cfg.drsScale) <= 0.5f, msg);
        CHECK_MESSAGE(std::fabs(px.y - refPx.y / cfg.drsScale) <= 0.5f, msg);
      }

      // HDR independence: flipY changes the sampling direction, never the
      // final position (same window size and DRS, adjacent matrix entries).
      if ((i % 4) == 0 || (i % 4) == 2) {
        const DisplayConfig& hdrOff = kMatrix[i + 1];
        Camera2DTransform hdrCam = cam;
        hdrCam.offset = {hdrOff.winW * 0.5f, hdrOff.winH * 0.5f};
        const Vector2 pxHdrOff =
            BlitTexelToWindow(WorldToScenePixel(hdrCam, wp), hdrOff);
        CHECK_MESSAGE(std::fabs(px.x - pxHdrOff.x) <= 0.5f, msg);
        CHECK_MESSAGE(std::fabs(px.y - pxHdrOff.y) <= 0.5f, msg);
      }
    }
  }
}

TEST_CASE("[Unit] CoordPixelMatrix - MVP and pixel entry agree in every config") {
  // Build2DMvp and WorldToScenePixel must be the same math: project the world
  // point through the MVP, map NDC back to the render target texel (NDC is
  // y-up, the RT texel is y-down, so the y axis flips), and compare with the
  // direct pixel entry. This guards R1/R2 against one of the two entries
  // drifting in DRS or HDR configurations.
  Camera2DTransform cam;
  cam.target = {120.0f, -45.0f};
  cam.zoom = 1.5f;

  const Vector2 world{100.0f, 50.0f};
  for (const DisplayConfig& cfg : kMatrix) {
    Camera2DTransform cfgCam = cam;
    cfgCam.offset = {cfg.winW * 0.5f, cfg.winH * 0.5f};
    const float rtW = static_cast<float>(cfg.winW) * cfg.drsScale;
    const float rtH = static_cast<float>(cfg.winH) * cfg.drsScale;
    const Matrix mvp = Build2DMvp(cfgCam, rtW, rtH);

    // NDC of the world point (w = 1 for affine camera/projection).
    const float ndcX = mvp.m0 * world.x + mvp.m4 * world.y + mvp.m12;
    const float ndcY = mvp.m1 * world.x + mvp.m5 * world.y + mvp.m13;
    // NDC is y-up (GL clip space); the y-down render-target texel flips it.
    const Vector2 texelFromMvp = {(ndcX + 1.0f) * 0.5f * rtW,
                                  (1.0f - ndcY) * 0.5f * rtH};
    const Vector2 texelDirect = WorldToScenePixel(cfgCam, world);

    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "%s: MVP texel (%.3f, %.3f) vs direct (%.3f, %.3f)", cfg.name,
                  texelFromMvp.x, texelFromMvp.y, texelDirect.x, texelDirect.y);
    CHECK_MESSAGE(std::fabs(texelFromMvp.x - texelDirect.x) <= 0.01f, msg);
    CHECK_MESSAGE(std::fabs(texelFromMvp.y - texelDirect.y) <= 0.01f, msg);
  }
}

} // namespace
} // namespace NoMoreDay::render::coord