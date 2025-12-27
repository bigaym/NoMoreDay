#include "InputSystem.hpp"
#include "../components/Common.hpp"
#include "raylib.h"

void InputSystem::update(entt::registry& registry) {
    auto view = registry.view<PlayerTag, InputComponent>();
    
    for (auto entity : view) {
        auto& input = view.get<InputComponent>(entity);
        
        // Reset
        input.moveX = 0.0f;
        input.moveY = 0.0f;
        
        // Keyboard mapping
        if (IsKeyDown(KEY_W)) input.moveY -= 1.0f;
        if (IsKeyDown(KEY_S)) input.moveY += 1.0f;
        if (IsKeyDown(KEY_A)) input.moveX -= 1.0f;
        if (IsKeyDown(KEY_D)) input.moveX += 1.0f;
        
        // Actions
        input.attack = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        input.dash = IsKeyDown(KEY_LEFT_SHIFT);

        // Normalize vector for diagonal movement consistency
        if (input.moveX != 0 || input.moveY != 0) {
            float length = std::sqrt(input.moveX * input.moveX + input.moveY * input.moveY);
            input.moveX /= length;
            input.moveY /= length;
        }
    }
}
