#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "ItemStats.hpp"

namespace NoMoreDay {

enum class ItemType {
    Weapon,
    Armor,
    Consumable,
    Material,
    Quest
};

// 装备槽位枚举
enum class EquipmentSlot {
    None = 0,
    MainHand,
    OffHand,
    Head,
    Shoulder,
    Chest,
    Hands,
    Legs,
    Feet,
    Neck,
    Ring1,
    Ring2,
    Count // For array sizing
};

// 物品稀有度枚举
enum class Rarity {
    Common,
    Magic,
    Rare,
    Uncommon,
    Set,
    Epic,
    Legendary,
    Mythic
};

// 标记实体为物品的组件
struct ItemComponent {
    uint32_t id;          // 物品的数据库/配置ID
    std::string name;     // 物品名称
    ItemType type;        // 物品类型
    EquipmentSlot slot;   // 装备槽位 (如果可装备)
    Rarity rarity;        // 物品稀有度
    
    int quantity = 1;     // 当前堆叠数量
    int maxStack = 1;     // 最大堆叠数量
    float value = 0.0f;   // 物品金币价值
    
    // 基础属性
    float attack = 0.0f;  // 基础武器伤害 (仅武器)
    float defense = 0.0f; // 基础护甲防御 (仅护甲)

    // --- 打造与属性 ---
    int forgingPotential = 0; // 打造潜力 (打造时消耗)
    int legendaryPotential = 0; // 传奇潜力 (用于独特物品)
    
    // 固有词缀 (物品基础类型自带的属性，例如板甲自带护甲，法杖自带法术伤害)
    std::vector<NoMoreDay::Affix> implicits; 

    // 显性词缀 (随机生成或打造的属性)
    std::vector<NoMoreDay::Affix> affixes; 
    
    // 插槽 (未来)
    // std::vector<entt::entity> sockets; 
    
    // Description
    std::string description;
};

// 掉落物条目类型
enum class LootEntryType {
    Item,
    Gold,
    SubTable
};

// 掉落物条目定义
struct LootEntry {
    LootEntryType type;     // 掉落物类型
    uint32_t id;            // 物品ID (基础物品类型)
    uint32_t minAmount = 1; // 最小数量
    uint32_t maxAmount = 1; // 最大数量
    float weight = 1.0f;    // 掉落权重
};


// 掉落池：包含一组可能的掉落物及其权重。通常不是组件，而是由AssetRegistry/ItemFactory管理的资源。
struct LootPool {
    std::string name;
    std::vector<LootEntry> entries;
    float totalWeight = 0.0f;
};

/**
 * @brief 附加到敌人实体上，定义其掉落物的组件。
 */ 
struct DropTableComponent {
    uint32_t poolId = 0;     // 特定掉落池的ID (0表示全局掉落池)
    float dropChance = 1.0f; // 掉落任何物品的总几率 (0.0到1.0)
    int minRolls = 1;        // 在掉落池中进行抽取的最小次数
    int maxRolls = 1;        // 在掉落池中进行抽取的最大次数

    DropTableComponent(uint32_t id = 0, float chance = 1.0f, int minR = 1, int maxR = 1)
        : poolId(id), dropChance(chance), minRolls(minR), maxRolls(maxR) {}
};

} // namespace NoMoreDay
