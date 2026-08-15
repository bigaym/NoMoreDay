#pragma once

#include "doctest.h"
#include "engine/render/LootTextBatcher.hpp"
#include "engine/render/resource/MSDFAtlasRegistry.hpp"

#include <cmath>
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

// Returns true when the value lies on the 1/zoom pixel grid
// (value * zoom is an integer, within float rounding tolerance).
bool OnZoomGrid(float value, float zoom) {
    const float grid = value * zoom;
    return std::abs(grid - std::round(grid)) < 1e-4f;
}

// --- Synthetic MSDF atlas metrics (em units) ---
//
// Mirrors the real v4 atlas contract: ASCII digits advance 0.5em, uppercase
// Latin glyphs carry their own advance/bearing/size. All values are chosen so
// that fontSize = 2 * emSize yields scale = 2.0 exactly and every expected
// result is trivially hand-computable.
struct MsdfSpec {
    uint32_t codepoint = 0;
    float advance = 0.0f;
    float bearingLeft = 0.0f;
    float bearingBottom = 0.0f;
    float sizeW = 0.0f;
    float sizeH = 0.0f;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
};

void RegisterSyntheticMsdf(const std::vector<MsdfSpec>& specs) {
    std::vector<MSDFGlyphMetric> glyphs;
    glyphs.reserve(specs.size());
    for (const MsdfSpec& s : specs) {
        MSDFGlyphMetric m = {};
        m.codepoint = s.codepoint;
        m.advance = s.advance;
        m.bearing[0] = s.bearingLeft;
        m.bearing[1] = s.bearingBottom;
        m.size[0] = s.sizeW;
        m.size[1] = s.sizeH;
        m.uvRect[0] = s.u0;
        m.uvRect[1] = s.v0;
        m.uvRect[2] = s.u1;
        m.uvRect[3] = s.v1;
        glyphs.push_back(m);
    }
    Texture2D tex = {};
    tex.id = 200;
    tex.width = 4096;
    tex.height = 2048;
    MSDFAtlasRegistry::Get().Register(tex, glyphs, 6.0f,
                                      MSDFAtlasRegistry::kV4AtlasEmSize);
}

std::vector<MsdfSpec> MakeSyntheticMsdfGlyphs() {
    return {
        // '0': half-em advance (v4 atlas contract).
        {'0', 0.5f, 0.05f, -0.10f, 0.50f, 0.72f, 0.40f, 0.20f, 0.55f, 0.42f},
        // 'A': full-width-ish Latin advance.
        {'A', 0.6f, 0.06f, -0.12f, 0.62f, 0.68f, 0.10f, 0.05f, 0.30f, 0.25f},
    };
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
    // Half-texel UV inset: min += 0.5/texSize, max -= 0.5/texSize.
    CHECK(a.uvMin.x == doctest::Approx(4.0f / 256.0f + 0.5f / 256.0f));
    CHECK(a.uvMin.y == doctest::Approx(5.0f / 128.0f + 0.5f / 128.0f));
    CHECK(a.uvMax.x == doctest::Approx(14.0f / 256.0f - 0.5f / 256.0f));
    CHECK(a.uvMax.y == doctest::Approx(17.0f / 128.0f - 0.5f / 128.0f));
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
    CHECK(b.uvMin.x == doctest::Approx(30.0f / 256.0f + 0.5f / 256.0f));
    CHECK(b.uvMin.y == doctest::Approx(8.0f / 128.0f + 0.5f / 128.0f));
    CHECK(b.uvMax.x == doctest::Approx(44.0f / 256.0f - 0.5f / 256.0f));
    CHECK(b.uvMax.y == doctest::Approx(18.0f / 128.0f - 0.5f / 128.0f));
    CHECK(b.offset.x == doctest::Approx(cursorAfterSpace));
    CHECK(b.offset.y == doctest::Approx(3.0f * scale));
    CHECK(b.advanceX == doctest::Approx(14.0f * scale + spacing)); // width fallback step
}

