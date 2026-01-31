#ifndef NOMOREDAY_LOOTTEXTBATCHER_HPP
#define NOMOREDAY_LOOTTEXTBATCHER_HPP

#pragma once
#include "raylib.h"
#include "engine/render/GPUData.hpp"
#include <vector>
#include <string>

namespace NoMoreDay::render {

/**
 * @brief Utility to batch text characters into GPU-ready glyph instances.
 * Supports UTF-8 (Chinese) by leveraging Raylib's font atlas.
 */
class LootTextBatcher {
public:
    /**
     * @brief Converts a string into a series of GPUGlyphInstance structures.
     * @param font The Raylib font to use (must be valid and loaded with required characters).
     * @param text The UTF-8 string to batch.
     * @param position Top-left position of the text.
     * @param fontSize Target font size.
     * @param color Text color.
     * @param outBuffer The destination vector for glyph instances.
     * @return Total width of the batched text.
     */
    static float BatchString(const Font& font, const std::string& text, 
                            Vector2 position, float fontSize, Color color, 
                            std::vector<NoMoreDay::components::GPUGlyphInstance>& outBuffer);

    /**
     * @brief Measures the width of a string (similar to MeasureTextEx but consistent with our batcher).
     */
    static Vector2 MeasureText(const Font& font, const std::string& text, float fontSize);
};

} // namespace NoMoreDay::render

#endif // NOMOREDAY_LOOTTEXTBATCHER_HPP
