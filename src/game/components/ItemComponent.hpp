#pragma once
#include "game/components/ItemStats.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/systems/item/LootTable.hpp"
#include <cstdint>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include "core/utils/HashUtils.hpp"


namespace NoMoreDay {

enum class ItemType {
  Weapon,
  Armor,
  Shield,
  Jewelry,
  Consumable,
  Material,
  Quest,
  Bag
};

// 为枚举提供简单的序列化支持 (转为底层整数)
inline void to_json(nlohmann::json &j, const ItemType &e) {
  j = static_cast<uint8_t>(e);
}
inline void from_json(const nlohmann::json &j, ItemType &e) {
  e = static_cast<ItemType>(j.get<uint8_t>());
}

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
  Ring, // 通用戒指槽位 (用于物品属性，不用于装备栏索引)
  Count // For array sizing
};

// 为枚举提供简单的序列化支持 (转为底层整数)
inline void to_json(nlohmann::json &j, const EquipmentSlot &e) {
  j = static_cast<uint8_t>(e);
}
inline void from_json(const nlohmann::json &j, EquipmentSlot &e) {
  e = static_cast<EquipmentSlot>(j.get<uint8_t>());
}

// 物品稀有度枚举
enum class Rarity {
  Common,
  Magic,
  Rare,
  Uncommon,
  Set,
  Epic,
  Legendary,
  Mythic,
  Ancient
};

// 为枚举提供简单的序列化支持 (转为底层整数)
inline void to_json(nlohmann::json &j, const Rarity &e) {
  j = static_cast<uint8_t>(e);
}
inline void from_json(const nlohmann::json &j, Rarity &e) {
  e = static_cast<Rarity>(j.get<uint8_t>());
}

// 套装奖励定义
struct SetBonus {
  int requiredCount;                     // 激活此奖励所需的套装件数
  std::vector<NoMoreDay::Affix> bonuses; // 奖励的词缀列表
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetBonus, requiredCount, bonuses)

// 标记实体为物品的组件
struct ItemComponent {
  uint32_t id = 0;                          // 物品的数据库/配置ID
  std::string name;                         // 物品名称
  ItemType type = ItemType::Material;       // 物品类型
  EquipmentSlot slot = EquipmentSlot::None; // 装备槽位 (如果可装备)
  Rarity rarity = Rarity::Common;           // 物品稀有度

  int quantity = 1;   // 当前堆叠数量
  int maxStack = 1;   // 最大堆叠数量
  float value = 0.0f; // 物品金币价值

  // 基础属性
  float attack = 0.0f;      // 基础武器伤害 (仅武器)
  float defense = 0.0f;     // 基础护甲防御 (仅护甲)
  int bagCapacity = 0;      // 背包扩容量 (仅背包)
  bool isTwoHanded = false; // 是否为双手武器

  // --- 套装属性 ---
  std::string setName; // 套装名称 (例如 "Immortal King")
  uint32_t setNameHash = 0; // 套装名称哈希 (提高战斗属性计算性能)
  std::vector<SetBonus>
      setBonuses; // 套装奖励定义 (通常每件同名套装物品都携带一份相同的定义)

  // --- 打造与属性 ---
  int forgingPotential = 0;   // 打造潜力 (打造时消耗)
  int legendaryPotential = 0; // 传奇潜力 (用于独特物品)

  // 固有词缀 (物品基础类型自带的属性，例如板甲自带护甲，法杖自带法术伤害)
  std::vector<NoMoreDay::Affix> implicits;

  // 显性词缀 (随机生成或打造的属性)
  std::vector<NoMoreDay::Affix> affixes;

  // 传奇/特殊效果 (如属性转化)
  std::vector<NoMoreDay::StatConversion> conversions;
  std::vector<NoMoreDay::DamageModifier> damage_modifiers;

  // 插槽
  int socketCount = 0;
  std::vector<entt::entity> sockets;

  // 资源ID (用于图标)
  entt::id_type textureId = 0;

  // Runeword ID (if active)
  uint32_t activeRunewordId = 0;

  // Description
  std::string description;

  bool isLocked = false; // Prevents accidental salvage/sell
};

// 避免在 JSON 中存储哈希值，反序列化时自动计算
inline void to_json(nlohmann::json& j, const ItemComponent& i) {
    j = nlohmann::json{
        {"id", i.id}, {"name", i.name}, {"type", i.type}, {"slot", i.slot}, {"rarity", i.rarity},
        {"quantity", i.quantity}, {"maxStack", i.maxStack}, {"value", i.value}, {"attack", i.attack},
        {"defense", i.defense}, {"bagCapacity", i.bagCapacity}, {"isTwoHanded", i.isTwoHanded},
        {"setName", i.setName}, {"setBonuses", i.setBonuses}, {"forgingPotential", i.forgingPotential},
        {"legendaryPotential", i.legendaryPotential}, {"implicits", i.implicits}, {"affixes", i.affixes},
        {"conversions", i.conversions}, {"damage_modifiers", i.damage_modifiers}, {"socketCount", i.socketCount},
        {"sockets", i.sockets}, {"textureId", i.textureId}, {"activeRunewordId", i.activeRunewordId},
        {"description", i.description}, {"isLocked", i.isLocked}
    };
}

inline void from_json(const nlohmann::json& j, ItemComponent& i) {
    j.at("id").get_to(i.id);
    j.at("name").get_to(i.name);
    j.at("type").get_to(i.type);
    j.at("slot").get_to(i.slot);
    j.at("rarity").get_to(i.rarity);
    j.at("quantity").get_to(i.quantity);
    j.at("maxStack").get_to(i.maxStack);
    j.at("value").get_to(i.value);
    j.at("attack").get_to(i.attack);
    j.at("defense").get_to(i.defense);
    j.at("bagCapacity").get_to(i.bagCapacity);
    j.at("isTwoHanded").get_to(i.isTwoHanded);
    j.at("setName").get_to(i.setName);
    j.at("setBonuses").get_to(i.setBonuses);
    j.at("forgingPotential").get_to(i.forgingPotential);
    j.at("legendaryPotential").get_to(i.legendaryPotential);
    j.at("implicits").get_to(i.implicits);
    j.at("affixes").get_to(i.affixes);
    j.at("conversions").get_to(i.conversions);
    j.at("damage_modifiers").get_to(i.damage_modifiers);
    j.at("socketCount").get_to(i.socketCount);
    j.at("sockets").get_to(i.sockets);
    j.at("textureId").get_to(i.textureId);
    j.at("activeRunewordId").get_to(i.activeRunewordId);
    j.at("description").get_to(i.description);
    if (j.contains("isLocked")) j.at("isLocked").get_to(i.isLocked);

    // 自动计算哈希
    if (!i.setName.empty()) {
        i.setNameHash = NoMoreDay::utils::Hash(i.setName);
    }
}

/**
 * @brief 附加到敌人实体上，定义其掉落物的组件。
 */
struct DropTableComponent {
  uint32_t poolId = 0;     // 特定掉落池的ID (0表示全局掉落池)
  float dropChance = 1.0f; // 掉落任何物品的总几率 (0.0到1.0)
  int minRolls = 1;        // 在掉落池中进行抽取的最小次数
  int maxRolls = 1;        // 在掉落池中进行抽取的最大次数

  DropTableComponent() = default;
  DropTableComponent(uint32_t id, float chance = 1.0f, int minR = 1,
                     int maxR = 1)
      : poolId(id), dropChance(chance), minRolls(minR), maxRolls(maxR) {}
};

} // namespace NoMoreDay
