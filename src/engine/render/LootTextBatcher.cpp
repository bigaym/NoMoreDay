#include "engine/render/LootTextBatcher.hpp"
#include "raymath.h"

namespace NoMoreDay::render {

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
        int index = GetGlyphIndex(font, codepoint);
        
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
            currentX += (fontSize * 0.5f + spacing); // Fallback for unknown chars
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
        int index = GetGlyphIndex(font, codepoint);
        
        if (font.glyphs[index].advanceX == 0) {
            currentX += (font.recs[index].width * scaleFactor + spacing);
        } else {
            currentX += (font.glyphs[index].advanceX * scaleFactor + spacing);
        }
        
        ptr += byteSize;
    }
    
    return {currentX, fontSize};
}

} // namespace NoMoreDay::render
