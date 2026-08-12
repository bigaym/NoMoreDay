#include "doctest.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// R6 (remediation, design §3.1/§3.4/§3.5): structural regression guards for
// the migrated overlay / character / inventory surfaces. They lock the source
// shape of the B-01 remediation: the quantity popup, context menu, character
// confirm and inventory interactions must no longer mutate gameplay state from
// the paint/draw phase, and the C-01 material filter must be a revision-keyed
// cache instead of per-frame filteredList/lowercase allocations. A future edit
// that reintroduces any of these paths fails the build test run.

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

TEST_CASE("[Tech] R6 - the overlay never executes gameplay mutators in the "
          "paint path (B-01)") {
  // The legacy OverlayController::DrawQuantityPopup called
  // InventorySystem::destroyItem/dropItem directly from the draw phase. R6
  // moved those behind ConfirmQuantityPopup, which enqueues DropItem/DestroyItem
  // intents executed by the GameUiCommandHandler in the next Update phase.
  const std::string source =
      ReadSource("src/game/application/ui/OverlayController.cpp");
  REQUIRE_MESSAGE(!source.empty(), "OverlayController.cpp not found");
  CHECK_MESSAGE(source.find("InventorySystem::destroyItem") ==
                    std::string::npos,
                "destroyItem must not be called from the overlay");
  CHECK_MESSAGE(source.find("InventorySystem::dropItem") == std::string::npos,
                "dropItem must not be called from the overlay");
  CHECK_MESSAGE(source.find("InventorySystem::") == std::string::npos,
                "no InventorySystem gameplay write may live in the overlay");
  CHECK_MESSAGE(source.find("ConfirmQuantityPopup") != std::string::npos,
                "the intent-enqueueing confirm seam must exist");
  CHECK_MESSAGE(source.find("GameUiIntentKind::DropItem") !=
                    std::string::npos,
                "the confirm must enqueue a DropItem intent");
  CHECK_MESSAGE(source.find("GameUiIntentKind::DestroyItem") !=
                    std::string::npos,
                "the confirm must enqueue a DestroyItem intent");
  CHECK_MESSAGE(source.find("void OverlayController::DrawQuantityPopup") ==
                    std::string::npos,
                "the legacy draw-phase popup definition must be gone");
  CHECK_MESSAGE(source.find("void OverlayController::DrawContextMenu") ==
                    std::string::npos,
                "the legacy draw-phase context menu definition must be gone");
}

TEST_CASE("[Tech] R6 - the character panel never writes attributes from the "
          "paint path (B-01)") {
  // The legacy UICharacterController::Draw get_or_emplace'd an
  // AttributeUIComponent, mutated PrimaryStats and flagged StatsDirty from the
  // draw phase. R6: the confirm click enqueues a ConfirmAttributeAllocation
  // intent (AllocateAttributePoints executes in the handler); the draft points
  // stay controller-local.
  const std::string source =
      ReadSource("src/game/application/ui/UICharacterController.cpp");
  REQUIRE_MESSAGE(!source.empty(), "UICharacterController.cpp not found");
  CHECK_MESSAGE(source.find("get_or_emplace<AttributeUIComponent>") ==
                    std::string::npos,
                "no AttributeUIComponent write may live in the controller");
  CHECK_MESSAGE(source.find("AttributeUIComponent") == std::string::npos,
                "the confirm state must not leak into an ECS component");
  CHECK_MESSAGE(source.find("StatsDirty") == std::string::npos,
                "no StatsDirty flag may be set from the controller");
  CHECK_MESSAGE(source.find("PrimaryStats") == std::string::npos,
                "no PrimaryStats write may live in the controller");
  CHECK_MESSAGE(source.find("GameUiIntentKind::ConfirmAttributeAllocation") !=
                    std::string::npos,
                "the confirm must enqueue the allocation intent");
  CHECK_MESSAGE(source.find("EnqueueAllocationIntent") != std::string::npos,
                "the allocation intent seam must exist");
  CHECK_MESSAGE(source.find("void UICharacterController::Draw") ==
                    std::string::npos,
                "the legacy registry Draw must be gone");
}

TEST_CASE("[Tech] R6 - the inventory controller paints from the snapshot, "
          "never the registry (B-01)") {
  // The legacy UIInventoryController::Draw executed equip/unequip/use/drop/
  // lock/organize/bag/socket mutators and then kept using the
  // InventoryComponent*/EquipmentComponent* pointers. R6: Update enqueues
  // intents (handler executes them next Update), Paint reads only the
  // snapshot + Update-phase caches.
  const std::string source =
      ReadSource("src/game/application/ui/UIInventoryController.cpp");
  REQUIRE_MESSAGE(!source.empty(), "UIInventoryController.cpp not found");
  CHECK_MESSAGE(source.find("InventorySystem::") == std::string::npos,
                "no InventorySystem gameplay write may live in the controller");
  CHECK_MESSAGE(source.find("StashSystem::") == std::string::npos,
                "no StashSystem gameplay write may live in the controller");
  CHECK_MESSAGE(source.find("CraftingSystem::") == std::string::npos,
                "no CraftingSystem gameplay write may live in the controller");
  CHECK_MESSAGE(source.find("Registry&") == std::string::npos,
                "the controller interface must not take a registry");
  CHECK_MESSAGE(source.find("registry.view<") == std::string::npos,
                "the controller must not iterate the ECS registry");
  CHECK_MESSAGE(source.find("registry.try_get<") == std::string::npos,
                "the controller must not read components");
  CHECK_MESSAGE(source.find("void UIInventoryController::Draw") ==
                    std::string::npos,
                "the legacy registry Draw must be gone");
  CHECK_MESSAGE(source.find("GameUiIntentKind::EquipItem") !=
                    std::string::npos,
                "drag-drop must enqueue the EquipItem intent");
  CHECK_MESSAGE(source.find("GameUiIntentKind::SocketRune") !=
                    std::string::npos,
                "rune socketing must enqueue the SocketRune intent");
  CHECK_MESSAGE(source.find("GameUiIntentKind::OrganizeInventory") !=
                    std::string::npos,
                "sort must enqueue the OrganizeInventory intent");
}

