#include "doctest.h"

#include "game/application/ui/SkillHotbarController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/foundation/components/Buff.hpp"

#include "raylib.h"

#include <fstream>
#include <iterator>
#include <string>

using namespace NoMoreDay;

namespace {

ui::UiInputFrame MakeInput() {
  ui::UiInputFrame input;
  input.deltaSeconds = 1.0f / 60.0f;
  input.tooltipTarget = ui::kInvalidUiId;
  return input;
}

std::string ReadFileContents(const char* path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("[Unit] SkillHotbarController - creates a display-only hotbar root node") {
  ui::UiRuntime runtime;
  ui::SkillHotbarController controller(runtime);

  const ui::UiId root = controller.NodeId();
  CHECK(root != ui::kInvalidUiId);

  const auto node = runtime.GetNode(root);
  REQUIRE(node.has_value());
  CHECK(node->id == root);
  CHECK(node->parent == ui::kRootUiId);
  CHECK(node->visible);
  CHECK_FALSE(node->modal);
  CHECK_FALSE(node->focusable);
  CHECK_FALSE(node->hitTestVisible);
  CHECK_FALSE(node->capturePointer);
  CHECK_FALSE(node->captureKeyboard);
  CHECK_FALSE(node->acceptsText);
  CHECK(node->zIndex == static_cast<std::int32_t>(ui::UiDrawLayer::Hud));
  CHECK(runtime.NodeCount() == 2); // runtime root + hotbar root

  // Full-viewport declarative anchor.
  CHECK(node->layout.kind == ui::UiLayoutKind::Overlay);
  CHECK(node->layout.width.kind == ui::UiLengthKind::Fraction);
  CHECK(node->layout.width.value == doctest::Approx(1.0f));
  CHECK(node->layout.height.kind == ui::UiLengthKind::Fraction);
  CHECK(node->layout.height.value == doctest::Approx(1.0f));
}

TEST_CASE("[Unit] SkillHotbarController - SetVisible mirrors into the runtime node") {
  ui::UiRuntime runtime;
  ui::SkillHotbarController controller(runtime);
  CHECK(controller.IsVisible());

  controller.SetVisible(false);
  CHECK_FALSE(controller.IsVisible());
  const auto hidden = runtime.GetNode(controller.NodeId());
  REQUIRE(hidden.has_value());
  CHECK_FALSE(hidden->visible);

  controller.SetVisible(true);
  CHECK(controller.IsVisible());
  const auto shown = runtime.GetNode(controller.NodeId());
  REQUIRE(shown.has_value());
  CHECK(shown->visible);
}

TEST_CASE("[Unit] SkillHotbarController - Enter/Leave gameplay resets session state") {
  ui::UiRuntime runtime;
  ui::SkillHotbarController controller(runtime);

  controller.EnterGameplay();
  controller.EnterGameplay(); // must be idempotent
  CHECK(controller.IsInGameplay());
  const auto inGame = runtime.GetNode(controller.NodeId());
  REQUIRE(inGame.has_value());
  CHECK(inGame->visible);

  controller.LeaveGameplay();
  CHECK_FALSE(controller.IsInGameplay());
  CHECK_FALSE(controller.HasPlayerData());
  const auto left = runtime.GetNode(controller.NodeId());
  REQUIRE(left.has_value());
  CHECK_FALSE(left->visible);

  // Re-entering gameplay restores the hotbar node.
  controller.EnterGameplay();
  CHECK(controller.IsInGameplay());
  const auto reentered = runtime.GetNode(controller.NodeId());
  REQUIRE(reentered.has_value());
  CHECK(reentered->visible);
}

TEST_CASE("[Unit] SkillHotbarController - Update handles empty and player snapshots") {
  ui::UiRuntime runtime;
  ui::SkillHotbarController controller(runtime);

  entt::registry registry;
  (void)registry;
  const ui::UiVec2 mouse{0.0f, 0.0f};
  (void)mouse;

  ui::GameUiSnapshot emptySnapshot;
  controller.Update(emptySnapshot, MakeInput());
  CHECK_FALSE(controller.HasPlayerData());

  ui::GameUiSnapshot snapshot;
  snapshot.revision = 1;
  snapshot.player.hasPlayer = true;
  snapshot.player.mana = 100.0f;
  snapshot.player.maxMana = 100.0f;
  controller.Update(snapshot, MakeInput());
  CHECK(controller.HasPlayerData());
}

TEST_CASE("[Unit] SkillHotbarController - Paint executes headless without crashing") {
  // R5 adaptation: the controller no longer draws from the registry; it
  // caches slot/buff display data from a fixed snapshot (Update) and emits
  // draw-list commands (Paint). The test drives the new contract directly.
  ui::UiRuntime runtime;
  ui::SkillHotbarController controller(runtime);

  ui::UiDrawList drawList;
  const ui::UiViewport viewport = ui::UiViewport::Fit({2560, 1440});

  // Empty snapshot: the hotbar early-outs on the player data.
  ui::GameUiSnapshot emptySnapshot;
  controller.Update(emptySnapshot, MakeInput());
  controller.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() == 0);

  // Path A: player without skills (empty hotbar slots + no effects).
  ui::GameUiSnapshot snapshot;
  snapshot.revision = 1;
  snapshot.player.hasPlayer = true;
  snapshot.player.mana = 100.0f;
  snapshot.player.maxMana = 100.0f;
  controller.Update(snapshot, MakeInput());
  controller.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() > 0);
  drawList.Clear();

