#include "doctest.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// R10 (remediation, 收尾): structural regression guards for the overlay
// interaction-phase closure. They lock the source shape of the B-01/R6
// follow-up: OverlayController::UpdateOverlays and its display-refresh helpers
// must be driven by the frame snapshot + session state, never by the gameplay
// registry, and the host must route the phase through the snapshot. A future
// edit that reintroduces a registry read into the overlay interaction phase
// fails the build test run.

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

TEST_CASE("[Tech] R10 - the overlay interaction phase is registry-free "
          "(snapshot-driven, B-01/R6 follow-up)") {
  // R6 kept UpdateOverlays(entt::registry&, viewport): the context-menu /
  // quantity-popup display refresh read ItemComponent straight from the
  // registry. R10 closed it: the phase validates targets against
  // snapshot.displayedItems (the builder resolves the menu target every frame
  // via GameUiSnapshotOptions.contextMenuItem). Any registry read here is a
  // regression of the interaction-phase closure.
  const std::string source =
      ReadSource("src/game/application/ui/OverlayController.cpp");
  REQUIRE_MESSAGE(!source.empty(), "OverlayController.cpp not found");
  CHECK_MESSAGE(source.find("entt::registry") == std::string::npos,
                "no registry type may appear in the overlay controller");
  CHECK_MESSAGE(source.find("registry.get<") == std::string::npos,
                "no ItemComponent registry read may live in the overlay");
  CHECK_MESSAGE(source.find("registry.valid(") == std::string::npos,
                "no registry validity check may live in the overlay");
  CHECK_MESSAGE(source.find("registry.view<") == std::string::npos,
                "no registry view may live in the overlay");
  CHECK_MESSAGE(source.find("registry.all_of<") == std::string::npos,
                "no registry all_of check may live in the overlay");
}

TEST_CASE("[Tech] R10 - UpdateOverlays takes the frame snapshot, not the "
          "registry (signature lock)") {
  // Locks the public + private signatures: the interaction entry point and
  // both display-refresh helpers consume the frame snapshot. The legacy
  // registry-taking signature must not come back.
  const std::string header =
      ReadSource("src/game/application/ui/OverlayController.hpp");
  REQUIRE_MESSAGE(!header.empty(), "OverlayController.hpp not found");
  CHECK_MESSAGE(header.find("UpdateOverlays(const GameUiSnapshot& snapshot,") !=
                    std::string::npos,
                "UpdateOverlays must take the frame snapshot");
  CHECK_MESSAGE(header.find("UpdateOverlays(entt::registry& registry,") ==
                    std::string::npos,
                "the registry-taking UpdateOverlays signature must be gone");
  CHECK_MESSAGE(header.find("RefreshContextMenuDisplay(const GameUiSnapshot&") !=
                    std::string::npos,
                "the context-menu refresh must be snapshot-driven");
  CHECK_MESSAGE(header.find("RefreshQuantityTarget(const GameUiSnapshot&") !=
                    std::string::npos,
                "the quantity-target refresh must be snapshot-driven");
  CHECK_MESSAGE(header.find("RefreshContextMenuDisplay(entt::registry&") ==
                    std::string::npos,
                "the registry-taking context-menu refresh must be gone");
  CHECK_MESSAGE(header.find("RefreshQuantityTarget(entt::registry&") ==
                    std::string::npos,
                "the registry-taking quantity refresh must be gone");
}

TEST_CASE("[Tech] R10 - the host routes the overlay phase through the frame "
          "snapshot") {
  // The host Update must pass its current frame snapshot to UpdateOverlays
  // (the snapshot carries the context-menu target in displayedItems). A
  // registry argument at the call site means the closure regressed.
  const std::string source =
      ReadSource("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_MESSAGE(!source.empty(), "GameUiHost.cpp not found");
  CHECK_MESSAGE(source.find("m_overlay.UpdateOverlays(m_snapshot, "
                            "m_viewport)") != std::string::npos,
                "the host must call UpdateOverlays with the frame snapshot");
  CHECK_MESSAGE(source.find("m_overlay.UpdateOverlays(registry, m_viewport)") ==
                    std::string::npos,
                "the host must not pass the registry to UpdateOverlays");
}
