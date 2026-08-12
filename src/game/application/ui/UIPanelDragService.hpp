#pragma once

#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/StashComponent.hpp"

#include <cstdint>

#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay {

// Panel Management. U8 收尾: 自 UIContext.hpp 迁入（UIContext 静态对象已删）。
enum class UIPanelID {
  None = -1,
  Character = 0,
  Inventory,
  Crafting,
  Stash,
  Count
};

struct PanelState {
  Vector2 position = {-1.0f, -1.0f};  // -1 indicates uninitialized (use default)
  bool isDragging = false;
  Vector2 dragOffset = {0.0f, 0.0f};
};

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

// U8: single instance owner of the cross-panel drag session. The inventory /
// stash controllers and the skill hotbar/hub route every drag state read and
// write through the UIDragSession the GameUiHost owns (via the host
// back-pointer); GameplayState's drag cleanup and the drag-phantom pass read
// it from the same instance. This replaces the legacy
// UISystem::State.draggedItem / isDraggingFrom* / dragSource* / draggedSkillId
// / isDraggingSkill fields as the authoritative drag state.
struct UIDragSession {
  entt::entity draggedItem = entt::null;
  bool isDraggingFromInventory = false;
  int dragSourceInventoryIndex = -1;
  EquipmentSlot dragSourceEquipmentSlot = EquipmentSlot::None;
  int dragSourceBagSlotIndex = -1;
  bool isDraggingFromStash = false;
  int dragSourceStashTab = -1;
  int dragSourceStashSlot = -1;
  StashType dragSourceStashType = StashType::Personal;
  uint32_t draggedSkillId = NoMoreDay::INVALID_SKILL_ID;
  bool isDraggingSkill = false;

  void Clear() noexcept { *this = UIDragSession{}; }
  [[nodiscard]] bool IsDragging() const noexcept {
    return draggedItem != entt::null || isDraggingSkill;
  }
};

class UIPanelDragService {
public:
  static void UpdatePanelDrag(PanelState &panelState, UIPanelID panelId,
                              UIPanelID &activePanel, float &x, float &y,
                              const UIPanelDragInputs &input,
                              const UIPanelDragBounds &bounds);
};

} // namespace NoMoreDay
