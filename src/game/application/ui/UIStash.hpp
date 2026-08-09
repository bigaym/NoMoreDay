#pragma once
#include <entt/entt.hpp>
#include "game/foundation/components/StashComponent.hpp"

namespace NoMoreDay {

class UIStash {
public:
    static bool IsVisible();
    static void Toggle();
    static void Open(StashType type);
    static void Close();
    
    static void Update(entt::registry& registry);
    static void Draw(entt::registry& registry);
    
    static StashType GetActiveType() { return m_activeType; }
    static int GetActiveTabIndex() { return m_activeTabIndex; }

private:
    static bool m_isVisible;
    static StashType m_activeType;
    static int m_activeTabIndex;
    
    static float m_alpha;
    static char m_searchBuffer[64];
    static char m_lastSearchBuffer[64]; // Cache Key
    static std::vector<std::pair<int, int>> m_cachedSearchResults; // Cache Value
    
    static bool m_isSearchFocused;
    
    // For unlocking confirmation
    static bool m_showUnlockConfirm;
};

} // namespace NoMoreDay
