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
    static int m_activeTab; // 0: Items, 1: Materials
    static float m_materialScrollOffset;
    
    // Filtering & Search
    static char m_searchBuffer[64];
    static std::string m_selectedCategory; // Empty = All
    static bool m_isSearchFocused;
};