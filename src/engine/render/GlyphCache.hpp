#pragma once

#include <cstdint>
#include <unordered_map>

#include <raylib.h>

namespace NoMoreDay::render {

// GlyphIndexCache: per-font codepoint -> glyph index lookup cache.
//
// raylib's GetGlyphIndex() performs a linear scan over the font glyph table on
// every call (rtext.c:1339-1365). For fonts with tens of thousands of glyphs
// (e.g. Chinese font packs) this dominates the per-character cost of the CPU
// label path. GlyphIndexCache builds the same mapping once per Font and reuses
// it across frames; rebuilds happen automatically when the font is rebuilt
// (glyphCount changes).
//
// Semantics are identical to raylib GetGlyphIndex(): a hit returns the index of
// the first glyph whose value equals the codepoint; a miss returns the index of
// the '?' glyph (value 63), or 0 when the font has no '?' glyph.
class GlyphIndexCache {
public:
    // Returns the cache entry for the given font, building it on first use.
    // The returned reference stays valid as long as the font is not rebuilt.
    static const GlyphIndexCache& Get(const Font& font);

    // Glyph index for codepoint; falls back to the '?' glyph index on miss.
    int GetIndex(int codepoint) const;

    // True when the cache still matches the given font; false after the font
    // has been rebuilt (glyphCount changed), which forces a rebuild on next Get.
    bool IsValidFor(const Font& font) const;

private:
    GlyphIndexCache() = default;
    void Rebuild(const Font& font);

    uint32_t m_fontId = 0;
    int m_glyphCount = 0;
    int m_fallbackIndex = 0;
    std::unordered_map<uint32_t, int> m_indices;
};

} // namespace NoMoreDay::render
