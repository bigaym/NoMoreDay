#pragma once

#include "game/application/ui/UiRuntimeTypes.hpp"

namespace NoMoreDay::ui {

inline constexpr UiVec2 kDefaultUiLogicalSize{2560.0f, 1440.0f};

class UiViewport {
public:
  [[nodiscard]] static UiViewport Fit(
      UiVec2 pixelSize, UiVec2 logicalSize = kDefaultUiLogicalSize,
      UiInsets safeInsets = {}) noexcept;

  [[nodiscard]] bool IsValid() const noexcept;
  [[nodiscard]] float Scale() const noexcept;
  [[nodiscard]] UiVec2 PixelSize() const noexcept;
  [[nodiscard]] UiVec2 LogicalSize() const noexcept;
  [[nodiscard]] const UiRect &ContentRect() const noexcept;

  [[nodiscard]] bool ContainsPixel(UiVec2 pixelPoint) const noexcept;
  [[nodiscard]] UiVec2 ToLogical(UiVec2 pixelPoint) const noexcept;
  [[nodiscard]] UiVec2 ToPixel(UiVec2 logicalPoint) const noexcept;
  [[nodiscard]] UiRect ToPixel(UiRect logicalRect) const noexcept;

private:
  UiVec2 m_pixelSize{};
  UiVec2 m_logicalSize = kDefaultUiLogicalSize;
  UiRect m_contentRect{};
  float m_scale = 0.0f;
};

} // namespace NoMoreDay::ui
