#include "game/application/ui/UiRuntime.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace NoMoreDay::ui {
namespace {

[[nodiscard]] bool IsHigherInZOrder(const UiNode &candidate,
                                    const UiNode &current) noexcept {
  return candidate.zIndex > current.zIndex ||
         (candidate.zIndex == current.zIndex && candidate.id > current.id);
}

[[nodiscard]] bool HasPositiveArea(UiRect rect) noexcept {
  return rect.size.x > 0.0f && rect.size.y > 0.0f;
}

} // namespace

UiRuntime::UiRuntime(std::size_t nodeCapacity) {
  Reserve(nodeCapacity);
  Reset();
}

void UiRuntime::Reserve(std::size_t nodeCapacity) { m_nodes.reserve(nodeCapacity); }

void UiRuntime::Reset() {
  m_nodes.clear();
  UiNode root;
  root.id = kRootUiId;
  root.layout.kind = UiLayoutKind::Overlay;
  m_nodes.push_back(std::move(root));
  m_inputState = {};
  m_inputCapture = {};
  m_tooltip.Reset();
}

bool UiRuntime::CreateNode(const UiNodeDesc &desc) {
  if (desc.id == kInvalidUiId || desc.id == kRootUiId ||
      FindNodeIndex(desc.id) != kInvalidUiNodeIndex) {
    return false;
  }

  const UiNodeIndex parentIndex = FindNodeIndex(desc.parent);
  if (parentIndex == kInvalidUiNodeIndex) {
    return false;
  }

  UiNode node;
  node.id = desc.id;
  node.parent = parentIndex;
  node.layout = desc.layout;
  node.intrinsicSize = desc.intrinsicSize;
  node.visible = desc.visible;
  node.hitTestVisible = desc.hitTestVisible;
  node.capturePointer = desc.capturePointer;
  node.focusable = desc.focusable;
  node.captureKeyboard = desc.captureKeyboard;
  node.acceptsText = desc.acceptsText;
  node.modal = desc.modal;
  node.zIndex = desc.zIndex;
  node.customPainter = desc.customPainter;

  const UiNodeIndex nodeIndex = m_nodes.size();
  m_nodes.push_back(std::move(node));
  m_nodes[parentIndex].children.push_back(nodeIndex);
  return true;
}

void UiRuntime::SetRootLayout(const UiLayoutStyle &layout) noexcept {
  m_nodes.front().layout = layout;
}

bool UiRuntime::SetNodeLayout(UiId id, const UiLayoutStyle &layout) {
  const UiNodeIndex index = FindNodeIndex(id);
  if (index == kInvalidUiNodeIndex) {
    return false;
  }
  m_nodes[index].layout = layout;
  return true;
}

bool UiRuntime::SetNodeIntrinsicSize(UiId id, UiVec2 intrinsicSize) {
  const UiNodeIndex index = FindNodeIndex(id);
  if (index == kInvalidUiNodeIndex) {
    return false;
  }
  m_nodes[index].intrinsicSize = intrinsicSize;
  return true;
}

bool UiRuntime::SetNodeVisible(UiId id, bool visible) {
  const UiNodeIndex index = FindNodeIndex(id);
  if (index == kInvalidUiNodeIndex) {
    return false;
  }
  m_nodes[index].visible = visible;
  return true;
}

bool UiRuntime::SetNodeModal(UiId id, bool modal) {
  const UiNodeIndex index = FindNodeIndex(id);
  if (index == kInvalidUiNodeIndex) {
    return false;
  }
  m_nodes[index].modal = modal;
  return true;
}

std::optional<UiNodeSnapshot> UiRuntime::GetNode(UiId id) const {
  const UiNodeIndex index = FindNodeIndex(id);
  if (index == kInvalidUiNodeIndex) {
    return std::nullopt;
  }
  return MakeSnapshot(index);
}

std::size_t UiRuntime::NodeCount() const noexcept { return m_nodes.size(); }

