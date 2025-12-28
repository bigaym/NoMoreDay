#pragma once
#include <entt/entt.hpp>
#include "../components/ItemComponent.hpp"
#include <vector>
#include <string>

namespace NoMoreDay {

class ItemFactory {
public:
    // Initialize random seed, load configs (if any)
    static void initialize();

    // Create a completely random item based on level
    static entt::entity createRandomLoot(entt::registry& registry, int level, float magicFind = 0.0f);

    // Create a specific item type with random stats
    static entt::entity createWeapon(entt::registry& registry, int level, Rarity rarity);
    static entt::entity createArmor(entt::registry& registry, int level, Rarity rarity, EquipmentSlot slot);

    static Affix generateRandomAffix(int level, bool isPrefix, EquipmentSlot slot);
    
    // Deterministic generation for Crafting
    static Affix createAffix(AffixType type, int tier);

    // --- Loot Pool Management ---
    static void addLootPool(uint32_t id, const LootPool& pool);
    static const LootPool* getLootPool(uint32_t id);

private:
    static Rarity rollRarity(float magicFind);
    static void rollAffixes(ItemComponent& item, int level);

    static std::map<uint32_t, LootPool> s_lootPools;
};

} // namespace NoMoreDay
