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

    /**
     * @brief Builds per-glyph layout templates for a text string.
     * Reuses the same layout math as BatchString (GetCodepointNext + glyph
     * index cache) but emits no screen coordinates: each template holds the
     * render bounds relative to the text origin plus the cursor step, so the
     * result can be cached and re-emitted at any origin via WriteInstances.
     * @param font The Raylib font to use (must be valid and loaded with required characters).
     * @param text The UTF-8 string to template.
     * @param fontSize Target font size.
     * @param out Destination vector for glyph templates (space and missing glyphs skipped).
     */
    static void BuildTemplates(const Font& font, const std::string& text, float fontSize,
                               std::vector<NoMoreDay::components::GlyphTemplate>& out);

    /**
     * @brief Writes cached (origin-relative) glyph instances into a buffer at an origin.
     * Translates each cachedRelative instance by origin and applies the given
     * packed color to every instance. Byte-equivalent to calling BatchString
     * with the same font/text/fontSize/color at the given origin when
     * cachedRelative was produced from the same layout.
     * @param templates Layout templates matching cachedRelative (sanity guard; sizes must match).
     * @param cachedRelative Absolute glyph instances relative to (0,0).
     * @param origin Screen/world position to translate the instances to.
     * @param color Packed RGBA8 color applied to every instance.
     * @param outBuffer The destination vector for glyph instances.
     */
    static void WriteInstances(const std::vector<NoMoreDay::components::GlyphTemplate>& templates,
                               const std::vector<NoMoreDay::components::GPUGlyphInstance>& cachedRelative,
                               Vector2 origin, uint32_t color,
                               std::vector<NoMoreDay::components::GPUGlyphInstance>& outBuffer);
};

} // namespace NoMoreDay::render

#endif // NOMOREDAY_LOOTTEXTBATCHER_HPP
