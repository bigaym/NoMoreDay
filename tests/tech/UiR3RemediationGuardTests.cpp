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

TEST_CASE("[Tech] R3 - WorldUiFrame BeginFrame precedes proxy collection and "
          "CPU label build (H-01, hybrid loot rendering)") {
  const std::string source = ReadSource("src/game/application/render/GameplayRenderAdapter.cpp");
  REQUIRE_MESSAGE(!source.empty(), "GameplayRenderAdapter.cpp not found");

  // The UIWorld pass must open a new frame token before ANY branch can
  // return, so the previous frame's vector/hover/token is never exposed to
  // the host tooltip and pickup readers.
  const size_t beginFramePos = source.find("BeginFrame(++m_frameCounter)");
  REQUIRE_MESSAGE(beginFramePos != std::string::npos,
                  "BeginFrame(++m_frameCounter) missing in ExecuteUIWorldPass");

  // 混合渲染契约：BuildCpuLootLabels 必须无条件执行（GPU loot 档位也叠加
  // CPU 精选文字标签 + 防重叠布局）。曾经的 `if (frame.gpuLootEnabled)
  // { return; }` 旁路导致 Ultra/High 档标签无文字且底板卡片重叠，禁止
  // 该旁路再次出现。
  const size_t collectPos = source.find("CollectVisibleItemProxies(frame)");
  REQUIRE_MESSAGE(collectPos != std::string::npos,
                  "CollectVisibleItemProxies call missing");
  CHECK_MESSAGE(beginFramePos < collectPos,
                "BeginFrame must precede proxy collection");

  const size_t buildLabelsPos = source.find("BuildCpuLootLabels(frame)");
  REQUIRE_MESSAGE(buildLabelsPos != std::string::npos,
                  "BuildCpuLootLabels call missing in ExecuteUIWorldPass");
  CHECK_MESSAGE(collectPos < buildLabelsPos,
                "proxy collection must precede the CPU label build");

  CHECK_MESSAGE(source.find("if (frame.gpuLootEnabled)") == std::string::npos,
                "the gpuLootEnabled early-return bypass must stay removed "
                "(hybrid rendering: CPU labels overlay GPU loot cards)");
}

TEST_CASE("[Tech] R3 - Engine UIWorld pass draws label/glyph on every loot "
          "path and gates CPU beams on GPU glow (hybrid rendering)") {
  const std::string source = ReadSource("src/engine/render/RenderSystem.cpp");
  REQUIRE_MESSAGE(!source.empty(), "RenderSystem.cpp not found");

  // 切出 ExecuteUIWorldPass 函数体（至下一个 pass 定义），锁定两处契约：
  // 1) 不得包含 gpuLootEnabled 提前 return（混合渲染下 CPU 标签/glyph
  //    叠加绘制，各绘制段自带 buffer 空守卫）；
  // 2) CPU beam 光效门控使用 gpuLootGlowActive（GPU loot 卡片停用后，
  //    glow 由 CPU beam 独立承担，只看用户 glow 开关）。
  const size_t passPos = source.find("void ExecuteUIWorldPass(");
  REQUIRE_MESSAGE(passPos != std::string::npos,
                  "ExecuteUIWorldPass definition missing");
  const size_t nextPassPos = source.find("void ExecuteCompositePass(", passPos);
  REQUIRE_MESSAGE(nextPassPos != std::string::npos,
                  "ExecuteCompositePass definition missing");
  const std::string passBody =
      source.substr(passPos, nextPassPos - passPos);

  CHECK_MESSAGE(passBody.find("if (frame.gpuLootEnabled)") == std::string::npos,
                "ExecuteUIWorldPass must not early-return on gpuLootEnabled");
  CHECK_MESSAGE(passBody.find("gpuLootGlowActive") != std::string::npos,
                "CPU beam draw must be gated on GPU loot glow");
  CHECK_MESSAGE(passBody.find("EndMode2D();") != std::string::npos,
                "ExecuteUIWorldPass must close its Mode2D scope");

  // GPU loot 卡片层（ExecuteGPULootPass）必须保持停用：其位置来自
  // GPU 自治推挤（loot_repulsion.compute），与 CPU 防重叠布局互不感知，
  // 同时启用必然造成卡片与文字两套位置（错位根因）。
  // 定义签名不含 "ExecuteGPULootPass(frame);"，逐行扫描只放行注释态调用。
  {
    size_t searchPos = 0;
    bool hasActiveCall = false;
    const std::string callToken = "ExecuteGPULootPass(frame);";
    size_t hitPos = source.find(callToken, searchPos);
    while (hitPos != std::string::npos) {
      const size_t lineStart = source.rfind('\n', hitPos) + 1;
      size_t codeStart = lineStart;
      while (codeStart < source.size() &&
             (source[codeStart] == ' ' || source[codeStart] == '\t')) {
        ++codeStart;
      }
      if (source.compare(codeStart, 2, "//") != 0) {
        hasActiveCall = true;
      }
      searchPos = hitPos + 1;
      hitPos = source.find(callToken, searchPos);
    }
    CHECK_MESSAGE(!hasActiveCall,
                  "GPU loot card pass must stay disabled: it double-renders "
                  "labels whose positions fight the CPU overlap layout");
  }
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
