#ifndef NOMOREDAY_LOOTTEXTBATCHER_HPP
#define NOMOREDAY_LOOTTEXTBATCHER_HPP

#pragma once
#include "raylib.h"
#include "engine/render/GPUData.hpp"
#include <vector>
#include <string>

namespace NoMoreDay::render {

/**
 * @brief Layout and batching utilities for GPU text glyphs.
 * The MSDF atlas (MSDFAtlasRegistry) is the single glyph source: metrics are
 * em units scaled by fontSize/emSize, offsets go through
 * coord::MsdfBearingToWorldOffset. The legacy bitmap font path was removed
 * (2026-08-25): the 24px bitmap atlas forced integer scale quantization,
 * which rendered labels oversized compared with the MSDF path.
 */
class LootTextBatcher {
public:
    /**
     * @brief Measures the width of a string using MSDF atlas metrics.
     * Identical cursor math to BuildTemplatesMsdf (advance * fontSize/emSize +
     * 1px spacing per glyph; unknown codepoints advance fontSize*0.5 + 1px),
     * so the label background always matches the glyph templates. When the
     * registry is unavailable a per-codepoint estimate is returned
     * (fontSize*0.5 per codepoint; no glyphs will render anyway).
     * @param text The UTF-8 string to measure.
     * @param fontSize Target font size in pixels.
     * @return {width, fontSize}.
     */
    static Vector2 MeasureTextMsdf(const std::string& text, float fontSize);

    /**
     * @brief Builds per-glyph layout templates from MSDF atlas metrics.
     * Templates are origin-relative and cached; scale = fontSize / emSize;
     * offset uses the glyph bearing (left, bottom) via
     * coord::MsdfBearingToWorldOffset; UVs are the atlas uvRect as-is (the
     * MSDF atlas packs glyphs with margin, so no half-texel inset). Missing
     * codepoints and spaces are skipped while the cursor still advances.
     * When the registry is unavailable this emits nothing.
     * @param text The UTF-8 string to template.
     * @param fontSize Target font size in pixels.
     * @param out Destination vector for glyph templates (spaces and missing glyphs skipped).
     */
    static void BuildTemplatesMsdf(const std::string& text, float fontSize,
                                   std::vector<NoMoreDay::components::GlyphTemplate>& out);

    /**
     * @brief Writes template glyph instances into a buffer at an origin.
     * Rebuilds each instance from its template (offset + origin, size, UVs),
     * applies the given packed color, and snaps every position/size to the
     * camera pixel grid (world unit = 1/zoom) to avoid subpixel bilinear
     * blurring. The template is the only layout source: the MSDF metrics
     * drive positions, sizes and UVs alike.
     * @param templates Layout templates (empty -> nothing emitted).
     * @param origin Screen/world position to translate the instances to.
     * @param color Packed RGBA8 color applied to every instance.
     * @param zoom Camera zoom (pixels per world unit); must be > 0, otherwise the snap degrades to a 1px grid.
     * @param outBuffer The destination vector for glyph instances.
     */
    static void WriteInstances(const std::vector<NoMoreDay::components::GlyphTemplate>& templates,
                               Vector2 origin, uint32_t color, float zoom,
                               std::vector<NoMoreDay::components::GPUGlyphInstance>& outBuffer);
};

} // namespace NoMoreDay::render

#endif // NOMOREDAY_LOOTTEXTBATCHER_HPP
