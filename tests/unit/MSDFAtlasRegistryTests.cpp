#include "doctest.h"

#include "engine/render/resource/MSDFAtlasRegistry.hpp"

#include <cstdint>
#include <vector>

namespace {

using NoMoreDay::render::MSDFAtlasRegistry;
using NoMoreDay::render::MSDFGlyphMetric;

constexpr float kDistanceRange = 6.0f;

std::vector<MSDFGlyphMetric> BuildDigitGlyphs() {
  std::vector<MSDFGlyphMetric> glyphs;
  glyphs.reserve(10);
  for (uint32_t cp = '0'; cp <= '9'; ++cp) {
    MSDFGlyphMetric m = {};
    m.codepoint = cp;
    m.uvRect[0] = 0.0f;
    m.uvRect[1] = 0.0f;
    m.uvRect[2] = 0.02f;
    m.uvRect[3] = 0.02f;
    m.bearing[0] = -0.06f;
    m.bearing[1] = -0.12f;
    m.size[0] = 0.62f;
    m.size[1] = 0.93f;
    // ASCII digits are half-em wide (matches real v4 atlas: advance('0')=0.5).
    m.advance = 0.5f;
    glyphs.push_back(m);
  }
  return glyphs;
}

Texture2D MakeFakeTexture(const unsigned int id) {
  Texture2D tex = {};
  tex.id = id;
  tex.width = 4096;
  tex.height = 2048;
  tex.mipmaps = 1;
  tex.format = 7; // PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
  return tex;
}

} // namespace

TEST_CASE("[Unit] MSDFAtlasRegistry - Find returns registered glyph data") {
  MSDFAtlasRegistry::Get().Clear();

  const auto glyphs = BuildDigitGlyphs();
  const Texture2D tex = MakeFakeTexture(123u);
  MSDFAtlasRegistry::Get().Register(tex, glyphs, kDistanceRange,
                                    MSDFAtlasRegistry::kV4AtlasEmSize);

  REQUIRE(MSDFAtlasRegistry::Get().IsAvailable());
  CHECK(MSDFAtlasRegistry::Get().GetTexture().id == 123u);
  CHECK(MSDFAtlasRegistry::Get().GetDistanceRange() == kDistanceRange);
  CHECK(MSDFAtlasRegistry::Get().GetEmSize() ==
        MSDFAtlasRegistry::kV4AtlasEmSize);

  const MSDFGlyphMetric *zero = MSDFAtlasRegistry::Get().Find('0');
  REQUIRE(zero != nullptr);
  CHECK(zero->codepoint == '0');
  CHECK(zero->size[0] == 0.62f);
  CHECK(zero->advance == 0.5f);

  const MSDFGlyphMetric *nine = MSDFAtlasRegistry::Get().Find('9');
  REQUIRE(nine != nullptr);
  CHECK(nine->codepoint == '9');

  MSDFAtlasRegistry::Get().Clear();
}

TEST_CASE("[Unit] MSDFAtlasRegistry - Find misses unknown codepoints") {
  MSDFAtlasRegistry::Get().Clear();

  // No registration at all -> no lookup can hit.
  CHECK_FALSE(MSDFAtlasRegistry::Get().IsAvailable());
  CHECK(MSDFAtlasRegistry::Get().Find('0') == nullptr);

  const auto glyphs = BuildDigitGlyphs();
  MSDFAtlasRegistry::Get().Register(MakeFakeTexture(1u), glyphs,
                                    kDistanceRange, 29.078125f);

  // Digit present, out-of-range codepoint absent.
  REQUIRE(MSDFAtlasRegistry::Get().Find('0') != nullptr);
  CHECK(MSDFAtlasRegistry::Get().Find(0xFFFFu) == nullptr);
  CHECK(MSDFAtlasRegistry::Get().Find(25105u) == nullptr); // '中' not registered

  MSDFAtlasRegistry::Get().Clear();
}

