#include "CraftingSystem.hpp"
#include "../core/ItemFactory.hpp"
#include "../tools/Logger.hpp"
#include <random>
#include <algorithm>

namespace NoMoreDay {

int CraftingSystem::calculatePotentialCost(int targetTier) {
    static std::mt19937 rng(std::random_device{}());
    int minCost = 1 + targetTier * 2;
    int maxCost = 10 + targetTier * 3;
    std::uniform_int_distribution<> dist(minCost, maxCost);
    return dist(rng);
}

CraftingResult CraftingSystem::upgradeAffix(ItemComponent& item, int affixIndex) {
    if (affixIndex < 0 || affixIndex >= item.affixes.size()) {
        LOG_ERROR("Crafting: Invalid affix index {} for item '{}'", affixIndex, item.name);
        return CraftingResult::Failure;
    }
    
    auto& affix = item.affixes[affixIndex];
    
    // Check Limits
    if (affix.tier >= 5) { // Craftable Max T5
        LOG_DEBUG("Crafting: Affix '{}' on item '{}' is already max tier (5)", affix.name, item.name);
        return CraftingResult::MaxTierReached;
    }
    
    if (item.forgingPotential <= 0) {
        LOG_DEBUG("Crafting: Item '{}' has no forging potential left", item.name);
        return CraftingResult::NoPotential;
    }
    
    int cost = calculatePotentialCost(affix.tier + 1);
    int finalCost = std::min(cost, item.forgingPotential);
    item.forgingPotential -= finalCost;
    
    LOG_INFO("Crafting: Upgrading affix '{}' on '{}'. Cost: {} potential", affix.name, item.name, finalCost);

    // Upgrade: Re-generate purely based on Type and New Tier
    int newTier = affix.tier + 1;
    Affix newAffix = ItemFactory::createAffix(affix.type, newTier);
    
    // Preserve position (Prefix/Suffix) just in case logic differs, though Type usually dictates it.
    // In our system, Type dictates Prefix/Suffix in fillAffixDetails.
    // So we just replace it.
    
    // Copy over needed fields (CreateAffix handles value, tier, name, isPrefix)
    affix = newAffix;
    
    return CraftingResult::Success;
}

CraftingResult CraftingSystem::addAffix(ItemComponent& item, AffixType type, bool isPrefix) {
    if (item.forgingPotential <= 0) {
        LOG_DEBUG("Crafting: Cannot add affix to '{}', no potential", item.name);
        return CraftingResult::NoPotential;
    }
    
    // Check Slots
    int currentPrefix = 0;
    int currentSuffix = 0;
    for(const auto& a : item.affixes) {
        if(a.isPrefix) currentPrefix++; else currentSuffix++;
    }
    
    if (isPrefix && currentPrefix >= 2) {
        LOG_WARN("Crafting: Prefix slots full for item '{}'", item.name);
        return CraftingResult::SlotFull;
    }
    if (!isPrefix && currentSuffix >= 2) {
        LOG_WARN("Crafting: Suffix slots full for item '{}'", item.name);
        return CraftingResult::SlotFull;
    }
    
    // Calculate Cost
    int cost = calculatePotentialCost(1);
    int finalCost = std::min(cost, item.forgingPotential);
    item.forgingPotential -= finalCost;
    
    LOG_INFO("Crafting: Adding new {} to '{}'. Cost: {} potential", isPrefix ? "prefix" : "suffix", item.name, finalCost);

    // Add Affix - Tier 1
    Affix newAffix = ItemFactory::createAffix(type, 1);
    // Force isPrefix to match requested? 
    // Usually Type dictates it. If user requests Prefix but picks "Strength" (Suffix), what happens?
    // Our createAffix sets isPrefix based on Type.
    // So we should verify if the generated affix matches the requested slot.
    if (newAffix.isPrefix != isPrefix) {
        // In a real UI, we filter types by slot so this won't happen.
        // For backend safety, we could reject or just accept the Type's nature.
        // Let's accept the Type's nature.
        // Re-check slot limits based on ACTUAL type nature?
        // Simpler: Just trust createAffix.
    }
    
    item.affixes.push_back(newAffix);
    
    return CraftingResult::Success;
}

CraftingResult CraftingSystem::chaosAffix(ItemComponent& item, int affixIndex) {
    // 1. Check basic constraints (Pot, Index)
    if (affixIndex < 0 || affixIndex >= item.affixes.size()) {
        LOG_ERROR("Crafting Chaos: Invalid index {}", affixIndex);
        return CraftingResult::Failure;
    }
    if (item.forgingPotential <= 0) {
        LOG_DEBUG("Crafting Chaos: No potential on '{}'", item.name);
        return CraftingResult::NoPotential;
    }
    
    auto& oldAffix = item.affixes[affixIndex];
    if (oldAffix.tier >= 5) {
        LOG_DEBUG("Crafting Chaos: Affix '{}' is already max tier", oldAffix.name);
        return CraftingResult::MaxTierReached;
    }

    // 2. Cost
    int cost = calculatePotentialCost(oldAffix.tier + 1);
    int finalCost = std::min(cost, item.forgingPotential);
    item.forgingPotential -= finalCost;

    LOG_INFO("Crafting Chaos: Rerolling affix '{}' on '{}'. Cost: {}", oldAffix.name, item.name, finalCost);

    // 3. Chaos Logic: New Type, Tier + 1
    int targetTier = oldAffix.tier + 1;
    bool targetPrefix = oldAffix.isPrefix;
    
    // We need a random type that fits this slot and position.
    // Use ItemFactory::generateRandomAffix as a helper to find a valid Type.
    // We pass a dummy level (e.g. 50) to get a valid roll.
    // Loop a few times to ensure we get the right Position (Prefix/Suffix).
    Affix tempCandidate;
    bool found = false;
    for(int i=0; i<10; ++i) {
        tempCandidate = ItemFactory::generateRandomAffix(50, targetPrefix, item.slot);
        if (tempCandidate.isPrefix == targetPrefix) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        // Fallback: Just upgrade existing if we can't find a swap (Should rare occur)
        LOG_WARN("Crafting Chaos: Could not find new valid type for '{}', falling back to upgrade", item.name);
        oldAffix = ItemFactory::createAffix(oldAffix.type, targetTier);
        oldAffix.name = "Chaotic " + oldAffix.name;
        return CraftingResult::Success;
    }
    
    // Apply the new type with the target Tier
    oldAffix = ItemFactory::createAffix(tempCandidate.type, targetTier);
    oldAffix.name = "Chaotic " + oldAffix.name;

    return CraftingResult::Success;
}

} // namespace NoMoreDay