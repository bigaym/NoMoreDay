#include "doctest.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Coordinate-system regression guards (Track refactor_coordinate_system_20260824).
// Lock R1-R3 / R5: coordinate math and Y/UV flips must go through CoordSystem;
// UI must use UiViewport; MSDF offsets must use the canonical helper. These
// are source-shape guards, mirroring UiR5RemediationGuardTests.

namespace {

std::string ReadSource(const char* relativePath) {
  namespace fs = std::filesystem;
  const std::array<fs::path, 3> candidates = {
      fs::path(relativePath),
      fs::path("../") / relativePath,
      fs::path("../../") / relativePath,
  };
  for (const auto& candidate : candidates) {
    if (!fs::exists(candidate)) continue;
    std::ifstream in(candidate, std::ios::in | std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string source = ss.str();
    if (!source.empty()) return source;
  }
  return {};
}

} // namespace

TEST_CASE("[Tech] CoordGuard - CoordSystem is the only custom MVP/Y-flip source") {
  const std::string coord = ReadSource("src/engine/render/CoordSystem.hpp");
  REQUIRE_MESSAGE(!coord.empty(), "CoordSystem.hpp not found");
  CHECK_MESSAGE(coord.find("Build2DMvp") != std::string::npos,
                "Build2DMvp must exist");
  CHECK_MESSAGE(coord.find("NativeYToGl") != std::string::npos,
                "NativeYToGl must exist");
  CHECK_MESSAGE(coord.find("WorldToScenePixel") != std::string::npos,
                "WorldToScenePixel must exist");
  CHECK_MESSAGE(coord.find("ScenePixelToWorld") != std::string::npos,
                "ScenePixelToWorld must exist");

  // Production engine/render sources must not re-construct MatrixOrtho.
  const std::string particle =
      ReadSource("src/engine/render/GPUParticleSystem.cpp");
  REQUIRE_MESSAGE(!particle.empty(), "GPUParticleSystem.cpp not found");
  CHECK_MESSAGE(particle.find("MatrixOrtho(") == std::string::npos,
                "GPUParticleSystem must delegate to Build2DMvp, not MatrixOrtho");
  CHECK_MESSAGE(particle.find("coord::Build2DMvp") != std::string::npos,
                "GPUParticleSystem must use coord::Build2DMvp");
}

TEST_CASE("[Tech] CoordGuard - UI uses UiViewport / CoordSystem, not raw camera math") {
  const std::string monster =
      ReadSource("src/game/application/ui/MonsterHealthBarController.cpp");
  REQUIRE_MESSAGE(!monster.empty(), "MonsterHealthBarController.cpp not found");
  CHECK_MESSAGE(monster.find("const float invZoom = 1.0f /") == std::string::npos,
                "hand-rolled inverse zoom must not return");
  CHECK_MESSAGE(monster.find("(cmd.worldX - m_camTargetX) * m_camZoom") ==
                    std::string::npos,
                "hand-rolled world-to-screen formula must not return");
  CHECK_MESSAGE(monster.find("NoMoreDay::render::coord::ScenePixelToWorld") !=
                    std::string::npos,
                "MonsterHealthBar must use CoordSystem");
  CHECK_MESSAGE(monster.find("NoMoreDay::render::coord::WorldToScenePixel") !=
                    std::string::npos,
                "MonsterHealthBar paint must use CoordSystem");

  const std::string host = ReadSource("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_MESSAGE(!host.empty(), "GameUiHost.cpp not found");
  CHECK_MESSAGE(host.find("UISystem::GetMousePositionLogic()") ==
                    std::string::npos,
                "legacy logical mouse helper must be gone from the UI draw path");
  CHECK_MESSAGE(host.find("m_viewport.ToLogical(UiVec2{mPos.x, mPos.y})") !=
                    std::string::npos,
                "drag phantom must convert through UiViewport");
}

TEST_CASE("[Tech] CoordGuard - MSDF offsets use the canonical helper") {
  const std::string ltb =
      ReadSource("src/engine/render/LootTextBatcher.cpp");
  REQUIRE_MESSAGE(!ltb.empty(), "LootTextBatcher.cpp not found");
  CHECK_MESSAGE(ltb.find("coord::MsdfBearingToWorldOffset") != std::string::npos,
                "MSDF offsets must go through coord::MsdfBearingToWorldOffset");
  CHECK_MESSAGE(ltb.find("metric->bearing[1] * scale") == std::string::npos,
                "raw MSDF bearing multiplication must be removed");
  CHECK_MESSAGE(ltb.find("metric->bearing[0] * scale") == std::string::npos,
                "raw MSDF bearing multiplication must be removed");
}

TEST_CASE("[Tech] CoordGuard - no ad-hoc FRAGCOORD flip in GameplayState") {
  const std::string gs =
      ReadSource("src/game/application/states/GameplayState.cpp");
  REQUIRE_MESSAGE(!gs.empty(), "GameplayState.cpp not found");
  CHECK_MESSAGE(gs.find("GetScreenHeight() - screenPlayer.y") == std::string::npos,
                "FRAGCOORD flip must use coord::NativeYToGl");
  CHECK_MESSAGE(gs.find("coord::NativeYToGl") != std::string::npos,
                "GameplayState must call coord::NativeYToGl");
}
