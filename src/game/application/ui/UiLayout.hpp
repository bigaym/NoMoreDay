#pragma once

#include "game/application/ui/UiRuntimeTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace NoMoreDay::ui {

using UiNodeIndex = std::size_t;

inline constexpr UiNodeIndex kInvalidUiNodeIndex =
    std::numeric_limits<UiNodeIndex>::max();

enum class UiLayoutKind : std::uint8_t {
  Overlay,
  Row,
  Column,
  Anchor,
};

enum class UiLengthKind : std::uint8_t {
  Auto,
  Pixels,
  Fraction,
};

struct UiLength {
  UiLengthKind kind = UiLengthKind::Auto;
  float value = 0.0f;

  [[nodiscard]] static constexpr UiLength Auto() noexcept {
    return {};
  }

  [[nodiscard]] static constexpr UiLength Pixels(float pixels) noexcept {
    return {UiLengthKind::Pixels, pixels};
  }

  [[nodiscard]] static constexpr UiLength Fraction(float fraction) noexcept {
    return {UiLengthKind::Fraction, fraction};
  }
};

enum class UiAlignment : std::uint8_t {
  Start,
  Center,
  End,
  Stretch,
};

struct UiAnchor {
  bool left = false;
  bool top = false;
  bool right = false;
  bool bottom = false;
  float leftOffset = 0.0f;
  float topOffset = 0.0f;
  float rightOffset = 0.0f;
  float bottomOffset = 0.0f;
};

struct UiLayoutStyle {
  UiLayoutKind kind = UiLayoutKind::Overlay;
  UiLength width = UiLength::Auto();
  UiLength height = UiLength::Auto();
  UiVec2 minSize{};
  UiVec2 maxSize{std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max()};
  UiInsets margin{};
  UiInsets padding{};
  float gap = 0.0f;
  bool clipChildren = false;
  UiAlignment horizontalAlignment = UiAlignment::Start;
  UiAlignment verticalAlignment = UiAlignment::Start;
  UiAnchor anchor{};
};

struct UiNode {
  UiId id = kInvalidUiId;
  UiNodeIndex parent = kInvalidUiNodeIndex;
  UiLayoutStyle layout{};
  UiVec2 intrinsicSize{};
  UiVec2 measuredSize{};
  UiRect arrangedRect{};
  UiRect clipRect{};
  bool visible = true;
  bool hitTestVisible = false;
  bool capturePointer = false;
  bool focusable = false;
  bool captureKeyboard = false;
  bool acceptsText = false;
  bool modal = false;
  std::int32_t zIndex = 0;
  UiResourceId customPainter = kInvalidUiResourceId;
  std::vector<UiNodeIndex> children;
};

void MeasureUiNodes(std::vector<UiNode> &nodes, UiNodeIndex rootIndex);
void ArrangeUiNodes(std::vector<UiNode> &nodes, UiNodeIndex rootIndex,
                    UiRect rootRect);

} // namespace NoMoreDay::ui
