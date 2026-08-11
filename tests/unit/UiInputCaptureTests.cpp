#include "game/application/ui/UiRuntime.hpp"
#include "game/application/input/InputSystem.hpp"
#include "game/foundation/components/Common.hpp"

#include "TestCommon.hpp"
#include "raylib.h"

#include <fstream>
#include <iterator>
#include <string>

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

// Builds a UiRuntime scenario, feeds it one input frame and returns the
// capture the runtime exposes. Mirrors UiRuntimeInputTests node patterns.
UiInputCapture CaptureForScenario(int scenario) {
  UiRuntime runtime;

  switch (scenario) {
  case 0: { // text focus: focusable + acceptsText field clicked
    UiNodeDesc textField = MakeInteractiveNode(2);
    textField.focusable = true;
    textField.captureKeyboard = true;
    textField.acceptsText = true;
    REQUIRE(runtime.CreateNode(textField));
    runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
    runtime.UpdateInput({{{10.0f, 10.0f}, true, false}});
    break;
  }
  case 1: { // modal: full-screen modal covering everything
    UiNodeDesc modal = MakeInteractiveNode(2);
    modal.layout.width = UiLength::Auto();
    modal.layout.height = UiLength::Auto();
    modal.layout.horizontalAlignment = UiAlignment::Stretch;
    modal.layout.verticalAlignment = UiAlignment::Stretch;
    modal.modal = true;
    modal.captureKeyboard = true;
    modal.acceptsText = true;
    REQUIRE(runtime.CreateNode(modal));
    runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
    runtime.UpdateInput({{{150.0f, 150.0f}, true, false}});
    break;
  }
  case 2: { // panel hover: pointer over a plain panel, no press
    REQUIRE(runtime.CreateNode(MakeInteractiveNode(2)));
    runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
    runtime.UpdateInput({{{10.0f, 10.0f}, false, false}});
    break;
  }
  case 3: { // drag capture: pointer press on a capturePointer node
    UiNodeDesc panel = MakeInteractiveNode(2);
    panel.capturePointer = true;
    REQUIRE(runtime.CreateNode(panel));
    runtime.Arrange({{0.0f, 0.0f}, {200.0f, 200.0f}});
    runtime.UpdateInput({{{10.0f, 10.0f}, true, false}});
    break;
  }
  case 4: // no UI focus: empty runtime
  default:
    break;
  }
  return runtime.InputCapture();
}

// Runs gameplay input through InputSystem with the given capture and returns
// the resulting InputComponent (POD, copied by value).
InputComponent RunGameplayInput(const UiInputCapture &capture) {
  entt::registry registry;
  const auto entity = registry.create();
  registry.emplace<PlayerTag>(entity);
  auto &input = registry.emplace<InputComponent>(entity);
  input.moveX = 1.0f;
  input.moveY = -1.0f;
  input.attack = true;
  input.dash = true;

  const Camera2D camera{};
  NoMoreDay::InputSystem::update(registry, camera, capture);
  return input;
}

} // namespace

TEST_CASE("[Unit] UI capture drives gameplay input gating") {
  struct ScenarioExpectation {
    int scenario;
    bool pointer;
    bool keyboard;
    bool text;
    bool modal;
    bool fullyBlocked; // modal/text/keyboard capture clears all gameplay input
    bool pointerOnly;  // pointer capture keeps keyboard dash, clears the rest
  };

  const ScenarioExpectation expectations[] = {
      {0, true, true, true, false, true, false},  // text focus
      {1, true, true, true, true, true, false},   // modal
      {2, true, false, false, false, false, true},// panel hover
      {3, true, false, false, false, false, true},// drag capture
      {4, false, false, false, false, false, false}, // no UI focus
  };

  for (const auto &e : expectations) {
    CAPTURE(e.scenario);

    const UiInputCapture capture = CaptureForScenario(e.scenario);
    CHECK(capture.pointer == e.pointer);
    CHECK(capture.keyboard == e.keyboard);
    CHECK(capture.text == e.text);
    CHECK(capture.modal == e.modal);

    const InputComponent input = RunGameplayInput(capture);
    if (e.fullyBlocked) {
      CHECK(input.moveX == 0.0f);
      CHECK(input.moveY == 0.0f);
      CHECK_FALSE(input.attack);
      CHECK_FALSE(input.dash);
    } else if (e.pointerOnly) {
      CHECK(input.moveX == 0.0f);
      CHECK(input.moveY == 0.0f);
      CHECK_FALSE(input.attack);
      // Keyboard dash stays live; headless raylib reports no key pressed.
      CHECK(input.dash == IsKeyDown(KEY_LEFT_SHIFT));
    } else {
      // No capture: the normal path runs (fields reset each frame, dash
      // follows raylib keyboard state).
      CHECK(input.moveX == 0.0f);
      CHECK(input.moveY == 0.0f);
      CHECK_FALSE(input.attack);
      CHECK(input.dash == IsKeyDown(KEY_LEFT_SHIFT));
    }
  }
}

TEST_CASE("[Unit] InputSystem - source no longer reads UI static state") {
  // U5 source guard: InputSystem must stay decoupled from the legacy UI
  // gate queries. Relative to the repository root (tests anchor their working
  // directory there).
  std::ifstream source("src/game/application/input/InputSystem.cpp");
  REQUIRE(source.is_open());
  const std::string contents{std::istreambuf_iterator<char>(source),
                             std::istreambuf_iterator<char>()};

  CHECK(contents.find("UISystem.hpp") == std::string::npos);
  CHECK(contents.find("UIAstrolabe.hpp") == std::string::npos);
  CHECK(contents.find("UISystem::State") == std::string::npos);
}

} // namespace NoMoreDay::ui
