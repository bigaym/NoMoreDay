#pragma once
#include <entt/entt.hpp>
#include "../components/ItemComponent.hpp"
#include <vector>
#include <string>

namespace NoMoreDay {

class ItemFactory {
public:
    // 初始化随机种子，加载配置（如果有）
    static void initialize();

    // 根据等级创建一个完全随机的物品
    static entt::entity createRandomLoot(entt::registry& registry, int level, float magicFind = 0.0f);

    // 创建一个具有随机属性的特定物品类型
    static entt::entity createWeapon(entt::registry& registry, int level, Rarity rarity);
    static entt::entity createArmor(entt::registry& registry, int level, Rarity rarity, EquipmentSlot slot);

    static Affix generateRandomAffix(int level, bool isPrefix, EquipmentSlot slot);
    
    // 用于打造的确定性生成
    static Affix createAffix(AffixType type, int tier);

    // --- 掉落池管理 ---
    static void addLootPool(uint32_t id, const LootPool& pool);
    static const LootPool* getLootPool(uint32_t id);

private:
    static Rarity rollRarity(float magicFind);
    static void rollAffixes(ItemComponent& item, int level);

    static std::map<uint32_t, LootPool> s_lootPools;
};

} // namespace NoMoreDay
