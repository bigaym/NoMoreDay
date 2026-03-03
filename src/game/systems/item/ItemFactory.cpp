#include "game/systems/item/ItemFactory.hpp"
#include "core/logging/Logger.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/AssetRegistry.hpp"
#include "engine/resource/EquipmentAssetRegistry.hpp"
#include "engine/resource/RuneAssetRegistry.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/systems/item/LootFilter.hpp"
#include "game/systems/item/MaterialRegistry.hpp"
#include "game/systems/item/RunewordSystem.hpp"
#include "game/components/WorldState.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <random>
#include <vector>

namespace NoMoreDay {

// Thread-local RNG for item generation
static thread_local std::mt19937 t_rng(std::random_device{}());

// Helper to pick a random texture ID from a compile-time array
template <typename T, size_t N>
static entt::id_type pickRandomAsset(const std::array<T, N> &assets) {
  if (N == 0)
    return 0;
  std::uniform_int_distribution<size_t> dist(0, N - 1);
  return assets[dist(t_rng)]->id;
}

static entt::id_type
getRandomTextureForType(ItemType type, EquipmentSlot slot,
                        const std::string &name,
                        WeaponSubtype subtype = WeaponSubtype::None) {
  using namespace assets::equipment;

  if (type == ItemType::Armor) {
    switch (slot) {
    case EquipmentSlot::Head:
      return pickRandomAsset(helmet::All);
    case EquipmentSlot::Chest:
      return pickRandomAsset(chest::All);
    case EquipmentSlot::Shoulder:
      return pickRandomAsset(pauldrons::All);
    case EquipmentSlot::Hands:
      return pickRandomAsset(gauntlets::All);
    case EquipmentSlot::Legs:
      return pickRandomAsset(leggings::All);
    case EquipmentSlot::Feet:
      return pickRandomAsset(boots::All);
    case EquipmentSlot::OffHand:
      return pickRandomAsset(shield::All);
    default:
      break;
    }
  } else if (type == ItemType::Weapon) {
    switch (subtype) {
    case WeaponSubtype::Sword:
      return pickRandomAsset(sword::All);
    case WeaponSubtype::Axe:
      return pickRandomAsset(axe::All);
    case WeaponSubtype::Dagger:
      return pickRandomAsset(dagger::All);
    case WeaponSubtype::Mace:
      return pickRandomAsset(hammer::All);
    case WeaponSubtype::Staff:
      return pickRandomAsset(staff::All);
    case WeaponSubtype::Wand:
      return pickRandomAsset(wand::All);
    case WeaponSubtype::None:
    default:
      if (name.find("大剑") != std::string::npos ||
          name.find("Great") != std::string::npos)
        return pickRandomAsset(greatsword::All);
      return pickRandomAsset(sword::All);
    }
  } else if (type == ItemType::Shield) {
    return pickRandomAsset(shield::All);
  } else if (type == ItemType::Jewelry) {
    if (slot == EquipmentSlot::Neck)
      return pickRandomAsset(amulet::All);
    if (slot == EquipmentSlot::Ring || slot == EquipmentSlot::Ring1 ||
        slot == EquipmentSlot::Ring2)
      return pickRandomAsset(ring::All);
  }

  return 0;
}

static bool TryAttachWorldSprite(entt::registry &registry, entt::entity entity,
                                 entt::id_type textureId) {
  if (textureId == 0) {
    return false;
  }

  Texture2D tex = AssetLoadingSystem::GetTexture(textureId);
  if (tex.id <= 0) {
    return false;
  }

  float dropScale = 32.0f / (float)std::max(tex.width, tex.height);
  registry.emplace<SpriteComponent>(entity, tex, dropScale);
  return true;
}

std::map<uint32_t, LootPool> ItemFactory::s_lootPools;
std::vector<AffixDefinition> ItemFactory::s_affixDefinitions;

static uint64_t BuildRequiredSkillTagsAllMask(const AffixDefinition &definition) {
  return static_cast<uint64_t>(definition.GetRequiredTags());
}

void ItemFactory::initialize() {
  // 加载词缀定义
  loadAffixDefinitions("assets/data/affixes.json");
  loadAffixDefinitions("assets/data/legendary_affixes.json");

  // 加载掉落池定义
  loadLootPools("assets/data/loot_tables.json");

  // 加载掉落过滤器
  LootFilter::load("assets/data/loot_filters/default.json");

  // Initialize Runeword System
  RunewordSystem::initialize();

  // Setup UI Lookup for Affix Names
  GetAffixNameLookup() = [](AffixType type) -> const char * {
    for (const auto &def : s_affixDefinitions) {
      if (def.type == type)
        return def.nameTemplate.c_str();
    }
    return nullptr;
  };
}

void ItemFactory::loadAffixDefinitions(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("ItemFactory: 无法打开词缀定义文件: {}", path);
    return;
  }

  try {
    nlohmann::json j;
    file >> j;
    auto newDefs = j.get<std::vector<AffixDefinition>>();
    s_affixDefinitions.insert(s_affixDefinitions.end(), newDefs.begin(),
                              newDefs.end());
    LOG_INFO("ItemFactory: 从 {} 成功加载了 {} 个词缀定义 (总计: {})。", path,
             newDefs.size(), s_affixDefinitions.size());
  } catch (const std::exception &e) {
    LOG_ERROR("ItemFactory: 解析词缀定义文件 {} 时出错: {}", path, e.what());
  }
}

void ItemFactory::loadLootPools(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("ItemFactory: 无法打开掉落池定义文件: {}", path);
    return;
  }

  try {
    nlohmann::json j;
    file >> j;
    std::vector<LootPool> pools = j.get<std::vector<LootPool>>();
    for (auto &pool : pools) {
      pool.calculateTotalWeight();
      s_lootPools[pool.id] = pool;
    }
    LOG_INFO("ItemFactory: 成功加载了 {} 个掉落池定义。", pools.size());
  } catch (const std::exception &e) {
    LOG_ERROR("ItemFactory: 解析掉落池定义文件时出错: {}", e.what());
  }
}

// ... existing code ...

void ItemFactory::addLootPool(uint32_t id, const LootPool &pool) {
  s_lootPools[id] = pool;
  float total = 0.0f;
  for (const auto &entry : pool.entries)
    total += entry.weight;
  s_lootPools[id].totalWeight = total;
}

const LootPool *ItemFactory::getLootPool(uint32_t id) {
  auto it = s_lootPools.find(id);
  if (it != s_lootPools.end())
    return &it->second;
  return nullptr;
}

