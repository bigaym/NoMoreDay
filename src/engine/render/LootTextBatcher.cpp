#include "engine/render/LootTextBatcher.hpp"
#include "engine/render/GlyphCache.hpp"
#include "engine/render/resource/MSDFAtlasRegistry.hpp"
#include "raymath.h"
#include <cmath>

namespace NoMoreDay::render {

namespace {

// Snaps a world-space value to the camera pixel grid: one world unit spans
// `zoom` pixels, so the snapped value is a multiple of 1/zoom. Keeps glyph
// quads on whole screen pixels and avoids subpixel bilinear blur.
float SnapToPixelGrid(float value, float zoom) {
    if (zoom <= 1e-4f) return value;
    return std::round(value * zoom) / zoom;
}

} // namespace

float LootTextBatcher::BatchString(const Font& font, const std::string& text, 
                                 Vector2 position, float fontSize, Color color, 
                                 std::vector<NoMoreDay::components::GPUGlyphInstance>& outBuffer) {
    if (font.texture.id == 0 || text.empty()) return 0.0f;

    float scaleFactor = fontSize / (float)font.baseSize;
    float spacing = 1.0f; // Default spacing
    float currentX = position.x;
    
    uint32_t packedColor = ColorToInt(color);
    float texWidth = (float)font.texture.width;
    float texHeight = (float)font.texture.height;

    const char* ptr = text.c_str();
    int byteSize = 0;
    
    while (*ptr != '\0') {
        int codepoint = GetCodepointNext(ptr, &byteSize);
        int index = GlyphIndexCache::Get(font).GetIndex(codepoint);
        
        if (codepoint != ' ' && index >= 0) {
            NoMoreDay::components::GPUGlyphInstance inst;
            
            Rectangle rec = font.recs[index];
            GlyphInfo glyph = font.glyphs[index];
            
            // Calculate render bounds
            inst.position.x = currentX + (float)glyph.offsetX * scaleFactor;
            inst.position.y = position.y + (float)glyph.offsetY * scaleFactor;
            
            inst.size.x = rec.width * scaleFactor;
            inst.size.y = rec.height * scaleFactor;
            
            // Calculate UVs (with safety check for texture dimensions)
            if (texWidth > 0 && texHeight > 0) {
                inst.uvMin.x = rec.x / texWidth;
                inst.uvMin.y = rec.y / texHeight;
                inst.uvMax.x = (rec.x + rec.width) / texWidth;
                inst.uvMax.y = (rec.y + rec.height) / texHeight;
            }
            
            inst.colorPacked = packedColor;
            inst.scale = 1.0f; 
            
            outBuffer.push_back(inst);
        }

        // Advance cursor
        if (index >= 0) {
            if (font.glyphs[index].advanceX == 0) {
                currentX += (font.recs[index].width * scaleFactor + spacing);
            } else {
                currentX += (font.glyphs[index].advanceX * scaleFactor + spacing);
            }
        } else {
            currentX += (fontSize * 0.5f + spacing); // Width estimate for unknown chars
        }
        
        ptr += byteSize;
    }

    return currentX - position.x;
}

Vector2 LootTextBatcher::MeasureText(const Font& font, const std::string& text, float fontSize) {
    if (font.texture.id == 0 || text.empty()) return {0, 0};
    
    float scaleFactor = fontSize / (float)font.baseSize;
    float spacing = 1.0f;
    float currentX = 0.0f;
    
    const char* ptr = text.c_str();
    int byteSize = 0;
    
    while (*ptr != '\0') {
        int codepoint = GetCodepointNext(ptr, &byteSize);
        int index = GlyphIndexCache::Get(font).GetIndex(codepoint);
        
        if (font.glyphs[index].advanceX == 0) {
            currentX += (font.recs[index].width * scaleFactor + spacing);
        } else {
            currentX += (font.glyphs[index].advanceX * scaleFactor + spacing);
        }
        
        ptr += byteSize;
    }
    
    return {currentX, fontSize};
}

void LootTextBatcher::BuildTemplates(const Font& font, const std::string& text, float fontSize,
                                     std::vector<NoMoreDay::components::GlyphTemplate>& out) {
    if (font.texture.id == 0 || text.empty()) return;

    float scaleFactor = fontSize / (float)font.baseSize;
    float spacing = 1.0f; // Default spacing
    float currentX = 0.0f;

    float texWidth = (float)font.texture.width;
    float texHeight = (float)font.texture.height;
    const GlyphIndexCache& glyphCache = GlyphIndexCache::Get(font);

    const char* ptr = text.c_str();
    int byteSize = 0;

    while (*ptr != '\0') {
        int codepoint = GetCodepointNext(ptr, &byteSize);
        int index = glyphCache.GetIndex(codepoint);

        if (codepoint != ' ' && index >= 0) {
            NoMoreDay::components::GlyphTemplate tpl;

            Rectangle rec = font.recs[index];
            GlyphInfo glyph = font.glyphs[index];

            // Render bounds relative to the text origin (no screen coordinates).
            tpl.offset.x = currentX + (float)glyph.offsetX * scaleFactor;
            tpl.offset.y = (float)glyph.offsetY * scaleFactor;

            tpl.size.x = rec.width * scaleFactor;
            tpl.size.y = rec.height * scaleFactor;

            // Calculate UVs (with safety check for texture dimensions).
            // Inset by half a texel so bilinear sampling never bleeds into
            // neighbouring glyphs of the packed atlas.
            if (texWidth > 0 && texHeight > 0) {
                tpl.uvMin.x = rec.x / texWidth + 0.5f / texWidth;
                tpl.uvMin.y = rec.y / texHeight + 0.5f / texHeight;
                tpl.uvMax.x = (rec.x + rec.width) / texWidth - 0.5f / texWidth;
                tpl.uvMax.y = (rec.y + rec.height) / texHeight - 0.5f / texHeight;
            }

            // Full cursor step, identical to BatchString's advance math.
            if (glyph.advanceX == 0) {
                tpl.advanceX = rec.width * scaleFactor + spacing;
            } else {
                tpl.advanceX = (float)glyph.advanceX * scaleFactor + spacing;
            }

            out.push_back(tpl);
        }

        // Advance cursor (identical to BatchString).
        if (index >= 0) {
            if (font.glyphs[index].advanceX == 0) {
                currentX += (font.recs[index].width * scaleFactor + spacing);
            } else {
                currentX += (font.glyphs[index].advanceX * scaleFactor + spacing);
            }
        } else {
            currentX += (fontSize * 0.5f + spacing); // Width estimate for unknown chars
        }

        ptr += byteSize;
    }
}

void LootTextBatcher::BuildTemplatesMsdf(const std::string& text, float fontSize,
                                         std::vector<NoMoreDay::components::GlyphTemplate>& out) {
    if (text.empty()) return;

    const MSDFAtlasRegistry& registry = MSDFAtlasRegistry::Get();
    if (!registry.IsAvailable()) return; // Caller falls back to the bitmap path.

    const float scale = fontSize / registry.GetEmSize();
    const float spacing = 1.0f; // Default spacing, identical to BuildTemplates
    float currentX = 0.0f;

    const char* ptr = text.c_str();
    int byteSize = 0;

    while (*ptr != '\0') {
        int codepoint = GetCodepointNext(ptr, &byteSize);
        const MSDFGlyphMetric* metric = registry.Find(static_cast<uint32_t>(codepoint));

        if (codepoint != ' ' && metric != nullptr) {
            NoMoreDay::components::GlyphTemplate tpl;

            // Render bounds relative to the text origin (no screen coordinates).
            // Bearing is (left, bottom) in em units; scaled to pixels here.
            tpl.offset.x = currentX + metric->bearing[0] * scale;
            tpl.offset.y = metric->bearing[1] * scale;

            tpl.size.x = metric->size[0] * scale;
            tpl.size.y = metric->size[1] * scale;

            // MSDF atlas packs each glyph with margin, so UVs are taken as-is
            // (no half-texel inset, unlike the bitmap path in BuildTemplates).
            tpl.uvMin.x = metric->uvRect[0];
            tpl.uvMin.y = metric->uvRect[1];
            tpl.uvMax.x = metric->uvRect[2];
            tpl.uvMax.y = metric->uvRect[3];

            // Full cursor step, identical to BuildTemplates' advance math.
            tpl.advanceX = metric->advance * scale + spacing;

            out.push_back(tpl);
        }

        // Advance cursor (identical to BuildTemplates: hit -> advance,
        // miss -> width estimate for unknown chars).
        if (metric != nullptr) {
            currentX += metric->advance * scale + spacing;
        } else {
            currentX += (fontSize * 0.5f + spacing);
        }

        ptr += byteSize;
    }
}

void LootTextBatcher::WriteInstances(const std::vector<NoMoreDay::components::GlyphTemplate>& templates,
                                     const std::vector<NoMoreDay::components::GPUGlyphInstance>& cachedRelative,
                                     Vector2 origin, uint32_t color, float zoom,
                                     std::vector<NoMoreDay::components::GPUGlyphInstance>& outBuffer) {
    if (templates.size() != cachedRelative.size()) return;

    outBuffer.reserve(outBuffer.size() + cachedRelative.size());
    for (size_t i = 0; i < cachedRelative.size(); ++i) {
        const NoMoreDay::components::GPUGlyphInstance& src = cachedRelative[i];
        const NoMoreDay::components::GlyphTemplate& tpl = templates[i];
        NoMoreDay::components::GPUGlyphInstance inst = src;
        inst.position.x = SnapToPixelGrid(src.position.x + origin.x, zoom);
        inst.position.y = SnapToPixelGrid(src.position.y + origin.y, zoom);
        inst.size.x = SnapToPixelGrid(src.size.x, zoom);
        inst.size.y = SnapToPixelGrid(src.size.y, zoom);
        // UVs come from the template (half-texel inset applied at build time);
        // the cached relative instances were produced by BatchString at (0,0)
        // and carry the un-inset UVs.
        inst.uvMin = tpl.uvMin;
        inst.uvMax = tpl.uvMax;
        inst.colorPacked = color;
        outBuffer.push_back(inst);
    }
}

} // namespace NoMoreDay::render
