#include "engine/render/debug/RenderProfiler.hpp"

#include "raylib.h"

#include <algorithm>

namespace NoMoreDay::render::debug {

void DrawProfilerHud(const RenderProfiler &profiler, float x, float y) {
  constexpr int kFontSize = 16;
  constexpr float kLineHeight = 18.0f;
  constexpr float kWidth = 680.0f;
  constexpr float kHeaderHeight = 24.0f;
  constexpr float kRowHeight = 18.0f;
  constexpr int kRowCount = static_cast<int>(RenderPassId::Count);
  constexpr float kHeight = kHeaderHeight + kRowHeight * (kRowCount + 1) + 8.0f;

  DrawRectangleRounded(
      Rectangle{x, y, kWidth, kHeight}, 0.05f, 6, Color{12, 14, 20, 200});
  DrawRectangleLinesEx(Rectangle{x, y, kWidth, kHeight}, 1.0f, Color{90, 98, 120, 220});

  DrawText("Render Pass Profiler (ms)", static_cast<int>(x + 10.0f),
           static_cast<int>(y + 4.0f), kFontSize, RAYWHITE);

  const float baseY = y + kHeaderHeight;
  DrawText("Pass", static_cast<int>(x + 10.0f), static_cast<int>(baseY), kFontSize,
           GRAY);
  DrawText("CPU Mean/P95", static_cast<int>(x + 170.0f), static_cast<int>(baseY),
           kFontSize, GRAY);
  DrawText("GPU Mean/P95", static_cast<int>(x + 360.0f), static_cast<int>(baseY),
           kFontSize, GRAY);
  DrawText("Budget", static_cast<int>(x + 540.0f), static_cast<int>(baseY), kFontSize,
           GRAY);
  DrawText("Over", static_cast<int>(x + 615.0f), static_cast<int>(baseY), kFontSize,
           GRAY);

  const auto stats = profiler.GetAllStats();
  for (int row = 0; row < kRowCount; ++row) {
    const auto passId = static_cast<RenderPassId>(row);
    const auto &entry = stats[static_cast<size_t>(row)];
    const float rowY = baseY + kLineHeight * static_cast<float>(row + 1);

    const float overMs = std::max(0.0f, entry.gpuMeanMs - entry.budgetMs);
    const float overPct =
        (entry.budgetMs > 0.0f) ? (overMs / entry.budgetMs * 100.0f) : 0.0f;
    const Color overColor = overMs > 0.0f ? Color{255, 120, 120, 255} : Color{150, 220, 140, 255};

    DrawText(RenderProfiler::ToString(passId), static_cast<int>(x + 10.0f),
             static_cast<int>(rowY), kFontSize, RAYWHITE);
    DrawText(TextFormat("%5.3f / %5.3f", entry.cpuMeanMs, entry.cpuP95Ms),
             static_cast<int>(x + 170.0f), static_cast<int>(rowY), kFontSize,
             Color{200, 216, 255, 255});
    DrawText(TextFormat("%5.3f / %5.3f", entry.gpuMeanMs, entry.gpuP95Ms),
             static_cast<int>(x + 360.0f), static_cast<int>(rowY), kFontSize,
             Color{200, 255, 220, 255});
    DrawText(TextFormat("%4.2f", entry.budgetMs), static_cast<int>(x + 540.0f),
             static_cast<int>(rowY), kFontSize, Color{230, 230, 180, 255});
    DrawText(TextFormat("%5.1f%%", overPct), static_cast<int>(x + 615.0f),
             static_cast<int>(rowY), kFontSize, overColor);
  }

  if (!profiler.IsGpuTimingAvailable()) {
    DrawText("GPU timer unavailable: displaying CPU-focused telemetry",
             static_cast<int>(x + 10.0f), static_cast<int>(y + kHeight - 20.0f),
             14, Color{255, 180, 120, 255});
  }
}

} // namespace NoMoreDay::render::debug
