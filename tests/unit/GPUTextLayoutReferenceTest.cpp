#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/GPUTextSystem.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

using NoMoreDay::components::GPUGlyphMetrics;
using NoMoreDay::components::GPUTextCommand;
using NoMoreDay::components::GPUTextQuad;
using NoMoreDay::render::GPUTextStringMeta;

void ApplyAnimation(const uint32_t style, const float t, float &posY, float &scale,
                    float &opacity) {
  if (style == 0u) {
    posY -= 42.0f * t;
  } else if (style == 2u) {
    const float fadeStart = 0.35f;
    const float ft = std::clamp((t - fadeStart) / (1.0f - fadeStart), 0.0f, 1.0f);
    opacity *= (1.0f - ft * ft * (3.0f - 2.0f * ft));
  } else if (style == 4u) {
    scale *= (1.0f + 0.5f * (1.0f - t));
  }
}

std::vector<GPUTextQuad> BuildLayoutReference(
    const std::vector<GPUTextCommand> &commands,
    const std::vector<GPUGlyphMetrics> &metrics,
    const std::vector<uint32_t> &glyphIndices,
    const std::vector<GPUTextStringMeta> &meta, const uint32_t maxQuadCount,
    const float t) {
  std::vector<GPUTextQuad> out;
  if (metrics.empty() || meta.empty()) {
    return out;
  }
  out.reserve(maxQuadCount);

  for (const auto &cmd : commands) {
    if (cmd.stringId >= meta.size()) {
      continue;
    }
    const auto &m = meta[cmd.stringId];
    const uint32_t glyphCount = std::min<uint32_t>(m.glyphCount, 64u);
    uint32_t style = (cmd.colorAndFlags >> 24u) & 0xFFu;
    if (style == 0u) {
      style = m.animStyle;
    }

    for (uint32_t i = 0; i < glyphCount && out.size() < maxQuadCount; ++i) {
      const uint32_t indexPos = m.glyphOffset + i;
      uint32_t metricIndex = 0u;
      if (indexPos < glyphIndices.size()) {
        metricIndex = glyphIndices[indexPos] % static_cast<uint32_t>(metrics.size());
      }
      const auto &gm = metrics[metricIndex];

      float posY = cmd.worldPosY + gm.offsetY;
      float scale = 1.0f;
      float opacity = 1.0f;
      ApplyAnimation(style, t, posY, scale, opacity);

      GPUTextQuad q = {};
      q.screenPosX = cmd.worldPosX + gm.offsetX + gm.advance * static_cast<float>(i);
      q.screenPosY = posY;
      q.sizeX = gm.sizeX * scale;
      q.sizeY = gm.sizeY * scale;
      q.uvMinX = gm.uvMinX;
      q.uvMinY = gm.uvMinY;
      q.uvMaxX = gm.uvMaxX;
      q.uvMaxY = gm.uvMaxY;
      q.colorPacked = (cmd.colorAndFlags & 0x00FFFFFFu) | 0xFF000000u;
      q.opacity = opacity;
      out.push_back(q);
    }
  }
  return out;
}

} // namespace

TEST_CASE("[Unit] GPU Text Layout Ref - mixed ASCII/CJK spacing") {
  GPUGlyphMetrics ascii = {};
  ascii.advance = 10.0f;
  ascii.offsetX = 1.0f;
  ascii.offsetY = 2.0f;
  ascii.sizeX = 8.0f;
  ascii.sizeY = 12.0f;

  GPUGlyphMetrics cjk = {};
  cjk.advance = 20.0f;
  cjk.offsetX = 2.0f;
  cjk.offsetY = 3.0f;
  cjk.sizeX = 16.0f;
  cjk.sizeY = 16.0f;

  const std::vector<GPUGlyphMetrics> metrics = {ascii, cjk};
  const std::vector<uint32_t> glyphIndices = {0u, 1u, 0u}; // A 中 B
  const std::vector<GPUTextStringMeta> meta = {{0u, 3u, 0u, 0u}};
  const std::vector<GPUTextCommand> commands = {{100.0f, 50.0f, 0u, 0x000000FFu}};

  const auto quads =
      BuildLayoutReference(commands, metrics, glyphIndices, meta, 16u, 0.25f);
  REQUIRE(quads.size() == 3u);
  CHECK(quads[0].screenPosX == doctest::Approx(101.0f));
  CHECK(quads[1].screenPosX == doctest::Approx(122.0f));
  CHECK(quads[2].screenPosX == doctest::Approx(121.0f));
}

TEST_CASE("[Unit] GPU Text Layout Ref - multi-line commands keep vertical separation") {
  GPUGlyphMetrics gm = {};
  gm.advance = 12.0f;
  gm.offsetX = 0.0f;
  gm.offsetY = 1.0f;
  gm.sizeX = 9.0f;
  gm.sizeY = 14.0f;

  const std::vector<GPUGlyphMetrics> metrics = {gm};
  const std::vector<uint32_t> glyphIndices = {0u, 0u};
  const std::vector<GPUTextStringMeta> meta = {{0u, 2u, 0u, 0u}};
  const std::vector<GPUTextCommand> commands = {
      {10.0f, 20.0f, 0u, 0x0000FFFFu},
      {10.0f, 40.0f, 0u, 0x0000FFFFu},
  };

  const auto quads =
      BuildLayoutReference(commands, metrics, glyphIndices, meta, 16u, 0.0f);
  REQUIRE(quads.size() == 4u);
  CHECK(quads[2].screenPosY - quads[0].screenPosY == doctest::Approx(20.0f));
}

TEST_CASE("[Unit] GPU Text Layout Ref - style override and 64 glyph clamp") {
  GPUGlyphMetrics gm = {};
  gm.advance = 6.0f;
  gm.sizeX = 6.0f;
  gm.sizeY = 10.0f;

  std::vector<uint32_t> glyphIndices(80u, 0u);
  const std::vector<GPUGlyphMetrics> metrics = {gm};
  const std::vector<GPUTextStringMeta> meta = {{0u, 80u, 0u, 0u}};
  const std::vector<GPUTextCommand> commands = {
      {0.0f, 0.0f, 0u, (2u << 24u) | 0x0000AA44u}};

  const auto quads =
      BuildLayoutReference(commands, metrics, glyphIndices, meta, 256u, 0.8f);
  REQUIRE(quads.size() == 64u);
  CHECK((quads[0].colorPacked & 0xFF000000u) == 0xFF000000u);
  CHECK(quads[0].opacity < 1.0f);
}
