#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/application/input/InputSystem.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"

namespace {

// Builds a gameplay entity with non-zero input and runs InputSystem::update
// with the given capture. Returns the resulting input component.
InputComponent RunInputSystem(const NoMoreDay::ui::UiInputCapture &capture) {
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

TEST_CASE("[Unit] InputSystem - text capture clears gameplay actions") {
    NoMoreDay::ui::UiInputCapture capture;
    capture.text = true;

    const InputComponent input = RunInputSystem(capture);

    CHECK(input.moveX == 0.0f);
    CHECK(input.moveY == 0.0f);
    CHECK_FALSE(input.attack);
    CHECK_FALSE(input.dash);
}

TEST_CASE("[Unit] InputSystem - modal capture clears gameplay actions") {
    NoMoreDay::ui::UiInputCapture capture;
    capture.modal = true;

    const InputComponent input = RunInputSystem(capture);

    CHECK(input.moveX == 0.0f);
    CHECK(input.moveY == 0.0f);
    CHECK_FALSE(input.attack);
    CHECK_FALSE(input.dash);
}

TEST_CASE("[Unit] InputSystem - keyboard capture clears gameplay actions") {
    NoMoreDay::ui::UiInputCapture capture;
    capture.keyboard = true;

    const InputComponent input = RunInputSystem(capture);

    CHECK(input.moveX == 0.0f);
    CHECK(input.moveY == 0.0f);
    CHECK_FALSE(input.attack);
    CHECK_FALSE(input.dash);
}

TEST_CASE("[Unit] InputSystem - pointer capture keeps keyboard dash path") {
    NoMoreDay::ui::UiInputCapture capture;
    capture.pointer = true;

    const InputComponent input = RunInputSystem(capture);

    // Mouse-driven actions are blocked while the pointer is over UI...
    CHECK(input.moveX == 0.0f);
    CHECK(input.moveY == 0.0f);
    CHECK_FALSE(input.attack);
    // ...but the keyboard dash stays live (driven by raylib state in the
    // headless test environment, which reports no key pressed).
    CHECK(input.dash == IsKeyDown(KEY_LEFT_SHIFT));
}

TEST_CASE("[Unit] InputSystem - empty capture leaves input path intact") {
    const NoMoreDay::ui::UiInputCapture capture;

    const InputComponent input = RunInputSystem(capture);

    // With no UI capture the normal path runs: fields are reset each frame and
    // dash follows the raylib keyboard state. Headless tests report no input.
    CHECK(input.moveX == 0.0f);
    CHECK(input.moveY == 0.0f);
    CHECK_FALSE(input.attack);
    CHECK(input.dash == IsKeyDown(KEY_LEFT_SHIFT));
}
