#include "doctest.h"

#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UISkillHub.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"
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

using NoMoreDay::ui::GameUiIntent;
using NoMoreDay::ui::GameUiSnapshot;

namespace {

std::string ReadFileContents(const char* path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

// R8: an empty frame-scoped snapshot + input for headless interaction smokes.
ui::UiInputFrame MakeEmptyInput() {
  ui::UiInputFrame input;
  input.deltaSeconds = 0.016f;
  input.tooltipTarget = ui::kInvalidUiId;
  return input;
}

ui::UiViewport MakeViewport() {
  return ui::UiViewport::Fit({1280.0f, 720.0f});
}

// Minimal Blade Ascendant player (profession + level), mirroring the setup in
// tests/unit/BladeMasteryTests.cpp. The handler resolves the player through
// PlayerTag, so the host-side scenarios tag the player entity.
entt::entity CreateBladeAscendant(entt::registry& registry, int level) {
  const entt::entity player = registry.create();
  registry.emplace<PlayerStats>(player).level = level;
  registry.emplace<CombatStats>(player);
  auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
  return player;
}

} // namespace

TEST_CASE("[Unit] UISkillHub UpdateInput/Paint are headless-safe with no host") {
  UISkillHub hub;
  GameUiSnapshot snapshot;
  ui::UiDrawList drawList;
  ui::UiViewport viewport = MakeViewport();

  // No host: UpdateInput early-outs (interactions degrade to UI-local state),
  // Paint only issues the custom command and PaintCanvas has no paint state.
  hub.UpdateInput(snapshot, MakeEmptyInput(), 1.0f);
  hub.Paint(drawList, viewport, snapshot, 1.0f);
  hub.PaintCanvas(ui::UiRect{{0.0f, 0.0f}, {1280.0f, 720.0f}});
  CHECK(hub.SelectedSkillId() == NoMoreDay::INVALID_SKILL_ID);
}

TEST_CASE("[Unit] UISkillHub UpdateInput + PaintCanvas render a minimal player "
          "without crashing") {
  // R8: the hub is a snapshot surface — the draw path is the registered
  // painter (PaintCanvas), fed by the paint state UpdateInput captured from
  // the frame snapshot. tests/main.cpp opens a hidden raylib window with a GL
  // context, so immediate-mode raylib drawing works. Data registries stay
  // unloaded here: BladeMasteryRegistry reports no profiles (empty-panel
  // branch), SkillRegistry reports no skills (empty grid), and
  // AssetLoadingSystem::GetTexture returns a null texture {0} when
  // uninitialized, so slot frames are the only texture-bound draw work.
  ui::GameUiHost host;
  UISkillHub hub;
  hub.SetHost(&host);

  GameUiSnapshot snapshot;
  snapshot.player.hasPlayer = true;
  snapshot.player.level = 1;
  snapshot.skillTree.availableTalentPoints = 3;

  hub.UpdateInput(snapshot, MakeEmptyInput(), 1.0f);
  BeginDrawing();
  hub.PaintCanvas(ui::UiRect{{0.0f, 0.0f}, {1280.0f, 720.0f}});
  EndDrawing();
}

TEST_CASE("[Unit] SkillSelectMastery intent mirrors BladeMasteryService") {
  REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
      "assets/data/blade_masteries.json"));

  // Deterministic: disable the debug unlock override regardless of state left
  // behind by other test cases.
  const bool previousDebugOverride =
      systems::BladeMasteryService::IsDebugUnlockOverrideEnabled();
  systems::BladeMasteryService::SetDebugUnlockOverrideEnabled(false);

  // R8: the hub no longer writes gameplay (TrySelectMastery is gone). The
  // mastery selection is a SkillSelectMastery intent executed by the command
  // handler; its failure notification is the contractual popup text the
  // legacy UI tests assert (UITests: Locked mastery selection shows popup).
  entt::registry registry;
  registry.emplace<PlayerTag>(CreateBladeAscendant(registry, 99));
  ui::GameUiCommandHandler handler;

  // Level 99 Blade Ascendant: SwordSaint (unlock_level 50) is selectable.
  GameUiIntent intent;
  intent.sourceNode = ui::kInvalidUiId;
  intent.kind = ui::GameUiIntentKind::SkillSelectMastery;
  intent.payload.masteryId = static_cast<std::uint8_t>(BladeMasteryId::SwordSaint);
  const auto select = handler.Execute(registry, intent);
  CHECK(select.success);
  const auto* mastery = registry.try_get<BladeMasteryComponent>(
      registry.view<PlayerTag>().front());
  REQUIRE(mastery != nullptr);
  CHECK(mastery->selected == BladeMasteryId::SwordSaint);

  // Level too low without the debug override: reject with the contractual
  // failure notification (surfaces via the hosted message box on Update).
  entt::registry lowRegistry;
  lowRegistry.emplace<PlayerTag>(CreateBladeAscendant(lowRegistry, 1));
  const auto rejected = handler.Execute(lowRegistry, intent);
  CHECK_FALSE(rejected.success);
  CHECK(rejected.notification == "等级或基础职业不满足职业专精条件");

  // No Blade Ascendant profession: reject regardless of level.
  entt::registry noProfRegistry;
  const entt::entity noProfession = noProfRegistry.create();
  noProfRegistry.emplace<PlayerTag>(noProfession);
  noProfRegistry.emplace<PlayerStats>(noProfession).level = 99;
  noProfRegistry.emplace<CombatStats>(noProfession);
  CHECK_FALSE(handler.Execute(noProfRegistry, intent).success);

  // Invalid mastery id (no profile in the registry): reject.
  entt::registry noneRegistry;
  noneRegistry.emplace<PlayerTag>(CreateBladeAscendant(noneRegistry, 99));
  GameUiIntent noneIntent = intent;
  noneIntent.payload.masteryId = static_cast<std::uint8_t>(BladeMasteryId::None);
  CHECK_FALSE(handler.Execute(noneRegistry, noneIntent).success);

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
