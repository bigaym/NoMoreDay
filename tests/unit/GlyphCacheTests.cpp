#pragma once

#include "doctest.h"
#include "engine/render/GlyphCache.hpp"

#include <vector>

using namespace NoMoreDay::render;

namespace {

struct SyntheticGlyphSpec {
    int value = 0;
    int offsetX = 0;
    int offsetY = 0;
    int advanceX = 0;
    float rectX = 0.0f;
    float rectY = 0.0f;
    float rectW = 0.0f;
    float rectH = 0.0f;
};

// Builds a Font whose glyph/rect tables are backed by the caller-provided
// vectors so tests can mutate the font in place (simulating a font rebuild).
Font MakeSyntheticFont(std::vector<SyntheticGlyphSpec> specs,
                       std::vector<Rectangle>& recs,
                       std::vector<GlyphInfo>& glyphs,
                       uint32_t textureId) {
    Font font = {};
    font.baseSize = 32;
    font.texture.id = textureId;
    font.texture.width = 256;
    font.texture.height = 128;

    recs.resize(specs.size());
    glyphs.resize(specs.size());
    for (size_t i = 0; i < specs.size(); ++i) {
        const SyntheticGlyphSpec& s = specs[i];
        glyphs[i] = {};
        glyphs[i].value = s.value;
        glyphs[i].offsetX = s.offsetX;
        glyphs[i].offsetY = s.offsetY;
        glyphs[i].advanceX = s.advanceX;
        recs[i] = {s.rectX, s.rectY, s.rectW, s.rectH};
    }
    font.glyphCount = static_cast<int>(specs.size());
    font.recs = recs.data();
    font.glyphs = glyphs.data();
    return font;
}

} // namespace

TEST_SUITE("GlyphIndexCache") {

TEST_CASE("[Unit] GlyphIndexCache returns the first matching glyph index") {
    std::vector<Rectangle> recs;
    std::vector<GlyphInfo> glyphs;
    Font font = MakeSyntheticFont(
        {{'?', 0, 0, 8, 0.0f, 0.0f, 4.0f, 4.0f},   // index 0
         {65, 1, 2, 12, 4.0f, 5.0f, 10.0f, 12.0f}, // 'A' index 1
         {65, 0, 0, 9, 20.0f, 8.0f, 6.0f, 6.0f}},  // duplicate 'A' index 2
        recs, glyphs, 1);

    const GlyphIndexCache& cache = GlyphIndexCache::Get(font);

    CHECK(cache.GetIndex(65) == 1); // first 'A' wins, matching raylib's linear scan
    CHECK(cache.GetIndex('?') == 0);
    CHECK(cache.IsValidFor(font));
}

TEST_CASE("[Unit] GlyphIndexCache falls back to the '?' glyph on miss") {
    std::vector<Rectangle> recs;
    std::vector<GlyphInfo> glyphs;
    Font font = MakeSyntheticFont(
        {{'?', 0, 0, 8, 0.0f, 0.0f, 4.0f, 4.0f},   // index 0
         {65, 1, 2, 12, 4.0f, 5.0f, 10.0f, 12.0f}, // 'A' index 1
         {66, 0, 3, 14, 20.0f, 8.0f, 7.0f, 5.0f}}, // 'B' index 2
        recs, glyphs, 2);

    const GlyphIndexCache& cache = GlyphIndexCache::Get(font);

    CHECK(cache.GetIndex(65) == 1);
    CHECK(cache.GetIndex(66) == 2);
    CHECK(cache.GetIndex(0x4E2D) == 0); // '中' miss -> '?' index 0
    CHECK(cache.GetIndex(-1) == 0);     // invalid codepoint -> fallback
}

TEST_CASE("[Unit] GlyphIndexCache falls back to 0 when the font has no '?' glyph") {
    std::vector<Rectangle> recs;
    std::vector<GlyphInfo> glyphs;
    Font font = MakeSyntheticFont(
        {{65, 0, 0, 12, 0.0f, 0.0f, 8.0f, 8.0f}, // 'A' index 0
         {66, 0, 0, 14, 8.0f, 0.0f, 8.0f, 8.0f}}, // 'B' index 1
        recs, glyphs, 3);

    const GlyphIndexCache& cache = GlyphIndexCache::Get(font);

    CHECK(cache.GetIndex(65) == 0);
    CHECK(cache.GetIndex(88) == 0); // 'X' miss -> 0
    CHECK(cache.GetIndex(63) == 0); // '?' itself missing -> 0
}

TEST_CASE("[Unit] GlyphIndexCache rebuilds when the font glyphCount changes") {
    std::vector<Rectangle> recs;
    std::vector<GlyphInfo> glyphs;
    Font font = MakeSyntheticFont(
        {{'?', 0, 0, 8, 0.0f, 0.0f, 4.0f, 4.0f},   // index 0
         {65, 1, 2, 12, 4.0f, 5.0f, 10.0f, 12.0f}, // 'A' index 1
         {66, 0, 3, 14, 20.0f, 8.0f, 7.0f, 5.0f}}, // 'B' index 2
        recs, glyphs, 4);
    font.glyphCount = 2; // simulate a font initially built with '?' and 'A' only

    const GlyphIndexCache& cache = GlyphIndexCache::Get(font);
    CHECK(cache.GetIndex(65) == 1);
    CHECK(cache.GetIndex(66) == 0); // 'B' unknown -> fallback '?' (index 0)
    CHECK(cache.IsValidFor(font));

    // Simulate a font rebuild with the same texture id: '?' moves to index 2.
    glyphs[0].value = 65;
    glyphs[1].value = 66;
    glyphs[2].value = '?';
    font.glyphCount = 3;

    CHECK_FALSE(cache.IsValidFor(font));

    const GlyphIndexCache& rebuilt = GlyphIndexCache::Get(font);
    CHECK(rebuilt.GetIndex(65) == 0);
    CHECK(rebuilt.GetIndex(66) == 1);
    CHECK(rebuilt.GetIndex('?') == 2);
    CHECK(rebuilt.GetIndex(0x4E2D) == 2); // miss -> new fallback index
    CHECK(rebuilt.IsValidFor(font));
}

} // TEST_SUITE("GlyphIndexCache")
