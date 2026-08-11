#include "game/application/ui/UiRuntime.hpp"

#include "doctest.h"

namespace NoMoreDay::ui {
namespace {

UiNodeDesc MakeInteractiveNode(UiId id) {
  UiNodeDesc desc;
  desc.id = id;
  desc.layout.width = UiLength::Pixels(100.0f);
  desc.layout.height = UiLength::Pixels(100.0f);
  desc.hitTestVisible = true;
  return desc;
}

} // namespace

TEST_CASE("[Unit] UI runtime hit tests topmost node and retains pointer capture") {
  UiRuntime runtime;

  UiNodeDesc background = MakeInteractiveNode(2);
  background.capturePointer = true;
  CHECK(runtime.CreateNode(background));

  UiNodeDesc foreground = MakeInteractiveNode(3);
  foreground.capturePointer = true;
  foreground.zIndex = 1;
  CHECK(runtime.CreateNode(foreground));

  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
  CHECK(runtime.HitTest({10.0f, 10.0f}) == 3);

  runtime.UpdateInput({{{10.0f, 10.0f}, true, false}});
  CHECK(runtime.InputState().pressedNode == 3);
  CHECK(runtime.InputState().pointerCaptureNode == 3);
  CHECK(runtime.InputCapture().pointer);

  runtime.UpdateInput({{{150.0f, 150.0f}, false, true}});
  CHECK(runtime.InputState().releasedNode == 3);
  CHECK(runtime.InputState().pointerCaptureNode == kInvalidUiId);
  CHECK(runtime.InputState().hoveredNode == kInvalidUiId);
}

TEST_CASE("[Unit] UI runtime modal blocks background and reports capture") {
  UiRuntime runtime;

  UiNodeDesc background = MakeInteractiveNode(2);
  background.capturePointer = true;
  background.focusable = true;
  background.captureKeyboard = true;
  CHECK(runtime.CreateNode(background));

  UiNodeDesc modal = MakeInteractiveNode(3);
  modal.layout.width = UiLength::Auto();
  modal.layout.height = UiLength::Auto();
  modal.layout.horizontalAlignment = UiAlignment::Stretch;
  modal.layout.verticalAlignment = UiAlignment::Stretch;
  modal.modal = true;
  modal.captureKeyboard = true;
  modal.acceptsText = true;
  modal.zIndex = 5;
  CHECK(runtime.CreateNode(modal));

  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
  CHECK(runtime.HitTest({150.0f, 150.0f}) == 3);

  runtime.UpdateInput({{{150.0f, 150.0f}, true, false}});
  CHECK(runtime.InputState().pressedNode == 3);
  CHECK(runtime.InputCapture().modal);
  CHECK(runtime.InputCapture().pointer);
  CHECK(runtime.InputCapture().keyboard);
  CHECK(runtime.InputCapture().text);
}

TEST_CASE("[Unit] UI runtime focus exposes keyboard and text ownership") {
  UiRuntime runtime;

  UiNodeDesc textField = MakeInteractiveNode(2);
  textField.focusable = true;
  textField.captureKeyboard = true;
  textField.acceptsText = true;
  CHECK(runtime.CreateNode(textField));

  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
  runtime.UpdateInput({{{10.0f, 10.0f}, true, false}});

  CHECK(runtime.InputState().focusedNode == 2);
  CHECK(runtime.InputCapture().keyboard);
  CHECK(runtime.InputCapture().text);
}

TEST_CASE("[Unit] UI runtime ignores children of hidden parents") {
  UiRuntime runtime;

  UiNodeDesc parent = MakeInteractiveNode(2);
  parent.layout.width = UiLength::Pixels(200.0f);
  parent.layout.height = UiLength::Pixels(200.0f);
  CHECK(runtime.CreateNode(parent));

  UiNodeDesc child = MakeInteractiveNode(3);
  child.parent = 2;
  child.capturePointer = true;
  CHECK(runtime.CreateNode(child));

  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
  CHECK(runtime.HitTest({10.0f, 10.0f}) == 3);

  CHECK(runtime.SetNodeVisible(2, false));
  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
  CHECK(runtime.HitTest({10.0f, 10.0f}) == kInvalidUiId);
}

TEST_CASE("[Unit] UI runtime clips descendants to clipping parents") {
  UiRuntime runtime;

  UiNodeDesc container = MakeInteractiveNode(2);
  container.layout.width = UiLength::Pixels(50.0f);
  container.layout.height = UiLength::Pixels(50.0f);
  container.layout.clipChildren = true;
  CHECK(runtime.CreateNode(container));

  UiNodeDesc child = MakeInteractiveNode(3);
  child.parent = 2;
  CHECK(runtime.CreateNode(child));

  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
  CHECK(runtime.HitTest({25.0f, 25.0f}) == 3);
  CHECK(runtime.HitTest({75.0f, 25.0f}) == kInvalidUiId);
}

TEST_CASE("[Unit] UI runtime applies nested clip rectangles to hit testing") {
  UiRuntime runtime;

  UiNodeDesc outer = MakeInteractiveNode(2);
  outer.layout.clipChildren = true;
  CHECK(runtime.CreateNode(outer));

  UiNodeDesc inner = MakeInteractiveNode(3);
  inner.parent = 2;
  inner.layout.clipChildren = true;
  CHECK(runtime.CreateNode(inner));

  UiNodeDesc overflowingChild = MakeInteractiveNode(4);
  overflowingChild.parent = 3;
  overflowingChild.layout.width = UiLength::Pixels(200.0f);
  overflowingChild.layout.height = UiLength::Pixels(100.0f);
  CHECK(runtime.CreateNode(overflowingChild));

  runtime.Arrange({{0.0f, 0.0f}, {300.0f, 200.0f}});
  CHECK(runtime.HitTest({50.0f, 50.0f}) == 4);
  CHECK(runtime.HitTest({150.0f, 50.0f}) == kInvalidUiId);
}

TEST_CASE("[Unit] UI runtime uses UiId as its stable equal-z tie break") {
  UiRuntime runtime;

  UiNodeDesc lowerId = MakeInteractiveNode(20);
  UiNodeDesc higherId = MakeInteractiveNode(30);
  CHECK(runtime.CreateNode(lowerId));
  CHECK(runtime.CreateNode(higherId));

  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
  CHECK(runtime.HitTest({10.0f, 10.0f}) == 30);
}

TEST_CASE("[Unit] UI runtime cancels background capture when a modal opens") {
  UiRuntime runtime;

  UiNodeDesc background = MakeInteractiveNode(2);
  background.capturePointer = true;
  background.focusable = true;
  background.captureKeyboard = true;
  background.acceptsText = true;
  CHECK(runtime.CreateNode(background));

  UiNodeDesc modal = MakeInteractiveNode(3);
  modal.layout.width = UiLength::Auto();
  modal.layout.height = UiLength::Auto();
  modal.layout.horizontalAlignment = UiAlignment::Stretch;
  modal.layout.verticalAlignment = UiAlignment::Stretch;
  modal.visible = false;
  modal.modal = true;
  modal.zIndex = 1;
  CHECK(runtime.CreateNode(modal));

  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
  runtime.UpdateInput({{{10.0f, 10.0f}, true, false}});
  CHECK(runtime.InputState().pointerCaptureNode == 2);
  CHECK(runtime.InputState().focusedNode == 2);

  CHECK(runtime.SetNodeVisible(3, true));
  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
  runtime.UpdateInput({{{150.0f, 150.0f}, true, false}});
  CHECK(runtime.InputState().pressedNode == 3);
  CHECK(runtime.InputState().pointerCaptureNode == kInvalidUiId);
  CHECK(runtime.InputState().focusedNode == kInvalidUiId);
  CHECK(runtime.InputCapture().modal);
  CHECK_FALSE(runtime.InputCapture().keyboard);
  CHECK_FALSE(runtime.InputCapture().text);

  CHECK(runtime.SetNodeVisible(3, false));
  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
  runtime.UpdateInput({{{10.0f, 10.0f}, true, false}});
  CHECK(runtime.InputState().pressedNode == 2);
  CHECK(runtime.InputState().pointerCaptureNode == 2);
  CHECK(runtime.InputState().focusedNode == 2);
}

TEST_CASE("[Unit] UI runtime ignores modals with an empty effective clip") {
  UiRuntime runtime;

  UiNodeDesc background = MakeInteractiveNode(2);
  CHECK(runtime.CreateNode(background));

  UiNodeDesc clippingParent = MakeInteractiveNode(3);
  clippingParent.layout.width = UiLength::Pixels(0.0f);
  clippingParent.layout.height = UiLength::Pixels(0.0f);
  clippingParent.layout.clipChildren = true;
  clippingParent.hitTestVisible = false;
  CHECK(runtime.CreateNode(clippingParent));

  UiNodeDesc clippedModal = MakeInteractiveNode(4);
  clippedModal.parent = 3;
  clippedModal.modal = true;
  CHECK(runtime.CreateNode(clippedModal));

  runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
  runtime.UpdateInput({{{10.0f, 10.0f}, true, false}});

  CHECK(runtime.HitTest({10.0f, 10.0f}) == 2);
  CHECK_FALSE(runtime.InputCapture().modal);
  CHECK(runtime.InputState().pressedNode == 2);
}

TEST_CASE("[Unit] UI runtime advances its tooltip controller once per frame") {
  UiRuntime runtime;

  UiInputFrame firstFrame;
  firstFrame.deltaSeconds = 0.12f;
  firstFrame.tooltipTarget = 42;
  runtime.UpdateInput(firstFrame);
  CHECK(runtime.Tooltip().State().activeTarget == 42);
  CHECK(runtime.Tooltip().State().alpha == doctest::Approx(0.0f));

  UiInputFrame secondFrame;
  secondFrame.deltaSeconds = 0.1f;
  secondFrame.tooltipTarget = 42;
  runtime.UpdateInput(secondFrame);
  CHECK(runtime.Tooltip().State().alpha == doctest::Approx(1.0f));
}

} // namespace NoMoreDay::ui
