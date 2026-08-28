#pragma once

#include "game/foundation/components/StashComponent.hpp"
#include "game/systems/item/StashConfig.hpp"
#include <vector>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

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

    nlohmann::json toJson(entt::registry& registry) const;
    void fromJson(const nlohmann::json& j, entt::registry& registry);

    // 生命周期: 双轨过渡垫片。旧版 SaveManager 轨道 (itemStoreEnabled=false)
    // 在加载时仍会清空场景注册表，因此存放在此处的物品必须在 registry.clear() 之前序列化并在之后恢复。
    // 一旦 T-P3-3 将 SaveManager 迁移至 ItemStorageService 单轨，这对方法即可移除。
    void suspend(entt::registry& registry);
    void resume(entt::registry& registry);

private:
    int m_unlockedTabs = 0;
    std::vector<StashTab> m_tabs;
    nlohmann::json m_suspendedData; // suspend/resume 窗口期间序列化的仓库数据
};

} // namespace NoMoreDay
