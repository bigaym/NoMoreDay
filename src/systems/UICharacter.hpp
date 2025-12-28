#pragma once
#include <entt/entt.hpp>

class UICharacter {
public:
    static void Draw(entt::registry& registry);
    
private:
    static void DrawStatRow(const char* label, const char* value, float x, float& y, float width, float fontSize = 20.0f);
};