#include "doctest.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// R7 (remediation, design §3.1/§3.3/§3.4): structural regression guards for
// the migrated stash / crafting surfaces. They lock the source shape of the
// B-01 remediation: the stash controller no longer keeps a StashTab* across
// operations and the crafting controller no longer holds entt::entity session
// targets; all stash/crafting gameplay actions are intents executed by the
// GameUiCommandHandler; both panels paint only from the snapshot view model;
// and the stash unlock cost reads a single authority (StashSystem), not the
// legacy StashConfig constant table. A future edit that reintroduces any of
// these paths fails the build test run.

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

TEST_CASE("[Tech] R7 - the stash controller never touches gameplay systems or "
          "the registry (B-01)") {
  // The legacy UIStashController::Draw called StashSystem::unlockTab / sortTab
  // / autoDeposit / quickWithdraw / transferItem / depositFromInventory and
  // held a StashTab* across the grid loop. R7: Update enqueues
  // StashTransfer/StashDeposit/StashWithdraw/StashUnlockTab/StashSort/
  // StashAutoDeposit intents, Paint reads only snapshot.stash + Update-phase
  // state, and the tab is re-fetched by the handler per operation.
  const std::string source =
      ReadSource("src/game/application/ui/UIStashController.cpp");
  REQUIRE_MESSAGE(!source.empty(), "UIStashController.cpp not found");
  CHECK_MESSAGE(source.find("StashSystem::") == std::string::npos,
                "no StashSystem gameplay write may live in the controller");
  CHECK_MESSAGE(source.find("entt::registry") == std::string::npos,
                "the controller must not take a registry");
  CHECK_MESSAGE(source.find("registry.view<") == std::string::npos,
                "the controller must not iterate the ECS registry");
  CHECK_MESSAGE(source.find("registry.try_get<") == std::string::npos,
                "the controller must not read components");
  CHECK_MESSAGE(source.find("void UIStashController::Draw") ==
                    std::string::npos,
                "the legacy registry Draw must be gone");
  CHECK_MESSAGE(source.find("StashTab*") == std::string::npos,
                "no StashTab* may be stored or held across operations");
  CHECK_MESSAGE(source.find("GameUiIntentKind::StashUnlockTab") !=
                    std::string::npos,
                "unlock must enqueue the StashUnlockTab intent");
  CHECK_MESSAGE(source.find("GameUiIntentKind::StashDeposit") !=
                    std::string::npos,
                "inventory drops must enqueue the StashDeposit intent");
  CHECK_MESSAGE(source.find("GameUiIntentKind::StashWithdraw") !=
                    std::string::npos,
                "quick-withdraw must enqueue the StashWithdraw intent");
  CHECK_MESSAGE(source.find("Paint(UiDrawList&") != std::string::npos,
                "the snapshot paint entry must exist");
}

TEST_CASE("[Tech] R7 - the crafting controller never executes crafting/salvage "
          "mutators and holds only domain-id targets (B-01)") {
  // The legacy UICraftingController::Draw called
  // CraftingSystem::upgradeAffix/chaosAffix/refineAffixValues/addAffix/
  // fuseLegendary and SalvageSystem::Execute/BatchExecute directly, and kept
  // entt::entity session targets (m_forgeItem/m_mergeBase/...). R7: session
  // targets are stable domain ids, all actions are intents, and ItemComponent
  // is only touched by the handler right before a single system call.
  const std::string source =
      ReadSource("src/game/application/ui/UICraftingController.cpp");
  REQUIRE_MESSAGE(!source.empty(), "UICraftingController.cpp not found");
  CHECK_MESSAGE(source.find("CraftingSystem::") == std::string::npos,
                "no CraftingSystem gameplay write may live in the controller");
  CHECK_MESSAGE(source.find("SalvageSystem::") == std::string::npos,
                "no SalvageSystem gameplay write may live in the controller");
  CHECK_MESSAGE(source.find("entt::registry") == std::string::npos,
                "the controller must not take a registry");
  CHECK_MESSAGE(source.find("registry.get<") == std::string::npos,
                "the controller must not fetch ItemComponent");
  CHECK_MESSAGE(source.find("void UICraftingController::Draw") ==
                    std::string::npos,
                "the legacy registry Draw must be gone");
  CHECK_MESSAGE(source.find("GameUiIntentKind::CraftAffixUpgrade") !=
                    std::string::npos,
                "affix upgrade must enqueue the CraftAffixUpgrade intent");
  CHECK_MESSAGE(source.find("GameUiIntentKind::CraftFuse") !=
                    std::string::npos,
                "fusion must enqueue the CraftFuse intent");
  CHECK_MESSAGE(source.find("GameUiIntentKind::CraftSalvage") !=
                    std::string::npos,
                "salvage must enqueue the CraftSalvage intent");
  CHECK_MESSAGE(source.find("GameUiIntentKind::CraftBatchSalvage") !=
                    std::string::npos,
                "batch salvage must enqueue the CraftBatchSalvage intent");
  CHECK_MESSAGE(source.find("Paint(UiDrawList&") != std::string::npos,
                "the snapshot paint entry must exist");
}