TEST_CASE("[Unit] LootTextBatcher WriteInstances translates, tints and guards mismatches") {
    std::vector<NoMoreDay::components::GlyphTemplate> templates(2);
    // UVs come from the templates (inset applied at build time), so give them
    // distinct values to verify the template UV wins over the cached instance.
    templates[0].uvMin = {0.10f, 0.20f};
    templates[0].uvMax = {0.30f, 0.40f};
    templates[1].uvMin = {0.50f, 0.60f};
    templates[1].uvMax = {0.70f, 0.80f};

    std::vector<NoMoreDay::components::GPUGlyphInstance> relative(2);
    relative[0].position = {1.0f, 2.0f};
    relative[0].uvMin = {0.01f, 0.02f}; // stale, must be overridden by the template
    relative[0].colorPacked = 0xFFFFFFFFu;
    relative[1].position = {5.0f, 8.0f};
    relative[1].colorPacked = 0xFFFFFFFFu;

    std::vector<NoMoreDay::components::GPUGlyphInstance> out;
    LootTextBatcher::WriteInstances(templates, relative, {10.0f, 20.0f}, 0x11223344u,
                                    1.0f, out);

    REQUIRE(out.size() == 2);
    CHECK(out[0].position.x == doctest::Approx(11.0f));
    CHECK(out[0].position.y == doctest::Approx(22.0f));
    CHECK(out[1].position.x == doctest::Approx(15.0f));
    CHECK(out[1].position.y == doctest::Approx(28.0f));
    CHECK(out[0].colorPacked == 0x11223344u);
    CHECK(out[1].colorPacked == 0x11223344u);
    CHECK(out[0].size.x == relative[0].size.x); // non-position fields preserved
    // UVs are taken from the template, not from the cached instance.
    CHECK(out[0].uvMin.x == doctest::Approx(0.10f));
    CHECK(out[0].uvMin.y == doctest::Approx(0.20f));
    CHECK(out[0].uvMax.x == doctest::Approx(0.30f));
    CHECK(out[0].uvMax.y == doctest::Approx(0.40f));
    CHECK(out[1].uvMin.x == doctest::Approx(0.50f));

    // Size mismatch between templates and instances -> nothing emitted.
    std::vector<NoMoreDay::components::GPUGlyphInstance> guarded;
    std::vector<NoMoreDay::components::GlyphTemplate> mismatched(3);
    LootTextBatcher::WriteInstances(mismatched, relative, {0.0f, 0.0f}, 0xFFFFFFFFu,
                                    1.0f, guarded);
    CHECK(guarded.empty());
}

TEST_CASE("[Unit] LootTextBatcher WriteInstances snaps position/size to the zoom pixel grid") {
    std::vector<Rectangle> recs;
    std::vector<GlyphInfo> glyphs;
    Font font = MakeLabelFont(recs, glyphs);

    const std::string text = "A文 B"; // 3 glyphs, space skipped
    const float fontSize = 16.0f;     // scale = 0.5
    const Vector2 origin = {101.3f, 205.7f};
    const Color color = {10, 20, 30, 255};
    const uint32_t packed = ColorToInt(color);

    std::vector<NoMoreDay::components::GlyphTemplate> templates;
    LootTextBatcher::BuildTemplates(font, text, fontSize, templates);

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

    for (const float zoom : {1.5f, 2.0f}) {
        CAPTURE(zoom);
        std::vector<NoMoreDay::components::GPUGlyphInstance> out;
        LootTextBatcher::WriteInstances(templates, relative, origin, packed, zoom, out);

        REQUIRE(out.size() == 3);
        for (size_t i = 0; i < out.size(); ++i) {
            CAPTURE(i);
            // Absolute position lands on the 1/zoom world grid.
            CHECK(OnZoomGrid(out[i].position.x, zoom));
            CHECK(OnZoomGrid(out[i].position.y, zoom));
            // Size quantized to the same grid (edge-to-edge on whole pixels).
            CHECK(OnZoomGrid(out[i].size.x, zoom));
            CHECK(OnZoomGrid(out[i].size.y, zoom));
            // Layout preserved relative to BatchString (same origin/size math,
            // snap only quantizes; UVs identical to the template).
            CHECK(out[i].uvMin.x == doctest::Approx(templates[i].uvMin.x));
            CHECK(out[i].uvMax.y == doctest::Approx(templates[i].uvMax.y));
            CHECK(out[i].colorPacked == packed);
        }
    }
}

