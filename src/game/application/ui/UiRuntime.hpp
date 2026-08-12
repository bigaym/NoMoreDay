#pragma once

#include "game/application/ui/UiLayout.hpp"
#include "game/application/ui/UiTooltipController.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace NoMoreDay::ui {

inline constexpr UiId kRootUiId = 1;

struct UiNodeDesc {
  UiId id = kInvalidUiId;
  UiId parent = kRootUiId;
  UiLayoutStyle layout{};
  UiVec2 intrinsicSize{};
  bool visible = true;
  bool hitTestVisible = false;
  bool capturePointer = false;
  bool focusable = false;
  bool captureKeyboard = false;
  bool acceptsText = false;
  bool modal = false;
  std::int32_t zIndex = 0;
  UiResourceId customPainter = kInvalidUiResourceId;
};

struct UiPointerInput {
  UiVec2 logicalPosition{};
  bool pressed = false;
  bool released = false;
  // R8: right-button edge (context menus; the hotbar/skill surfaces route
  // right-click through this instead of reading raylib during Update).
  bool pressedRight = false;
  // R8: sustained button state + wheel delta. Astrolabe camera pan/zoom and
  // the vow hold-to-confirm need these without touching raylib in Update.
  bool down = false;
  bool rightDown = false;
  float mouseWheel = 0.0f;
};

struct UiInputFrame {
  UiPointerInput pointer{};
  float deltaSeconds = 0.0f;
  UiId tooltipTarget = kInvalidUiId;
};

struct UiInputState {
  UiId hoveredNode = kInvalidUiId;
  UiId pressedNode = kInvalidUiId;
  UiId releasedNode = kInvalidUiId;
  UiId pointerCaptureNode = kInvalidUiId;
  UiId focusedNode = kInvalidUiId;
};

struct UiNodeSnapshot {
  UiId id = kInvalidUiId;
  UiId parent = kInvalidUiId;
  UiLayoutStyle layout{};
  UiVec2 intrinsicSize{};
  UiVec2 measuredSize{};
  UiRect arrangedRect{};
  UiRect clipRect{};
  bool visible = false;
  bool hitTestVisible = false;
  bool capturePointer = false;
  bool focusable = false;
  bool captureKeyboard = false;
  bool acceptsText = false;
  bool modal = false;
  std::int32_t zIndex = 0;
  UiResourceId customPainter = kInvalidUiResourceId;
};

class UiRuntime {
public:
  explicit UiRuntime(std::size_t nodeCapacity = 0);

  void Reserve(std::size_t nodeCapacity);
  void Reset();

  [[nodiscard]] bool CreateNode(const UiNodeDesc &desc);
  void SetRootLayout(const UiLayoutStyle &layout) noexcept;
  [[nodiscard]] bool SetNodeLayout(UiId id, const UiLayoutStyle &layout);
  [[nodiscard]] bool SetNodeIntrinsicSize(UiId id, UiVec2 intrinsicSize);
  [[nodiscard]] bool SetNodeVisible(UiId id, bool visible);
  [[nodiscard]] bool SetNodeModal(UiId id, bool modal);
  [[nodiscard]] std::optional<UiNodeSnapshot> GetNode(UiId id) const;
  [[nodiscard]] std::size_t NodeCount() const noexcept;

  void Arrange(UiRect rootRect);
  void UpdateInput(const UiInputFrame &input);

  [[nodiscard]] UiId HitTest(UiVec2 logicalPoint) const;
  [[nodiscard]] const UiInputCapture &InputCapture() const noexcept;
  [[nodiscard]] const UiInputState &InputState() const noexcept;
  [[nodiscard]] const UiTooltipController &Tooltip() const noexcept;
  void MarkTooltipInitialized() noexcept;

private:
  [[nodiscard]] UiNodeIndex FindNodeIndex(UiId id) const noexcept;
  [[nodiscard]] UiNodeSnapshot MakeSnapshot(UiNodeIndex index) const;
  [[nodiscard]] UiNodeIndex FindTopmostModalIndex() const noexcept;
  [[nodiscard]] bool IsEffectivelyVisible(UiNodeIndex nodeIndex) const noexcept;
  [[nodiscard]] bool IsWithinModal(UiNodeIndex nodeIndex,
                                   UiNodeIndex modalIndex) const noexcept;
  [[nodiscard]] UiId HitTest(UiVec2 logicalPoint,
                             UiNodeIndex modalIndex) const;
  void RefreshInputCapture();

  std::vector<UiNode> m_nodes;
  UiInputState m_inputState{};
  UiInputCapture m_inputCapture{};
  UiTooltipController m_tooltip{};
};

} // namespace NoMoreDay::ui
