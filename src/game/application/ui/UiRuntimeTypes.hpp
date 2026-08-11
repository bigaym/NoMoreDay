#pragma once

#include <cstdint>

namespace NoMoreDay::ui {

struct UiVec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct UiRect {
  UiVec2 origin;
  UiVec2 size;

  [[nodiscard]] constexpr float Left() const noexcept { return origin.x; }
  [[nodiscard]] constexpr float Top() const noexcept { return origin.y; }
  [[nodiscard]] constexpr float Right() const noexcept {
    return origin.x + size.x;
  }
  [[nodiscard]] constexpr float Bottom() const noexcept {
    return origin.y + size.y;
  }

  [[nodiscard]] constexpr bool Contains(UiVec2 point) const noexcept {
    return point.x >= Left() && point.x < Right() && point.y >= Top() &&
           point.y < Bottom();
  }

  [[nodiscard]] constexpr UiRect Intersection(UiRect other) const noexcept {
    const float left = Left() > other.Left() ? Left() : other.Left();
    const float top = Top() > other.Top() ? Top() : other.Top();
    const float right = Right() < other.Right() ? Right() : other.Right();
    const float bottom = Bottom() < other.Bottom() ? Bottom() : other.Bottom();
    return {{left, top}, {right > left ? right - left : 0.0f,
                          bottom > top ? bottom - top : 0.0f}};
  }
};

struct UiInsets {
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
};

using UiId = std::uint32_t;
using UiResourceId = std::uint32_t;

inline constexpr UiId kInvalidUiId = 0;
inline constexpr UiResourceId kInvalidUiResourceId = 0;

struct UiInputCapture {
  bool pointer = false;
  bool keyboard = false;
  bool text = false;
  bool modal = false;

  [[nodiscard]] constexpr bool CapturesAny() const noexcept {
    return pointer || keyboard || text || modal;
  }
};

} // namespace NoMoreDay::ui
