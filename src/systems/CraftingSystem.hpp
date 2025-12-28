#pragma once
#include <entt/entt.hpp>
#include "../components/ItemComponent.hpp"
#include "../components/ItemStats.hpp"

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

private:
    static int calculatePotentialCost(int targetTier);
};

} // namespace NoMoreDay
