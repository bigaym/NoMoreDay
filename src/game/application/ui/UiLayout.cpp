#include "game/application/ui/UiLayout.hpp"

#include <algorithm>

namespace NoMoreDay::ui {
namespace {

[[nodiscard]] float NonNegative(float value) noexcept {
  return std::max(0.0f, value);
}

[[nodiscard]] UiInsets NonNegative(UiInsets insets) noexcept {
  return {NonNegative(insets.left), NonNegative(insets.top),
          NonNegative(insets.right), NonNegative(insets.bottom)};
}

[[nodiscard]] UiRect Inset(UiRect rect, UiInsets insets) noexcept {
  insets = NonNegative(insets);
  const float width = NonNegative(rect.size.x);
  const float height = NonNegative(rect.size.y);
  const float left = std::clamp(insets.left, 0.0f, width);
  const float top = std::clamp(insets.top, 0.0f, height);
  const float right = std::clamp(insets.right, 0.0f, width - left);
  const float bottom = std::clamp(insets.bottom, 0.0f, height - top);
  return {{rect.origin.x + left, rect.origin.y + top},
          {width - left - right, height - top - bottom}};
}

[[nodiscard]] float ClampSize(float value, float minimum,
                              float maximum) noexcept {
  const float minSize = NonNegative(minimum);
  const float maxSize = std::max(minSize, NonNegative(maximum));
  return std::clamp(NonNegative(value), minSize, maxSize);
}

[[nodiscard]] float ResolveLength(UiLength length, float intrinsic,
                                  float available, bool stretch) noexcept {
  switch (length.kind) {
  case UiLengthKind::Pixels:
    return NonNegative(length.value);
  case UiLengthKind::Fraction:
    return NonNegative(length.value) * NonNegative(available);
  case UiLengthKind::Auto:
    return stretch ? NonNegative(available) : NonNegative(intrinsic);
  }

  return 0.0f;
}

[[nodiscard]] float ResolveDimension(UiLength length, float intrinsic,
                                     float available, bool stretch,
                                     float minimum,
                                     float maximum) noexcept {
  return ClampSize(ResolveLength(length, intrinsic, available, stretch),
                   minimum, maximum);
}

[[nodiscard]] float AlignmentOffset(float available, float size,
                                    UiAlignment alignment) noexcept {
  const float remainder = std::max(0.0f, available - size);
  switch (alignment) {
  case UiAlignment::Center:
    return remainder * 0.5f;
  case UiAlignment::End:
    return remainder;
  case UiAlignment::Start:
  case UiAlignment::Stretch:
    return 0.0f;
  }

  return 0.0f;
}

[[nodiscard]] bool IsValidNode(const std::vector<UiNode> &nodes,
                               UiNodeIndex index) noexcept {
  return index < nodes.size();
}

[[nodiscard]] float ResolveMeasuredDimension(UiLength length, float content,
                                             float minimum,
                                             float maximum) noexcept {
  const float requested = length.kind == UiLengthKind::Pixels
                              ? NonNegative(length.value)
                              : NonNegative(content);
  return ClampSize(requested, minimum, maximum);
}

[[nodiscard]] UiRect DescendantClip(const UiNode &parent) noexcept {
  if (!parent.layout.clipChildren) {
    return parent.clipRect;
  }
  return parent.clipRect.Intersection(
      Inset(parent.arrangedRect, parent.layout.padding));
}

void SetChildClip(const UiNode &parent, UiNode &child) noexcept {
  child.clipRect = DescendantClip(parent);
}

UiVec2 MeasureNode(std::vector<UiNode> &nodes, UiNodeIndex nodeIndex) {
  UiNode &node = nodes[nodeIndex];
  if (!node.visible) {
    node.measuredSize = {};
    return node.measuredSize;
  }

  UiVec2 content{NonNegative(node.intrinsicSize.x),
                 NonNegative(node.intrinsicSize.y)};
  float rowMain = 0.0f;
  float rowCross = 0.0f;
  float columnMain = 0.0f;
  float columnCross = 0.0f;
  std::size_t visibleCount = 0;

  for (const UiNodeIndex childIndex : node.children) {
    if (!IsValidNode(nodes, childIndex) || !nodes[childIndex].visible) {
      continue;
    }

    const UiVec2 childSize = MeasureNode(nodes, childIndex);
    const UiInsets margin = NonNegative(nodes[childIndex].layout.margin);
    rowMain += childSize.x + margin.left + margin.right;
    rowCross = std::max(rowCross, childSize.y + margin.top + margin.bottom);
    columnMain += childSize.y + margin.top + margin.bottom;
    columnCross =
        std::max(columnCross, childSize.x + margin.left + margin.right);
    ++visibleCount;
  }

  const UiInsets padding = NonNegative(node.layout.padding);
  const float gap = NonNegative(node.layout.gap) *
                    static_cast<float>(visibleCount > 0 ? visibleCount - 1 : 0);
  switch (node.layout.kind) {
  case UiLayoutKind::Row:
    content.x = std::max(content.x, rowMain + gap + padding.left + padding.right);
    content.y = std::max(content.y, rowCross + padding.top + padding.bottom);
    break;
  case UiLayoutKind::Column:
    content.x =
        std::max(content.x, columnCross + padding.left + padding.right);
    content.y = std::max(content.y, columnMain + gap + padding.top + padding.bottom);
    break;
  case UiLayoutKind::Overlay:
  case UiLayoutKind::Anchor:
    for (const UiNodeIndex childIndex : node.children) {
      if (!IsValidNode(nodes, childIndex) || !nodes[childIndex].visible) {
        continue;
      }

      const UiNode &child = nodes[childIndex];
      const UiInsets margin = NonNegative(child.layout.margin);
      const UiAnchor &anchor = child.layout.anchor;
      const float horizontalOffsets =
          (anchor.left ? NonNegative(anchor.leftOffset) : 0.0f) +
          (anchor.right ? NonNegative(anchor.rightOffset) : 0.0f);
      const float verticalOffsets =
          (anchor.top ? NonNegative(anchor.topOffset) : 0.0f) +
          (anchor.bottom ? NonNegative(anchor.bottomOffset) : 0.0f);
      content.x = std::max(content.x, child.measuredSize.x + margin.left +
                                          margin.right + horizontalOffsets +
                                          padding.left + padding.right);
      content.y = std::max(content.y, child.measuredSize.y + margin.top +
                                          margin.bottom + verticalOffsets +
                                          padding.top + padding.bottom);
    }
    break;
  }

  node.measuredSize = {
      ResolveMeasuredDimension(node.layout.width, content.x, node.layout.minSize.x,
                               node.layout.maxSize.x),
      ResolveMeasuredDimension(node.layout.height, content.y, node.layout.minSize.y,
                               node.layout.maxSize.y)};
  return node.measuredSize;
}

void ArrangeChildren(std::vector<UiNode> &nodes, UiNodeIndex parentIndex);

void ArrangeOverlayChild(UiNode &child, UiRect content) {
  const UiInsets margin = NonNegative(child.layout.margin);
  const UiRect available = Inset(content, margin);
  const UiAnchor &anchor = child.layout.anchor;

  const float horizontalOffsets =
      (anchor.left ? NonNegative(anchor.leftOffset) : 0.0f) +
      (anchor.right ? NonNegative(anchor.rightOffset) : 0.0f);
  const float verticalOffsets =
      (anchor.top ? NonNegative(anchor.topOffset) : 0.0f) +
      (anchor.bottom ? NonNegative(anchor.bottomOffset) : 0.0f);

  const float width = ClampSize(
      anchor.left && anchor.right
          ? std::max(0.0f, available.size.x - horizontalOffsets)
          : ResolveLength(child.layout.width, child.measuredSize.x,
                          available.size.x,
                          child.layout.horizontalAlignment == UiAlignment::Stretch),
      child.layout.minSize.x, child.layout.maxSize.x);
  const float height = ClampSize(
      anchor.top && anchor.bottom
          ? std::max(0.0f, available.size.y - verticalOffsets)
          : ResolveLength(child.layout.height, child.measuredSize.y,
                          available.size.y,
                          child.layout.verticalAlignment == UiAlignment::Stretch),
      child.layout.minSize.y, child.layout.maxSize.y);

  float x = available.origin.x +
            AlignmentOffset(available.size.x, width,
                            child.layout.horizontalAlignment);
  float y = available.origin.y +
            AlignmentOffset(available.size.y, height,
                            child.layout.verticalAlignment);
  if (anchor.left) {
    x = available.origin.x + NonNegative(anchor.leftOffset);
  } else if (anchor.right) {
    x = available.origin.x + available.size.x - NonNegative(anchor.rightOffset) -
        width;
  }
  if (anchor.top) {
    y = available.origin.y + NonNegative(anchor.topOffset);
  } else if (anchor.bottom) {
    y = available.origin.y + available.size.y - NonNegative(anchor.bottomOffset) -
        height;
  }

  child.arrangedRect = {{x, y}, {width, height}};
}

void ArrangeOverlay(std::vector<UiNode> &nodes, UiNodeIndex parentIndex) {
  const UiRect content = Inset(nodes[parentIndex].arrangedRect,
                               nodes[parentIndex].layout.padding);
  for (const UiNodeIndex childIndex : nodes[parentIndex].children) {
    if (!IsValidNode(nodes, childIndex) || !nodes[childIndex].visible) {
      continue;
    }

    ArrangeOverlayChild(nodes[childIndex], content);
    SetChildClip(nodes[parentIndex], nodes[childIndex]);
    ArrangeChildren(nodes, childIndex);
  }
}

void ArrangeLinear(std::vector<UiNode> &nodes, UiNodeIndex parentIndex,
                   bool horizontal) {
  const UiRect content = Inset(nodes[parentIndex].arrangedRect,
                               nodes[parentIndex].layout.padding);
  const float mainAvailable = horizontal ? content.size.x : content.size.y;
  const float crossAvailable = horizontal ? content.size.y : content.size.x;
  const float gap = NonNegative(nodes[parentIndex].layout.gap);

  float fixedMain = 0.0f;
  float totalFraction = 0.0f;
  std::size_t visibleCount = 0;
  for (const UiNodeIndex childIndex : nodes[parentIndex].children) {
    if (!IsValidNode(nodes, childIndex) || !nodes[childIndex].visible) {
      continue;
    }

    const UiNode &child = nodes[childIndex];
    const UiInsets margin = NonNegative(child.layout.margin);
    const UiLength length = horizontal ? child.layout.width : child.layout.height;
    const float intrinsic = horizontal ? child.measuredSize.x : child.measuredSize.y;
    const float minimum = horizontal ? child.layout.minSize.x : child.layout.minSize.y;
    const float maximum = horizontal ? child.layout.maxSize.x : child.layout.maxSize.y;
    const float marginMain =
        horizontal ? margin.left + margin.right : margin.top + margin.bottom;

    if (length.kind == UiLengthKind::Fraction) {
      totalFraction += NonNegative(length.value);
      fixedMain += marginMain;
    } else {
      fixedMain += ResolveDimension(length, intrinsic, mainAvailable, false,
                                    minimum, maximum) +
                   marginMain;
    }
    ++visibleCount;
  }

  if (visibleCount > 1) {
    fixedMain += gap * static_cast<float>(visibleCount - 1);
  }
  const float fractionSpace = std::max(0.0f, mainAvailable - fixedMain);
  float mainPosition = horizontal ? content.origin.x : content.origin.y;

  for (const UiNodeIndex childIndex : nodes[parentIndex].children) {
    if (!IsValidNode(nodes, childIndex) || !nodes[childIndex].visible) {
      continue;
    }

    UiNode &child = nodes[childIndex];
    const UiInsets margin = NonNegative(child.layout.margin);
    const UiLength mainLength = horizontal ? child.layout.width : child.layout.height;
    const UiLength crossLength = horizontal ? child.layout.height : child.layout.width;
    const float mainIntrinsic =
        horizontal ? child.measuredSize.x : child.measuredSize.y;
    const float crossIntrinsic =
        horizontal ? child.measuredSize.y : child.measuredSize.x;
    const float mainMinimum =
        horizontal ? child.layout.minSize.x : child.layout.minSize.y;
    const float mainMaximum =
        horizontal ? child.layout.maxSize.x : child.layout.maxSize.y;
    const float crossMinimum =
        horizontal ? child.layout.minSize.y : child.layout.minSize.x;
    const float crossMaximum =
        horizontal ? child.layout.maxSize.y : child.layout.maxSize.x;
    const UiAlignment crossAlignment = horizontal
                                           ? child.layout.verticalAlignment
                                           : child.layout.horizontalAlignment;
    const float marginMainStart = horizontal ? margin.left : margin.top;
    const float marginMainEnd = horizontal ? margin.right : margin.bottom;
    const float marginCrossStart = horizontal ? margin.top : margin.left;
    const float marginCrossEnd = horizontal ? margin.bottom : margin.right;
    const float childCrossAvailable =
        std::max(0.0f, crossAvailable - marginCrossStart - marginCrossEnd);

    const float mainSize = ClampSize(
        mainLength.kind == UiLengthKind::Fraction && totalFraction > 0.0f
            ? fractionSpace * NonNegative(mainLength.value) / totalFraction
            : ResolveLength(mainLength, mainIntrinsic, mainAvailable, false),
        mainMinimum, mainMaximum);
    const float crossSize = ResolveDimension(
        crossLength, crossIntrinsic, childCrossAvailable,
        crossAlignment == UiAlignment::Stretch, crossMinimum, crossMaximum);
    const float crossPosition =
        (horizontal ? content.origin.y : content.origin.x) + marginCrossStart +
        AlignmentOffset(childCrossAvailable, crossSize, crossAlignment);

    mainPosition += marginMainStart;
    child.arrangedRect = horizontal
                             ? UiRect{{mainPosition, crossPosition},
                                      {mainSize, crossSize}}
                             : UiRect{{crossPosition, mainPosition},
                                      {crossSize, mainSize}};
    SetChildClip(nodes[parentIndex], child);
    mainPosition += mainSize + marginMainEnd + gap;
    ArrangeChildren(nodes, childIndex);
  }
}

void ArrangeChildren(std::vector<UiNode> &nodes, UiNodeIndex parentIndex) {
  if (!IsValidNode(nodes, parentIndex) || !nodes[parentIndex].visible) {
    return;
  }

  switch (nodes[parentIndex].layout.kind) {
  case UiLayoutKind::Row:
    ArrangeLinear(nodes, parentIndex, true);
    break;
  case UiLayoutKind::Column:
    ArrangeLinear(nodes, parentIndex, false);
    break;
  case UiLayoutKind::Overlay:
  case UiLayoutKind::Anchor:
    ArrangeOverlay(nodes, parentIndex);
    break;
  }
}

} // namespace

void MeasureUiNodes(std::vector<UiNode> &nodes, UiNodeIndex rootIndex) {
  if (!IsValidNode(nodes, rootIndex)) {
    return;
  }

  MeasureNode(nodes, rootIndex);
}

void ArrangeUiNodes(std::vector<UiNode> &nodes, UiNodeIndex rootIndex,
                    UiRect rootRect) {
  if (!IsValidNode(nodes, rootIndex)) {
    return;
  }

  MeasureUiNodes(nodes, rootIndex);
  nodes[rootIndex].arrangedRect = rootRect;
  nodes[rootIndex].clipRect = rootRect;
  ArrangeChildren(nodes, rootIndex);
}

} // namespace NoMoreDay::ui
