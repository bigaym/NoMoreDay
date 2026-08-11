#include "game/application/ui/UiViewport.hpp"

#include <algorithm>

namespace NoMoreDay::ui {

UiViewport UiViewport::Fit(UiVec2 pixelSize, UiVec2 logicalSize,
                           UiInsets safeInsets) noexcept {
  UiViewport viewport;
  viewport.m_pixelSize = {std::max(0.0f, pixelSize.x),
                          std::max(0.0f, pixelSize.y)};
  viewport.m_logicalSize = logicalSize;

  const float left = std::clamp(safeInsets.left, 0.0f,
                                viewport.m_pixelSize.x);
  const float top =
      std::clamp(safeInsets.top, 0.0f, viewport.m_pixelSize.y);
  const float right = std::clamp(safeInsets.right, 0.0f,
                                 viewport.m_pixelSize.x - left);
  const float bottom = std::clamp(safeInsets.bottom, 0.0f,
                                  viewport.m_pixelSize.y - top);
  const float usableWidth = viewport.m_pixelSize.x - left - right;
  const float usableHeight = viewport.m_pixelSize.y - top - bottom;

  viewport.m_contentRect.origin = {left + usableWidth * 0.5f,
                                   top + usableHeight * 0.5f};

  if (logicalSize.x <= 0.0f || logicalSize.y <= 0.0f ||
      usableWidth <= 0.0f || usableHeight <= 0.0f) {
    return viewport;
  }

  viewport.m_scale = std::min(usableWidth / logicalSize.x,
                              usableHeight / logicalSize.y);
  viewport.m_contentRect.size = {logicalSize.x * viewport.m_scale,
                                 logicalSize.y * viewport.m_scale};
  viewport.m_contentRect.origin = {
      left + (usableWidth - viewport.m_contentRect.size.x) * 0.5f,
      top + (usableHeight - viewport.m_contentRect.size.y) * 0.5f};
  return viewport;
}

bool UiViewport::IsValid() const noexcept { return m_scale > 0.0f; }

float UiViewport::Scale() const noexcept { return m_scale; }

UiVec2 UiViewport::PixelSize() const noexcept { return m_pixelSize; }

UiVec2 UiViewport::LogicalSize() const noexcept { return m_logicalSize; }

const UiRect &UiViewport::ContentRect() const noexcept {
  return m_contentRect;
}

bool UiViewport::ContainsPixel(UiVec2 pixelPoint) const noexcept {
  return IsValid() && m_contentRect.Contains(pixelPoint);
}

UiVec2 UiViewport::ToLogical(UiVec2 pixelPoint) const noexcept {
  if (!IsValid()) {
    return {};
  }

  return {(pixelPoint.x - m_contentRect.origin.x) / m_scale,
          (pixelPoint.y - m_contentRect.origin.y) / m_scale};
}

UiVec2 UiViewport::ToPixel(UiVec2 logicalPoint) const noexcept {
  return {m_contentRect.origin.x + logicalPoint.x * m_scale,
          m_contentRect.origin.y + logicalPoint.y * m_scale};
}

UiRect UiViewport::ToPixel(UiRect logicalRect) const noexcept {
  return {ToPixel(logicalRect.origin),
          {logicalRect.size.x * m_scale, logicalRect.size.y * m_scale}};
}

} // namespace NoMoreDay::ui
