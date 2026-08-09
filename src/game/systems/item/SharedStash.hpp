#pragma once

#include "game/components/StashComponent.hpp"
#include "game/systems/item/StashConfig.hpp"
#include <vector>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

namespace NoMoreDay {

class SharedStash {
public:
    static SharedStash& Get() {
        static SharedStash instance;
        return instance;
    }

    void initialize(); 

    bool unlockNextTab(int& playerGold);
    
    // Returns true if slot was empty and item placed, false otherwise
    bool putItem(int tabIndex, int slotIndex, entt::entity item);
    
    // Returns the entity at slot and clears the slot (does not destroy entity)
    entt::entity takeItem(int tabIndex, int slotIndex);
    
    // Returns the entity at slot without removing it
    entt::entity getItem(int tabIndex, int slotIndex) const;

    StashTab* getTab(int tabIndex);
    const StashTab* getTab(int tabIndex) const;
    
    int getUnlockedTabCount() const { return m_unlockedTabs; }
    int getMaxTabs() const { return 10; } // Could use StashConfig

    nlohmann::json toJson(entt::registry& registry) const;
    void fromJson(const nlohmann::json& j, entt::registry& registry);

    // Lifecycle Management (Suspend/Resume across registry clears)
    void suspend(entt::registry& registry);
    void resume(entt::registry& registry);

private:
    SharedStash();
    
    int m_unlockedTabs = 0;
    std::vector<StashTab> m_tabs;
    
    // Temp storage for suspension
    nlohmann::json m_suspendedData;
};

} // namespace NoMoreDay
