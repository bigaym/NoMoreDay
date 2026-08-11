#include "doctest.h"

#include "game/application/ui/SkillHotbarController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"

#include "raylib.h"

#include <fstream>
#include <iterator>
#include <string>

using namespace NoMoreDay;

namespace {

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

TEST_CASE("[Unit] SkillHotbarController - Update handles empty and player registries") {
  ui::UiRuntime runtime;
  ui::SkillHotbarController controller(runtime);

  entt::registry registry;
  controller.Update(registry); // empty registry must not crash
  CHECK_FALSE(controller.HasPlayerData());

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<CombatStats>(player);
  registry.emplace<ActiveSkillsComponent>(player);
  registry.emplace<ActiveEffectsComponent>(player);
  controller.Update(registry);
  CHECK(controller.HasPlayerData());
}

TEST_CASE("[Unit] SkillHotbarController - Draw executes headless without crashing") {
  // The test harness (tests/main.cpp) opens a hidden raylib window with a GL
  // context, so immediate-mode raylib drawing works. UIRenderer falls back to
  // raylib DrawText when UISystem::State.globalFont is unset, and both panels
  // guard icon loading (skill icons load only for mapped skills; buff visuals
  // fall back to the default entry with a null texture asset), so the legacy
  // draw bodies are safe to run here without UISystem::Initialize.
  ui::UiRuntime runtime;
  ui::SkillHotbarController controller(runtime);

  entt::registry registry;

  // Empty registry: both panels early-out on the player view.
  BeginDrawing();
  controller.Draw(registry);
  EndDrawing();

  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& stats = registry.emplace<CombatStats>(player);
  stats.mana = 100.0f;
  auto& active = registry.emplace<ActiveSkillsComponent>(player);
  auto& effects = registry.emplace<ActiveEffectsComponent>(player);

  // Path A: empty hotbar slots + no effects (hotbar renders empty slots,
  // buff strip early-outs on the empty effects vector).
  BeginDrawing();
  controller.Draw(registry);
  EndDrawing();

  // Path B: a mapped skill id (data registry lookup) + one buff and one debuff
  // (visual lookup falls back to the default entry: no texture asset).
  active.slots[0].id = 999999; // unmapped: GetSkill returns nullptr, no texture
  active.slots[1].id = 0;      // Basic Attack fallback: icon_id == 0, no texture
  active.slots[1].cooldown = 2.0f;
  effects.effects.push_back(BuffEffect{});
  effects.effects.back().id = "test_buff";
  effects.effects.back().name = "Test Buff";
  effects.effects.back().type = BuffType::AttackUp;
  effects.effects.back().duration = 10.0f;
  effects.effects.back().remaining = 5.0f;
  effects.effects.back().stacks = 2;
  effects.effects.push_back(BuffEffect{});
  effects.effects.back().id = "test_debuff";
  effects.effects.back().name = "Test Debuff";
  effects.effects.back().type = BuffType::Stun;
  effects.effects.back().is_debuff = true;
  effects.effects.back().duration = -1.0f; // infinite: ratio stays 0
  BeginDrawing();
  controller.Draw(registry);
  EndDrawing();
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
