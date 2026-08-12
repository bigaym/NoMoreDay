#include "doctest.h"

#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/OverlayController.hpp"
#include "game/application/ui/UiRuntime.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/ItemComponent.hpp"

#include <fstream>
#include <iterator>
#include <string>

namespace NoMoreDay {

// U8 final: UISystem::UpdatePanelDrag / IsModalInputCaptured /
// DrawQuantityPopup are gone; the panel-drag state lives in each panel
// controller (UIPanelDragServiceTests covers the drag math) and the modal
// input gate lives on GameUiHost, driven by the hosted overlay.

TEST_CASE("[Integration] GameUiHost - modal input capture follows the quantity popup") {
  ui::UiRuntime runtime;
  ui::GameUiHost host;
  entt::registry registry;

  CHECK_FALSE(host.IsModalInputCaptured());

  host.OpenQuantityPopup(entt::null, 0);
  CHECK(host.IsModalInputCaptured());

  host.CloseQuantityPopup();
  CHECK_FALSE(host.IsModalInputCaptured());
}

TEST_CASE("[Integration] GameUiHost - modal input capture follows the skill tree") {
  ui::UiRuntime runtime;
  ui::GameUiHost host;
  entt::registry registry;

  host.ToggleSkillTree(registry);
  CHECK(host.IsModalInputCaptured());

  host.CloseSkillTree();
  CHECK_FALSE(host.IsModalInputCaptured());
}

TEST_CASE("[Integration] OverlayController - quantity popup close clears typing") {
  ui::UiRuntime runtime;
  ui::OverlayController overlay(runtime);

  entt::registry registry;
  const entt::entity item = registry.create();
  // DrawQuantityPopup requires a valid item (with a quantity) and a player
  // tag; otherwise it closes the popup immediately (legacy semantics).
  registry.emplace<ItemComponent>(item);
  registry.get<ItemComponent>(item).quantity = 5;
  registry.emplace<PlayerTag>(registry.create());

  overlay.OpenQuantityPopup(item, 1);
  CHECK(overlay.IsQuantityPopupVisible());
  // isTyping is set by the draw pass while the popup input is live (legacy
  // DrawQuantityPopup semantics), not by OpenQuantityPopup.
  BeginDrawing();
  overlay.DrawOverlays(registry);
  EndDrawing();
  CHECK(overlay.IsTyping());

  overlay.CloseQuantityPopup();
  CHECK_FALSE(overlay.IsQuantityPopupVisible());
  CHECK_FALSE(overlay.IsTyping());
}

TEST_CASE("[Integration] panel controllers own instance drag state") {
  // U8 final: no panel controller reads the removed UISystem::State panel
  // drag fields (PanelState/activeDragPanel live per controller). The
  // controllers call UIPanelDragService::UpdatePanelDrag directly, so only
  // the legacy UISystem::UpdatePanelDrag entry point is forbidden.
  const std::string needles[] = {"UISystem::UpdatePanelDrag", "State.panelStates",
                                 "State.activeDragPanel"};
  const std::string files[] = {
      "src/game/application/ui/UIInventoryController.cpp",
      "src/game/application/ui/UIStashController.cpp",
      "src/game/application/ui/UICharacterController.cpp",
      "src/game/application/ui/UICraftingController.cpp",
  };
  for (const auto& file : files) {
    std::ifstream in(file);
    REQUIRE_MESSAGE(in.good(), "cannot open ", file);
    const std::string contents((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    for (const auto& needle : needles) {
      CHECK_MESSAGE(contents.find(needle) == std::string::npos, file, " -> ",
                    needle);
    }
  }
}

} // namespace NoMoreDay