// Helper to serialize an item entity to DTO (Duplicated from SaveManager for
// now)
SerializedItem ItemFactory::serializeItem(entt::registry &registry,
                                          entt::entity entity) {
  SerializedItem dto;
  if (!registry.all_of<ItemComponent>(entity))
    return dto;

  const auto &item = registry.get<ItemComponent>(entity);
  dto.itemId = item.id;
  dto.name = item.name;
  dto.type = item.type;
  dto.textureId = item.textureId;
  dto.quantity = item.quantity;

  dto.stats.rarity = item.rarity;
  dto.stats.level = item.itemLevel; // [NEW] Save item level
  dto.stats.slot = item.slot;
  dto.stats.attack = item.attack;
  dto.stats.defense = item.defense;
  dto.stats.forgingPotential = item.forgingPotential;
  dto.stats.legendaryPotential = item.legendaryPotential;
  dto.stats.value = item.value;

  for (const auto &aff : item.affixes) {
    SerializedItem::SavedAffix sAff;
    sAff.type = aff.type;
    sAff.tier = aff.tier;
    sAff.value = aff.value;
    sAff.isPrefix = aff.isPrefix;
    sAff.isLegendary = aff.isLegendary;
    sAff.required_tags = aff.required_tags;
    sAff.modifier_record_ids = aff.modifier_record_ids;
    dto.affixes.push_back(sAff);
  }

  for (const auto &aff : item.implicits) {
    SerializedItem::SavedAffix sAff;
    sAff.type = aff.type;
    sAff.tier = aff.tier;
    sAff.value = aff.value;
    sAff.isPrefix = aff.isPrefix;
    sAff.isLegendary = aff.isLegendary;
    sAff.required_tags = aff.required_tags;
    sAff.modifier_record_ids = aff.modifier_record_ids;
    dto.implicits.push_back(sAff);
  }

  for (auto socketEntity : item.sockets) {
    if (registry.valid(socketEntity)) {
      dto.socketedItems.push_back(serializeItem(registry, socketEntity));
    }
  }

  return dto;
}

// -----------------------------------------------------------------------------
// 基础物品定义 (内部数据库)
// -----------------------------------------------------------------------------
struct BaseItemDef {
  std::string name;
  int minLevel;
  float baseStatMin;
  float baseStatMax;
  AffixType implicitType;
};

// --- 武器基底定义 ---

static const std::vector<BaseItemDef> WEAPON_SWORD_BASES = {
    {"锈蚀铁剑", 1, 5.0f, 8.0f, AffixType::PercentPhysicalDamage},
    {"精铁长剑", 10, 12.0f, 18.0f, AffixType::PercentPhysicalDamage},
    {"骑士阔剑", 25, 25.0f, 35.0f, AffixType::CritChance},
    {"秘银长剑", 45, 45.0f, 60.0f, AffixType::AttackSpeed},
    {"符文剑", 60, 70.0f, 90.0f, AffixType::PercentFireDamage},
    {"龙牙剑", 75, 100.0f, 130.0f, AffixType::CritDamage}};

static const std::vector<BaseItemDef> WEAPON_AXE_BASES = {
    {"伐木斧", 1, 6.0f, 10.0f, AffixType::FlatPhysicalDamage},
    {"铁手斧", 10, 14.0f, 20.0f, AffixType::FlatPhysicalDamage},
    {"战斗斧", 25, 28.0f, 38.0f, AffixType::CritDamage},
    {"狂战士斧", 45, 50.0f, 65.0f, AffixType::PercentPhysicalDamage},
    {"斩首斧", 60, 75.0f, 95.0f, AffixType::LifeSteal},
    {"毁灭者", 75, 110.0f, 140.0f, AffixType::CritDamage}};

static const std::vector<BaseItemDef> WEAPON_DAGGER_BASES = {
    {"磨损匕首", 1, 3.0f, 6.0f, AffixType::CritChance},
    {"猎人短刀", 10, 8.0f, 14.0f, AffixType::CritChance},
    {"刺客匕首", 25, 18.0f, 26.0f, AffixType::CritDamage},
    {"锯齿刃", 45, 35.0f, 48.0f, AffixType::FlatPoisonDamage},
    {"幽冥匕首", 60, 55.0f, 75.0f, AffixType::PercentPoisonDamage},
    {"龙骨匕首", 75, 80.0f, 100.0f, AffixType::CritChance}};

static const std::vector<BaseItemDef> WEAPON_HAMMER_BASES = {
    {"木锤", 1, 7.0f, 11.0f, AffixType::FlatPhysicalDamage},
    {"铁战锤", 10, 16.0f, 24.0f, AffixType::PercentPhysicalDamage},
    {"碎骨锤", 25, 32.0f, 45.0f, AffixType::PercentPhysicalDamage},
    {"重型战锤", 45, 55.0f, 75.0f, AffixType::FlatLightningDamage},
    {"雷神之锤", 60, 85.0f, 110.0f, AffixType::PercentLightningDamage},
    {"泰坦之锤", 75, 120.0f, 160.0f, AffixType::PercentPhysicalDamage}};

static const std::vector<BaseItemDef> WEAPON_GREATSWORD_BASES = {
    {"训练大剑", 1, 8.0f, 12.0f, AffixType::PercentPhysicalDamage},
    {"铁大剑", 10, 18.0f, 26.0f, AffixType::PercentPhysicalDamage},
    {"巨剑", 25, 35.0f, 50.0f, AffixType::FlatPhysicalDamage},
    {"斩马刀", 45, 60.0f, 80.0f, AffixType::CritDamage},
    {"处刑者", 60, 90.0f, 120.0f, AffixType::LifeOnHit},
    {"诸神黄昏", 75, 130.0f, 170.0f, AffixType::PercentPhysicalDamage}};

static const std::vector<BaseItemDef> WEAPON_STAFF_BASES = {
    {"枯木法杖", 1, 4.0f, 8.0f, AffixType::FlatMana},
    {"橡木法杖", 10, 10.0f, 16.0f, AffixType::PercentFireDamage},
    {"宝石法杖", 25, 22.0f, 32.0f, AffixType::PercentColdDamage},
    {"元素法杖", 45, 40.0f, 55.0f, AffixType::ResistAll},
    {"贤者法杖", 60, 65.0f, 85.0f, AffixType::PercentLightningDamage},
    {"世界树枝", 75, 95.0f, 125.0f, AffixType::Intelligence}};

static const std::vector<BaseItemDef> WEAPON_WAND_BASES = {
    {"学徒魔杖", 1, 3.0f, 7.0f, AffixType::FlatMana},
    {"骨魔杖", 10, 9.0f, 15.0f, AffixType::FlatShadowDamage},
    {"水晶魔杖", 25, 20.0f, 30.0f, AffixType::PercentShadowDamage},
    {"秘法魔杖", 45, 38.0f, 52.0f, AffixType::CastSpeed},
    {"虚空魔杖", 60, 60.0f, 80.0f, AffixType::PercentShadowDamage},
    {"星辰魔杖", 75, 90.0f, 115.0f, AffixType::Intelligence}};

// --- 防具基底定义 ---

static const std::vector<BaseItemDef> ARMOR_HEAD_BASES = {
    {"皮帽", 1, 2.0f, 4.0f, AffixType::FlatMana},
    {"铁盔", 10, 6.0f, 10.0f, AffixType::FlatHealth},
    {"骑士头盔", 25, 15.0f, 22.0f, AffixType::FlatArmor},
    {"统帅头盔", 45, 30.0f, 40.0f, AffixType::PercentArmor},
    {"龙盔", 60, 50.0f, 70.0f, AffixType::Vitality}};

