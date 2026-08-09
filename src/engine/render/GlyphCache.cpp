#include "engine/render/GlyphCache.hpp"

#include <cstdint>

namespace NoMoreDay::render {

// Mirror of raylib rtext.c GetGlyphIndex() (rtext.c:1339-1365): the index of
// the first glyph whose value equals the codepoint, with the '?' glyph (63)
// used when no exact match exists.
const GlyphIndexCache& GlyphIndexCache::Get(const Font& font) {
    static GlyphIndexCache s_cache;

    if (!s_cache.IsValidFor(font)) {
        s_cache.Rebuild(font);
    }
    return s_cache;
}

int GlyphIndexCache::GetIndex(int codepoint) const {
    if (codepoint < 0) {
        return m_fallbackIndex;
    }
    const auto it = m_indices.find(static_cast<uint32_t>(codepoint));
    if (it != m_indices.end()) {
        return it->second;
    }
    return m_fallbackIndex;
}

bool GlyphIndexCache::IsValidFor(const Font& font) const {
    return m_fontId == font.texture.id && m_glyphCount == font.glyphCount;
}

void GlyphIndexCache::Rebuild(const Font& font) {
    m_fontId = font.texture.id;
    m_glyphCount = font.glyphCount;
    m_fallbackIndex = 0;
    m_indices.clear();
    m_indices.reserve(static_cast<size_t>(font.glyphCount) + 8u);

    for (int i = 0; i < font.glyphCount; ++i) {
        const int value = font.glyphs[i].value;
        if (value == '?') {
            m_fallbackIndex = i;
        }
        if (value > 0) {
            m_indices.emplace(static_cast<uint32_t>(value), i);
        }
    }
}

} // namespace NoMoreDay::render
