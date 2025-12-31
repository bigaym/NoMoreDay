#include "InputSystem.hpp"
#include "../components/Common.hpp"
#include "raylib.h"
#include "UISystem.hpp" // For UI state check

void InputSystem::update(entt::registry& registry) {
    auto view = registry.view<PlayerTag, InputComponent>();
    
    for (auto entity : view) {
        auto& input = view.get<InputComponent>(entity);
        
        // 重置
        input.moveX = 0.0f;
        input.moveY = 0.0f;
        
        // 键盘映射
        if (IsKeyDown(KEY_W)) input.moveY -= 1.0f;
        if (IsKeyDown(KEY_S)) input.moveY += 1.0f;
        if (IsKeyDown(KEY_A)) input.moveX -= 1.0f;
        if (IsKeyDown(KEY_D)) input.moveX += 1.0f;
        
        // 动作
        // Only allow mouse actions if not hovering over UI
        if (!UISystem::State.isMouseOverUI) {
            input.attack = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            // Only allow dash towards mouse if not over UI, OR if using keyboard
            // Note: Dash logic in GameplayState handles direction.
            // But triggering dash via Shift is keyboard. 
            // Triggering via mouse might be right click (not implemented here yet).
            // Usually Shift is Dash.
            input.dash = IsKeyDown(KEY_LEFT_SHIFT);
        } else {
            input.attack = false;
            input.dash = IsKeyDown(KEY_LEFT_SHIFT); // Allow keyboard dash even if mouse over UI (standard ARPG behavior)
        }

        // 归一化向量以保持对角线移动的一致性
        if (input.moveX != 0 || input.moveY != 0) {
            float length = std::sqrt(input.moveX * input.moveX + input.moveY * input.moveY);
            input.moveX /= length;
            input.moveY /= length;
        }
    }
}