static const std::vector<BaseItemDef> ARMOR_CHEST_BASES = {
    {"破旧法袍", 1, 3.0f, 6.0f, AffixType::FlatMana},
    {"硬皮上衣", 10, 10.0f, 15.0f, AffixType::FlatHealth},
    {"锁子甲", 25, 25.0f, 35.0f, AffixType::ResistAll},
    {"板甲", 45, 50.0f, 65.0f, AffixType::PercentArmor},
    {"龙鳞甲", 70, 80.0f, 100.0f, AffixType::FlatHealth}};

static const std::vector<BaseItemDef> ARMOR_SHOULDER_BASES = {
    {"皮护肩", 1, 2.0f, 4.0f, AffixType::FlatHealth},
    {"铁护肩", 10, 5.0f, 9.0f, AffixType::Strength},
    {"钢护肩", 25, 12.0f, 18.0f, AffixType::FlatArmor},
    {"刺客护肩", 45, 25.0f, 35.0f, AffixType::Dexterity},
    {"泰坦护肩", 60, 45.0f, 60.0f, AffixType::PercentArmor}};

static const std::vector<BaseItemDef> ARMOR_HANDS_BASES = {
    {"皮手套", 1, 1.0f, 3.0f, AffixType::AttackSpeed},
    {"铁手套", 10, 4.0f, 7.0f, AffixType::FlatArmor},
    {"钢手套", 25, 10.0f, 15.0f, AffixType::CritChance},
    {"符文手套", 45, 20.0f, 30.0f, AffixType::CastSpeed},
    {"龙爪手套", 60, 35.0f, 50.0f, AffixType::CritDamage}};

static const std::vector<BaseItemDef> ARMOR_LEGS_BASES = {
    {"布裤", 1, 2.0f, 4.0f, AffixType::MoveSpeed},
    {"皮护腿", 10, 6.0f, 10.0f, AffixType::FlatHealth},
    {"锁甲护腿", 25, 15.0f, 22.0f, AffixType::FlatArmor},
    {"板甲护腿", 45, 30.0f, 42.0f, AffixType::PercentArmor},
    {"龙鳞护腿", 60, 55.0f, 75.0f, AffixType::Vitality}};

static const std::vector<BaseItemDef> ARMOR_FEET_BASES = {
    {"破旧靴子", 1, 1.0f, 3.0f, AffixType::MoveSpeed},
    {"皮靴", 10, 4.0f, 8.0f, AffixType::MoveSpeed},
    {"铁靴", 25, 10.0f, 16.0f, AffixType::FlatArmor},
    {"战靴", 45, 22.0f, 32.0f, AffixType::Strength},
    {"飞翼靴", 60, 40.0f, 55.0f, AffixType::MoveSpeed}};

static const std::vector<BaseItemDef> ARMOR_OFFHAND_BASES = {
    {"圆盾", 1, 5.0f, 10.0f, AffixType::FlatArmor},
    {"鸢盾", 10, 15.0f, 25.0f, AffixType::FlatHealth},
    {"塔盾", 25, 35.0f, 50.0f, AffixType::ResistAll},
    {"圣盾", 45, 60.0f, 80.0f, AffixType::PercentArmor},
    {"埃癸斯", 60, 90.0f, 120.0f, AffixType::DamageReduction}};

static const std::vector<BaseItemDef> JEWELRY_NECK_BASES = {
    {"铜项链", 1, 0.0f, 0.0f, AffixType::FlatHealth},
    {"银项链", 15, 0.0f, 0.0f, AffixType::ResistCold},
    {"金项链", 30, 0.0f, 0.0f, AffixType::ResistFire},
    {"红宝石项链", 50, 0.0f, 0.0f, AffixType::FlatFireDamage},
    {"龙骨项链", 70, 0.0f, 0.0f, AffixType::CritDamage}};

static const std::vector<BaseItemDef> JEWELRY_RING_BASES = {
    {"铁戒指", 1, 0.0f, 0.0f, AffixType::FlatHealth},
    {"银戒指", 15, 0.0f, 0.0f, AffixType::ResistLightning},
    {"金戒指", 30, 0.0f, 0.0f, AffixType::ResistAll},
    {"蓝宝石戒指", 50, 0.0f, 0.0f, AffixType::FlatMana},
    {"钻石戒指", 70, 0.0f, 0.0f, AffixType::CritChance}};

static const BaseItemDef &selectBaseItem(const std::vector<BaseItemDef> &db,
                                         int level) {
  int bestIndex = 0;
  for (size_t i = 0; i < db.size(); ++i) {
    if (level >= db[i].minLevel)
      bestIndex = i;
    else
      break;
  }
  if (bestIndex > 0 && std::uniform_int_distribution<>(0, 100)(t_rng) < 20)
    bestIndex--;
  return db[bestIndex];
}

Rarity ItemFactory::rollRarity(float magicFind) {
  int roll = std::uniform_int_distribution<>(0, 10000)(t_rng);
  int mfBoost = (int)(magicFind * 10);
  LOG_TRACE("根据魔法寻宝率 {} 掷骰稀有度，骰子结果: {}，魔法寻宝加成: {}",
            magicFind, roll, mfBoost);
  if (roll > 9500 - mfBoost) {
    LOG_DEBUG("掷出传奇稀有度");
    return Rarity::Legendary;
  }
  if (roll > 8000 - mfBoost) {
    LOG_DEBUG("掷出稀有稀有度");
    return Rarity::Rare;
  }
  if (roll > 5000 - mfBoost) {
    LOG_DEBUG("掷出魔法稀有度");
    return Rarity::Magic;
  }
  LOG_DEBUG("掷出普通稀有度");
  return Rarity::Common;
}

// -----------------------------------------------------------------------------
// Shared Value Logic
// -----------------------------------------------------------------------------
static void fillAffixDetails(Affix &affix, AffixType type, int tier) {
  affix.type = type;
  affix.tier = tier;
  float scale = (float)tier;

  auto rollVal = [&](float base, float variance) {
    return (base * scale) +
           std::uniform_real_distribution<>(0.0f, variance)(t_rng);
  };

  switch (type) {
  case AffixType::Strength:
  case AffixType::Dexterity:
  case AffixType::Intelligence:
  case AffixType::Vitality:
    affix.value = rollVal(3.0f, 2.0f);
    affix.isPrefix = false; // 后缀
    break;
  case AffixType::FlatHealth:
    affix.value = rollVal(10.0f, 5.0f);
    affix.isPrefix = true; // 前缀
    break;
  case AffixType::FlatMana:
    affix.value = rollVal(8.0f, 4.0f);
    affix.isPrefix = true;
    break;
  case AffixType::PercentPhysicalDamage:
  case AffixType::PercentFireDamage:
  case AffixType::PercentLightningDamage:
    affix.value = rollVal(5.0f, 3.0f);
    affix.isPrefix = true;
    break;
  case AffixType::FlatPhysicalDamage:
  case AffixType::FlatFireDamage:
    affix.value = rollVal(2.0f, 2.0f);
    affix.isPrefix = true;
    break;
  case AffixType::CritChance:
    affix.value = 1.0f + (scale * 0.8f);
    affix.isPrefix = false; // 通常是后缀
    break;
  case AffixType::MoveSpeed:
    affix.value = 5.0f + (scale * 2.0f);
    affix.isPrefix = false;
    break;
  case AffixType::AttackSpeed:
    affix.value = 5.0f + (scale * 1.5f);
    affix.isPrefix = false;
    break;
  case AffixType::FlatArmor:
  case AffixType::PercentArmor:
    affix.value = rollVal(10.0f, 5.0f);
    affix.isPrefix = true;
    break;
  case AffixType::ResistAll:
  case AffixType::ResistFire:
  case AffixType::ResistCold:
  case AffixType::ResistLightning:
    affix.value = rollVal(5.0f, 3.0f);
    affix.isPrefix = false;
    break;
  default:
    affix.value = rollVal(5.0f, 0.0f);
    break;
  }
}

