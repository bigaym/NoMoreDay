#include "doctest.h"

#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/world/LevelManager.hpp"

#include <entt/entt.hpp>
#include <string>

namespace NoMoreDay {
namespace {

const ui::UiId kOverlayNodeId = entt::hashed_string("ui_overlay");

} // namespace

// R4 (remediation, design §3.1/§3.4): the message box is the first real panel
// that paints through the new host pipeline end to end. These tests drive the
// production call sequence (Initialize -> ShowMessageBox -> Update ->
// PrepareRender -> Draw) and assert that (a) the draw list carries real Image +
// Text commands under the overlay runtime node (no immediate raylib fallback),
// (b) the runtime reconcile/input/layout steps actually run, and (c) the
// host-owned capacities are reserved at Initialize time.

TEST_CASE("[Unit] GameUiHost R4 - message box paints real commands into the "
          "draw list") {
  ResourceManager resourceManager;
  ui::GameUiHost host;
  host.Initialize(resourceManager);
  REQUIRE(host.IsInitialized());

  host.ShowMessageBox("背包已满");
  host.PrepareRender();

  ui::UiDrawList &list = host.DrawList();
  REQUIRE_FALSE(list.IsEmpty());
  CHECK(list.IsFinalized());
  CHECK(list.CommandOverflow() == 0);
  CHECK(list.TextOverflow() == 0);
  CHECK(list.ClipOverflow() == 0);

  const auto &commands = list.Commands();
  REQUIRE(commands.size() == 2);  // frame Image + label Text

  const ui::UiDrawCommand &frame = commands[0];
  CHECK(frame.kind == ui::UiDrawKind::Image);
  CHECK(frame.layer == ui::UiDrawLayer::Modal);
  CHECK(frame.nodeId == kOverlayNodeId);
  CHECK(frame.resourceId == ui::kMessageBoxTextureResourceId);

  const ui::UiDrawCommand &label = commands[1];
  CHECK(label.kind == ui::UiDrawKind::Text);
  CHECK(label.layer == ui::UiDrawLayer::Modal);
  CHECK(label.nodeId == kOverlayNodeId);
  CHECK(label.resourceId == ui::kGlobalFontResourceId);
  CHECK(std::string(list.TextAt(label)) == "背包已满");

  host.Shutdown();
}

TEST_CASE("[Unit] GameUiHost R4 - no message box leaves the finalized list "
          "empty") {
  ResourceManager resourceManager;
  ui::GameUiHost host;
  host.Initialize(resourceManager);
  REQUIRE(host.IsInitialized());

  host.PrepareRender();
  CHECK(host.DrawList().IsEmpty());
  CHECK(host.DrawList().IsFinalized());

  host.Shutdown();
}

TEST_CASE("[Unit] GameUiHost R4 - Update runs reconcile input and layout on "
          "the runtime") {
  ResourceManager resourceManager;
  ui::GameUiHost host;
  host.Initialize(resourceManager);
  REQUIRE(host.IsInitialized());

  entt::registry registry;
  LevelManager levelManager;
  levelManager.initialize(resourceManager, registry);
  levelManager.loadNewLevel(NoMoreDay::BiomeID::Town, 64, 64);

  // No message box: reconcile keeps the overlay root hidden.
  host.Update(registry, levelManager);
  {
    const auto node = host.Runtime().GetNode(kOverlayNodeId);
    REQUIRE(node.has_value());
    CHECK_FALSE(node->visible);
  }

  // Message box up: reconcile makes the overlay root visible and the layout
  // step arranged it (full-screen Fraction(1,1) node over the logical
  // viewport; the test window is 100x100 so the logical size stays 2560x1440).
  host.ShowMessageBox("测试");
  host.Update(registry, levelManager);
  {
    const auto node = host.Runtime().GetNode(kOverlayNodeId);
    REQUIRE(node.has_value());
    CHECK(node->visible);
    CHECK(node->arrangedRect.origin.x == doctest::Approx(0.0f));
    CHECK(node->arrangedRect.origin.y == doctest::Approx(0.0f));
    CHECK(node->arrangedRect.size.x == doctest::Approx(2560.0f));
    CHECK(node->arrangedRect.size.y == doctest::Approx(1440.0f));
  }

  // Dismiss: reconcile hides it again.
  host.ClearMessageBox();
  host.Update(registry, levelManager);
  {
    const auto node = host.Runtime().GetNode(kOverlayNodeId);
    REQUIRE(node.has_value());
    CHECK_FALSE(node->visible);
  }

  host.Shutdown();
}

TEST_CASE("[Unit] GameUiHost R4 - draw list capacities are reserved and real "
          "resources registered at initialize") {
  ResourceManager resourceManager;
  ui::GameUiHost host;
  host.Initialize(resourceManager);
  REQUIRE(host.IsInitialized());

  CHECK(host.DrawList().CommandCapacity() >= 256);
  CHECK(host.DrawList().TextCapacity() >= 4096);
  // The backend holds the real resources under the shared ids the controllers
  // reference (raylib types never cross into the controller layer).
  CHECK(host.Backend().IsRegistered(ui::kGlobalFontResourceId));
  CHECK(host.Backend().IsRegistered(ui::kMessageBoxTextureResourceId));

  host.Shutdown();
}

TEST_CASE("[Tech] GameUiHost R4 - message box renders end to end through the "
          "real pipeline") {
  ResourceManager resourceManager;
  ui::GameUiHost host;
  host.Initialize(resourceManager);
  REQUIRE(host.IsInitialized());

  entt::registry registry;
  LevelManager levelManager;
  levelManager.initialize(resourceManager, registry);
  levelManager.loadNewLevel(NoMoreDay::BiomeID::Town, 64, 64);
  Camera2D camera{};
  systems::SpatialHashGrid spatialGrid(100, 100, 50);

  host.EnterGameplay();
  // ASCII text in the rendering path (the test font lacks CJK glyphs; the
  // UTF-8 arena round-trip is covered by the non-rendering paint test above).
  host.ShowMessageBox("Bag full");

  // Mirror GameplayState::OnUpdate/OnRender: Update (reconcile/input/layout),
  // PrepareRender (paint + finalize), Draw (submit) inside the GL window.
  host.Update(registry, levelManager);
  host.PrepareRender();
  BeginDrawing();
  // R8: the registry parameter is gone from Draw (snapshot/intent surfaces).
  host.Draw(levelManager, camera, &spatialGrid);
  EndDrawing();

  // The commands that were submitted are the real message box paint commands.
  REQUIRE_FALSE(host.DrawList().IsEmpty());
  const auto &commands = host.DrawList().Commands();
  REQUIRE(commands.size() == 2);
  CHECK(commands[0].kind == ui::UiDrawKind::Image);
  CHECK(commands[1].kind == ui::UiDrawKind::Text);
  CHECK(std::string(host.DrawList().TextAt(commands[1])) == "Bag full");
  CHECK(host.DrawList().CommandOverflow() == 0);

  host.LeaveGameplay();
  host.Shutdown();
}

} // namespace NoMoreDay