TEST_CASE("[Tech] R7 - the crafting controller header stores no entt::entity "
          "session targets (B-01)") {
  // The domain-id refactor is structural: the five session targets must be
  // std::uint64_t, never entt::entity (the legacy m_forgeItem/m_mergeBase/
  // m_mergeFodder/m_mergeCatalyst/m_salvageItem).
  const std::string header =
      ReadSource("src/game/application/ui/UICraftingController.hpp");
  REQUIRE_MESSAGE(!header.empty(), "UICraftingController.hpp not found");
  CHECK_MESSAGE(header.find("entt::entity m_") == std::string::npos,
                "no entt::entity session target may be stored");
  CHECK_MESSAGE(header.find("GetForgeTargetDomainId") != std::string::npos,
                "the domain-id forge accessor must exist");
  CHECK_MESSAGE(header.find("GetSalvageItemDomainId") != std::string::npos,
                "the domain-id salvage accessor must exist");
  CHECK_MESSAGE(header.find("ClearConsumedTarget") != std::string::npos,
                "the host-side consumed-target clear seam must exist");
}

TEST_CASE("[Tech] R7 - the snapshot builder reads the stash unlock cost from "
          "the StashSystem authority") {
  // The legacy builder read NoMoreDay::Constants::StashConfig::getUnlockCost
  // directly (a second price table besides StashSystem::getNextUnlockCost).
  // R7 unifies the read on StashSystem::getNextUnlockCost.
  const std::string source =
      ReadSource("src/game/application/ui/GameUiSnapshotBuilder.cpp");
  REQUIRE_MESSAGE(!source.empty(), "GameUiSnapshotBuilder.cpp not found");
  CHECK_MESSAGE(source.find("StashConfig::getUnlockCost") ==
                    std::string::npos,
                "the StashConfig constant-table read must be gone");
  CHECK_MESSAGE(source.find("getNextUnlockCost") != std::string::npos,
                "the unlock cost must come from StashSystem");
}

TEST_CASE("[Tech] R7 - the host paints stash/crafting through the draw list, "
          "never immediate-mode (design §3.4)") {
  // R7 removed the immediate-mode draw passes: the host Draw phase no longer
  // calls m_stash.Draw / m_crafting.Draw, and PrepareRender paints both panels
  // through the draw list; Update drives them from the snapshot + input frame.
  const std::string host = ReadSource("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_MESSAGE(!host.empty(), "GameUiHost.cpp not found");
  CHECK_MESSAGE(host.find("m_stash.Draw(") == std::string::npos,
                "the legacy stash Draw must be gone from the host");
  CHECK_MESSAGE(host.find("m_crafting.Draw(") == std::string::npos,
                "the legacy crafting Draw must be gone from the host");
  CHECK_MESSAGE(host.find("m_stash.Paint(m_drawList") != std::string::npos,
                "PrepareRender must paint the stash panel");
  CHECK_MESSAGE(host.find("m_crafting.Paint(m_drawList") != std::string::npos,
                "PrepareRender must paint the crafting panel");
  CHECK_MESSAGE(host.find("m_stash.Update(m_snapshot") != std::string::npos,
                "Update must drive the stash from the snapshot");
  CHECK_MESSAGE(host.find("m_crafting.Update(m_snapshot") !=
                    std::string::npos,
                "Update must drive the crafting from the snapshot");
  CHECK_MESSAGE(host.find("ClearConsumedTarget") != std::string::npos,
                "the result loop must clear consumed crafting targets");
}
