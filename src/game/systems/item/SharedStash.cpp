#include "SharedStash.hpp"
#include "game/systems/item/StashConfig.hpp"
#include "game/foundation/components/Common.hpp"

namespace NoMoreDay {

using namespace Constants;

SharedStash::SharedStash() {
    initialize();
}

void SharedStash::initialize() {
    if (m_tabs.empty()) {
        m_tabs.resize(1); // 从 1 个分页开始
        m_unlockedTabs = 1;
        m_tabs[0].name = "Shared 1";
        m_tabs[0].type = StashTabType::Normal;
    }
}

bool SharedStash::unlockNextTab(int& playerGold) {
    if (m_unlockedTabs >= StashConfig::MAX_TABS) return false;
    
    int cost = StashConfig::getUnlockCost(m_unlockedTabs); // 下一个分页的花费 (当前数量即为下一个索引)
    if (playerGold < cost) return false;
    
    playerGold -= cost;
    m_unlockedTabs++;
    m_tabs.resize(m_unlockedTabs);
    m_tabs.back().name = "Shared " + std::to_string(m_unlockedTabs);
    
    return true;
}

bool SharedStash::putItem(int tabIndex, int slotIndex, entt::entity item) {
    if (tabIndex < 0 || tabIndex >= m_unlockedTabs) return false;
    if (slotIndex < 0 || slotIndex >= StashTab::CAPACITY) return false;
    
    if (m_tabs[tabIndex].items[slotIndex] != entt::null) return false;
    
    m_tabs[tabIndex].items[slotIndex] = item;
    return true;
}

entt::entity SharedStash::takeItem(int tabIndex, int slotIndex) {
    if (tabIndex < 0 || tabIndex >= m_unlockedTabs) return entt::null;
    if (slotIndex < 0 || slotIndex >= StashTab::CAPACITY) return entt::null;
    
    entt::entity item = m_tabs[tabIndex].items[slotIndex];
    m_tabs[tabIndex].items[slotIndex] = entt::null;
    return item;
}

entt::entity SharedStash::getItem(int tabIndex, int slotIndex) const {
    if (tabIndex < 0 || tabIndex >= m_unlockedTabs) return entt::null;
    if (slotIndex < 0 || slotIndex >= StashTab::CAPACITY) return entt::null;
    
    return m_tabs[tabIndex].items[slotIndex];
}

StashTab* SharedStash::getTab(int tabIndex) {
    if (tabIndex < 0 || tabIndex >= m_unlockedTabs) return nullptr;
    return &m_tabs[tabIndex];
}

const StashTab* SharedStash::getTab(int tabIndex) const {
    if (tabIndex < 0 || tabIndex >= m_unlockedTabs) return nullptr;
    return &m_tabs[tabIndex];
}

} // namespace NoMoreDay
