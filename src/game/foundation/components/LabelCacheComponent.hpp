#pragma once

#include "engine/render/GPUData.hpp"
#include "raylib.h"
#include <vector>
#include <cstdint>

// 标签缓存组件：用于加速世界坐标中文字标签的渲染
struct LabelCacheComponent {
    char cachedText[64] = {0};
    Vector2 cachedSize = {0, 0};
    int lastFontSize = 0;
    uint32_t lastRarityHash = 0;
    bool isValid = false;
    // 模板来源（MSDF 图集是唯一来源，保留该标志以便未来切换时重建模板）
    bool lastUsedMsdf = false;

    // 字形布局模板（相对文本原点，不含屏幕坐标），由
    // LootTextBatcher::BuildTemplatesMsdf 产出（MSDF 图集是唯一字形来源，
    // 位图路径已于 2026-08-25 删除）
    std::vector<NoMoreDay::components::GlyphTemplate> glyphTemplates;

    // Helper to force re-validation
    void Invalidate() { isValid = false; }
};
