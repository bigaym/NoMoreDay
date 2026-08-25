#pragma once

#include "raylib.h"
#include "raymath.h"

namespace NoMoreDay::render::coord {

// Canonical coordinate spaces. Single source of truth; see
// docs/designs/2026-08-24-coordinate-system-convention-design.md.
enum class Space {
  World,      // game world, y-down, camera units (Camera2D target/offset/zoom)
  ScenePixel, // scene render-target pixels, y-down (raylib convention)
  UiLogical,  // UI logical reference pixels (default 2560x1440), y-down
  UiNative,   // window/native pixels, y-down
  Ndc,        // OpenGL clip/NDC, y-up (internal; not encoded directly)
  FboTexel,   // framebuffer/texture coordinates
  MsdfMetric  // FreeType/MSDF metrics, baseline-origin
};

// Minimal POD view of raylib Camera2D so callers outside a live raylib
// render context use the same math without holding a Camera2D object.
struct Camera2DTransform {
  Vector2 target{};
  Vector2 offset{};
  float zoom = 1.0f;

  static Camera2DTransform From(const Camera2D &cam) noexcept {
    return Camera2DTransform{cam.target, cam.offset, cam.zoom};
  }

  Camera2D ToRaylib() const noexcept {
    Camera2D cam{};
    cam.target = target;
    cam.offset = offset;
    cam.rotation = 0.0f;
    cam.zoom = zoom;
    return cam;
  }
};

// R1: the only world -> scene-pixel conversion in the codebase.
inline Vector2 WorldToScenePixel(const Camera2DTransform &cam,
                                 const Vector2 &world) noexcept {
  return {(world.x - cam.target.x) * cam.zoom + cam.offset.x,
          (world.y - cam.target.y) * cam.zoom + cam.offset.y};
}

// R1: the only scene-pixel -> world conversion in the codebase.
inline Vector2 ScenePixelToWorld(const Camera2DTransform &cam,
                                 const Vector2 &pixel) noexcept {
  const float invZoom = cam.zoom > 1e-6f ? 1.0f / cam.zoom : 1.0f;
  return {(pixel.x - cam.offset.x) * invZoom + cam.target.x,
          (pixel.y - cam.offset.y) * invZoom + cam.target.y};
}

// R2: the single Y-down orthographic MVP builder for all custom GPU passes.
// Matches the legacy GPUParticleSystem::BuildMVP construction bit-for-bit.
inline Matrix Build2DMvp(const Camera2DTransform &cam,
                         float framebufferWidth,
                         float framebufferHeight) noexcept {
  const Matrix view = GetCameraMatrix2D(cam.ToRaylib());
  const Matrix proj = MatrixOrtho(0.0f, framebufferWidth, framebufferHeight,
                                  0.0f, -1.0f, 1.0f);
  return MatrixMultiply(view, proj);
}

// R3: the only native(y-down) -> GL fragment(y-up) Y flip. Never write
// `height - y` anywhere else.
inline float NativeYToGl(float nativeY, float height) noexcept {
  return height - nativeY;
}

// R5: MSDF/FreeType metrics are stored in em units and conventioned by
// export_msdf_metrics.py (planeBounds values). This is the single helper
// used to convert a metric value into the world-space offset consumed by
// text layout. It intentionally preserves the exporter's current numeric
// semantics while making the boundary explicit (and is unit-tested).
inline float MsdfBearingToWorldOffset(float bearingMetric, float emSize,
                                      float fontSize) noexcept {
  return bearingMetric * (fontSize / emSize);
}

// Texture V-origin contract (R3): raylib textures are sampled with V=0 at
// the image top; shaders/atlases should declare their native origin and
// use this helper when remapping is required.
enum class TextureVOrigin { Top, Bottom };

inline float RemapTextureV(float v, TextureVOrigin fromOrigin,
                           TextureVOrigin toOrigin) noexcept {
  if (fromOrigin == toOrigin) return v;
  return 1.0f - v;
}
} // namespace NoMoreDay::render::coord