void UiRuntime::Arrange(UiRect rootRect) {
  ArrangeUiNodes(m_nodes, 0, rootRect);
}

void UiRuntime::UpdateInput(const UiInputFrame &input) {
  m_inputState.pressedNode = kInvalidUiId;
  m_inputState.releasedNode = kInvalidUiId;

  const UiNodeIndex focusedIndex = FindNodeIndex(m_inputState.focusedNode);
  const UiNodeIndex modalIndex = FindTopmostModalIndex();
  if (focusedIndex == kInvalidUiNodeIndex ||
      !IsEffectivelyVisible(focusedIndex) ||
      !IsWithinModal(focusedIndex, modalIndex)) {
    m_inputState.focusedNode = kInvalidUiId;
  }
  const UiNodeIndex pointerCaptureIndex =
      FindNodeIndex(m_inputState.pointerCaptureNode);
  if (pointerCaptureIndex == kInvalidUiNodeIndex ||
      !IsEffectivelyVisible(pointerCaptureIndex) ||
      !IsWithinModal(pointerCaptureIndex, modalIndex)) {
    m_inputState.pointerCaptureNode = kInvalidUiId;
  }

  const UiId hitNode = HitTest(input.pointer.logicalPosition);
  const UiId routedNode = m_inputState.pointerCaptureNode != kInvalidUiId
                              ? m_inputState.pointerCaptureNode
                              : hitNode;
  m_inputState.hoveredNode = routedNode;

  if (input.pointer.pressed) {
    m_inputState.pressedNode = routedNode;
    const UiNodeIndex routedIndex = FindNodeIndex(routedNode);
    if (routedIndex != kInvalidUiNodeIndex) {
      const UiNode &node = m_nodes[routedIndex];
      if (node.capturePointer) {
        m_inputState.pointerCaptureNode = routedNode;
      }
      if (node.focusable) {
        m_inputState.focusedNode = routedNode;
      }
    } else if (modalIndex == kInvalidUiNodeIndex) {
      m_inputState.focusedNode = kInvalidUiId;
    }
  }

  if (input.pointer.released) {
    m_inputState.releasedNode = routedNode;
    m_inputState.pointerCaptureNode = kInvalidUiId;
    m_inputState.hoveredNode = hitNode;
  }

  RefreshInputCapture();
  m_tooltip.Update(input.tooltipTarget, input.deltaSeconds);
}

UiId UiRuntime::HitTest(UiVec2 logicalPoint) const {
  return HitTest(logicalPoint, FindTopmostModalIndex());
}

const UiInputCapture &UiRuntime::InputCapture() const noexcept {
  return m_inputCapture;
}

const UiInputState &UiRuntime::InputState() const noexcept {
  return m_inputState;
}

const UiTooltipController &UiRuntime::Tooltip() const noexcept {
  return m_tooltip;
}

void UiRuntime::MarkTooltipInitialized() noexcept {
  m_tooltip.MarkInitialized();
}

UiNodeIndex UiRuntime::FindNodeIndex(UiId id) const noexcept {
  for (UiNodeIndex index = 0; index < m_nodes.size(); ++index) {
    if (m_nodes[index].id == id) {
      return index;
    }
  }
  return kInvalidUiNodeIndex;
}

UiNodeSnapshot UiRuntime::MakeSnapshot(UiNodeIndex index) const {
  const UiNode &node = m_nodes[index];
  UiNodeSnapshot snapshot;
  snapshot.id = node.id;
  snapshot.parent = node.parent == kInvalidUiNodeIndex
                        ? kInvalidUiId
                        : m_nodes[node.parent].id;
  snapshot.layout = node.layout;
  snapshot.intrinsicSize = node.intrinsicSize;
  snapshot.measuredSize = node.measuredSize;
  snapshot.arrangedRect = node.arrangedRect;
  snapshot.clipRect = node.clipRect;
  snapshot.visible = node.visible;
  snapshot.hitTestVisible = node.hitTestVisible;
  snapshot.capturePointer = node.capturePointer;
  snapshot.focusable = node.focusable;
  snapshot.captureKeyboard = node.captureKeyboard;
  snapshot.acceptsText = node.acceptsText;
  snapshot.modal = node.modal;
  snapshot.zIndex = node.zIndex;
  snapshot.customPainter = node.customPainter;
  return snapshot;
}

