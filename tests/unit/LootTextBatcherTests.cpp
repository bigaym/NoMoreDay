#pragma once

#include "doctest.h"
#include "engine/render/LootTextBatcher.hpp"

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

// Builds a Font whose glyph/rect tables are backed by the caller-provided vectors.
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

// Layout font: '?'(0), 'A'(1), '文' U+6587(2), 'B'(3, advanceX=0 -> width fallback), ' '(4).
// All values are exactly representable in float so byte-for-byte equality holds.
Font MakeLabelFont(std::vector<Rectangle>& recs, std::vector<GlyphInfo>& glyphs) {
    return MakeSyntheticFont(
        {{'?', 0, 0, 8, 0.0f, 0.0f, 4.0f, 4.0f},      // index 0
         {65, 1, 2, 12, 4.0f, 5.0f, 10.0f, 12.0f},    // 'A' index 1
         {0x6587, 2, 3, 16, 16.0f, 8.0f, 12.0f, 10.0f},// '文' index 2
         {66, 0, 3, 0, 30.0f, 8.0f, 14.0f, 10.0f},    // 'B' index 3 (advanceX 0)
         {' ', 0, 0, 6, 48.0f, 0.0f, 1.0f, 1.0f}},    // space index 4
        recs, glyphs, 100);
}

bool GlyphInstancesEqual(const NoMoreDay::components::GPUGlyphInstance& a,
                         const NoMoreDay::components::GPUGlyphInstance& b) {
    return a.position.x == b.position.x && a.position.y == b.position.y &&
           a.size.x == b.size.x && a.size.y == b.size.y &&
           a.uvMin.x == b.uvMin.x && a.uvMin.y == b.uvMin.y &&
           a.uvMax.x == b.uvMax.x && a.uvMax.y == b.uvMax.y &&
           a.colorPacked == b.colorPacked && a.scale == b.scale &&
           a.padding[0] == b.padding[0] && a.padding[1] == b.padding[1];
}

} // namespace

