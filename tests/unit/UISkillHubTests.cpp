#include "doctest.h"

#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UISkillHub.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BladeMasteryRegistry.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"

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

// Draw early-outs while the supplied alpha <= 0. Raising it lets the draw
// body run; the hub takes the alpha as a parameter (U8: the legacy
// UISystem::State.skillTreeAlpha slot is gone).
struct SkillTreeAlphaScoped {
  explicit SkillTreeAlphaScoped(float alpha) : alpha(alpha) {}
  float alpha;
};

// Minimal Blade Ascendant player (profession + level), mirroring the setup in
// tests/unit/BladeMasteryTests.cpp.
entt::entity CreateBladeAscendant(entt::registry& registry, int level) {
  const entt::entity player = registry.create();
  registry.emplace<PlayerStats>(player).level = level;
  registry.emplace<CombatStats>(player);
  auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
  return player;
}

} // namespace

TEST_CASE("[Unit] UISkillHub Draw is headless-safe with no valid player") {
  UISkillHub hub;
  entt::registry registry;

  // alpha 0: Draw early-outs before touching the player.
  hub.Draw(registry, entt::null, 0.0f);

  // With the panel visible, Draw reaches the player lookup and early-outs on
  // try_get<ActiveSkillsComponent> == nullptr (entt::null and bare entities
  // must not crash).
  {
    const float alpha = 1.0f;
    hub.Draw(registry, entt::null, alpha);

    const entt::entity bare = registry.create();
    hub.Draw(registry, bare, alpha);
  }
}

TEST_CASE("[Unit] UISkillHub Draw renders a minimal player without crashing") {
  // tests/main.cpp opens a hidden raylib window with a GL context, so
  // immediate-mode raylib drawing works. Data registries stay unloaded here:
  // BladeMasteryRegistry reports no profiles (empty-panel branch),
  // SkillRegistry reports no skills (empty grid), and
  // AssetLoadingSystem::GetTexture returns a null texture {0} when
  // uninitialized, so slot frames are the only texture-bound draw work.
  UISkillHub hub;
  entt::registry registry;

  const entt::entity player = registry.create();
  registry.emplace<PlayerStats>(player).level = 1;
  registry.emplace<CombatStats>(player);
  auto& active = registry.emplace<ActiveSkillsComponent>(player);
  active.available_talent_points = 3;

  SkillTreeAlphaScoped alpha(1.0f);
  BeginDrawing();
  hub.Draw(registry, player, alpha.alpha);
  EndDrawing();
}

TEST_CASE("[Unit] UISkillHub TrySelectMastery mirrors BladeMasteryService") {
  REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
      "assets/data/blade_masteries.json"));

  // Deterministic: disable the debug unlock override regardless of state left
  // behind by other test cases.
  const bool previousDebugOverride =
      systems::BladeMasteryService::IsDebugUnlockOverrideEnabled();
  systems::BladeMasteryService::SetDebugUnlockOverrideEnabled(false);

  // U8: the failure box routes through the hosted GameUiHost (the legacy
  // UISystem::State.showMessageBox write is gone).
  NoMoreDay::ui::GameUiHost host;
  UISkillHub hub;
  hub.SetHost(&host);
  entt::registry registry;

  // Level 99 Blade Ascendant: SwordSaint (unlock_level 50) is selectable.
  const entt::entity player = CreateBladeAscendant(registry, 99);
  CHECK(hub.TrySelectMastery(registry, player, BladeMasteryId::SwordSaint));
  const auto* mastery = registry.try_get<BladeMasteryComponent>(player);
  REQUIRE(mastery != nullptr);
  CHECK(mastery->selected == BladeMasteryId::SwordSaint);

  // Level too low without the debug override: reject and queue the failure
  // message box.
  const entt::entity lowLevel = CreateBladeAscendant(registry, 1);
  CHECK_FALSE(
      hub.TrySelectMastery(registry, lowLevel, BladeMasteryId::SwordSaint));
  CHECK(host.IsMessageBoxVisible());

  // No Blade Ascendant profession: reject regardless of level.
  const entt::entity noProfession = registry.create();
  registry.emplace<PlayerStats>(noProfession).level = 99;
  registry.emplace<CombatStats>(noProfession);
  CHECK_FALSE(hub.TrySelectMastery(registry, noProfession,
                                   BladeMasteryId::SwordSaint));

  // Invalid mastery id (no profile in the registry): reject.
  CHECK_FALSE(hub.TrySelectMastery(registry, player, BladeMasteryId::None));

  systems::BladeMasteryService::SetDebugUnlockOverrideEnabled(
      previousDebugOverride);
}

TEST_CASE("[Unit] UISkillHub header holds no static data members") {
  const std::string path = "src/game/application/ui/UISkillHub.hpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());

  const std::string needle = "static ";
  const std::string classNeedle = "class UISkillHub";
  std::string::size_type classPos = contents.find(classNeedle);
  REQUIRE_MESSAGE(classPos != std::string::npos, "class declaration not found");
  const std::string body = contents.substr(classPos);

  std::string::size_type pos = body.find(needle);
  while (pos != std::string::npos) {
    // A static member function always has a '(' before the terminating ';' or
    // '{'; a static data member declaration does not.
    const std::string::size_type end =
        body.find_first_of(";{", pos + needle.size());
    const std::string::size_type paren = body.find('(', pos + needle.size());
    CHECK_MESSAGE(end != std::string::npos, "unterminated declaration in '",
                  body.substr(pos, end - pos + 1), "'");
    CHECK_MESSAGE(paren != std::string::npos,
                  "static data member must not exist in UISkillHub: '",
                  body.substr(pos, end - pos + 1), "'");
    CHECK_MESSAGE(paren < end,
                  "static data member must not exist in UISkillHub: '",
                  body.substr(pos, end - pos + 1), "'");
    pos = body.find(needle, pos + needle.size());
  }
}

TEST_CASE("[Unit] UISkillHub implementation declares no static mutable UI state") {
  const std::string source =
      ReadFileContents("src/game/application/ui/UISkillHub.cpp");
  REQUIRE_FALSE(source.empty());
  for (const char* needle : {"static bool", "static float", "static int",
                             "static uint32_t", "static Texture2D",
                             "static Font", "static Shader",
                             "static std::string", "static std::vector",
                             "static Color"}) {
    CHECK_MESSAGE(source.find(needle) == std::string::npos, needle);
  }
}