Affix ItemFactory::createAffix(AffixType type, int tier) {
  Affix affix;
  affix.type = type;
  affix.tier = tier;

  // Try to find in definitions first (for JSON-driven values)
  for (const auto &def : s_affixDefinitions) {
    if (def.type == type) {
      affix.isPrefix = def.isPrefix;
      affix.required_tags =
          static_cast<Tag>(BuildRequiredSkillTagsAllMask(def));
      affix.modifier_record_ids = def.modifierRecordIds;
      // Find tier
      for (const auto &t : def.tiers) {
        if (t.tier == tier) {
          affix.value = (t.maxValue > t.minValue)
                            ? std::uniform_real_distribution<float>(
                                  t.minValue, t.maxValue)(t_rng)
                            : t.minValue;
          return affix;
        }
      }
      // Default to first tier if requested tier not found
      if (!def.tiers.empty()) {
        affix.value = def.tiers[0].minValue;
        return affix;
      }
    }
  }

  // Fallback to hardcoded logic
  fillAffixDetails(affix, type, tier);
  return affix;
}

std::pair<float, float> ItemFactory::getAffixRange(AffixType type, int tier) {
  for (const auto &def : s_affixDefinitions) {
    if (def.type == type) {
      for (const auto &t : def.tiers) {
        if (t.tier == tier) {
          return {t.minValue, t.maxValue};
        }
      }
    }
  }

  // Fallback if not found in definitions (maybe it was from fillAffixDetails)
  // For now, return a wide range or zero
  return {0.0f, 0.0f};
}

// 辅助函数：在所有基底列表中查找匹配名称的基底
static const BaseItemDef *findBaseByName(const std::string &name) {
  auto check =
      [&](const std::vector<BaseItemDef> &list) -> const BaseItemDef * {
    for (const auto &base : list) {
      if (name.find(base.name) != std::string::npos)
        return &base;
    }
    return nullptr;
  };

  if (auto *p = check(WEAPON_SWORD_BASES))
    return p;
  if (auto *p = check(WEAPON_AXE_BASES))
    return p;
  if (auto *p = check(WEAPON_DAGGER_BASES))
    return p;
  if (auto *p = check(WEAPON_HAMMER_BASES))
    return p;
  if (auto *p = check(WEAPON_GREATSWORD_BASES))
    return p;
  if (auto *p = check(WEAPON_STAFF_BASES))
    return p;
  if (auto *p = check(WEAPON_WAND_BASES))
    return p;

  if (auto *p = check(ARMOR_HEAD_BASES))
    return p;
  if (auto *p = check(ARMOR_CHEST_BASES))
    return p;
  if (auto *p = check(ARMOR_SHOULDER_BASES))
    return p;
  if (auto *p = check(ARMOR_HANDS_BASES))
    return p;
  if (auto *p = check(ARMOR_LEGS_BASES))
    return p;
  if (auto *p = check(ARMOR_FEET_BASES))
    return p;
  if (auto *p = check(ARMOR_OFFHAND_BASES))
    return p;

  if (auto *p = check(JEWELRY_NECK_BASES))
    return p;
  if (auto *p = check(JEWELRY_RING_BASES))
    return p;

  return nullptr;
}

std::pair<float, float>
ItemFactory::getBaseStatRange(const ItemComponent &item) {
  const BaseItemDef *base = findBaseByName(item.name);
  if (base) {
    return {base->baseStatMin, base->baseStatMax};
  }
  return {0.0f, 0.0f};
}

Affix ItemFactory::generateRandomAffix(int level, bool isPrefix,
                                       EquipmentSlot slot) {
  // 1. 获取所有候选词缀定义
  std::vector<const AffixDefinition *> candidates;
  if (!s_affixDefinitions.empty()) {
    // 确定槽位标签
    std::vector<std::string> slotTags;
    switch (slot) {
    case EquipmentSlot::MainHand:
      slotTags = {"weapon"};
      break;
    case EquipmentSlot::OffHand:
      slotTags = {"weapon", "armor"};
      break;
    case EquipmentSlot::Head:
    case EquipmentSlot::Shoulder:
    case EquipmentSlot::Chest:
    case EquipmentSlot::Legs:
      slotTags = {"armor"};
      break;
    case EquipmentSlot::Hands:
      slotTags = {"armor", "gloves"};
      break;
    case EquipmentSlot::Feet:
      slotTags = {"armor", "boots"};
      break;
    case EquipmentSlot::Neck:
    case EquipmentSlot::Ring1:
    case EquipmentSlot::Ring2:
      slotTags = {"jewelry"};
      break;
    default:
      slotTags = {"misc"};
      break;
    }

    for (const auto &def : s_affixDefinitions) {
      if (def.isPrefix != isPrefix)
        continue;
      if (!IsRandomRollableAffix(def.type))
        continue; // 排除传奇/独特词缀

      bool tagMatch = false;
      for (const auto &sTag : slotTags) {
        for (const auto &aTag : def.allowedTags) {
          if (sTag == aTag) {
            tagMatch = true;
            break;
          }
        }
        if (tagMatch)
          break;
      }
      if (!tagMatch)
        continue;

      if (def.tiers.empty() || def.tiers[0].minLevel > level)
        continue;

      candidates.push_back(&def);
    }
  }

  if (candidates.empty()) {
    // 回退到基础属性
    std::vector<AffixType> fallbacks = {
        AffixType::Strength, AffixType::Dexterity, AffixType::Intelligence,
        AffixType::Vitality};
    std::uniform_int_distribution<> fDist(0, (int)fallbacks.size() - 1);
    AffixType selectedType = fallbacks[fDist(t_rng)];
    Affix aff = createAffix(selectedType, 1);
    aff.isPrefix = isPrefix;
    return aff;
  }

  // 从候选池随机选择一个
  std::uniform_int_distribution<> dist(0, (int)candidates.size() - 1);
  const AffixDefinition *selectedDef = candidates[dist(t_rng)];

  // 选择最高可用 Tier
  int bestTierIdx = 0;
  for (int i = 0; i < (int)selectedDef->tiers.size(); ++i) {
    if (selectedDef->tiers[i].minLevel <= level)
      bestTierIdx = i;
    else
      break;
  }

  const auto &tier = selectedDef->tiers[bestTierIdx];
  Affix result;
  result.type = selectedDef->type;
  result.tier = tier.tier;
  result.isPrefix = selectedDef->isPrefix;
  result.required_tags =
      static_cast<Tag>(BuildRequiredSkillTagsAllMask(*selectedDef));
  result.modifier_record_ids = selectedDef->modifierRecordIds;
  // result.name = selectedDef->nameTemplate; // REMOVED
  result.value = (tier.maxValue > tier.minValue)
                     ? std::uniform_real_distribution<float>(
                           tier.minValue, tier.maxValue)(t_rng)
                     : tier.minValue;

  return result;
}

