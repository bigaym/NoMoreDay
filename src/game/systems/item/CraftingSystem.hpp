#pragma once
#include <entt/entt.hpp>
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"

namespace NoMoreDay {

enum class CraftingResult {
    Success, // 成功
    CriticalSuccess, // 暴击成功 (节省潜力？)
    Failure, // 失败
    NoPotential, // 没有潜力
    MaxTierReached, // 已达最大等级
    SlotFull, // 槽位已满
    MaterialMissing // 材料缺失
};

class CraftingSystem {
public:
    // 尝试升级物品上的特定词缀
    // affixIndex: item.affixes 向量中的索引
    static CraftingResult upgradeAffix(ItemComponent& item, int affixIndex);

    // 尝试添加新词缀
    static CraftingResult addAffix(ItemComponent& item, AffixType type, bool isPrefix);

    // 混沌: 升级并随机化类型
    static CraftingResult chaosAffix(ItemComponent& item, int affixIndex);

    // 洗练: 在当前等级内重新滚动词缀数值
    static CraftingResult refineAffixValues(ItemComponent& item, int affixIndex);

    // 洗练: 重新滚动物品的基础数值 (attack 或 defense)
    static CraftingResult refineBaseStats(ItemComponent& item);

    // 插槽: 将符文放入插槽
    static CraftingResult socketRune(entt::registry& registry, entt::entity itemEntity, entt::entity runeEntity, int socketIndex);

    // 插槽: 从插槽中移除符文 (通常需要特殊材料或金币，此处简化)
    static CraftingResult unsocketRune(entt::registry& registry, entt::entity itemEntity, int socketIndex);

    // 融合: 结合两个物品 (占位符)
    static CraftingResult fuseItems(ItemComponent& baseItem, ItemComponent& fodderItem);

    // 传奇融合: 将Exalted物品的词缀融合到Unique物品上
    static CraftingResult fuseLegendary(entt::registry& registry, entt::entity baseEntity, entt::entity fodderEntity, entt::entity catalystEntity, int selectedAffixIndex);

private:
    static int calculatePotentialCost(int targetTier);
};

} // namespace NoMoreDay
