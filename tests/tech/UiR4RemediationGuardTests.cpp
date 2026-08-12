#include "doctest.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// R4 (remediation, design §3.4): structural regression guards for the runtime
// draw-list pipeline. They lock the source shape of the C-01 remediation:
// no per-frame std::string command payloads, no ordered-insert sorting, no
// per-layer backend rescan, no immediate message-box draw, and no placeholder
// Reserve(64) in PrepareRender. A future edit that reintroduces any of these
// paths fails the build test run.

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

TEST_CASE("[Tech] R4 - UiDrawList sorts by total order, never by ordered "
          "insert (C-01)") {
  const std::string source = ReadSource("src/game/application/ui/UiDrawList.cpp");
  REQUIRE_MESSAGE(!source.empty(), "UiDrawList.cpp not found");

  // The old per-command O(n) insertion (std::upper_bound + insert) is gone;
  // the commands are appended in paint order and sorted once by Finalize.
  // (The source comment may still mention the historical name in prose.)
  CHECK_MESSAGE(source.find("std::upper_bound") == std::string::npos,
                "ordered upper_bound insertion must not return");
  CHECK_MESSAGE(source.find("m_commands.insert") == std::string::npos,
                "ordered insert into the command list must not return");
  CHECK_MESSAGE(source.find("void UiDrawList::Finalize") != std::string::npos,
                "Finalize must exist");
  CHECK_MESSAGE(source.find("std::sort") != std::string::npos,
                "Finalize must sort with std::sort");
}

TEST_CASE("[Tech] R4 - text payloads live in the draw list arena (C-01)") {
  const std::string header =
      ReadSource("src/game/application/ui/UiDrawList.hpp");
  REQUIRE_MESSAGE(!header.empty(), "UiDrawList.hpp not found");
  CHECK_MESSAGE(header.find("std::string text") == std::string::npos,
                "UiDrawCommand must not own a per-frame std::string");
  CHECK_MESSAGE(header.find("textOffset") != std::string::npos,
                "UiDrawCommand must reference the arena via textOffset");
  CHECK_MESSAGE(header.find("textLength") != std::string::npos,
                "UiDrawCommand must reference the arena via textLength");

  const std::string source = ReadSource("src/game/application/ui/UiDrawList.cpp");
  REQUIRE_MESSAGE(!source.empty(), "UiDrawList.cpp not found");
  CHECK_MESSAGE(source.find("TextAt") != std::string::npos,
                "the arena read-back accessor must exist");
}

TEST_CASE("[Tech] R4 - the backend submits the sorted list in a single pass "
          "(C-01)") {
  const std::string source =
      ReadSource("src/game/application/ui/UiRaylibBackend.cpp");
  REQUIRE_MESSAGE(!source.empty(), "UiRaylibBackend.cpp not found");

  // The per-layer rescan loop is gone: Render walks the pre-sorted commands
  // exactly once.
  CHECK_MESSAGE(source.find("kFirstLayerValue") == std::string::npos,
                "per-layer rescan loop must not return");
  CHECK_MESSAGE(source.find("layerValue") == std::string::npos,
                "per-layer rescan loop must not return");
  CHECK_MESSAGE(source.find("IsFinalized") != std::string::npos,
                "Render must require a finalized draw list");
  CHECK_MESSAGE(source.find("TextAt") != std::string::npos,
                "Render must read text from the draw list arena");
}

TEST_CASE("[Tech] R4 - the message box paints through the draw list, never "
          "immediately (A-01)") {
  const std::string overlay =
      ReadSource("src/game/application/ui/OverlayController.cpp");
  REQUIRE_MESSAGE(!overlay.empty(), "OverlayController.cpp not found");

  // The immediate raylib message box draw is gone from the overlay; the paint
  // path appends Image + Text commands to the draw list instead.
  CHECK_MESSAGE(overlay.find("UIRenderer::DrawMessageBox") == std::string::npos,
                "the immediate message box draw must not return");
  CHECK_MESSAGE(overlay.find("void OverlayController::DrawMessageBox") ==
                    std::string::npos,
                "the legacy immediate DrawMessageBox definition must be gone");
  CHECK_MESSAGE(overlay.find("PaintMessageBox") != std::string::npos,
                "the draw-list paint entry must exist");

  const std::string host = ReadSource("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_MESSAGE(!host.empty(), "GameUiHost.cpp not found");
  // R6: PrepareRender routes through the overlay controller's Paint() entry
  // (which internally paints the message box through the draw list); the
  // direct PaintMessageBox call moved into the controller paint path.
  CHECK_MESSAGE(host.find("m_overlay.Paint(m_drawList, m_viewport)") !=
                    std::string::npos,
                "PrepareRender must paint through the controller");
}

TEST_CASE("[Tech] R4 - PrepareRender paints and finalizes, no placeholder "
          "reserve") {
  const std::string source = ReadSource("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_MESSAGE(!source.empty(), "GameUiHost.cpp not found");

  // The old PrepareRender placeholder (per-frame Clear + Reserve(64)) is gone;
  // capacities are reserved once at Initialize (m_drawList.Reserve(cap)).
  CHECK_MESSAGE(source.find("m_drawList.Reserve(64)") == std::string::npos,
                "the PrepareRender Reserve(64) placeholder must be gone");
  CHECK_MESSAGE(source.find("m_drawList.Finalize()") != std::string::npos,
                "PrepareRender must finalize the draw list");
  CHECK_MESSAGE(source.find("m_drawList.Clear()") != std::string::npos,
                "PrepareRender must clear the draw list");
  CHECK_MESSAGE(source.find("m_drawList.ReserveText(") != std::string::npos,
                "the host must reserve the text arena at Initialize");
}

TEST_CASE("[Tech] R4 - Update runs the runtime pipeline every frame") {
  const std::string source = ReadSource("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_MESSAGE(!source.empty(), "GameUiHost.cpp not found");

  // Design §3.4: reconcile -> UpdateInput -> Arrange run on every UI update,
  // in that order, and the viewport fit feeds the input in logical space.
  const size_t reconcilePos = source.find("m_overlay.ReconcileRuntime()");
  const size_t updateInputPos = source.find("m_runtime.UpdateInput(uiInput)");
  const size_t arrangePos = source.find("m_runtime.Arrange(");
  REQUIRE_MESSAGE(reconcilePos != std::string::npos,
                  "ReconcileRuntime call missing");
  REQUIRE_MESSAGE(updateInputPos != std::string::npos,
                  "m_runtime.UpdateInput call missing");
  REQUIRE_MESSAGE(arrangePos != std::string::npos,
                  "m_runtime.Arrange call missing");
  CHECK_MESSAGE(reconcilePos < updateInputPos,
                "reconcile must run before UpdateInput");
  CHECK_MESSAGE(updateInputPos < arrangePos,
                "UpdateInput must run before Arrange");
}