void ItemFactory::rollAffixes(ItemComponent &item, int level) {
  int maxPrefix = 0, maxSuffix = 0;

  switch (item.rarity) {
  case Rarity::Magic:
    maxPrefix = 1;
    maxSuffix = 1;
    break;
  case Rarity::Rare:
  case Rarity::Epic:
    maxPrefix = 2;
    maxSuffix = 2;
    break;
  case Rarity::Legendary:
    maxPrefix = 2;
    maxSuffix = 2;
    break;
  case Rarity::Ancient:
  case Rarity::Mythic:
    maxPrefix = 3;
    maxSuffix = 3;
    break;
  default:
    break;
  }

  int prefixCount = 0, suffixCount = 0;
  if (item.rarity != Rarity::Common) {
    if (std::uniform_int_distribution<>(0, 1)(t_rng)) {
      prefixCount = (maxPrefix > 0) ? std::uniform_int_distribution<>(1, maxPrefix)(t_rng) : 0;
      suffixCount = (maxSuffix >= 0) ? std::uniform_int_distribution<>(0, maxSuffix)(t_rng) : 0;
    } else {
      prefixCount = (maxPrefix >= 0) ? std::uniform_int_distribution<>(0, maxPrefix)(t_rng) : 0;
      suffixCount = (maxSuffix > 0) ? std::uniform_int_distribution<>(1, maxSuffix)(t_rng) : 0;
    }
  }

  // 记录已有的词缀类型以避免重复
  std::vector<AffixType> existingTypes;
  for (const auto &aff : item.implicits)
    existingTypes.push_back(aff.type);

  auto pickAffixes = [&](bool isPrefix, int count) {
    if (count <= 0)
      return;

    // 1. 获取所有符合条件的候选词缀定义
    std::vector<const AffixDefinition *> candidates;

    // 确定槽位标签
    std::vector<std::string> slotTags;
    switch (item.slot) {
    case EquipmentSlot::MainHand:
      slotTags = {"weapon"};
      break;
    case EquipmentSlot::OffHand:
      slotTags = {"weapon", "armor"};
      break;
    case EquipmentSlot::Head:
    case EquipmentSlot::Shoulder:
    case EquipmentSlot::Chest:
    case EquipmentSlot::Legs:
      slotTags = {"armor"};
      break;
    case EquipmentSlot::Hands:
      slotTags = {"armor", "gloves"};
      break;
    case EquipmentSlot::Feet:
      slotTags = {"armor", "boots"};
      break;
    case EquipmentSlot::Neck:
    case EquipmentSlot::Ring1:
    case EquipmentSlot::Ring2:
      slotTags = {"jewelry"};
      break;
    default:
      slotTags = {"misc"};
      break;
    }

    // 从数据库筛选
    for (const auto &def : s_affixDefinitions) {
      if (def.isPrefix != isPrefix)
        continue;
      if (!IsRandomRollableAffix(def.type))
        continue; // 排除传奇/独特词缀

      bool tagMatch = false;
      for (const auto &sTag : slotTags) {
        for (const auto &aTag : def.allowedTags) {
          if (sTag == aTag) {
            tagMatch = true;
            break;
          }
        }
        if (tagMatch)
          break;
      }
      if (!tagMatch)
        continue;
      if (def.tiers.empty() || def.tiers[0].minLevel > level)
        continue;

      // 检查是否重复
      bool isDuplicate = false;
      for (auto et : existingTypes) {
        if (et == def.type) {
          isDuplicate = true;
          break;
        }
      }
      if (isDuplicate)
        continue;

      candidates.push_back(&def);
    }

    // 如果数据库候选不足，使用基础属性回退
    if (candidates.empty()) {
      std::vector<AffixType> fallbacks = {
          AffixType::Strength, AffixType::Dexterity, AffixType::Intelligence,
          AffixType::Vitality};
      for (auto fType : fallbacks) {
        bool isDuplicate = false;
        for (auto et : existingTypes)
          if (et == fType) {
            isDuplicate = true;
            break;
          }
        if (isDuplicate)
          continue;

        // 创建临时定义（或者直接生成词缀）
        Affix aff = createAffix(fType, 1);
        aff.isPrefix = isPrefix;
        item.affixes.push_back(aff);
        existingTypes.push_back(fType);
        if (--count <= 0)
          break;
      }
      return;
    }

    // 打乱候选池并按需挑选
    std::shuffle(candidates.begin(), candidates.end(), t_rng);
    int toAdd = std::min(count, (int)candidates.size());

    for (int i = 0; i < toAdd; ++i) {
      const auto *def = candidates[i];

      // 选择 Tier
      int bestTierIdx = 0;
      for (int j = 0; j < (int)def->tiers.size(); ++j) {
        if (def->tiers[j].minLevel <= level)
          bestTierIdx = j;
        else
          break;
      }
      const auto &tier = def->tiers[bestTierIdx];

      Affix result;
      result.type = def->type;
      result.tier = tier.tier;
      result.isPrefix = def->isPrefix;
      result.required_tags =
          static_cast<Tag>(BuildRequiredSkillTagsAllMask(*def));
      result.modifier_record_ids = def->modifierRecordIds;
      // result.name = def->nameTemplate; // REMOVED
      result.value = (tier.maxValue > tier.minValue)
                         ? std::uniform_real_distribution<float>(
                               tier.minValue, tier.maxValue)(t_rng)
                         : tier.minValue;

      item.affixes.push_back(result);
      existingTypes.push_back(result.type);
    }
  };

  pickAffixes(true, prefixCount);
  pickAffixes(false, suffixCount);

  LOG_DEBUG("ItemFactory: Generated {} affixes for {}", item.affixes.size(),
            item.name);
}

// -----------------------------------------------------------------------------
// 创建方法 (与之前相同)
// -----------------------------------------------------------------------------
entt::entity ItemFactory::createRandomLoot(entt::registry &registry, int level,
                                           float magicFind) {
  float effectiveMF = magicFind;
  if(registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
      effectiveMF += registry.ctx().get<NoMoreDay::ActiveDimensionalState>().calculatedRarity * 100.0f;
  }
  
  LOG_DEBUG("创建随机掉落物，地图等级: {}，物品等级: {}，魔法寻宝率: {}", level, level, effectiveMF);
  Rarity rarity = rollRarity(effectiveMF);
  entt::entity result;
  if (std::uniform_int_distribution<>(0, 2)(t_rng) == 0) {
    LOG_DEBUG("正在生成武器");
    result = createWeapon(registry, level, rarity);
  } else {
    LOG_DEBUG("正在生成护甲/首饰");

    // 定义有效的随机槽位列表 (使用通用 Ring，排除 Ring1/Ring2)
    static const std::vector<EquipmentSlot> validSlots = {
        EquipmentSlot::OffHand, EquipmentSlot::Head,  EquipmentSlot::Shoulder,
        EquipmentSlot::Chest,   EquipmentSlot::Hands, EquipmentSlot::Legs,
        EquipmentSlot::Feet,    EquipmentSlot::Neck,  EquipmentSlot::Ring};

    int idx =
        std::uniform_int_distribution<>(0, (int)validSlots.size() - 1)(t_rng);
    EquipmentSlot slot = validSlots[idx];
    result = createArmor(registry, level, rarity, slot);
  }
  LOG_DEBUG("Created random loot entity: {}", (uint32_t)result);
  return result;
}

