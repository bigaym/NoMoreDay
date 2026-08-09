#include "doctest.h"

#include "game/application/ui/UISystem.hpp"

namespace NoMoreDay {

TEST_CASE("[Integration] UISystem - Panel Drag initializes default panel position") {
  UISystem::State.panelStates[(int)UIPanelID::Inventory] = PanelState{};
  UISystem::State.activeDragPanel = UIPanelID::None;
  UISystem::State.scaleFactor = 1.0f;

  float x = 150.0f;
  float y = 260.0f;
  UISystem::UpdatePanelDrag(UIPanelID::Inventory, x, y, 240.0f, 320.0f, 48.0f);

  const PanelState &panel = UISystem::State.panelStates[(int)UIPanelID::Inventory];
  CHECK(panel.position.x == doctest::Approx(150.0f));
  CHECK(panel.position.y == doctest::Approx(260.0f));
}

TEST_CASE("[Integration] UISystem - Panel Drag clears stale drag when button is not held") {
  PanelState panel;
  panel.position = {410.0f, 470.0f};
  panel.isDragging = true;
  panel.dragOffset = {15.0f, 15.0f};
  UISystem::State.panelStates[(int)UIPanelID::Inventory] = panel;
  UISystem::State.activeDragPanel = UIPanelID::Inventory;
  UISystem::State.scaleFactor = 1.0f;

  float x = 410.0f;
  float y = 470.0f;
  UISystem::UpdatePanelDrag(UIPanelID::Inventory, x, y, 240.0f, 320.0f, 48.0f);

  const PanelState &updated = UISystem::State.panelStates[(int)UIPanelID::Inventory];
  CHECK_FALSE(updated.isDragging);
  CHECK(UISystem::State.activeDragPanel == UIPanelID::None);
}

TEST_CASE("[Integration] UISystem - Quantity popup reports modal input capture") {
  UISystem::State.showQuantityPopup = false;
  CHECK_FALSE(UISystem::IsModalInputCaptured());

  UISystem::State.showQuantityPopup = true;
  CHECK(UISystem::IsModalInputCaptured());

  UISystem::State.showQuantityPopup = false;
}

TEST_CASE("[Integration] UISystem - Quantity popup close clears typing flag") {
  entt::registry registry;

  UISystem::State.showQuantityPopup = true;
  UISystem::State.isTyping = true;
  UISystem::State.quantityTargetItem = entt::null;

  UISystem::DrawQuantityPopup(registry);

  CHECK_FALSE(UISystem::State.showQuantityPopup);
  CHECK_FALSE(UISystem::State.isTyping);
}

} // namespace NoMoreDay
