#include "SharedStash.hpp"
#include "game/systems/item/StashConfig.hpp"
#include "game/components/Common.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/data/StashData.hpp"

namespace NoMoreDay {

using namespace Constants;

SharedStash::SharedStash() {
    initialize();
}

void SharedStash::initialize() {
    if (m_tabs.empty()) {
        m_tabs.resize(1); // Start with 1 tab
        m_unlockedTabs = 1;
        m_tabs[0].name = "Shared 1";
        m_tabs[0].type = StashTabType::Normal;
    }
}

bool SharedStash::unlockNextTab(int& playerGold) {
    if (m_unlockedTabs >= StashConfig::MAX_TABS) return false;
    
    int cost = StashConfig::getUnlockCost(m_unlockedTabs); // Cost for next tab (current count is next index)
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

nlohmann::json SharedStash::toJson(entt::registry& registry) const {
    SerializedStash sStash;
    sStash.unlockedTabs = m_unlockedTabs;
    
    for (const auto& tab : m_tabs) {
        SerializedStashTab sTab;
        sTab.name = tab.name;
        sTab.type = tab.type;
        sTab.iconId = tab.iconId;
        sTab.color = tab.color;
        
        for (int i = 0; i < StashTab::CAPACITY; ++i) {
            if (registry.valid(tab.items[i])) {
                SerializedStashSlot slot;
                slot.slotIndex = i;
                slot.item = ItemFactory::serializeItem(registry, tab.items[i]);
                sTab.items.push_back(slot);
            }
        }
        sStash.tabs.push_back(sTab);
    }
    
    nlohmann::json j = sStash;
    return j;
}

void SharedStash::fromJson(const nlohmann::json& j, entt::registry& registry) {
    if (j.is_null() || j.empty()) return;

    SerializedStash sStash = j.get<SerializedStash>();
    
    m_unlockedTabs = sStash.unlockedTabs;
    m_tabs.clear();
    m_tabs.resize(m_unlockedTabs);
    
    const auto& sTabs = sStash.tabs;
    for (size_t i = 0; i < sTabs.size(); ++i) {
        if (i >= m_tabs.size()) break;
        auto& t = m_tabs[i];
        const auto& sT = sTabs[i];
        
        t.name = sT.name;
        t.type = sT.type;
        t.iconId = sT.iconId;
        t.color = sT.color;
        
        for (const auto& slot : sT.items) {
            if (slot.slotIndex >= 0 && slot.slotIndex < StashTab::CAPACITY) {
                t.items[slot.slotIndex] = ItemFactory::restoreItem(registry, slot.item);
            }
        }
    }
}

void SharedStash::suspend(entt::registry& registry) {
    m_suspendedData = toJson(registry);
    // Clearing m_tabs entities logic? 
    // They are about to be destroyed by registry.clear().
    // We should clear our references to them.
    for (auto& tab : m_tabs) {
        tab.items.fill(entt::null);
    }
}

void SharedStash::resume(entt::registry& registry) {
    if (!m_suspendedData.empty()) {
        fromJson(m_suspendedData, registry);
        m_suspendedData.clear();
    }
}

} // namespace NoMoreDay