TEST_CASE("[Unit] MSDFAtlasRegistry - Clear resets all state") {
  MSDFAtlasRegistry::Get().Clear();

  const auto glyphs = BuildDigitGlyphs();
  MSDFAtlasRegistry::Get().Register(MakeFakeTexture(7u), glyphs,
                                    kDistanceRange, 29.078125f);
  REQUIRE(MSDFAtlasRegistry::Get().IsAvailable());

  MSDFAtlasRegistry::Get().Clear();

  CHECK_FALSE(MSDFAtlasRegistry::Get().IsAvailable());
  CHECK(MSDFAtlasRegistry::Get().Find('0') == nullptr);
  CHECK(MSDFAtlasRegistry::Get().GetTexture().id == 0u);
  CHECK(MSDFAtlasRegistry::Get().GetDistanceRange() == 0.0f);
  CHECK(MSDFAtlasRegistry::Get().GetEmSize() == 0.0f);
}

TEST_CASE("[Unit] MSDFAtlasRegistry - re-Register replaces previous data") {
  MSDFAtlasRegistry::Get().Clear();

  const auto first = BuildDigitGlyphs();
  MSDFAtlasRegistry::Get().Register(MakeFakeTexture(11u), first,
                                    kDistanceRange, 29.078125f);
  REQUIRE(MSDFAtlasRegistry::Get().Find('0') != nullptr);

  // Second registration carries a different atlas (single CJK glyph).
  std::vector<MSDFGlyphMetric> second;
  MSDFGlyphMetric cjk = {};
  cjk.codepoint = 25105u; // '中'
  cjk.advance = 1.0f;
  second.push_back(cjk);

  MSDFAtlasRegistry::Get().Register(MakeFakeTexture(22u), second, 6.0f,
                                    MSDFAtlasRegistry::kV4AtlasEmSize);

  // Old glyphs are gone, new glyph is found, handle/range refreshed.
  CHECK(MSDFAtlasRegistry::Get().Find('0') == nullptr);
  REQUIRE(MSDFAtlasRegistry::Get().Find(25105u) != nullptr);
  CHECK(MSDFAtlasRegistry::Get().Find(25105u)->advance == 1.0f);
  CHECK(MSDFAtlasRegistry::Get().GetTexture().id == 22u);

  MSDFAtlasRegistry::Get().Clear();
}

TEST_CASE("[Unit] MSDFAtlasRegistry - owns copied glyphs after source dies") {
  MSDFAtlasRegistry::Get().Clear();

  {
    auto local = BuildDigitGlyphs();
    MSDFAtlasRegistry::Get().Register(MakeFakeTexture(5u), local,
                                      kDistanceRange, 29.078125f);
    local.clear(); // Source vector destroyed; registry must stay valid.
  }

  const MSDFGlyphMetric *zero = MSDFAtlasRegistry::Get().Find('0');
  REQUIRE(zero != nullptr);
  CHECK(zero->codepoint == '0');
  CHECK(zero->advance == 0.5f);

  MSDFAtlasRegistry::Get().Clear();
}

TEST_CASE("[Unit] MSDFAtlasRegistry - ASCII digit advance is half an em") {
  // Real v4 atlas evidence (metrics.bin): advance('0') = 0.5000 (em units),
  // CJK full-width glyphs advance 1.0. Locks the em-unit contract used by the
  // label system and the pixel conversion (0.5em * emSize = 14.539px at the
  // pinned emSize 29.078125 px/em, tolerance 5%).
  const auto glyphs = BuildDigitGlyphs();
  MSDFAtlasRegistry::Get().Register(MakeFakeTexture(9u), glyphs,
                                    kDistanceRange,
                                    MSDFAtlasRegistry::kV4AtlasEmSize);

  const MSDFGlyphMetric *zero = MSDFAtlasRegistry::Get().Find('0');
  REQUIRE(zero != nullptr);
  CHECK(zero->advance == doctest::Approx(0.5f).epsilon(0.05f));

  const float advancePx = zero->advance * MSDFAtlasRegistry::kV4AtlasEmSize;
  CHECK(advancePx == doctest::Approx(14.5390625f).epsilon(0.001f));

  MSDFAtlasRegistry::Get().Clear();
}