TEST_CASE("[Unit] LootTextBatcher BuildTemplates+WriteInstances preserves BatchString layout") {
    std::vector<Rectangle> recs;
    std::vector<GlyphInfo> glyphs;
    Font font = MakeLabelFont(recs, glyphs);

    const std::string text = "A文 B"; // UTF-8 decode + space skip + width-fallback 'B'
    const float fontSize = 16.0f;
    const Vector2 origin = {100.0f, 200.0f};
    const Color color = {10, 20, 30, 255};
    const uint32_t packed = ColorToInt(color);

    // Same layout input for every zoom; the snap may shift positions by at
    // most half a screen pixel (0.5/zoom world units), sizes stay identical
    // (they are integer screen pixels at these zooms).
    for (const float zoom : {1.0f, 1.5f, 2.0f}) {
        CAPTURE(zoom);
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
        LootTextBatcher::WriteInstances(templates, relative, origin, packed, zoom, viaCache);

        REQUIRE(direct.size() == viaCache.size());
        REQUIRE(direct.size() == templates.size());
        REQUIRE(direct.size() == 3); // 'A', 文, 'B' (space skipped)
        for (size_t i = 0; i < direct.size(); ++i) {
            CAPTURE(i);
            // Identical glyph count and per-glyph layout math: the cached path
            // reproduces BatchString's positions/sizes within the snap
            // quantization error (half a screen pixel = 0.5/zoom world units).
            CHECK(std::abs(viaCache[i].position.x - direct[i].position.x) <=
                  0.5f / zoom + 1e-4f);
            CHECK(std::abs(viaCache[i].position.y - direct[i].position.y) <=
                  0.5f / zoom + 1e-4f);
            // Sizes are quantized to the same grid (e.g. 5.0 at zoom=1.5 ->
            // round(7.5)/1.5 = 5.333), so compare within half a screen pixel.
            CHECK(std::abs(viaCache[i].size.x - direct[i].size.x) <=
                  0.5f / zoom + 1e-4f);
            CHECK(std::abs(viaCache[i].size.y - direct[i].size.y) <=
                  0.5f / zoom + 1e-4f);
            CHECK(viaCache[i].colorPacked == direct[i].colorPacked);
            // UVs are the template's half-texel-inset values (BatchString keeps
            // its un-inset UVs by design).
            CHECK(viaCache[i].uvMin.x == doctest::Approx(templates[i].uvMin.x));
            CHECK(viaCache[i].uvMin.y == doctest::Approx(templates[i].uvMin.y));
            CHECK(viaCache[i].uvMax.x == doctest::Approx(templates[i].uvMax.x));
            CHECK(viaCache[i].uvMax.y == doctest::Approx(templates[i].uvMax.y));
        }
    }
}

} // TEST_SUITE("LootTextBatcher")

