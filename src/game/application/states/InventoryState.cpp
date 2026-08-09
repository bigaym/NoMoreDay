#include "game/application/states/InventoryState.hpp"
#include "game/application/ui/UISystem.hpp" // For State access and Facade
#include "game/application/ui/UIInventory.hpp"
#include "game/application/scene/StateManager.hpp"
#include "game/application/ui/UIRenderer.hpp" // Direct renderer access
#include "game/foundation/ui_shared/UiShared.hpp"

namespace NoMoreDay {

    void InventoryState::OnEnter() {
        // Sync with global context for compatibility
        UISystem::State.showInventory = true;
        // Reset specific UI states
        UISystem::State.showContextMenu = false;
        UISystem::State.draggedItem = entt::null;
    }

    void InventoryState::OnExit() {
        UISystem::State.showInventory = false;
        UISystem::State.showContextMenu = false;
        UISystem::State.draggedItem = entt::null;
        
        // Also ensure quantity popup is closed
        UISystem::State.showQuantityPopup = false;
        UISystem::State.isTyping = false;
    }

    bool InventoryState::OnUpdate(float dt) {
        auto& registry = *m_context->registry;

        // Close logic
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_I) || IsKeyPressed(KEY_TAB)) {
            // If context menu or popup is open, close them first (standard UI behavior)
            // But for simplicity in this refactor step, we just pop the state which cleans up everything in OnExit
            // However, a more polished UX might just close the top-most overlay.
            
            if (UISystem::State.showQuantityPopup) {
                UISystem::State.showQuantityPopup = false;
                UISystem::State.isTyping = false;
            } else if (UISystem::State.showContextMenu) {
                UISystem::State.showContextMenu = false;
            } else {
                m_stateManager->PopState();
            }
            return false; // Consumed input
        }

        // Update logic
        // We can call UIInventory::Update if it has logic, or handle inputs here.
        // Currently UIInventory::Update is empty, logic is in UIInventory::Draw (immediate modeish) or InputSystem.
        // But since we are blocking the underlying InputSystem (if it runs in GameplayState),
        // we might need to manually handle things if they relied on InputSystem.
        // Fortunately, UI interactions (mouse clicks) are usually checked in Draw or explicit Update.
        // UIInventory::Draw handles the click logic.
        
        UIInventory::Update(registry);

        return true; // Allow updates for states below (No more time-stop)
    }

    void InventoryState::OnRender() {
        auto& registry = *m_context->registry;
        
        // Reset per-frame state
        UiShared::HoveredItem() = entt::null;
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);

        // 1. Draw Inventory Panel
        // UIInventory::Draw handles the panel and slot interactions
        UIInventory::Draw(registry);

        // 2. Draw Overlays (Tooltip, ContextMenu, Dragging)
        // These need to be drawn ON TOP of the inventory.
        // Since UIInventory::Draw might populate 'hoveredItem', we draw tooltip after.
        
        // Tooltip
        if (UiShared::HoveredItem() != entt::null && registry.valid(UiShared::HoveredItem())) {
            UIRenderer::DrawTooltip(UISystem::State.globalFont, registry, UiShared::HoveredItem());
        }

        // Context Menu
        if (UISystem::State.showContextMenu) {
            UIRenderer::DrawContextMenu(UISystem::State.globalFont, UISystem::State, registry);
        }

        // Quantity Popup (Placeholder/Future)
        if (UISystem::State.showQuantityPopup) {
            UISystem::DrawQuantityPopup(registry); // Use the Facade for now as it contains the logic placeholder
        }
        
        // Message Box
        if (UISystem::State.showMessageBox) {
            UIRenderer::DrawMessageBox(UISystem::State.globalFont, UISystem::State);
        }

        // 3. Draw Phantom (Topmost)
        UISystem::DrawDraggingPhantom(registry);

        // 4. Cleanup Dragging at the very end of frame
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            UISystem::State.draggedItem = entt::null;
            UISystem::State.isDraggingSkill = false;
            UISystem::State.draggedSkillId = NoMoreDay::INVALID_SKILL_ID;
        }

        // Reset hovered item for next frame (Standard IMGUI pattern)
        // Note: UISystem::Draw did this at start of frame. 
        // We should do it here or let UIInventory::Draw handle it?
        // UIInventory::Draw sets it if collision happens.
        // But if we don't clear it, it might stick.
        // UISystem::Draw cleared it at the start.
        // So we should clear it at the start of OnRender.
    }

}
