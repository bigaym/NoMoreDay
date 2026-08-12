#include "doctest.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// R3 (remediation, design §3.5/§3.6): structural regression guards for H-01
// (WorldUiFrame BeginFrame must run before any branch early-returns, so GPU
// loot passes never expose a stale frame) and H-02 (GameplayState must check
// Escape consumption AFTER the UI update, and must not use the removed
// IsInventoryVisible() pause proxy).
//
// These are source-level guards (the same technique UITests.cpp uses for the
// frame-order contracts): they lock the file order of the R3 contract, so a
// future edit that moves BeginFrame below the gpuLootEnabled return or reorders
// the pause check fails the build test run.

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

TEST_CASE("[Tech] R3 - WorldUiFrame BeginFrame precedes the gpuLootEnabled "
          "early return (H-01)") {
  const std::string source = ReadSource("src/game/application/render/GameplayRenderAdapter.cpp");
  REQUIRE_MESSAGE(!source.empty(), "GameplayRenderAdapter.cpp not found");

  // The UIWorld pass must open a new frame token before ANY branch can
  // return: the GPU loot early return (which skips the CPU label/glyph/beam
  // output) must come AFTER BeginFrame, otherwise the previous frame's
  // vector/hover/token is exposed to the host tooltip and pickup readers.
  const size_t beginFramePos = source.find("BeginFrame(++m_frameCounter)");
  const size_t gpuLootReturnPos = source.find("if (frame.gpuLootEnabled) {");
  REQUIRE_MESSAGE(beginFramePos != std::string::npos,
                  "BeginFrame(++m_frameCounter) missing in ExecuteUIWorldPass");
  REQUIRE_MESSAGE(gpuLootReturnPos != std::string::npos,
                  "gpuLootEnabled early return missing");
  CHECK_MESSAGE(beginFramePos < gpuLootReturnPos,
                "BeginFrame must precede the gpuLootEnabled early return");

  // The read-only proxy collection must run on every branch (CPU and GPU):
  // the gpuLootEnabled early return must come after CollectVisibleItemProxies.
  const size_t collectPos = source.find("CollectVisibleItemProxies(frame)");
  REQUIRE_MESSAGE(collectPos != std::string::npos,
                  "CollectVisibleItemProxies call missing");
  CHECK_MESSAGE(collectPos < gpuLootReturnPos,
                "proxy collection must precede the gpuLootEnabled early "
                "return (GPU path still fills the proxy)");
}

TEST_CASE("[Tech] R3 - WorldUiFrame proxy is written by both item and gold "
          "branches (H-01)") {
  const std::string source = ReadSource("src/game/application/render/GameplayRenderAdapter.cpp");
  REQUIRE_MESSAGE(!source.empty(), "GameplayRenderAdapter.cpp not found");

  // The proxy producer (CollectVisibleItemProxies) fills the frame for every
  // visible loot entity regardless of the CPU/GPU output split.
  const size_t collectPos = source.find("void GameplayRenderAdapter::CollectVisibleItemProxies");
  REQUIRE_MESSAGE(collectPos != std::string::npos,
                  "CollectVisibleItemProxies definition missing");
  // Two AddItem sites: the ItemComponent branch and the GoldComponent branch.
  size_t pos = collectPos;
  int addItemCount = 0;
  while ((pos = source.find("m_worldFrame->AddItem(", pos)) != std::string::npos) {
    ++addItemCount;
    pos += std::string("m_worldFrame->AddItem(").size();
  }
  CHECK_MESSAGE(addItemCount >= 2,
                "proxy AddItem must exist in both the item and gold branches");
}

TEST_CASE("[Tech] R3 - WorldUiFrameView readers reject invalid views before "
          "reading proxies (H-01)") {
  // Host pickup detection: the view gate must precede any proxy read.
  {
    const std::string source =
        ReadSource("src/game/application/ui/GameUiHost.cpp");
    REQUIRE_MESSAGE(!source.empty(), "GameUiHost.cpp not found");
    const size_t gatePos = source.find("if (!worldView.IsValid()) {");
    REQUIRE_MESSAGE(gatePos != std::string::npos,
                    "worldView.IsValid gate missing in DetectPickupClick");
    const size_t readPos = source.find("worldView.VisibleItems()", gatePos);
    const bool readFollowsGate =
        readPos != std::string::npos && readPos > gatePos;
    CHECK_MESSAGE(readFollowsGate,
                  "proxy read must follow the view validity gate");
  }
  // Tooltip ground hover: same contract in TooltipController.
  {
    const std::string source =
        ReadSource("src/game/application/ui/TooltipController.cpp");
    REQUIRE_MESSAGE(!source.empty(), "TooltipController.cpp not found");
    const size_t gatePos = source.find("if (!worldView.IsValid()) {");
    REQUIRE_MESSAGE(gatePos != std::string::npos,
                    "worldView.IsValid gate missing in DetectGroundHover");
    const size_t readPos = source.find("worldView.VisibleItems()", gatePos);
    const bool readFollowsGate =
        readPos != std::string::npos && readPos > gatePos;
    CHECK_MESSAGE(readFollowsGate,
                  "proxy read must follow the view validity gate");
  }
}

TEST_CASE("[Tech] R3 - GameplayState pauses only after the UI update consumed "
          "nothing (H-02)") {
  const std::string source =
      ReadSource("src/game/application/states/GameplayState.cpp");
  REQUIRE_MESSAGE(!source.empty(), "GameplayState.cpp not found");

  // The pause check must run after the UI update (the host owns the Escape
  // key and reports consumption through EscapeConsumedThisFrame).
  const size_t hostUpdatePos = source.find("m_uiHost->Update(registry,");
  REQUIRE_MESSAGE(hostUpdatePos != std::string::npos,
                  "m_uiHost->Update missing");
  const size_t escapeCheckPos = source.find("!m_uiHost->EscapeConsumedThisFrame()");
  REQUIRE_MESSAGE(escapeCheckPos != std::string::npos,
                  "EscapeConsumedThisFrame check missing");
  const size_t pausePushPos = source.find("PushState<PauseState>()");
  REQUIRE_MESSAGE(pausePushPos != std::string::npos,
                  "PushState<PauseState> missing");

  CHECK_MESSAGE(hostUpdatePos < escapeCheckPos,
                "pause check must run after the UI update");
  CHECK_MESSAGE(escapeCheckPos < pausePushPos,
                "PushState<PauseState> must be gated by EscapeConsumedThisFrame");

  // The old IsInventoryVisible() pause proxy is gone: no Escape-path guard
  // may read it (the remaining IsInventoryVisible use is the drag-cleanup
  // fallback, which is a different contract).
  const size_t oldProxyPos = source.find(
      "if (IsKeyPressed(KEY_ESCAPE) && !m_uiHost->IsInventoryVisible())");
  CHECK_MESSAGE(oldProxyPos == std::string::npos,
                "the IsInventoryVisible() Escape proxy must be removed");
}