UiNodeIndex UiRuntime::FindTopmostModalIndex() const noexcept {
  UiNodeIndex result = kInvalidUiNodeIndex;
  for (UiNodeIndex index = 1; index < m_nodes.size(); ++index) {
    const UiNode &node = m_nodes[index];
    if (!node.modal || !IsEffectivelyVisible(index) ||
        !HasPositiveArea(node.arrangedRect.Intersection(node.clipRect)) ||
        (result != kInvalidUiNodeIndex &&
         !IsHigherInZOrder(node, m_nodes[result]))) {
      continue;
    }
    result = index;
  }
  return result;
}

bool UiRuntime::IsEffectivelyVisible(UiNodeIndex nodeIndex) const noexcept {
  while (nodeIndex != kInvalidUiNodeIndex && nodeIndex < m_nodes.size()) {
    if (!m_nodes[nodeIndex].visible) {
      return false;
    }
    nodeIndex = m_nodes[nodeIndex].parent;
  }
  return true;
}

bool UiRuntime::IsWithinModal(UiNodeIndex nodeIndex,
                              UiNodeIndex modalIndex) const noexcept {
  if (modalIndex == kInvalidUiNodeIndex) {
    return true;
  }

  while (nodeIndex != kInvalidUiNodeIndex && nodeIndex < m_nodes.size()) {
    if (nodeIndex == modalIndex) {
      return true;
    }
    nodeIndex = m_nodes[nodeIndex].parent;
  }
  return false;
}

UiId UiRuntime::HitTest(UiVec2 logicalPoint,
                        UiNodeIndex modalIndex) const {
  UiNodeIndex result = kInvalidUiNodeIndex;
  for (UiNodeIndex index = 1; index < m_nodes.size(); ++index) {
    const UiNode &node = m_nodes[index];
    if (!node.hitTestVisible || !IsEffectivelyVisible(index) ||
        !node.arrangedRect.Contains(logicalPoint) ||
        !node.clipRect.Contains(logicalPoint) ||
        !IsWithinModal(index, modalIndex) ||
        (result != kInvalidUiNodeIndex &&
         !IsHigherInZOrder(node, m_nodes[result]))) {
      continue;
    }
    result = index;
  }
  return result == kInvalidUiNodeIndex ? kInvalidUiId : m_nodes[result].id;
}

void UiRuntime::RefreshInputCapture() {
  m_inputCapture = {};
  const UiNodeIndex modalIndex = FindTopmostModalIndex();
  const UiNodeIndex focusedIndex = FindNodeIndex(m_inputState.focusedNode);
  const UiNode *focusedNode = focusedIndex == kInvalidUiNodeIndex
                                  ? nullptr
                                  : &m_nodes[focusedIndex];

  m_inputCapture.modal = modalIndex != kInvalidUiNodeIndex;
  m_inputCapture.pointer =
      m_inputCapture.modal || m_inputState.hoveredNode != kInvalidUiId ||
      m_inputState.pointerCaptureNode != kInvalidUiId;
  m_inputCapture.keyboard =
      (focusedNode != nullptr && focusedNode->captureKeyboard) ||
      (modalIndex != kInvalidUiNodeIndex &&
       m_nodes[modalIndex].captureKeyboard);
  m_inputCapture.text =
      (focusedNode != nullptr && focusedNode->acceptsText) ||
      (modalIndex != kInvalidUiNodeIndex && m_nodes[modalIndex].acceptsText);
}

} // namespace NoMoreDay::ui
