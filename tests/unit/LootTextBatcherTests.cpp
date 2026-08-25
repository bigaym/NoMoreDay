#pragma once

#include "doctest.h"
#include "engine/render/LootTextBatcher.hpp"
#include "engine/render/resource/MSDFAtlasRegistry.hpp"

#include <cmath>
#include <vector>

using namespace NoMoreDay::render;

namespace {

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

// Common fixture: registers the synthetic atlas and restores the registry to
// the cleared state afterwards so other suites are unaffected by ordering.
struct MsdfFixture {
    MsdfFixture() { MSDFAtlasRegistry::Get().Clear(); }
    ~MsdfFixture() { MSDFAtlasRegistry::Get().Clear(); }
};

} // namespace

TEST_SUITE("LootText MSDF templates") {

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

TEST_CASE("[Unit] LootText - WriteInstances rebuilds instances from templates") {
    MsdfFixture fixture;
    RegisterSyntheticMsdf(MakeSyntheticMsdfGlyphs());

    const float emSize = MSDFAtlasRegistry::kV4AtlasEmSize;
    const float fontSize = 2.0f * emSize;

    std::vector<NoMoreDay::components::GlyphTemplate> templates;
    LootTextBatcher::BuildTemplatesMsdf("A0", fontSize, templates);
    REQUIRE(templates.size() == 2);

    const Vector2 origin = {101.3f, 205.7f};
    const uint32_t packed = ColorToInt({10, 20, 30, 255});

    for (const float zoom : {1.5f, 2.0f}) {
        CAPTURE(zoom);
        std::vector<NoMoreDay::components::GPUGlyphInstance> out;
        LootTextBatcher::WriteInstances(templates, origin, packed, zoom, out);

        REQUIRE(out.size() == 2);
        for (size_t i = 0; i < out.size(); ++i) {
            CAPTURE(i);
            // Absolute position lands on the 1/zoom world grid and equals the
            // template offset translated by the origin (within the snap
            // quantization error of half a screen pixel = 0.5/zoom world units).
            CHECK(OnZoomGrid(out[i].position.x, zoom));
            CHECK(OnZoomGrid(out[i].position.y, zoom));
            CHECK(std::abs(out[i].position.x -
                           (templates[i].offset.x + origin.x)) <=
                  0.5f / zoom + 1e-4f);
            CHECK(std::abs(out[i].position.y -
                           (templates[i].offset.y + origin.y)) <=
                  0.5f / zoom + 1e-4f);
            // Size quantized to the same grid (edge-to-edge on whole pixels).
            CHECK(OnZoomGrid(out[i].size.x, zoom));
            CHECK(OnZoomGrid(out[i].size.y, zoom));
            // UVs and color come straight from the template/argument.
            CHECK(out[i].uvMin.x == doctest::Approx(templates[i].uvMin.x));
            CHECK(out[i].uvMax.y == doctest::Approx(templates[i].uvMax.y));
            CHECK(out[i].colorPacked == packed);
        }
    }

    // Empty templates -> nothing emitted.
    std::vector<NoMoreDay::components::GPUGlyphInstance> empty;
    LootTextBatcher::WriteInstances({}, origin, packed, 1.0f, empty);
    CHECK(empty.empty());
}

TEST_CASE("[Unit] LootText - MeasureTextMsdf matches template cursor math") {
    MsdfFixture fixture;
    RegisterSyntheticMsdf(MakeSyntheticMsdfGlyphs());

    const float emSize = MSDFAtlasRegistry::kV4AtlasEmSize;
    const float fontSize = 2.0f * emSize;
    const float spacing = 1.0f;

    // "A0": width = advance(A)*scale + spacing + advance(0)*scale + spacing.
    const float expect = 0.6f * 2.0f + spacing + 0.5f * 2.0f + spacing;
    const Vector2 measured = LootTextBatcher::MeasureTextMsdf("A0", fontSize);
    CHECK(measured.x == doctest::Approx(expect));
    CHECK(measured.y == doctest::Approx(fontSize));

    // Unknown codepoint advances fontSize*0.5 + spacing (same as templates).
    const Vector2 withMiss = LootTextBatcher::MeasureTextMsdf("A中0", fontSize);
    CHECK(withMiss.x == doctest::Approx(expect + fontSize * 0.5f + spacing));

    // Empty string measures zero.
    CHECK(LootTextBatcher::MeasureTextMsdf("", fontSize).x == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] LootText - MeasureTextMsdf estimates when registry unavailable") {
    MsdfFixture fixture;
    CHECK_FALSE(MSDFAtlasRegistry::Get().IsAvailable());

    const float fontSize = 24.0f;
    const Vector2 measured = LootTextBatcher::MeasureTextMsdf("ABC", fontSize);
    // 3 codepoints * fontSize*0.5 each (no glyphs will render anyway).
    CHECK(measured.x == doctest::Approx(3.0f * fontSize * 0.5f));
    CHECK(measured.y == doctest::Approx(fontSize));
}

} // TEST_SUITE("LootText MSDF templates")
