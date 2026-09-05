#include "engine/render/LootTextBatcher.hpp"
#include "engine/render/CoordSystem.hpp"
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

Vector2 LootTextBatcher::MeasureTextMsdf(const std::string& text, float fontSize) {
    const MSDFAtlasRegistry& registry = MSDFAtlasRegistry::Get();

    int codepointCount = 0;
    {
        const char* ptr = text.c_str();
        int byteSize = 0;
        while (*ptr != '\0') {
            GetCodepointNext(ptr, &byteSize);
            ++codepointCount;
            ptr += byteSize;
        }
    }
    if (codepointCount == 0) return {0, 0};
    if (!registry.IsAvailable()) {
        // No glyphs will render anyway; estimate a background width so the
        // label box stays visible.
        return {fontSize * 0.5f * static_cast<float>(codepointCount), fontSize};
    }

    // 度量换算：metrics.bin 的 size/bearing/advance 均为 em 单位，而 fontSize
    // 的语义是"每个 em 对应的世界单位（像素）数"，因此 em → 世界单位直接乘
    // fontSize 即可。旧实现额外除以 atlas 的 emSize（生成期 px/em），导致整个
    // 文字布局塌缩约 29 倍、字形退化为亚像素方块（SnapToPixelGrid 后变成
    // 0/0.5 尺寸的不可见 quad）。
    const float scale = fontSize;
    const float spacing = 1.0f; // Default spacing, identical to BuildTemplatesMsdf
    float currentX = 0.0f;

    const char* ptr = text.c_str();
    int byteSize = 0;
    while (*ptr != '\0') {
        int codepoint = GetCodepointNext(ptr, &byteSize);
        const MSDFGlyphMetric* metric = registry.Find(static_cast<uint32_t>(codepoint));
        if (metric != nullptr) {
            currentX += metric->advance * scale + spacing;
        } else {
            currentX += (fontSize * 0.5f + spacing); // Width estimate for unknown chars
        }
        ptr += byteSize;
    }

    return {currentX, fontSize};
}

void LootTextBatcher::BuildTemplatesMsdf(const std::string& text, float fontSize,
                                         std::vector<NoMoreDay::components::GlyphTemplate>& out) {
    if (text.empty()) return;

    const MSDFAtlasRegistry& registry = MSDFAtlasRegistry::Get();
    if (!registry.IsAvailable()) return; // No glyphs; caller skips instances.

    // em → 世界单位换算与 MeasureTextMsdf 一致：直接乘 fontSize（px/em），
    // 不得再除以 atlas emSize（详见 MeasureTextMsdf 处的换算说明）。
    const float scale = fontSize;
    const float spacing = 1.0f; // Default spacing, identical to MeasureTextMsdf
    float currentX = 0.0f;

    const char* ptr = text.c_str();
    int byteSize = 0;

    while (*ptr != '\0') {
        int codepoint = GetCodepointNext(ptr, &byteSize);
        const MSDFGlyphMetric* metric = registry.Find(static_cast<uint32_t>(codepoint));

        if (codepoint != ' ' && metric != nullptr) {
            NoMoreDay::components::GlyphTemplate tpl;

            // Render bounds relative to the text origin (no screen coordinates).
            // Bearing is (left, bottom) in em units; scale to world pixels by
            // fontSize directly（em × px/em），与 size/advance 换算保持一致。
            // 此处不再经过 coord::MsdfBearingToWorldOffset（其 /emSize 公式
            // 仅服务 Game.cpp 的 V4 GPU 路径语义）。
            tpl.offset.x = currentX + metric->bearing[0] * fontSize;
            tpl.offset.y = metric->bearing[1] * fontSize;

            tpl.size.x = metric->size[0] * scale;
            tpl.size.y = metric->size[1] * scale;

            // MSDF atlas packs each glyph with margin, so UVs are taken as-is
            // (no half-texel inset).
            tpl.uvMin.x = metric->uvRect[0];
            tpl.uvMin.y = metric->uvRect[1];
            tpl.uvMax.x = metric->uvRect[2];
            tpl.uvMax.y = metric->uvRect[3];

            // Full cursor step, identical to MeasureTextMsdf's advance math.
            tpl.advanceX = metric->advance * scale + spacing;

            out.push_back(tpl);
        }

        // Advance cursor (hit -> advance, miss -> width estimate for unknown chars).
        if (metric != nullptr) {
            currentX += metric->advance * scale + spacing;
        } else {
            currentX += (fontSize * 0.5f + spacing);
        }

        ptr += byteSize;
    }
}

void LootTextBatcher::WriteInstances(const std::vector<NoMoreDay::components::GlyphTemplate>& templates,
                                     Vector2 origin, uint32_t color, float zoom,
                                     std::vector<NoMoreDay::components::GPUGlyphInstance>& outBuffer) {
    outBuffer.reserve(outBuffer.size() + templates.size());
    for (const NoMoreDay::components::GlyphTemplate& tpl : templates) {
        // The template is the single layout source: MSDF metrics drive
        // positions, sizes and UVs alike.
        NoMoreDay::components::GPUGlyphInstance inst;
        inst.position.x = SnapToPixelGrid(tpl.offset.x + origin.x, zoom);
        inst.position.y = SnapToPixelGrid(tpl.offset.y + origin.y, zoom);
        inst.size.x = SnapToPixelGrid(tpl.size.x, zoom);
        inst.size.y = SnapToPixelGrid(tpl.size.y, zoom);
        inst.uvMin = tpl.uvMin;
        inst.uvMax = tpl.uvMax;
        inst.colorPacked = color;
        inst.scale = 1.0f;
        outBuffer.push_back(inst);
    }
}

} // namespace NoMoreDay::render
