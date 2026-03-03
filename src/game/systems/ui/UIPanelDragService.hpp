#pragma once

#include "game/systems/ui/UIContext.hpp"

namespace NoMoreDay {

struct UIPanelDragInputs {
  Vector2 mousePosition = {0.0f, 0.0f};
  bool isMousePressed = false;
  bool isMouseDown = false;
};

struct UIPanelDragBounds {
  float panelWidth = 0.0f;
  float panelHeight = 0.0f;
  float headerHeight = 0.0f;
  float minVisiblePixels = 50.0f;
  float uiRefWidth = 0.0f;
  float uiRefHeight = 0.0f;
};

class UIPanelDragService {
public:
  static void UpdatePanelDrag(PanelState &panelState, UIPanelID panelId,
                              UIPanelID &activePanel, float &x, float &y,
                              const UIPanelDragInputs &input,
                              const UIPanelDragBounds &bounds);
};

} // namespace NoMoreDay