TEST_SUITE("LootText MSDF templates") {

// Common fixture: registers the synthetic atlas and restores the registry to
// the cleared state afterwards so other suites are unaffected by ordering.
struct MsdfFixture {
    MsdfFixture() { MSDFAtlasRegistry::Get().Clear(); }
    ~MsdfFixture() { MSDFAtlasRegistry::Get().Clear(); }
};

TEST_CASE("[Unit] LootText - MSDF templates lay out glyphs from atlas metrics") {
    MsdfFixture fixture;
    RegisterSyntheticMsdf(MakeSyntheticMsdfGlyphs());
    REQUIRE(MSDFAtlasRegistry::Get().IsAvailable());

    const float emSize = MSDFAtlasRegistry::kV4AtlasEmSize;
    const float fontSize = 2.0f * emSize; // scale = 2.0 exactly
    const float scale = 2.0f;
    const float spacing = 1.0f;

    std::vector<NoMoreDay::components::GlyphTemplate> templates;
    LootTextBatcher::BuildTemplatesMsdf("A0", fontSize, templates);

    REQUIRE(templates.size() == 2);

    // 'A': offset = {0 + bearingLeft*scale, bearingBottom*scale}
    const NoMoreDay::components::GlyphTemplate& a = templates[0];
    CHECK(a.offset.x == doctest::Approx(0.06f * scale));      // 0.12
    CHECK(a.offset.y == doctest::Approx(-0.12f * scale));     // -0.24
    CHECK(a.size.x == doctest::Approx(0.62f * scale));        // 1.24
    CHECK(a.size.y == doctest::Approx(0.68f * scale));        // 1.36
    // UVs taken as-is from uvRect (no half-texel inset).
    CHECK(a.uvMin.x == doctest::Approx(0.10f));
    CHECK(a.uvMin.y == doctest::Approx(0.05f));
    CHECK(a.uvMax.x == doctest::Approx(0.30f));
    CHECK(a.uvMax.y == doctest::Approx(0.25f));
    CHECK(a.advanceX == doctest::Approx(0.6f * scale + spacing)); // 2.2

    // '0' after 'A': cursor advanced by 'A' advance.
    const float cursorAfterA = 0.6f * scale + spacing; // 2.2
    const NoMoreDay::components::GlyphTemplate& zero = templates[1];
    CHECK(zero.offset.x == doctest::Approx(cursorAfterA + 0.05f * scale)); // 2.3
    CHECK(zero.offset.y == doctest::Approx(-0.10f * scale));               // -0.2
    CHECK(zero.size.x == doctest::Approx(0.50f * scale));                  // 1.0
    CHECK(zero.size.y == doctest::Approx(0.72f * scale));                  // 1.44
    CHECK(zero.uvMin.x == doctest::Approx(0.40f));
    CHECK(zero.uvMin.y == doctest::Approx(0.20f));
    CHECK(zero.uvMax.x == doctest::Approx(0.55f));
    CHECK(zero.uvMax.y == doctest::Approx(0.42f));
    CHECK(zero.advanceX == doctest::Approx(0.5f * scale + spacing)); // 2.0 (half-em)
}

TEST_CASE("[Unit] LootText - MSDF templates empty when registry unavailable") {
    MsdfFixture fixture;
    CHECK_FALSE(MSDFAtlasRegistry::Get().IsAvailable());

    std::vector<NoMoreDay::components::GlyphTemplate> templates;
    LootTextBatcher::BuildTemplatesMsdf("A0", 2.0f * MSDFAtlasRegistry::kV4AtlasEmSize,
                                        templates);
    CHECK(templates.empty());
}

TEST_CASE("[Unit] LootText - MSDF templates skip unknown codepoints and advance") {
    MsdfFixture fixture;
    RegisterSyntheticMsdf(MakeSyntheticMsdfGlyphs());

    const float emSize = MSDFAtlasRegistry::kV4AtlasEmSize;
    const float fontSize = 2.0f * emSize; // scale = 2.0
    const float spacing = 1.0f;

    // '中' (U+4E2D) is not in the synthetic atlas -> skipped, cursor advances
    // by the width estimate fontSize*0.5 + spacing.
    std::vector<NoMoreDay::components::GlyphTemplate> templates;
    LootTextBatcher::BuildTemplatesMsdf("A中0", fontSize, templates);

    REQUIRE(templates.size() == 2);

    const float cursorAfterA = 0.6f * 2.0f + spacing;              // 2.2
    const float cursorAfterMiss = cursorAfterA + fontSize * 0.5f + spacing; // 32.278125
    const NoMoreDay::components::GlyphTemplate& zero = templates[1];
    CHECK(zero.offset.x == doctest::Approx(cursorAfterMiss + 0.05f * 2.0f));
    CHECK(zero.offset.y == doctest::Approx(-0.10f * 2.0f));
    CHECK(zero.advanceX == doctest::Approx(0.5f * 2.0f + spacing));
}

TEST_CASE("[Unit] LootText - MSDF templates write instances snapped to zoom grid") {
    MsdfFixture fixture;
    RegisterSyntheticMsdf(MakeSyntheticMsdfGlyphs());

    const float emSize = MSDFAtlasRegistry::kV4AtlasEmSize;
    const float fontSize = 2.0f * emSize;

    std::vector<NoMoreDay::components::GlyphTemplate> templates;
    LootTextBatcher::BuildTemplatesMsdf("A0", fontSize, templates);
    REQUIRE(templates.size() == 2);

    // Reconstruct origin-relative instances purely from templates.
    std::vector<NoMoreDay::components::GPUGlyphInstance> relative;
    relative.reserve(templates.size());
    for (const NoMoreDay::components::GlyphTemplate& t : templates) {
        NoMoreDay::components::GPUGlyphInstance inst;
        inst.position = t.offset;
        inst.size = t.size;
        inst.uvMin = t.uvMin;
        inst.uvMax = t.uvMax;
        inst.colorPacked = 0xFFFFFFFFu;
        inst.scale = 1.0f;
        relative.push_back(inst);
    }

    const Vector2 origin = {101.3f, 205.7f};
    const uint32_t packed = ColorToInt({10, 20, 30, 255});

    for (const float zoom : {1.5f, 2.0f}) {
        CAPTURE(zoom);
        std::vector<NoMoreDay::components::GPUGlyphInstance> out;
        LootTextBatcher::WriteInstances(templates, relative, origin, packed, zoom, out);

        REQUIRE(out.size() == 2);
        for (size_t i = 0; i < out.size(); ++i) {
            CAPTURE(i);
            CHECK(OnZoomGrid(out[i].position.x, zoom));
            CHECK(OnZoomGrid(out[i].position.y, zoom));
            CHECK(OnZoomGrid(out[i].size.x, zoom));
            CHECK(OnZoomGrid(out[i].size.y, zoom));
            CHECK(out[i].uvMin.x == doctest::Approx(templates[i].uvMin.x));
            CHECK(out[i].uvMax.y == doctest::Approx(templates[i].uvMax.y));
            CHECK(out[i].colorPacked == packed);
        }
    }
}

} // TEST_SUITE("LootText MSDF templates")