entt::entity ItemFactory::restoreItem(entt::registry &registry,
                                      const SerializedItem &dto) {
  auto entity = registry.create();
  ItemComponent item;
  item.id = dto.itemId;
  item.name = dto.name;
  item.type = dto.type;
  item.textureId = dto.textureId;
  item.quantity = dto.quantity;

  // Restore stats from snapshot
  item.rarity = dto.stats.rarity;
  item.slot = dto.stats.slot;
  item.attack = dto.stats.attack;
  item.defense = dto.stats.defense;
  item.forgingPotential = dto.stats.forgingPotential;
  item.legendaryPotential = dto.stats.legendaryPotential;
  item.value = dto.stats.value;

  // Restore affixes
  for (const auto &sAff : dto.affixes) {
    Affix aff;
    aff.type = sAff.type;
    aff.tier = sAff.tier;
    aff.value = sAff.value;
    aff.isPrefix = sAff.isPrefix;
    aff.isLegendary = sAff.isLegendary;
    // aff.name = sAff.name; // REMOVED
    aff.required_tags = sAff.required_tags;
    aff.modifier_record_ids = sAff.modifier_record_ids;
    item.affixes.push_back(aff);
  }

  for (const auto &sAff : dto.implicits) {
    Affix aff;
    aff.type = sAff.type;
    aff.tier = sAff.tier;
    aff.value = sAff.value;
    aff.isPrefix = sAff.isPrefix;
    aff.isLegendary = sAff.isLegendary;
    // aff.name = sAff.name; // REMOVED
    aff.required_tags = sAff.required_tags;
    aff.modifier_record_ids = sAff.modifier_record_ids;
    item.implicits.push_back(aff);
  }

  // Sockets (Recursive)
  for (const auto &sSocket : dto.socketedItems) {
    auto socketEntity = restoreItem(registry, sSocket);
    item.sockets.push_back(socketEntity);
  }
  item.socketCount = (int)item.sockets.size();

  registry.emplace<ItemComponent>(entity, item);

  // Visuals: Try to restore Sprite if textureId is valid
  if (item.textureId != 0) {
    Texture2D tex = AssetLoadingSystem::GetTexture(item.textureId);
    if (tex.id > 0) {
      float dropScale = 32.0f / (float)std::max(tex.width, tex.height);
      registry.emplace<SpriteComponent>(entity, tex, dropScale);
    }
  } else {
    // Fallback or generic visuals for materials etc.
    if (item.type == ItemType::Material) {
      registry.emplace<ColorComponent>(entity, YELLOW);
    }
  }

  return entity;
}

entt::entity ItemFactory::createWeapon(entt::registry &registry, int level,
                                       Rarity rarity) {
  LOG_DEBUG("Creating weapon with level: {}, rarity: {}", level,
            static_cast<int>(rarity));
  auto entity = registry.create();
  ItemComponent item;
  item.itemLevel = level; // [NEW] Set Item Level
  item.type = ItemType::Weapon;
  item.slot = EquipmentSlot::MainHand;
  item.rarity = rarity;
  item.id = std::uniform_int_distribution<>(1000, 9999)(t_rng);

  // 随机选择武器类型
  const std::vector<BaseItemDef> *baseList = &WEAPON_SWORD_BASES;
  int typeRoll = std::uniform_int_distribution<>(0, 6)(t_rng);
  switch (typeRoll) {
  case 0:
    baseList = &WEAPON_SWORD_BASES;
    item.weaponSubtype = WeaponSubtype::Sword;
    break;
  case 1:
    baseList = &WEAPON_AXE_BASES;
    item.weaponSubtype = WeaponSubtype::Axe;
    break;
  case 2:
    baseList = &WEAPON_DAGGER_BASES;
    item.weaponSubtype = WeaponSubtype::Dagger;
    break;
  case 3:
    baseList = &WEAPON_HAMMER_BASES;
    item.weaponSubtype = WeaponSubtype::Mace;
    break;
  case 4:
    baseList = &WEAPON_GREATSWORD_BASES;
    item.weaponSubtype = WeaponSubtype::Sword; // Or Greatsword
    item.isTwoHanded = true;
    break;
  case 5:
    baseList = &WEAPON_STAFF_BASES;
    item.weaponSubtype = WeaponSubtype::Staff;
    item.isTwoHanded = true;
    break;
  case 6:
    baseList = &WEAPON_WAND_BASES;
    item.weaponSubtype = WeaponSubtype::Wand;
    break;
  }

  const auto &base = selectBaseItem(*baseList, level);
  item.name = base.name;
  LOG_DEBUG("Selected base weapon: {}", base.name);

  float baseVal = std::uniform_real_distribution<>(base.baseStatMin,
                                                   base.baseStatMax)(t_rng);
  // [NEW] Level Scaling
  float multiplier = Constants::Items::GetLevelMultiplier(level);
  item.attack = baseVal * multiplier;
  item.value = item.attack * 5.0f; // Basic value estimation

  // Implicit
  item.implicits.push_back(
      createAffix(base.implicitType,
                  1)); // Implicit usually unscaled or custom? Assume T1 for now
  item.implicits.back().value =
      std::uniform_real_distribution<>(5.0f, 15.0f)(t_rng) + (level * 0.5f);
  item.implicits.back().tier = 0;
  // item.implicits.back().name = "固有"; // REMOVED

  item.forgingPotential = std::uniform_int_distribution<>(20, 50)(t_rng);

  // IMPORTANT: Legendary items (Uniques) roll LP (0-4). Sockets are
  // independent.
  if (rarity == Rarity::Legendary) {
    item.name = "远古 " + item.name;
    // In Last Epoch style, LP is rarity-based. Here we use a simple weighted
    // roll.
    // Apply Dimensional Modifiers to LP Roll
    float lpBoost = 0.0f;
    if (registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
         lpBoost = registry.ctx().get<NoMoreDay::ActiveDimensionalState>().calculatedRarity;
         // e.g. 100% Rarity => +20 to roll? Or scale?
         // Let's use scale strategy: shift the roll towards 100
    }

    int lpRoll = std::uniform_int_distribution<>(0, 100)(t_rng);
    
    // Boost logic: shift roll based on rarity bonus
    // 100% Rarity (+1.0) -> +15 flat roll (Moves 85->100, Massive buff for high LP)
    // 500% Rarity (+5.0) -> +75 flat roll
    int flatBonus = static_cast<int>(lpBoost * 15.0f);
    lpRoll = std::min(100, lpRoll + flatBonus);

    if (lpRoll < 60)
      item.legendaryPotential = 0;
    else if (lpRoll < 85)
      item.legendaryPotential = 1;
    else if (lpRoll < 95)
      item.legendaryPotential = 2;
    else if (lpRoll < 99)
      item.legendaryPotential = 3;
    else
      item.legendaryPotential = 4;

    LOG_DEBUG("Created legendary weapon: {} with LP {}", item.name,
              item.legendaryPotential);
  } else if (rarity == Rarity::Rare) {
    item.name = "稀有 " + item.name;
    LOG_DEBUG("Created rare weapon: {}", item.name);
  } else {
    LOG_DEBUG("Created common/magic weapon: {}", item.name);
  }

  // Assign random texture
  item.textureId = getRandomTextureForType(item.type, item.slot, item.name,
                                           item.weaponSubtype);

  rollAffixes(item, level);

  // Sockets for Weapons (Independent of Rarity/LP)
  // 40% chance to have sockets
  int socketChance = 40;
  if (registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
      socketChance = static_cast<int>(40 * (1.0f + registry.ctx().get<NoMoreDay::ActiveDimensionalState>().calculatedRarity * 0.5f));
  }
  if (std::uniform_int_distribution<>(0, 100)(t_rng) < socketChance) {
    item.socketCount = std::uniform_int_distribution<>(1, 3)(t_rng);
    LOG_DEBUG("Weapon '{}' rolled with {} sockets", item.name,
              item.socketCount);
  }

  registry.emplace<ItemComponent>(entity, item);

  // Assign Sprite based on item type/name (Legacy/World)
  // Use textureId if available
  if (item.textureId != 0) {
    Texture2D tex = AssetLoadingSystem::GetTexture(item.textureId);
    if (tex.id > 0) {
      float dropScale = 32.0f / (float)std::max(tex.width, tex.height);
      registry.emplace<SpriteComponent>(entity, tex, dropScale);
      LOG_DEBUG("Assigned weapon sprite (ID: {}) to entity: {}", item.textureId,
                (uint32_t)entity);
    }
  } else if (item.type == ItemType::Weapon) {
    // Fallback
    Texture2D tex =
        AssetLoadingSystem::GetTexture(assets::textures::Weapon_Sword.id);
    if (tex.id > 0) {
      float dropScale = 32.0f / (float)std::max(tex.width, tex.height);
      registry.emplace<SpriteComponent>(entity, tex, dropScale);
    }
  }

  LOG_DEBUG("Weapon created with entity ID: {}", (uint32_t)entity);
  return entity;
}

