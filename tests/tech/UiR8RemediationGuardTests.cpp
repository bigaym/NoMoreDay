#include "doctest.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// R8 (remediation, design §3.1/§3.3/§3.4): structural regression guards for
// the migrated skill / astrolabe surfaces — the final immediate surfaces of
// the B-01 remediation. They lock the source shape of the R8 migration: the
// host Draw phase takes no registry (A-01); the skill hotbar, talent tree,
// skill hub and astrolabe controllers run an Update(snapshot, input) +
// Paint(draw list) contract with every gameplay write routed as an intent
// (SkillAssign / SkillUnassign / SkillResetTalents / SkillAllocateTalentPoint
// / SkillSelectMastery / SkillSetAttunement / SkillSetDebugUnlock /
// AstrolabeAddPoint / AstrolabeTakeVow) executed by the GameUiCommandHandler;
// and raylib draw calls are confined to the registered backend painters
// (PaintCanvas / DrawInternal). A future edit that reintroduces any of these
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

TEST_CASE("[Tech] R8 - GameUiHost::Draw takes no registry (A-01)") {
  // R1-R7 closed every panel surface except the host Draw pass itself. R8:
  // Draw(levelManager, camera, grid) — the registry parameter is gone and the
  // final immediate surface list (astrolabe Draw / skill-tree Draw / legacy
  // UISkillHub) is closed. This is the global completion condition for A-01.
  const std::string source = ReadSource("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_MESSAGE(!source.empty(), "GameUiHost.cpp not found");
  CHECK_MESSAGE(source.find("void GameUiHost::Draw(const LevelManager") !=
                    std::string::npos,
                "Draw must take (levelManager, camera, spatialGrid) only");
  CHECK_MESSAGE(source.find("GameUiHost::Draw(entt::registry") ==
                    std::string::npos,
                "the legacy registry Draw overload must be gone");
  CHECK_MESSAGE(source.find("m_astrolabe.Draw(registry)") ==
                    std::string::npos,
                "the astrolabe immediate Draw must be gone from the host");
  CHECK_MESSAGE(source.find("m_skillTree.Draw(registry") ==
                    std::string::npos,
                "the skill-tree immediate Draw must be gone from the host");
  CHECK_MESSAGE(source.find("m_astrolabe.Paint(m_drawList") !=
                    std::string::npos,
                "PrepareRender must paint the astrolabe through the draw list");
  CHECK_MESSAGE(source.find("m_skillTree.Paint(m_drawList") !=
                    std::string::npos,
                "PrepareRender must paint the skill tree through the draw list");
  CHECK_MESSAGE(source.find("m_tooltip.Paint(m_drawList") !=
                    std::string::npos,
                "the tooltip must paint through the draw list");
  CHECK_MESSAGE(source.find("m_skillHotbar.Update(m_snapshot") !=
                    std::string::npos,
                "the hotbar must be driven from the snapshot");
  CHECK_MESSAGE(source.find("void GameUiHost::DrawDraggingPhantom()") !=
                    std::string::npos,
                "the drag phantom pass must take no registry");
  CHECK_MESSAGE(source.find("GameUiHost::SetHoveredItem(") ==
                    std::string::npos,
                "the entt::entity SetHoveredItem overload must be gone");
}

TEST_CASE("[Tech] R8 - the skill hotbar never writes gameplay and takes no "
          "registry (B-01)") {
  // The legacy SkillHotbarController::Update held the registry and wrote
  // ActiveSkillsComponent slots[].id directly for drag-drop (R5 marker).
  // R8: Update(snapshot, input), drops enqueue SkillAssign intents and the
  // controller carries only display caches.
  const std::string source =
      ReadSource("src/game/application/ui/SkillHotbarController.cpp");
  REQUIRE_MESSAGE(!source.empty(), "SkillHotbarController.cpp not found");
  CHECK_MESSAGE(source.find("entt::registry") == std::string::npos,
                "the controller must not take a registry");
  CHECK_MESSAGE(source.find("registry.view<") == std::string::npos,
                "the controller must not iterate the ECS registry");
  CHECK_MESSAGE(source.find("void SkillHotbarController::Update(const "
                            "GameUiSnapshot& snapshot,") !=
                    std::string::npos,
                "Update must take (snapshot, input)");
  CHECK_MESSAGE(source.find("GameUiIntentKind::SkillAssign") !=
                    std::string::npos,
                "drops must enqueue the SkillAssign intent");
  CHECK_MESSAGE(source.find("Paint(UiDrawList&") != std::string::npos,
                "the snapshot paint entry must exist");
}

TEST_CASE("[Tech] R8 - the talent tree never touches the registry and routes "
          "resets/allocation as intents (B-01)") {
  // The legacy SkillTreeUI::Draw(registry, player, ...) called
  // SkillSystem::ResetTalents / AddTalentPoint directly. R8: UpdateInput
  // (snapshot, input) + Paint + PaintCanvas (backend painter), with
  // gameplay writes as SkillResetTalents / SkillAllocateTalentPoint intents.
  const std::string source =
      ReadSource("src/game/application/ui/UISkillTalentTree.cpp");
  REQUIRE_MESSAGE(!source.empty(), "UISkillTalentTree.cpp not found");
  CHECK_MESSAGE(source.find("entt::registry") == std::string::npos,
                "the tree must not take a registry");
  CHECK_MESSAGE(source.find("SkillSystem::ResetTalents(") == std::string::npos,
                "reset must not call the skill system directly");
  CHECK_MESSAGE(source.find("SkillSystem::AddTalentPoint(") ==
                    std::string::npos,
                "allocation must not call the skill system directly");
  CHECK_MESSAGE(source.find("void SkillTreeUI::UpdateInput(const "
                            "GameUiSnapshot&") != std::string::npos,
                "the snapshot interaction phase must exist");
  CHECK_MESSAGE(source.find("GameUiIntentKind::SkillResetTalents") !=
                    std::string::npos,
                "reset must enqueue the SkillResetTalents intent");
  CHECK_MESSAGE(source.find("GameUiIntentKind::SkillAllocateTalentPoint") !=
                    std::string::npos,
                "node clicks must enqueue the SkillAllocateTalentPoint intent");
  CHECK_MESSAGE(source.find("void SkillTreeUI::PaintCanvas(UiRect") !=
                    std::string::npos,
                "the backend painter canvas must exist");
}

TEST_CASE("[Tech] R8 - the skill hub never touches the registry and routes "
          "mastery/assignment as intents (B-01)") {
  // The legacy UISkillHub::Draw(registry, player) wrote specialized slots,
  // selected masteries and the debug override directly through
  // BladeMasteryService / SkillSystem. R8: UpdateInput + Paint + PaintCanvas,
  // all writes as intents; TrySelectMastery is gone.
  const std::string source = ReadSource("src/game/application/ui/UISkillHub.cpp");
  REQUIRE_MESSAGE(!source.empty(), "UISkillHub.cpp not found");
  CHECK_MESSAGE(source.find("entt::registry") == std::string::npos,
                "the hub must not take a registry");
  CHECK_MESSAGE(source.find("void UISkillHub::UpdateInput(const "
                            "GameUiSnapshot&") != std::string::npos,
                "the snapshot interaction phase must exist");
  CHECK_MESSAGE(source.find("TrySelectMastery(") == std::string::npos,
                "the legacy mastery selector must be gone");
  CHECK_MESSAGE(source.find("GameUiIntentKind::SkillSelectMastery") !=
                    std::string::npos,
                "mastery selection must enqueue SkillSelectMastery");
  CHECK_MESSAGE(source.find("GameUiIntentKind::SkillSetAttunement") !=
                    std::string::npos,
                "attunement must enqueue SkillSetAttunement");
  CHECK_MESSAGE(source.find("GameUiIntentKind::SkillSetDebugUnlock") !=
                    std::string::npos,
                "the debug override must enqueue SkillSetDebugUnlock");
  CHECK_MESSAGE(source.find("GameUiIntentKind::SkillAssign") !=
                    std::string::npos,
                "drops must enqueue SkillAssign");
  CHECK_MESSAGE(source.find("GameUiIntentKind::SkillUnassign") !=
                    std::string::npos,
                "unassign must enqueue SkillUnassign");
  CHECK_MESSAGE(source.find("void UISkillHub::PaintCanvas(UiRect") !=
                    std::string::npos,
                "the backend painter canvas must exist");
}

TEST_CASE("[Tech] R8 - the astrolabe never touches the registry and routes "
          "star allocation / vows as intents (B-01)") {
  // The legacy AstrolabeController::Draw(registry, player) walked the
  // AstrolabeComponent and wrote points/vows directly. R8: Update(snapshot,
  // input) + Paint(draw list) + DrawInternal (backend painter), with the
  // authoritative writes in the command handler.
  const std::string source =
      ReadSource("src/game/application/ui/AstrolabeController.cpp");
  REQUIRE_MESSAGE(!source.empty(), "AstrolabeController.cpp not found");
  CHECK_MESSAGE(source.find("entt::registry") == std::string::npos,
                "the astrolabe must not take a registry");
  CHECK_MESSAGE(source.find("void AstrolabeController::Update(const "
                            "GameUiSnapshot&") != std::string::npos,
                "Update must take (snapshot, input)");
  CHECK_MESSAGE(source.find("GameUiIntentKind::AstrolabeAddPoint") !=
                    std::string::npos,
                "star allocation must enqueue AstrolabeAddPoint");
  CHECK_MESSAGE(source.find("GameUiIntentKind::AstrolabeTakeVow") !=
                    std::string::npos,
                "vows must enqueue AstrolabeTakeVow");
  CHECK_MESSAGE(source.find("void AstrolabeController::DrawInternal()") !=
                    std::string::npos,
                "the backend painter canvas must exist");
  CHECK_MESSAGE(source.find("void AstrolabeController::Toggle()") !=
                    std::string::npos,
                "Toggle must take no registry");
  CHECK_MESSAGE(source.find("m_renderer.Draw(") != std::string::npos,
                "raylib drawing must delegate to the renderer");
}

TEST_CASE("[Tech] R8 - the tooltip carries domain ids, never entt::entity "
          "(B-01)") {
  // The legacy TooltipController stored entt::entity hover state and took
  // (registry) in UpdateState / DrawTooltip. R8: hover/active items are
  // stable domain ids and the content reads the snapshot view model.
  const std::string header =
      ReadSource("src/game/application/ui/TooltipController.hpp");
  REQUIRE_MESSAGE(!header.empty(), "TooltipController.hpp not found");
  CHECK_MESSAGE(header.find("entt::entity m_") == std::string::npos,
                "no entt::entity session state may be stored");
  CHECK_MESSAGE(header.find("SetHoveredItem(") == std::string::npos,
                "the entt::entity hover setter must be gone");
  CHECK_MESSAGE(header.find("SetHoveredItemDomain(") != std::string::npos,
                "the domain-id hover setter must exist");
  CHECK_MESSAGE(header.find("ActiveTooltipItemDomain(") != std::string::npos,
                "the domain-id active accessor must exist");
  CHECK_MESSAGE(header.find("UpdateState(const GameUiSnapshot&") !=
                    std::string::npos,
                "UpdateState must read the snapshot");
  CHECK_MESSAGE(header.find("void Paint(UiDrawList&") != std::string::npos,
                "the tooltip must paint through the draw list");
}

TEST_CASE("[Tech] R8 - the skill-tree controller takes no registry and closes "
          "the astrolabe through the host channel (B-01)") {
  // The legacy SkillTreeController::Toggle(registry) reached into the
  // SharedContext to close the astrolabe. R8: Toggle() is parameterless and
  // sibling closing goes through GameUiHost::CloseAstrolabe.
  const std::string source =
      ReadSource("src/game/application/ui/SkillTreeController.cpp");
  REQUIRE_MESSAGE(!source.empty(), "SkillTreeController.cpp not found");
  CHECK_MESSAGE(source.find("entt::registry") == std::string::npos,
                "the controller must not take a registry");
  CHECK_MESSAGE(source.find("SharedContext") == std::string::npos,
                "the SharedContext reach must be gone");
  CHECK_MESSAGE(source.find("void SkillTreeController::Toggle()") !=
                    std::string::npos,
                "Toggle must take no registry");
  CHECK_MESSAGE(source.find("CloseAstrolabe()") != std::string::npos,
                "sibling closing must use the host channel");
  CHECK_MESSAGE(source.find("void SkillTreeController::Update(const "
                            "GameUiSnapshot&") != std::string::npos,
                "the snapshot interaction phase must exist");
  CHECK_MESSAGE(source.find("void SkillTreeController::Paint(UiDrawList&") !=
                    std::string::npos,
                "the draw-list paint entry must exist");
}

TEST_CASE("[Tech] R8 - the host drives the skill-tree interaction phase every "
          "frame (B-01)") {
  // BUG: the host never called SkillTreeController::Update, so the hub/talent
  // tree painters captured no snapshot (m_paint.snapshot stayed null), the
  // tree rendered nothing while its visible flag still blocked gameplay input.
  // The host Update must feed the controller Update(snapshot, input) every
  // frame, like the other migrated panels (stash/astrolabe/crafting).
  const std::string source =
      ReadSource("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_MESSAGE(!source.empty(), "GameUiHost.cpp not found");
  CHECK_MESSAGE(source.find("m_skillTree.Update(m_snapshot, uiInput)") !=
                    std::string::npos,
                "the host must drive the skill-tree Update(snapshot, input)");
}

TEST_CASE("[Tech] R8 - the host frame input carries sustained buttons and "
          "wheel (B-01)") {
  // BUG: the UiInputFrame builder only populated pressed/released/pressedRight
  // edges. The astrolabe camera pan/zoom, the talent-tree pan/zoom and the vow
  // hold-to-confirm all read pointer.down/rightDown/mouseWheel, which stayed
  // at their defaults — the N panel could no longer be panned or zoomed.
  const std::string source =
      ReadSource("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_MESSAGE(!source.empty(), "GameUiHost.cpp not found");
  CHECK_MESSAGE(source.find("uiInput.pointer.down = IsMouseButtonDown(") !=
                    std::string::npos,
                "the frame input must carry the held left-button state");
  CHECK_MESSAGE(source.find("uiInput.pointer.rightDown = IsMouseButtonDown(") !=
                    std::string::npos,
                "the frame input must carry the held right-button state");
  CHECK_MESSAGE(source.find("uiInput.pointer.mouseWheel = GetMouseWheelMove();") !=
                    std::string::npos,
                "the frame input must carry the wheel delta");
}
