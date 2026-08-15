#pragma once

#include "engine/render/resource/MSDFAtlasLoader.hpp"
#include "raylib.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace NoMoreDay::render {

// CPU-side registry for the currently active MSDF atlas glyph metrics.
//
// Ownership model:
//  - The GPU texture stays owned by GPUTextSystem (sole owner). This registry
//    only stores a handle copy via Register() and never releases the texture.
//  - Register() copies the caller's metrics vector, so the caller may destroy
//    its source data immediately afterwards (no dangling references).
class MSDFAtlasRegistry {
public:
  // Em size (px/em) of the v4 MSDF atlas. Written by msdf-atlas-gen into
  // assets/textures/fonts/msdf/v4_msdf_gb2312_4096.json ("atlas.size";
  // auto-fit result of `-minsize 18 -pxrange 6`, font simsun.ttc). The
  // metrics.bin header does not carry this field, so the value is pinned here
  // as the single source of truth shared by Game.cpp and unit tests.
  static constexpr float kV4AtlasEmSize = 29.078125f;

  static MSDFAtlasRegistry &Get() {
    static MSDFAtlasRegistry instance;
    return instance;
  }

  // Replaces any previous registration. Copies glyph metrics; does not take
  // ownership of the caller's vector or of the texture resource.
  // Returns true on success (the registry is available afterwards).
  bool Register(Texture2D texture, const std::vector<MSDFGlyphMetric> &glyphs,
                float distanceRange, float emSize);

  void Clear() noexcept;

  [[nodiscard]] bool IsAvailable() const noexcept { return m_available; }
  [[nodiscard]] const MSDFGlyphMetric *Find(uint32_t codepoint) const noexcept;
  [[nodiscard]] Texture2D GetTexture() const noexcept { return m_texture; }
  [[nodiscard]] float GetDistanceRange() const noexcept { return m_distanceRange; }
  [[nodiscard]] float GetEmSize() const noexcept { return m_emSize; }

private:
  MSDFAtlasRegistry() = default;

  bool m_available = false;
  Texture2D m_texture = {};
  float m_distanceRange = 0.0f;
  float m_emSize = 0.0f;
  std::vector<MSDFGlyphMetric> m_glyphs;
  std::unordered_map<uint32_t, size_t> m_index;
};

} // namespace NoMoreDay::render
