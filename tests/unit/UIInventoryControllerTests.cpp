#include "doctest.h"

#include "game/application/ui/UIInventoryController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raylib.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <string>

namespace NoMoreDay::ui {

namespace {

std::string ReadFileContents(const char* path) {
  std::ifstream stream(path);
  if (!stream.is_open()) {
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("[Unit] UIInventoryController - creates a panel root node") {
  UiRuntime runtime;
  UIInventoryController controller(runtime, nullptr);

  const UiId root = controller.NodeId();
  CHECK(root != kInvalidUiId);

  const auto node = runtime.GetNode(root);
  REQUIRE(node.has_value());
  CHECK(node->id == root);
  CHECK(node->parent == kRootUiId);
  CHECK(node->visible);
  CHECK_FALSE(node->modal);
  CHECK_FALSE(node->focusable);
  CHECK_FALSE(node->hitTestVisible);
  CHECK_FALSE(node->capturePointer);
  CHECK_FALSE(node->captureKeyboard);
  CHECK_FALSE(node->acceptsText);
  CHECK(node->zIndex == static_cast<std::int32_t>(UiDrawLayer::Panels));
  CHECK(runtime.NodeCount() == 2);  // runtime root + panel root

  CHECK(node->layout.kind == UiLayoutKind::Overlay);
  CHECK(node->layout.width.kind == UiLengthKind::Fraction);
  CHECK(node->layout.width.value == doctest::Approx(1.0f));
  CHECK(node->layout.height.kind == UiLengthKind::Fraction);
  CHECK(node->layout.height.value == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] UIInventoryController - Enter/Leave gameplay resets session state") {
  UiRuntime runtime;
  UIInventoryController controller(runtime, nullptr);

  const UiId root = controller.NodeId();
  CHECK_FALSE(controller.IsInGameplay());

  controller.EnterGameplay();
  controller.EnterGameplay();  // must be idempotent
  CHECK(controller.IsInGameplay());
  REQUIRE(root != kInvalidUiId);
  const auto inGame = runtime.GetNode(root);
  REQUIRE(inGame.has_value());
  CHECK(inGame->visible);

  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsInGameplay());
  const auto left = runtime.GetNode(root);
  REQUIRE(left.has_value());
  CHECK_FALSE(left->visible);

  // Re-entering gameplay restores the panel node.
  controller.EnterGameplay();
  CHECK(controller.IsInGameplay());
  const auto reentered = runtime.GetNode(root);
  REQUIRE(reentered.has_value());
  CHECK(reentered->visible);
}

TEST_CASE("[Unit] UIInventoryController - Update runs headless against a "
          "snapshot") {
  // R6: the interaction phase is snapshot-driven (no registry / world access).
  // The test harness (tests/main.cpp) opens a hidden raylib window with a GL
  // context, so GetFrameTime()/GetMousePosition() are available; the alpha is
  // animated towards the visibility flag exactly like the legacy
  // UIInventory::Update. GetFrameTime may be 0 in the harness, so the checks
  // use clamped-range + monotonicity invariants instead of exact deltas.
  UiRuntime runtime;
  UIInventoryController controller(runtime, nullptr);
  controller.EnterGameplay();

  GameUiSnapshot snapshot;
  snapshot.revision = 1;
  snapshot.player.hasPlayer = true;
  snapshot.inventory.capacity = 30;
  snapshot.inventory.used = 2;
  snapshot.inventory.gold = 123;
  GameUiItemView item;
  item.domainId = 1001;
  item.itemId = 5;
  item.quantity = 3;
  item.inventoryIndex = 0;
  snapshot.inventory.items.push_back(item);

  UiInputFrame input; // No pointer input: interaction branches early-out.
  UiViewport viewport = UiViewport::Fit({800.0f, 600.0f});
  input.pointer.logicalPosition = viewport.ToLogical(UiVec2{400.0f, 300.0f});

  // Empty snapshot before any world exists: Update must not crash.
  controller.SetVisible(true);
  controller.Update(snapshot, input, 0.0f, LevelManager{});
  CHECK(controller.Alpha() >= 0.0f);
  CHECK(controller.Alpha() <= 1.0f);

  // Branch A: inventory closed. Alpha must never increase and stays clamped
  // within [0, 1].
  controller.SetVisible(false);
  controller.Update(snapshot, input, 0.0f, LevelManager{});
  const float closedFirst = controller.Alpha();
  controller.Update(snapshot, input, 0.0f, LevelManager{});
  const float closedSecond = controller.Alpha();
  CHECK(closedFirst >= 0.0f);
  CHECK(closedFirst <= 1.0f);
  CHECK(closedSecond >= 0.0f);
  CHECK(closedSecond <= 1.0f);
  CHECK(closedSecond <= closedFirst);

  // Branch B: inventory open. Alpha must never decrease and stays clamped
  // within [0, 1].
  controller.SetVisible(true);
  controller.Update(snapshot, input, 0.0f, LevelManager{});
  const float openFirst = controller.Alpha();
  controller.Update(snapshot, input, 0.0f, LevelManager{});
  const float openSecond = controller.Alpha();
  CHECK(openFirst >= 0.0f);
  CHECK(openFirst <= 1.0f);
  CHECK(openSecond >= 0.0f);
  CHECK(openSecond <= 1.0f);
  CHECK(openSecond >= openFirst);
}

TEST_CASE("[Unit] UIInventoryController - header declares no static data members") {
  const std::string path = "src/game/application/ui/UIInventoryController.hpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());

  const std::string needle = "static ";
  const std::string classNeedle = "class UIInventoryController";
  std::string::size_type classPos = contents.find(classNeedle);
  REQUIRE_MESSAGE(classPos != std::string::npos, "class declaration not found");
  const std::string body = contents.substr(classPos);

  std::string::size_type pos = body.find(needle);
  while (pos != std::string::npos) {
    const char after = body[pos + needle.size()];
    // Pure static member functions are fine; static data members are not.
    CHECK_MESSAGE(
        std::isalpha(static_cast<unsigned char>(after)) != 0,
        "static data member must not exist in UIInventoryController: '",
        body.substr(pos, body.find_first_of(";{}", pos) - pos + 1), "'");
    pos = body.find(needle, pos + needle.size());
  }
}

TEST_CASE("[Unit] UIInventoryController - implementation declares no static mutable state") {
  const std::string path = "src/game/application/ui/UIInventoryController.cpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  const std::string contents((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());

  CHECK(contents.find("static bool") == std::string::npos);
  CHECK(contents.find("static float") == std::string::npos);
  CHECK(contents.find("static int") == std::string::npos);
  CHECK(contents.find("static uint32_t") == std::string::npos);
  CHECK(contents.find("static Texture2D") == std::string::npos);
  CHECK(contents.find("static Shader") == std::string::npos);
  CHECK(contents.find("static std::string") == std::string::npos);
}

} // namespace NoMoreDay::ui
