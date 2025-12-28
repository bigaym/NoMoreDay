#include "InputSystem.hpp"
#include "../components/Common.hpp"
#include "raylib.h"

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
        input.attack = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        input.dash = IsKeyDown(KEY_LEFT_SHIFT);

        // 归一化向量以保持对角线移动的一致性
        if (input.moveX != 0 || input.moveY != 0) {
            float length = std::sqrt(input.moveX * input.moveX + input.moveY * input.moveY);
            input.moveX /= length;
            input.moveY /= length;
        }
    }
}