entt::entity ItemFactory::createArmor(entt::registry &registry, int level,
                                      Rarity rarity, EquipmentSlot slot) {
  LOG_DEBUG("Creating armor with level: {}, rarity: {}, slot: {}", level,
            static_cast<int>(rarity), static_cast<int>(slot));
  auto entity = registry.create();
  ItemComponent item;
  item.itemLevel = level; // [NEW] Set Item Level
  if (slot == EquipmentSlot::Neck || slot == EquipmentSlot::Ring ||
      slot == EquipmentSlot::Ring1 || slot == EquipmentSlot::Ring2) {
    item.type = ItemType::Jewelry;
  } else {
    item.type = ItemType::Armor;

    // Sockets for Armor
    // 40% chance
    int socketChance = 40;
    if (registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
        socketChance = static_cast<int>(40 * (1.0f + registry.ctx().get<NoMoreDay::ActiveDimensionalState>().calculatedRarity * 0.5f));
    }
    if (std::uniform_int_distribution<>(0, 100)(t_rng) < socketChance) {
      int maxS = 1;
      if (slot == EquipmentSlot::Chest || slot == EquipmentSlot::OffHand)
        maxS = 3;
      else if (slot == EquipmentSlot::Head || slot == EquipmentSlot::Legs)
        maxS = 2;

      item.socketCount = std::uniform_int_distribution<>(1, maxS)(t_rng);
      LOG_DEBUG("Armor/Jewelry '{}' rolled with {} sockets", item.name,
                item.socketCount);
    }
  }

  item.slot = slot;
  item.rarity = rarity;
  item.id = std::uniform_int_distribution<>(1000, 9999)(t_rng);

  // 根据槽位选择正确的基底列表
  const std::vector<BaseItemDef> *baseList = &ARMOR_CHEST_BASES;
  switch (slot) {
  case EquipmentSlot::Head:
    baseList = &ARMOR_HEAD_BASES;
    break;
  case EquipmentSlot::Chest:
    baseList = &ARMOR_CHEST_BASES;
    break;
  case EquipmentSlot::Shoulder:
    baseList = &ARMOR_SHOULDER_BASES;
    break;
  case EquipmentSlot::Hands:
    baseList = &ARMOR_HANDS_BASES;
    break;
  case EquipmentSlot::Legs:
    baseList = &ARMOR_LEGS_BASES;
    break;
  case EquipmentSlot::Feet:
    baseList = &ARMOR_FEET_BASES;
    break;
  case EquipmentSlot::OffHand:
    baseList = &ARMOR_OFFHAND_BASES;
    break;
  case EquipmentSlot::Neck:
    baseList = &JEWELRY_NECK_BASES;
    break;
  case EquipmentSlot::Ring:
    baseList = &JEWELRY_RING_BASES;
    break;
  case EquipmentSlot::Ring1:
  case EquipmentSlot::Ring2:
    baseList = &JEWELRY_RING_BASES;
    break;
  default:
    break; // 默认为胸甲
  }

  const auto &base = selectBaseItem(*baseList, level);
  item.name = base.name;
  LOG_DEBUG("Selected base armor: {}", base.name);

  float baseVal = std::uniform_real_distribution<>(base.baseStatMin,
                                                   base.baseStatMax)(t_rng);
  // [NEW] Level Scaling
  float multiplier = Constants::Items::GetLevelMultiplier(level);
  item.defense = baseVal * multiplier;
  item.value = item.defense * 5.0f; // Basic value estimation

  item.implicits.push_back(createAffix(base.implicitType, 1));
  item.implicits.back().tier = 0;

  item.forgingPotential = std::uniform_int_distribution<>(20, 50)(t_rng);

  if (rarity == Rarity::Legendary) {
    item.name = "远古 " + item.name;
    // In Last Epoch style, LP is rarity-based. Here we use a simple weighted
    // roll.
    // Apply Dimensional Modifiers to LP Roll
    float lpBoost = 0.0f;
    if (registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
         lpBoost = registry.ctx().get<NoMoreDay::ActiveDimensionalState>().calculatedRarity;
    }

    int lpRoll = std::uniform_int_distribution<>(0, 100)(t_rng);
    int flatBonus = static_cast<int>(lpBoost * 15.0f);
    lpRoll = std::min(100, lpRoll + flatBonus);

    if (lpRoll < 70)
      item.legendaryPotential = 0;
    else if (lpRoll < 90)
      item.legendaryPotential = 1;
    else if (lpRoll < 97)
      item.legendaryPotential = 2;
    else
      item.legendaryPotential = 3;

    LOG_DEBUG("Created legendary armor/jewelry: {} with LP {}", item.name,
              item.legendaryPotential);
  } else {
    LOG_DEBUG("Created common/magic/rare armor/jewelry: {}", item.name);
  }

  // Assign random texture
  item.textureId = getRandomTextureForType(item.type, item.slot, item.name,
                                           item.weaponSubtype);

  rollAffixes(item, level);
  registry.emplace<ItemComponent>(entity, item);

  // Assign Sprite for World (Dropped Item)
  if (item.textureId != 0) {
    Texture2D tex = AssetLoadingSystem::GetTexture(item.textureId);
    if (tex.id > 0) {
      float dropScale = 32.0f / (float)std::max(tex.width, tex.height);
      registry.emplace<SpriteComponent>(entity, tex, dropScale);
    }
  }

  LOG_DEBUG("Armor created with entity ID: {}", (uint32_t)entity);
  return entity;
}