TEST_CASE("[Tech] R6 - the inventory material filter is a revision-keyed cache "
          "(C-01)") {
  // The legacy Draw rebuilt filteredList (std::vector of pointers), a lowercase
  // search string and a per-material lowercase name string every frame. R6:
  // the cache rebuilds only when the snapshot revision, the query or the
  // category changed, and the per-material match is a zero-allocation
  // case-insensitive substring scan.
  const std::string source =
      ReadSource("src/game/application/ui/UIInventoryController.cpp");
  REQUIRE_MESSAGE(!source.empty(), "UIInventoryController.cpp not found");
  CHECK_MESSAGE(source.find("RebuildMaterialFilter") != std::string::npos,
                "the cache rebuild entry must exist");
  CHECK_MESSAGE(source.find("m_filterCacheRevision") != std::string::npos,
                "the cache must be keyed on the snapshot revision");
  CHECK_MESSAGE(source.find("std::string lowerSearch") == std::string::npos,
                "no per-frame lowercase query string may be allocated");
  CHECK_MESSAGE(source.find("std::string lowerName") == std::string::npos,
                "no per-material lowercase name string may be allocated");
  CHECK_MESSAGE(source.find("filteredList.push_back") == std::string::npos,
                "the legacy per-frame filteredList allocation must be gone");
  CHECK_MESSAGE(
      source.find("vector<const MaterialEntry*> filteredList") ==
          std::string::npos,
      "the legacy per-frame filteredList allocation must be gone");

  const std::string header =
      ReadSource("src/game/application/ui/UIInventoryController.hpp");
  REQUIRE_MESSAGE(!header.empty(), "UIInventoryController.hpp not found");
  CHECK_MESSAGE(header.find("MaterialFilterCount") != std::string::npos,
                "the cache accessor for tests must exist");
}

TEST_CASE("[Tech] R6 - the overlay/character/inventory surfaces paint through "
          "the host draw list (design §3.4)") {
  // R6 removed the immediate-mode draw passes: the host Draw phase no longer
  // calls m_inventory.Draw / DrawOverlays, and PrepareRender paints the three
  // migrated surfaces through the draw list.
  const std::string host = ReadSource("src/game/application/ui/GameUiHost.cpp");
  REQUIRE_MESSAGE(!host.empty(), "GameUiHost.cpp not found");
  CHECK_MESSAGE(host.find("m_inventory.Draw(") == std::string::npos,
                "the legacy inventory Draw must be gone from the host");
  CHECK_MESSAGE(host.find("m_overlay.DrawOverlays(") == std::string::npos,
                "the legacy overlay DrawOverlays must be gone from the host");
  CHECK_MESSAGE(host.find("m_inventory.Paint(m_drawList") != std::string::npos,
                "PrepareRender must paint the inventory panel");
  CHECK_MESSAGE(host.find("m_character.Paint(m_drawList") != std::string::npos,
                "PrepareRender must paint the character panel");
  CHECK_MESSAGE(host.find("m_overlay.Paint(m_drawList") != std::string::npos,
                "PrepareRender must paint the overlay surfaces");
  CHECK_MESSAGE(host.find("m_overlay.UpdateOverlays(") != std::string::npos,
                "Update must run the overlay interaction phase");
  CHECK_MESSAGE(host.find("m_character.UpdateInput(") != std::string::npos,
                "Update must run the character interaction phase");
}

TEST_CASE("[Tech] R6 - UIRenderer no longer draws the migrated immediate-mode "
          "surfaces (B-01)") {
  // The legacy UIRenderer::DrawContextMenu executed equip/use/unequip/drop and
  // wrote itemComp->isLocked from the draw phase; DrawMessageBox is a residual
  // of the R4 migration. Both are deleted in R6.
  const std::string source = ReadSource("src/game/application/ui/UIRenderer.cpp");
  REQUIRE_MESSAGE(!source.empty(), "UIRenderer.cpp not found");
  CHECK_MESSAGE(source.find("UIRenderer::DrawContextMenu") ==
                    std::string::npos,
                "the immediate-mode context menu draw must be gone");
  CHECK_MESSAGE(source.find("UIRenderer::DrawMessageBox") ==
                    std::string::npos,
                "the residual immediate message box draw must be gone");
  CHECK_MESSAGE(source.find("isLocked = !") == std::string::npos,
                "no direct isLocked write may live in the renderer");
  CHECK_MESSAGE(source.find("InventorySystem::") == std::string::npos,
                "no InventorySystem write may live in the renderer");
}
