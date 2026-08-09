#pragma once
#include <entt/entt.hpp>
#include <string>
#include <vector>
#include "game/foundation/components/StashComponent.hpp"

namespace NoMoreDay {

enum class StashSortMode {
    RarityDesc,
    RarityAsc,
    Type,
    Level
};

class StashSystem {
public:
    static void Update(entt::registry& registry);

    // Queries
    static StashTab* getTab(entt::registry& registry, StashType type, int tabIndex);
    static int getUnlockedTabCount(entt::registry& registry, StashType type);
    static int getNextUnlockCost(entt::registry& registry, StashType type);

    // Core Interactions
    static bool transferItem(entt::registry& registry, 
                             StashType srcType, int srcTab, int srcSlot,
                             StashType dstType, int dstTab, int dstSlot);
                             
    // Helper to check if item can be stored (rejects Materials)
    static bool canStoreItem(entt::registry& registry, entt::entity item);

    // Quick Actions (Ctrl+Click)
    // From Inventory/Equip -> Stash
    static bool quickDeposit(entt::registry& registry, entt::entity item, StashType targetStash);
    
    // From Stash -> Inventory
    static bool quickWithdraw(entt::registry& registry, StashType srcType, int srcTab, int srcSlot);

    // From Inventory -> Specific Stash Slot (Swap/Move)
    static bool depositFromInventory(entt::registry& registry, entt::entity invItem, int invSlotIndex, 
                                     StashType targetType, int targetTab, int targetSlot);

    // From Stash -> Specific Inventory Slot (Swap/Move)
    static bool withdrawToSpecificSlot(entt::registry& registry, 
                                      StashType srcType, int srcTab, int srcSlot, 
                                      entt::entity playerEntity, int invSlot);

    // Tab Management
    static bool unlockTab(entt::registry& registry, StashType type);
    static bool renameTab(entt::registry& registry, StashType type, int tabIndex, const std::string& newName);
    static bool setTabIcon(entt::registry& registry, StashType type, int tabIndex, uint32_t iconId);
    static bool setTabColor(entt::registry& registry, StashType type, int tabIndex, uint32_t color);

    // Batch Ops
    static void sortTab(entt::registry& registry, StashType type, int tabIndex, StashSortMode mode);
    static int autoDeposit(entt::registry& registry, StashType targetStash); // Returns count deposited
    
    // Search
    static std::vector<std::pair<int, int>> search(entt::registry& registry, StashType type, const std::string& keyword);

private:
    // Helper to get PersonalStashComponent
    static PersonalStashComponent* getPersonalStash(entt::registry& registry);
};

} // namespace NoMoreDay
