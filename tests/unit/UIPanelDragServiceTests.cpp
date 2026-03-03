#include "doctest.h"

#include "game/systems/ui/UIPanelDragService.hpp"

namespace NoMoreDay {

namespace {

UIPanelDragBounds MakeDefaultBounds() {
  UIPanelDragBounds bounds;
  bounds.panelWidth = 240.0f;
  bounds.panelHeight = 320.0f;
  bounds.headerHeight = 48.0f;
  bounds.minVisiblePixels = 50.0f;
  bounds.uiRefWidth = 1920.0f;
  bounds.uiRefHeight = 1080.0f;
  return bounds;
}

} // namespace

TEST_CASE("[Unit] UIPanelDragService - starts drag and captures offset") {
  PanelState panelState;
  UIPanelID activePanel = UIPanelID::None;
  float x = 200.0f;
  float y = 300.0f;

  UIPanelDragInputs input;
  input.mousePosition = {225.0f, 320.0f};
  input.isMousePressed = true;
  input.isMouseDown = true;

  const UIPanelDragBounds bounds = MakeDefaultBounds();
  UIPanelDragService::UpdatePanelDrag(panelState, UIPanelID::Inventory,
                                      activePanel, x, y, input, bounds);

  CHECK(activePanel == UIPanelID::Inventory);
  CHECK(panelState.isDragging);
  CHECK(panelState.dragOffset.x == doctest::Approx(25.0f));
  CHECK(panelState.dragOffset.y == doctest::Approx(20.0f));
  CHECK(x == doctest::Approx(200.0f));
  CHECK(y == doctest::Approx(300.0f));
}

TEST_CASE("[Unit] UIPanelDragService - clamps dragged position to panel bounds") {
  PanelState panelState;
  panelState.position = {300.0f, 300.0f};
  panelState.isDragging = true;
  panelState.dragOffset = {10.0f, 10.0f};

  UIPanelID activePanel = UIPanelID::Inventory;
  float x = panelState.position.x;
  float y = panelState.position.y;

  UIPanelDragInputs input;
  input.mousePosition = {-500.0f, 2000.0f};
  input.isMouseDown = true;

  const UIPanelDragBounds bounds = MakeDefaultBounds();
  UIPanelDragService::UpdatePanelDrag(panelState, UIPanelID::Inventory,
                                      activePanel, x, y, input, bounds);

  CHECK(x == doctest::Approx(-190.0f));
  CHECK(y == doctest::Approx(1030.0f));
  CHECK(panelState.position.x == doctest::Approx(x));
  CHECK(panelState.position.y == doctest::Approx(y));
}

TEST_CASE("[Unit] UIPanelDragService - ignores drag start while another panel is active") {
  PanelState panelState;
  panelState.position = {200.0f, 300.0f};

  UIPanelID activePanel = UIPanelID::Character;
  float x = 200.0f;
  float y = 300.0f;

  UIPanelDragInputs input;
  input.mousePosition = {220.0f, 320.0f};
  input.isMousePressed = true;
  input.isMouseDown = true;

  const UIPanelDragBounds bounds = MakeDefaultBounds();
  UIPanelDragService::UpdatePanelDrag(panelState, UIPanelID::Inventory,
                                      activePanel, x, y, input, bounds);

  CHECK(activePanel == UIPanelID::Character);
  CHECK_FALSE(panelState.isDragging);
}

TEST_CASE("[Unit] UIPanelDragService - releases active drag when button is up") {
  PanelState panelState;
  panelState.position = {400.0f, 500.0f};
  panelState.isDragging = true;

  UIPanelID activePanel = UIPanelID::Inventory;
  float x = panelState.position.x;
  float y = panelState.position.y;

  UIPanelDragInputs input;
  input.mousePosition = {450.0f, 550.0f};
  input.isMouseDown = false;

  const UIPanelDragBounds bounds = MakeDefaultBounds();
  UIPanelDragService::UpdatePanelDrag(panelState, UIPanelID::Inventory,
                                      activePanel, x, y, input, bounds);

  CHECK_FALSE(panelState.isDragging);
  CHECK(activePanel == UIPanelID::None);
}

} // namespace NoMoreDay