TEST_SUITE("LootTextBatcher") {

TEST_CASE("[Unit] LootTextBatcher BuildTemplates lays out glyphs relative to the text origin") {
    std::vector<Rectangle> recs;
    std::vector<GlyphInfo> glyphs;
    Font font = MakeLabelFont(recs, glyphs);

    const std::string text = "A B"; // 'A', space, 'B' -> 2 templates (space skipped)
    const float fontSize = 16.0f;
    const float scale = fontSize / (float)font.baseSize; // 0.5
    const float spacing = 1.0f;

    std::vector<NoMoreDay::components::GlyphTemplate> templates;
    LootTextBatcher::BuildTemplates(font, text, fontSize, templates);

    REQUIRE(templates.size() == 2);

    // 'A' (index 1): offsetX=1, offsetY=2, advanceX=12, rect {4,5,10,12}
    const NoMoreDay::components::GlyphTemplate& a = templates[0];
    CHECK(a.size.x == doctest::Approx(10.0f * scale));
    CHECK(a.size.y == doctest::Approx(12.0f * scale));
    CHECK(a.uvMin.x == doctest::Approx(4.0f / 256.0f));
    CHECK(a.uvMin.y == doctest::Approx(5.0f / 128.0f));
    CHECK(a.uvMax.x == doctest::Approx(14.0f / 256.0f));
    CHECK(a.uvMax.y == doctest::Approx(17.0f / 128.0f));
    CHECK(a.offset.x == doctest::Approx(1.0f * scale)); // text origin + glyph offset
    CHECK(a.offset.y == doctest::Approx(2.0f * scale));
    CHECK(a.advanceX == doctest::Approx(12.0f * scale + spacing));

    // Space advances the cursor (6*scale + spacing) but emits no template.
    // 'B' (index 3): offsetX=0, offsetY=3, advanceX=0 -> width fallback, rect {30,8,14,10}
    const float cursorAfterA = 12.0f * scale + spacing;        // 7.0
    const float cursorAfterSpace = cursorAfterA + 6.0f * scale + spacing; // 11.0
    const NoMoreDay::components::GlyphTemplate& b = templates[1];
    CHECK(b.size.x == doctest::Approx(14.0f * scale));
    CHECK(b.size.y == doctest::Approx(10.0f * scale));
    CHECK(b.uvMin.x == doctest::Approx(30.0f / 256.0f));
    CHECK(b.uvMin.y == doctest::Approx(8.0f / 128.0f));
    CHECK(b.uvMax.x == doctest::Approx(44.0f / 256.0f));
    CHECK(b.uvMax.y == doctest::Approx(18.0f / 128.0f));
    CHECK(b.offset.x == doctest::Approx(cursorAfterSpace));
    CHECK(b.offset.y == doctest::Approx(3.0f * scale));
    CHECK(b.advanceX == doctest::Approx(14.0f * scale + spacing)); // width fallback step
}

TEST_CASE("[Unit] LootTextBatcher WriteInstances translates, tints and guards mismatches") {
    std::vector<NoMoreDay::components::GlyphTemplate> templates(2);
    std::vector<NoMoreDay::components::GPUGlyphInstance> relative(2);
    relative[0].position = {1.0f, 2.0f};
    relative[0].colorPacked = 0xFFFFFFFFu;
    relative[1].position = {5.0f, 8.0f};
    relative[1].colorPacked = 0xFFFFFFFFu;

    std::vector<NoMoreDay::components::GPUGlyphInstance> out;
    LootTextBatcher::WriteInstances(templates, relative, {10.0f, 20.0f}, 0x11223344u, out);

    REQUIRE(out.size() == 2);
    CHECK(out[0].position.x == doctest::Approx(11.0f));
    CHECK(out[0].position.y == doctest::Approx(22.0f));
    CHECK(out[1].position.x == doctest::Approx(15.0f));
    CHECK(out[1].position.y == doctest::Approx(28.0f));
    CHECK(out[0].colorPacked == 0x11223344u);
    CHECK(out[1].colorPacked == 0x11223344u);
    CHECK(out[0].size.x == relative[0].size.x); // non-position fields preserved
    CHECK(out[1].uvMin.x == relative[1].uvMin.x);

    // Size mismatch between templates and instances -> nothing emitted.
    std::vector<NoMoreDay::components::GPUGlyphInstance> guarded;
    std::vector<NoMoreDay::components::GlyphTemplate> mismatched(3);
    LootTextBatcher::WriteInstances(mismatched, relative, {0.0f, 0.0f}, 0xFFFFFFFFu, guarded);
    CHECK(guarded.empty());
}

TEST_CASE("[Unit] LootTextBatcher BuildTemplates+WriteInstances matches BatchString byte-for-byte") {
    std::vector<Rectangle> recs;
    std::vector<GlyphInfo> glyphs;
    Font font = MakeLabelFont(recs, glyphs);

    const std::string text = "A文 B"; // UTF-8 decode + space skip + width-fallback 'B'
    const float fontSize = 16.0f;
    const Vector2 origin = {100.0f, 200.0f};
    const Color color = {10, 20, 30, 255};
    const uint32_t packed = ColorToInt(color);

    std::vector<NoMoreDay::components::GPUGlyphInstance> direct;
    LootTextBatcher::BatchString(font, text, origin, fontSize, color, direct);

    std::vector<NoMoreDay::components::GlyphTemplate> templates;
    LootTextBatcher::BuildTemplates(font, text, fontSize, templates);

    // Reconstruct origin-relative (0,0) instances purely from templates.
    std::vector<NoMoreDay::components::GPUGlyphInstance> relative;
    relative.reserve(templates.size());
    for (const NoMoreDay::components::GlyphTemplate& t : templates) {
        NoMoreDay::components::GPUGlyphInstance inst;
        inst.position = t.offset;
        inst.size = t.size;
        inst.uvMin = t.uvMin;
        inst.uvMax = t.uvMax;
        inst.colorPacked = packed;
        inst.scale = 1.0f;
        relative.push_back(inst);
    }

    std::vector<NoMoreDay::components::GPUGlyphInstance> viaCache;
    LootTextBatcher::WriteInstances(templates, relative, origin, packed, viaCache);

    REQUIRE(direct.size() == viaCache.size());
    REQUIRE(direct.size() == templates.size());
    REQUIRE(direct.size() == 3); // 'A', 文, 'B' (space skipped)
    for (size_t i = 0; i < direct.size(); ++i) {
        CAPTURE(i);
        CHECK(GlyphInstancesEqual(direct[i], viaCache[i]));
    }
}

} // TEST_SUITE("LootTextBatcher")
