#pragma once
#include <entt/entt.hpp>

class UIInventory {
public:
    static void Update(entt::registry& registry);
    static void Draw(entt::registry& registry);

    // 状态访问
    static bool IsVisible();
    static void Toggle();
    static void SetPage(int page);

private:
    static int m_inventoryPage;
};