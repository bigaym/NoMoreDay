#include "doctest.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// R5 (remediation, design §3.2/§3.4): structural regression guards for the
// snapshot-only HUD panels. They lock the source shape of the C-01
// remediation: the display-only panels (PlayerHudController,
// MonsterHealthBarController, SkillHotbarController, MinimapController,
// SwordIntentWidget) must paint from the GameUiSnapshot view model only —
// no per-frame std::string/std::map allocations in the paint path, no
// registry reads in the controller draw/paint path, no legacy immediate
// raylib draws. A future edit that reintroduces any of these paths fails
// the build test run.

namespace {

// Reads a source file relative to the test working directory (which varies
// between CTest and direct invocation). Returns empty when not found.
std::string ReadSource(const char* relativePath) {
  namespace fs = std::filesystem;
  const std::array<fs::path, 3> candidates = {
      fs::path(relativePath),
      fs::path("../") / relativePath,
      fs::path("../../") / relativePath,
  };
  for (const auto& candidate : candidates) {
    if (!fs::exists(candidate)) {
      continue;
    }
    std::ifstream in(candidate, std::ios::in | std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string source = ss.str();
    if (!source.empty()) {
      return source;
    }
  }
  return {};
}

} // namespace

TEST_CASE("[Tech] R5 - HUD controllers never read the ECS registry in their "
          "paint path (C-01)") {
  // The migrated panels take the GameUiSnapshot view model in Update and emit
  // draw-list commands in Paint. registry.view / try_get must not appear in
  // the controller sources (the registry handle is not even a parameter of
  // PlayerHudController::Update; SkillHotbarController keeps it only for the
  // R8 drag-drop write, never for display).
  const std::array<const char*, 3> controllers = {
      "src/game/application/ui/PlayerHudController.cpp",
      "src/game/application/ui/MonsterHealthBarController.cpp",
      "src/game/application/ui/MinimapController.cpp",
  };
  for (const char* path : controllers) {
    CAPTURE(path);
    const std::string source = ReadSource(path);
    REQUIRE_MESSAGE(!source.empty(), "source not found");
    CHECK_MESSAGE(source.find("registry.view<") == std::string::npos,
                  "paint path must not iterate the ECS registry");
    CHECK_MESSAGE(source.find("registry.try_get<") == std::string::npos,
                  "paint path must not read components from the registry");
    CHECK_MESSAGE(source.find("registry.get<") == std::string::npos,
                  "paint path must not read components from the registry");
    CHECK_MESSAGE(source.find("entt::registry& registry)") ==
                      std::string::npos,
                  "controller Update must take the snapshot, not the registry");
  }
}

TEST_CASE("[Tech] R5 - HUD panel paint paths allocate no per-frame "
          "std::string/std::map (C-01)") {
  // The C-01 hotspots: PlayerHudController built three std::maps and several
  // std::strings every frame; MonsterHealthBarController allocated name/hpText
  // strings. The migrated panels format into fixed controller-owned buffers
  // (revision-cached). std::map must not reappear at all; std::string is
  // allowed only off the hot path (e.g. icon-key joins during Update caches),
  // so the guard targets the per-frame construction sites in the paint
  // functions.
  const std::string hud = ReadSource("src/game/application/ui/PlayerHudController.cpp");
  REQUIRE_MESSAGE(!hud.empty(), "PlayerHudController.cpp not found");
  CHECK_MESSAGE(hud.find("std::map") == std::string::npos,
                "per-frame summon maps must not return");
  CHECK_MESSAGE(hud.find("std::string ") == std::string::npos,
                "per-frame std::string temporaries must not return");
  CHECK_MESSAGE(hud.find("std::to_string") == std::string::npos,
                "numeric text must go through the fixed buffers, not "
                "std::to_string");

  const std::string monster =
      ReadSource("src/game/application/ui/MonsterHealthBarController.cpp");
  REQUIRE_MESSAGE(!monster.empty(), "MonsterHealthBarController.cpp not found");
  CHECK_MESSAGE(monster.find("std::string ") == std::string::npos,
                "per-frame name/hpText std::string must not return");
  CHECK_MESSAGE(monster.find("std::to_string") == std::string::npos,
                "hpText must go through snprintf, not std::to_string");
  CHECK_MESSAGE(monster.find("reserve(200)") == std::string::npos,
                "the legacy per-frame reserve(200) must be gone");

  const std::string hotbar =
      ReadSource("src/game/application/ui/SkillHotbarController.cpp");
  REQUIRE_MESSAGE(!hotbar.empty(), "SkillHotbarController.cpp not found");
  CHECK_MESSAGE(hotbar.find("std::map") == std::string::npos,
                "buff/slot maps must not return");
  CHECK_MESSAGE(hotbar.find("std::to_string") == std::string::npos,
                "slot/buff text must go through fixed buffers");
}

TEST_CASE("[Tech] R5 - HUD panels paint through the draw list into the Hud "
          "layer (design §3.4)") {
  // Each migrated panel exposes Paint(UiDrawList&, const UiViewport&) and the
  // host calls it from PrepareRender; the backend submits in a single pass.
  const std::string hudSrc =
      ReadSource("src/game/application/ui/PlayerHudController.cpp");
  REQUIRE_MESSAGE(!hudSrc.empty(), "PlayerHudController.cpp not found");
  CHECK_MESSAGE(hudSrc.find("void PlayerHudController::Paint") !=
                    std::string::npos,
                "Paint entry must exist");

  const std::string monsterSrc =
      ReadSource("src/game/application/ui/MonsterHealthBarController.cpp");
  REQUIRE_MESSAGE(!monsterSrc.empty(), "MonsterHealthBarController.cpp not found");
  CHECK_MESSAGE(monsterSrc.find("void MonsterHealthBarController::Paint") !=
                    std::string::npos,
                "Paint entry must exist");

  const std::string hotbarSrc =
      ReadSource("src/game/application/ui/SkillHotbarController.cpp");
  REQUIRE_MESSAGE(!hotbarSrc.empty(), "SkillHotbarController.cpp not found");
  CHECK_MESSAGE(hotbarSrc.find("void SkillHotbarController::Paint") !=
                    std::string::npos,
                "Paint entry must exist");

  const std::string minimapSrc =
      ReadSource("src/game/application/ui/MinimapController.cpp");
  REQUIRE_MESSAGE(!minimapSrc.empty(), "MinimapController.cpp not found");
  CHECK_MESSAGE(minimapSrc.find("void MinimapController::Paint") !=
                    std::string::npos,
                "Paint entry must exist");

  const std::string host = ReadSource("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_MESSAGE(!host.empty(), "GameUiHost.cpp not found");
  CHECK_MESSAGE(host.find("m_playerHud.Paint(") != std::string::npos,
                "PrepareRender must paint the player HUD");
  CHECK_MESSAGE(host.find("m_monsterHealthBars.Paint(") != std::string::npos,
                "PrepareRender must paint the monster health bars");
  CHECK_MESSAGE(host.find("m_skillHotbar.Paint(") != std::string::npos,
                "PrepareRender must paint the hotbar/buff strip");
  CHECK_MESSAGE(host.find("m_minimap.Paint(") != std::string::npos,
                "PrepareRender must paint the minimap");
}

TEST_CASE("[Tech] R5 - MonsterHealthBarController consumes the snapshot, not "
          "the registry (C-01)") {
  // R5 requirement #4: the controller side of monster health display must read
  // the GameUiSnapshot MonsterHealthBar data (built by GameUiSnapshotBuilder).
  // The raylib Camera2D is gone from the interface: the host forwards the
  // camera transform as plain floats and the mouse in pixels.
  const std::string header =
      ReadSource("src/game/application/ui/MonsterHealthBarController.hpp");
  REQUIRE_MESSAGE(!header.empty(), "MonsterHealthBarController.hpp not found");
  CHECK_MESSAGE(header.find("Render(") == std::string::npos,
                "legacy Render(registry, camera) must be gone");
  CHECK_MESSAGE(header.find("RenderUI(") == std::string::npos,
                "legacy RenderUI(registry) must be gone");
  CHECK_MESSAGE(header.find("Camera2D") == std::string::npos,
                "raylib Camera2D must not appear on the controller interface");
  CHECK_MESSAGE(header.find("void Update(const GameUiSnapshot&") !=
                    std::string::npos,
                "Update must take the frame snapshot");

  const std::string source =
      ReadSource("src/game/application/ui/MonsterHealthBarController.cpp");
  REQUIRE_MESSAGE(!source.empty(), "MonsterHealthBarController.cpp not found");
  CHECK_MESSAGE(source.find("GetScreenToWorld2D(") == std::string::npos,
                "raylib world-space picking must be gone");
  CHECK_MESSAGE(source.find("snapshot.monsters") != std::string::npos,
                "hover picking must read the snapshot monsters");
}

TEST_CASE("[Tech] R5 - SwordIntentWidget no longer self-loads raylib "
          "resources (C-01)") {
  // The widget's legacy path loaded its own texture/shader via UISystem.
  // After R5 it holds a UiResourceId registered by the host and paints icons
  // through the draw list.
  const std::string source =
      ReadSource("src/game/application/ui/SwordIntentWidget.cpp");
  REQUIRE_MESSAGE(!source.empty(), "SwordIntentWidget.cpp not found");
  CHECK_MESSAGE(source.find("LoadTexture") == std::string::npos,
                "the widget must not load textures itself");
  CHECK_MESSAGE(source.find("LoadShader") == std::string::npos,
                "the widget must not load shaders itself");
  CHECK_MESSAGE(source.find("void SwordIntentWidget::Update") !=
                    std::string::npos,
                "the state-caching Update must exist");
  CHECK_MESSAGE(source.find("void SwordIntentWidget::Paint") !=
                    std::string::npos,
                "the draw-list Paint must exist");
}
