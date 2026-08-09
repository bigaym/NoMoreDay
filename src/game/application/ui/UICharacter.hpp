#pragma once
#include <entt/entt.hpp>

    class UICharacter {
    public:
        static void Draw(entt::registry& registry);
        static void DrawStatRow(const char* label, const char* value, float x, float& y, float width, float fontSize = 16.0f, float alpha = 1.0f);
    };