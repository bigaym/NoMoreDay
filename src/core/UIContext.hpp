#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "../components/ItemComponent.hpp"

namespace NoMoreDay {

    struct UIContext {
        // Global Font Resource
        Font globalFont = { 0 };
        
        // UI Scale
        float scaleFactor = 1.0f;

        // State Flags
        bool isMouseOverUI = false; // Tracks if mouse is over any UI element
        bool showCharacterPanel = false;
        float characterPanelAlpha = 0.0f; // New
        
        bool showInventory = false;
        float inventoryAlpha = 0.0f; // New
        
        bool showContextMenu = false;

        // Drag & Drop State
        entt::entity draggedItem = entt::null;
        bool isDraggingFromInventory = false;
        int dragSourceInventoryIndex = -1;
        EquipmentSlot dragSourceEquipmentSlot = EquipmentSlot::None;
        int dragSourceBagSlotIndex = -1;

        // Interaction State
        entt::entity hoveredItem = entt::null;
        int hoveredSkillSlot = -1; // 0-4
        int hoveredBuffIdx = -1;

        // Interaction Alphas/Scales
        struct ElementAnim {
            float hoverValue = 0.0f; // 0 to 1
            float scale = 1.0f;
        };
        std::vector<ElementAnim> inventorySlotAnims;
        std::vector<ElementAnim> equipmentSlotAnims;
        std::vector<ElementAnim> bagSlotAnims;

        // Message Box (Simple global overlay)
        bool showMessageBox = false;
        char messageBoxText[64] = { 0 };
        float messageBoxTimer = 0.0f;

        // Context Menu State
        entt::entity contextMenuItem = entt::null;
        Vector2 contextMenuPos = { 0, 0 };
        bool isContextFromInventory = false;
        int contextSourceInventoryIndex = -1;
        EquipmentSlot contextSourceEquipmentSlot = EquipmentSlot::None;
        
        // Quantity Popup State
        bool showQuantityPopup = false;
        entt::entity quantityTargetItem = entt::null;
        int quantityActionType = 0; // 0: Drop, 1: Destroy
        int quantityVal = 1;
        int quantityMax = 1;
        char quantityInputBuf[16] = {0};
    };

}
