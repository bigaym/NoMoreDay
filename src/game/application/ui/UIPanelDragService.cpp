#include "game/application/ui/UIPanelDragService.hpp"

#include "raylib.h"

#include <algorithm>

namespace NoMoreDay {

void UIPanelDragService::UpdatePanelDrag(PanelState &panelState,
                                         UIPanelID panelId,
                                         UIPanelID &activePanel, float &x,
                                         float &y,
                                         const UIPanelDragInputs &input,
                                         const UIPanelDragBounds &bounds) {
  if (panelState.position.x < 0.0f) {
    panelState.position = {x, y};
  }

  x = panelState.position.x;
  y = panelState.position.y;

  const bool isMouseOverHeader = CheckCollisionPointRec(
      input.mousePosition, {x, y, bounds.panelWidth, bounds.headerHeight});

  if (isMouseOverHeader && input.isMousePressed &&
      activePanel == UIPanelID::None) {
    activePanel = panelId;
    panelState.isDragging = true;
    panelState.dragOffset = {input.mousePosition.x - x,
                             input.mousePosition.y - y};
  }

  if (panelState.isDragging && activePanel == panelId) {
    if (input.isMouseDown) {
      panelState.position.x = input.mousePosition.x - panelState.dragOffset.x;
      panelState.position.y = input.mousePosition.y - panelState.dragOffset.y;

      panelState.position.x =
          std::clamp(panelState.position.x,
                     -bounds.panelWidth + bounds.minVisiblePixels,
                     bounds.uiRefWidth - bounds.minVisiblePixels);
      panelState.position.y =
          std::clamp(panelState.position.y,
                     -bounds.panelHeight + bounds.minVisiblePixels,
                     bounds.uiRefHeight - bounds.minVisiblePixels);

      x = panelState.position.x;
      y = panelState.position.y;
    } else {
      panelState.isDragging = false;
      activePanel = UIPanelID::None;
    }
  }
}

} // namespace NoMoreDay