entt::entity ItemFactory::createBag(entt::registry &registry, int level,
                                    Rarity rarity) {
  auto entity = registry.create();
  ItemComponent item;
  item.type = ItemType::Bag;
  item.rarity = rarity;
  item.slot = EquipmentSlot::None;
  item.id = std::uniform_int_distribution<>(5000, 5999)(t_rng);

  // 基础容量
  // 修改：每个背包现在提供一个完整的页面 (56格)
  int baseCap = 56;

  // 稀有度加成 (可选：也许稀有背包提供更多页？目前保持一致)
  // if (rarity >= Rarity::Magic) baseCap += 0;

  item.name = (rarity == Rarity::Common ? "亚麻背包" : "魔法背包");
  item.bagCapacity = baseCap;
  item.description = "增加一个背包页面 (" + std::to_string(baseCap) + " 格)。";

  const std::array<entt::id_type, 2> iconCandidates = {
      "item_bag_default"_hs,
      assets::ui::textures::Inventory_Slot.id,
  };

  item.textureId = iconCandidates.back();

  for (entt::id_type candidate : iconCandidates) {
    Texture2D tex = AssetLoadingSystem::GetTexture(candidate);
    if (tex.id > 0) {
      item.textureId = candidate;
      break;
    }
  }

  registry.emplace<ItemComponent>(entity, item);

  if (!TryAttachWorldSprite(registry, entity, item.textureId)) {
    registry.emplace<ColorComponent>(entity, BROWN);
  }

  return entity;
}

entt::entity ItemFactory::createMaterial(entt::registry &registry,
                                         uint32_t materialId, int quantity) {
  // 1. Try Material Registry
  if (const auto *def = MaterialRegistry::Get().GetMaterial(materialId)) {
    auto entity = registry.create();
    ItemComponent item;
    item.type = ItemType::Material;
    item.id = materialId;
    item.name = def->name;
    item.description = def->description;
    item.rarity = def->rarity;
    item.maxStack = def->maxStack;
    item.quantity = quantity;
    item.slot = EquipmentSlot::None;
    
    // Fix: Force Legendary Core (Catalyst) to be Consumable so it can have "Use" action (Open UI)
    if (materialId == 10001) {
        item.type = ItemType::Consumable;
    }

    registry.emplace<ItemComponent>(entity, item);

    // Visuals: Use ColorComponent based on rarity for now
    Color color = WHITE;
    switch (def->rarity) {
    case Rarity::Common:
      color = LIGHTGRAY;
      break;
    case Rarity::Uncommon:
      color = GREEN;
      break;
    case Rarity::Rare:
      color = BLUE;
      break;
    case Rarity::Epic:
      color = PURPLE;
      break;
    case Rarity::Legendary:
      color = ORANGE;
      break;
    case Rarity::Ancient:
      color = RED;
      break;
    default:
      break;
    }
    registry.emplace<ColorComponent>(entity, color);

    LOG_DEBUG("Created material '{}' (ID: {}) x{} with entity ID: {}",
              def->name, materialId, quantity, (uint32_t)entity);
    return entity;
  }

  // 2. Try Runeword System (Runes)
  if (const auto *runeDef = RunewordSystem::getRune(materialId)) {
    auto entity = registry.create();
    ItemComponent item;
    item.type = ItemType::Material; // Runes are materials
    item.id = materialId;
    item.name = "符文·" + runeDef->name;
    item.description = "Can be socketed into items.";
    // Determine rarity based on Tier
    if (runeDef->tier >= 3)
      item.rarity = Rarity::Legendary;
    else if (runeDef->tier == 2)
      item.rarity = Rarity::Rare;
    else
      item.rarity = Rarity::Uncommon;

    item.maxStack = 99;
    item.quantity = quantity;
    item.slot = EquipmentSlot::None;

    // Add AffixStats info to description? (Optional for now)

    // Assign Texture from RuneAssetRegistry
    int idx = (int)materialId - 3001;
    if (idx >= 0 && idx < (int)assets::runes::general::All.size()) {
      item.textureId = assets::runes::general::All[idx]->id;
    }

    registry.emplace<ItemComponent>(entity, item);
    registry.emplace<ColorComponent>(entity, ORANGE); // Runes distinct color

    // Visuals: Sprite
    if (item.textureId != 0) {
      Texture2D tex = AssetLoadingSystem::GetTexture(item.textureId);
      if (tex.id > 0) {
        float dropScale = 32.0f / (float)std::max(tex.width, tex.height);
        registry.emplace<SpriteComponent>(entity, tex, dropScale);
      }
    }

    LOG_DEBUG("Created Rune '{}' (ID: {}) x{}", runeDef->name, materialId,
              quantity);
    return entity;
  }

  LOG_ERROR("ItemFactory: Failed to create material with ID {}", materialId);
  return entt::null;
}

entt::entity ItemFactory::createPotion(entt::registry &registry, int type,
                                       int quantity) {
  auto entity = registry.create();
  ItemComponent item;
  item.type = ItemType::Consumable;
  item.rarity = Rarity::Common;
  item.quantity = quantity;
  item.maxStack = 99; // 药水可堆叠
  item.slot = EquipmentSlot::None;

  if (type == 0) {
    item.id = 101; // ID 约定: 101 红药水
    item.name = "生命药水";
    item.description = "使用: 恢复 50 点生命值";
    item.value = 10;
    registry.emplace<ColorComponent>(entity, RED); // 地面显示红色

    const std::array<entt::id_type, 2> iconCandidates = {
        "item_potion_health"_hs,
        assets::ui::textures::Slot_Ring_1.id,
    };
    item.textureId = iconCandidates.back();
    for (entt::id_type candidate : iconCandidates) {
      Texture2D tex = AssetLoadingSystem::GetTexture(candidate);
      if (tex.id > 0) {
        item.textureId = candidate;
        break;
      }
    }
  } else {
    item.id = 102; // ID 约定: 102 蓝药水
    item.name = "法力药水";
    item.description = "使用: 恢复 50 点法力值";
    item.value = 10;
    registry.emplace<ColorComponent>(entity, BLUE); // 地面显示蓝色

    const std::array<entt::id_type, 2> iconCandidates = {
        "item_potion_mana"_hs,
        assets::ui::textures::Slot_Ring_2.id,
    };
    item.textureId = iconCandidates.back();
    for (entt::id_type candidate : iconCandidates) {
      Texture2D tex = AssetLoadingSystem::GetTexture(candidate);
      if (tex.id > 0) {
        item.textureId = candidate;
        break;
      }
    }
  }

  registry.emplace<ItemComponent>(entity, item);

  TryAttachWorldSprite(registry, entity, item.textureId);

  return entity;
}

} // namespace NoMoreDay