  // Path B: two mapped slots + one buff and one debuff.
  snapshot.skillBar.slots.resize(2);
  snapshot.skillBar.slots[0].skillId = 999999; // unmapped: iconId stays 0
  snapshot.skillBar.slots[0].slotIndex = 0;
  snapshot.skillBar.slots[0].cooldown = 0.0f;
  snapshot.skillBar.slots[0].cooldownMax = 10.0f;
  snapshot.skillBar.slots[1].skillId = 0; // Basic Attack fallback: iconId == 0
  snapshot.skillBar.slots[1].slotIndex = 1;
  snapshot.skillBar.slots[1].cooldown = 2.0f;
  snapshot.skillBar.slots[1].cooldownMax = 10.0f;

  ui::GameUiBuffView buff;
  buff.buffType = static_cast<std::uint8_t>(BuffType::AttackUp);
  buff.duration = 10.0f;
  buff.remaining = 5.0f;
  buff.stacks = 2;
  buff.isDebuff = false;
  snapshot.buffs.push_back(buff);

  ui::GameUiBuffView debuff;
  debuff.buffType = static_cast<std::uint8_t>(BuffType::Stun);
  debuff.duration = -1.0f; // infinite: ratio stays 0
  debuff.remaining = -1.0f;
  debuff.stacks = 1;
  debuff.isDebuff = true;
  snapshot.buffs.push_back(debuff);

  controller.Update(snapshot, MakeInput());
  controller.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() > 0);
}

TEST_CASE("[Unit] SkillHotbarController - implementation declares no static "
          "mutable UI state") {
  const std::string source =
      ReadFileContents("src/game/application/ui/SkillHotbarController.cpp");
  REQUIRE_FALSE(source.empty());
  for (const char* needle : {"static bool", "static float", "static int",
                             "static uint32_t", "static Texture2D",
                             "static Font", "static Shader",
                             "static std::string", "static std::vector",
                             "static Color"}) {
    CHECK_MESSAGE(source.find(needle) == std::string::npos, needle);
  }
}

TEST_CASE("[Unit] SkillHotbarController - UISystem no longer routes the panels") {
  const std::string source =
      ReadFileContents("src/game/application/ui/UISystem.cpp");
  REQUIRE_FALSE(source.empty());
  CHECK(source.find("DrawSkillHotbar(registry)") == std::string::npos);
  CHECK(source.find("DrawBuffs(registry)") == std::string::npos);
}
