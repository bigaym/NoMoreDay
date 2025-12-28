#pragma once
#include <entt/entt.hpp>
#include "../components/ItemComponent.hpp"
#include "../components/ItemStats.hpp"

namespace NoMoreDay {

enum class CraftingResult {
    Success,
    CriticalSuccess, // Saves potential?
    Failure,
    NoPotential,
    MaxTierReached,
    SlotFull,
    MaterialMissing
};

class CraftingSystem {
public:
    // Try to upgrade a specific affix on an item
    // affixIndex: index in item.affixes vector
    static CraftingResult upgradeAffix(ItemComponent& item, int affixIndex);

    // Try to add a new affix
    static CraftingResult addAffix(ItemComponent& item, AffixType type, bool isPrefix);

    // Chaos: Upgrade and randomize type
    static CraftingResult chaosAffix(ItemComponent& item, int affixIndex);

private:
    static int calculatePotentialCost(int targetTier);
};

} // namespace NoMoreDay
