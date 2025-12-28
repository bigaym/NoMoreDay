#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "../components/ItemComponent.hpp"

namespace NoMoreDay {

// UI 全局共享状态
struct UIState_t {
    bool showCharacterPanel = false;
    bool showInventory = false;
    bool showContextMenu = false;
    
    // 拖拽状态
    entt::entity draggedItem = entt::null;
    bool isDraggingFromInventory = false;
    int dragSourceInventoryIndex = -1;
    EquipmentSlot dragSourceEquipmentSlot = EquipmentSlot::None;
    int dragSourceBagSlotIndex = -1;

    entt::entity hoveredItem = entt::null;

    // 消息提示框
    bool showMessageBox = false;
    char messageBoxText[64] = {0};
    float messageBoxTimer = 0.0f;
};

} // namespace NoMoreDay