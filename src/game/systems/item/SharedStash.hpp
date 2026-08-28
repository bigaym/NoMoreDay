#pragma once

#include "game/foundation/components/StashComponent.hpp"
#include "game/systems/item/StashConfig.hpp"
#include <vector>
#include <entt/entt.hpp>

namespace NoMoreDay {

class SharedStash {
public:
    SharedStash();
    void initialize(); 

    bool unlockNextTab(int& playerGold);
    
    // 若槽位为空且物品成功放置返回 true，否则返回 false
    bool putItem(int tabIndex, int slotIndex, entt::entity item);
    
    // 返回槽位中的实体并清空槽位 (不销毁实体)
    entt::entity takeItem(int tabIndex, int slotIndex);
    
    // 返回槽位中的实体且不移除它
    entt::entity getItem(int tabIndex, int slotIndex) const;

    StashTab* getTab(int tabIndex);
    const StashTab* getTab(int tabIndex) const;
    
    int getUnlockedTabCount() const { return m_unlockedTabs; }
    int getMaxTabs() const { return 10; } // 可使用 StashConfig

private:
    int m_unlockedTabs = 0;
    std::vector<StashTab> m_tabs;
};

} // namespace NoMoreDay
