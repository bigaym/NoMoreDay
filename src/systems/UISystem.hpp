#pragma once
#include <entt/entt.hpp>

class MapSystem; // 前置声明

class UISystem {
public:
    // 渲染 UI (屏幕空间，不需要 Camera)
    static void render(const entt::registry& registry, const MapSystem* mapSystem = nullptr);
};