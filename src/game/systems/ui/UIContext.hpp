#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "game/components/ItemComponent.hpp"
#include "game/components/StashComponent.hpp"

namespace NoMoreDay {

    // Panel Management
    enum class UIPanelID {
        None = -1,
        Character = 0,
        Inventory,
        Crafting,
        Stash,
        Count
    };

    struct PanelState {
        Vector2 position = { -1.0f, -1.0f }; // -1 indicates uninitialized (use default)
        bool isDragging = false;
        Vector2 dragOffset = { 0.0f, 0.0f };
    };

    struct UIContext {
        // Cached Player Reference
        entt::entity playerEntity = entt::null;

        // Global Font Resource
        Font globalFont = { 0 };
        Font emojiFont = { 0 };
        
        // UI Scale
        float scaleFactor = 1.0f;

        // State Flags
        bool isMouseOverUI = false; // Tracks if mouse is over any UI element
        bool isTyping = false; // Tracks if user is typing in any input field (blocks gameplay input)
        bool showCharacterPanel = false;
        float characterPanelAlpha = 0.0f; // New
        
        bool showInventory = false;
        float inventoryAlpha = 0.0f; // New

        bool showStash = false;
        float stashAlpha = 0.0f; 
        
        bool showSkillTree = false; // New: Skill Specialization UI (Hotkey: S)
        float skillTreeAlpha = 0.0f; // New
        uint32_t selectedSkillId = 0; // Skill currently being viewed in talent tree

        // Panel Management State
        PanelState panelStates[(int)UIPanelID::Count];
        UIPanelID activeDragPanel = UIPanelID::None;

        bool showContextMenu = false;

        // Drag & Drop State
        entt::entity draggedItem = entt::null;
        bool isDraggingFromInventory = false;
        int dragSourceInventoryIndex = -1;
        EquipmentSlot dragSourceEquipmentSlot = EquipmentSlot::None;
        int dragSourceBagSlotIndex = -1;

        // Stash Dragging
        bool isDraggingFromStash = false;
        int dragSourceStashTab = -1;
        int dragSourceStashSlot = -1;
        StashType dragSourceStashType = StashType::Personal;

        // Skill Dragging
        uint32_t draggedSkillId = 0;
        bool isDraggingSkill = false;

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
        bool isSkillContext = false;
        int contextSourceSkillSlot = -1;
        
        // Quantity Popup State
        bool showQuantityPopup = false;
        entt::entity quantityTargetItem = entt::null;
        int quantityActionType = 0; // 0: Drop, 1: Destroy
        int quantityVal = 1;
        int quantityMax = 1;
        char quantityInputBuf[16] = {0};
    };

}
