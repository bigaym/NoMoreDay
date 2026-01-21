#pragma once
#include "game/components/ItemComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SerializedItem.hpp"
#include <entt/entt.hpp>
#include <map>
#include <string>
#include <vector>

namespace NoMoreDay {

class ItemFactory {
public:
  // 初始化随机种子，加载配置（如果有）
  static void initialize();

  static void loadAffixDefinitions(const std::string &path);
  static void loadLootPools(const std::string &path);

  // 根据等级创建一个完全随机的物品
  static entt::entity createRandomLoot(entt::registry &registry, int level,
                                       float magicFind = 0.0f);

  // 从快照恢复物品 (确定性)
  static entt::entity restoreItem(entt::registry &registry,
                                  const SerializedItem &dto);

  // 序列化物品
  static SerializedItem serializeItem(entt::registry &registry, entt::entity entity);

  // 创建一个具有随机属性的特定物品类型
  static entt::entity createWeapon(entt::registry &registry, int level,
                                   Rarity rarity);
  static entt::entity createArmor(entt::registry &registry, int level,
                                  Rarity rarity, EquipmentSlot slot);

  // 创建背包容器
  static entt::entity createBag(entt::registry &registry, int level,
                                Rarity rarity);

  // 创建药水 (0: 生命药水, 1: 法力药水)
  static entt::entity createPotion(entt::registry &registry, int type,
                                   int quantity = 1);

  // 创建材料
  static entt::entity createMaterial(entt::registry &registry,
                                     uint32_t materialId, int quantity = 1);

  static Affix generateRandomAffix(int level, bool isPrefix,
                                   EquipmentSlot slot);

  // 用于打造的确定性生成
  static Affix createAffix(AffixType type, int tier);

  // 获取词缀在其等级下的数值范围 (minValue, maxValue)
  static std::pair<float, float> getAffixRange(AffixType type, int tier);

  // 获取物品基础属性的范围 (attack 或 defense)
  static std::pair<float, float> getBaseStatRange(const ItemComponent &item);

  // --- 掉落池管理 ---
  static void addLootPool(uint32_t id, const LootPool &pool);
  static const LootPool *getLootPool(uint32_t id);

private:
  static Rarity rollRarity(float magicFind);
  static void rollAffixes(ItemComponent &item, int level);

  static std::map<uint32_t, LootPool> s_lootPools;
  static std::vector<AffixDefinition> s_affixDefinitions;
};

} // namespace NoMoreDay
