#include "ItemFactory.hpp"
#include <random>
#include <algorithm>
#include <map>
#include <vector>
#include "../tools/Logger.hpp"
#include "AssetLoadingSystem.hpp"
#include "AssetRegistry.hpp"
#include "../components/Common.hpp"

namespace NoMoreDay {

// Simple Random Helper
static std::mt19937 g_rng;

std::map<uint32_t, LootPool> ItemFactory::s_lootPools;

void ItemFactory::initialize() {
 std::random_device rd;
 g_rng.seed(rd());

 // 初始化全局掉落池 (ID 0)
 LootPool globalPool;
 globalPool.name = "全局掉落池";
 globalPool.entries = {
 { LootEntryType::Item, 0, 1, 1, 10.0f }, // 随机物品
 { LootEntryType::Gold, 0, 5, 20, 90.0f } // 金币
 };
 globalPool.totalWeight = 100.0f;
 s_lootPools[0] = globalPool;

 LOG_INFO("ItemFactory 已使用全局掉落池初始化。");
}

// ... existing code ...

void ItemFactory::addLootPool(uint32_t id, const LootPool& pool) {
    s_lootPools[id] = pool;
    float total = 0.0f;
    for (const auto& entry : pool.entries) total += entry.weight;
    s_lootPools[id].totalWeight = total;
}

const LootPool* ItemFactory::getLootPool(uint32_t id) {
    auto it = s_lootPools.find(id);
    if (it != s_lootPools.end()) return &it->second;
    return nullptr;
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

static const std::vector<BaseItemDef> WEAPON_BASES = {
    {"Rusty Sword",      1,  5.0f,  8.0f,  AffixType::PercentPhysicalDamage},
    {"Iron Longsword",   10, 12.0f, 18.0f, AffixType::PercentPhysicalDamage},
    {"Steel Claymore",   25, 25.0f, 35.0f, AffixType::CritChance},
    {"Mithril Blade",    45, 45.0f, 60.0f, AffixType::AttackSpeed},
    {"Crystal Saber",    60, 70.0f, 90.0f, AffixType::PercentFireDamage},
    {"Demon Edge",       75, 100.0f,130.0f,AffixType::CritDamage}
};

static const std::vector<BaseItemDef> ARMOR_BASES = {
    {"Tattered Robe",    1,  2.0f,  5.0f,  AffixType::FlatMana},
    {"Leather Tunic",    10, 8.0f,  12.0f, AffixType::FlatHealth},
    {"Chainmail",        25, 20.0f, 30.0f, AffixType::ResistAll},
    {"Plate Mail",       45, 40.0f, 55.0f, AffixType::PercentArmor},
    {"Dragon Scale",     70, 70.0f, 90.0f, AffixType::FlatHealth}
};

static const BaseItemDef& selectBaseItem(const std::vector<BaseItemDef>& db, int level) {
    int bestIndex = 0;
    for (size_t i = 0; i < db.size(); ++i) {
        if (level >= db[i].minLevel) bestIndex = i; else break; 
    }
    if (bestIndex > 0 && std::uniform_int_distribution<>(0, 100)(g_rng) < 20) bestIndex--;
    return db[bestIndex];
}

Rarity ItemFactory::rollRarity(float magicFind) {
 int roll = std::uniform_int_distribution<>(0, 10000)(g_rng);
 int mfBoost = (int)(magicFind * 10);
 LOG_TRACE("根据魔法寻宝率 {} 掷骰稀有度，骰子结果: {}，魔法寻宝加成: {}", magicFind, roll, mfBoost);
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
static void fillAffixDetails(Affix& affix, AffixType type, int tier) {
    affix.type = type;
    affix.tier = tier;
    float scale = (float)tier;

    auto rollVal = [&](float base, float variance) {
        return (base * scale) + std::uniform_real_distribution<>(0.0f, variance)(g_rng);
    };

    switch (type) {
        case AffixType::Strength: 
        case AffixType::Dexterity:
        case AffixType::Intelligence:
        case AffixType::Vitality:
            affix.value = rollVal(3.0f, 2.0f);
            affix.name = "力量";
            affix.isPrefix = false; // 后缀
            break;
        case AffixType::FlatHealth:
            affix.value = rollVal(10.0f, 5.0f);
            affix.name = "坚韧的";
            affix.isPrefix = true; // 前缀
            break;
        case AffixType::FlatMana:
            affix.value = rollVal(8.0f, 4.0f);
            affix.name = "神秘的";
            affix.isPrefix = true;
            break;
        case AffixType::PercentPhysicalDamage:
        case AffixType::PercentFireDamage:
        case AffixType::PercentLightningDamage:
            affix.value = rollVal(5.0f, 3.0f);
            affix.name = "残酷的";
            affix.isPrefix = true;
            break;
        case AffixType::FlatPhysicalDamage:
        case AffixType::FlatFireDamage:
             affix.value = rollVal(2.0f, 2.0f);
             affix.name = "锋利的";
             affix.isPrefix = true;
             break;
        case AffixType::CritChance:
            affix.value = 1.0f + (scale * 0.8f); 
            affix.name = "致命的";
            affix.isPrefix = false; // 通常是后缀
            break;
        case AffixType::MoveSpeed:
            affix.value = 5.0f + (scale * 2.0f); 
            affix.name = "迅捷的";
            affix.isPrefix = false; 
            break;
        case AffixType::AttackSpeed:
            affix.value = 5.0f + (scale * 1.5f); 
            affix.name = "快速的";
            affix.isPrefix = false;
            break;
        case AffixType::FlatArmor:
        case AffixType::PercentArmor:
            affix.value = rollVal(10.0f, 5.0f);
            affix.name = "强化的";
            affix.isPrefix = true;
            break;
        case AffixType::ResistAll:
        case AffixType::ResistFire:
        case AffixType::ResistCold:
        case AffixType::ResistLightning:
            affix.value = rollVal(5.0f, 3.0f);
            affix.name = "守护的";
            affix.isPrefix = false;
            break;
        default:
            affix.value = rollVal(5.0f, 0.0f);
            affix.name = "Enhanced";
            break;
    }
}

Affix ItemFactory::createAffix(AffixType type, int tier) {
    Affix affix;
    fillAffixDetails(affix, type, tier);
    return affix;
}

Affix ItemFactory::generateRandomAffix(int level, bool isPrefix, EquipmentSlot slot) {
    // 1. 根据槽位和位置选择词缀类型候选
    std::vector<AffixType> candidates;

    if (isPrefix) {
        // --- 前缀 ---
        if (slot == EquipmentSlot::MainHand || slot == EquipmentSlot::OffHand) {
             candidates = { 
                AffixType::FlatPhysicalDamage, AffixType::FlatFireDamage,
                AffixType::PercentPhysicalDamage, AffixType::PercentFireDamage,
                AffixType::PercentLightningDamage 
             };
        } else if (slot == EquipmentSlot::Ring1 || slot == EquipmentSlot::Ring2 || slot == EquipmentSlot::Neck) {
             candidates = { 
                AffixType::FlatPhysicalDamage, AffixType::FlatFireDamage,
                AffixType::PercentPhysicalDamage, AffixType::FlatMana
             };
        } else {
             candidates = { 
                AffixType::FlatHealth, AffixType::FlatMana, 
                AffixType::PercentArmor, AffixType::FlatArmor
             };
        }
    } else {
        // --- 后缀 ---
        candidates = { 
            AffixType::Strength, AffixType::Dexterity, 
            AffixType::Intelligence, AffixType::Vitality,
            AffixType::ResistFire, AffixType::ResistCold, AffixType::ResistLightning
        };
        if (slot == EquipmentSlot::MainHand) {
            candidates.push_back(AffixType::AttackSpeed);
            candidates.push_back(AffixType::CritChance);
            candidates.push_back(AffixType::CritDamage);
        } else if (slot == EquipmentSlot::Feet) {
            candidates.push_back(AffixType::MoveSpeed);
        } else if (slot == EquipmentSlot::Hands) {
            candidates.push_back(AffixType::AttackSpeed);
        } else if (slot == EquipmentSlot::Ring1 || slot == EquipmentSlot::Ring2 || slot == EquipmentSlot::Neck) {
             candidates.push_back(AffixType::CastSpeed);
             candidates.push_back(AffixType::CritChance);
        }
    }

    if (candidates.empty()) candidates.push_back(AffixType::Strength);

    std::uniform_int_distribution<> dist(0, candidates.size() - 1);
    AffixType type = candidates[dist(g_rng)];
    
    // 2. 确定词缀等级
    int maxTier = std::min(7, (level / 8) + 1);
    int minTier = std::max(1, maxTier - 2);
    int tier = std::uniform_int_distribution<>(minTier, maxTier)(g_rng);
    
    return createAffix(type, tier);
}

void ItemFactory::rollAffixes(ItemComponent& item, int level) {
    int maxPrefix = 0, maxSuffix = 0;
    
    switch (item.rarity) {
        case Rarity::Magic: maxPrefix = 1; maxSuffix = 1; break;
        case Rarity::Rare: maxPrefix = 2; maxSuffix = 2; break;
        case Rarity::Legendary: maxPrefix = 3; maxSuffix = 3; break; 
        default: break;
    }
    
    int prefixCount = 0, suffixCount = 0;
    if (item.rarity != Rarity::Common) {
        if (std::uniform_int_distribution<>(0, 1)(g_rng)) {
             prefixCount = std::uniform_int_distribution<>(1, maxPrefix)(g_rng);
             suffixCount = std::uniform_int_distribution<>(0, maxSuffix)(g_rng);
        } else {
             prefixCount = std::uniform_int_distribution<>(0, maxPrefix)(g_rng);
             suffixCount = std::uniform_int_distribution<>(1, maxSuffix)(g_rng);
        }
    }

    // 记录已有的词缀类型以避免重复
    std::vector<AffixType> existingTypes;
    for(const auto& aff : item.implicits) existingTypes.push_back(aff.type);

    auto addUniqueAffix = [&](bool isPrefix) {
        for(int attempt=0; attempt<20; ++attempt) {
            Affix aff = generateRandomAffix(level, isPrefix, item.slot);
            bool duplicate = false;
            for(auto t : existingTypes) if(t == aff.type) { duplicate = true; break; }
            
            if(!duplicate) {
                item.affixes.push_back(aff);
                existingTypes.push_back(aff.type);
                return;
            }
        }
        LOG_WARN("ItemFactory: Failed to generate unique affix for item {} (Prefix: {})", item.name, isPrefix);
    };

    for(int i=0; i<prefixCount; ++i) addUniqueAffix(true);
    for(int i=0; i<suffixCount; ++i) addUniqueAffix(false);
    
    LOG_DEBUG("ItemFactory: Generated {} affixes for {}", item.affixes.size(), item.name);
}

// -----------------------------------------------------------------------------
// 创建方法 (与之前相同)
// -----------------------------------------------------------------------------
entt::entity ItemFactory::createRandomLoot(entt::registry& registry, int level, float magicFind) {
    LOG_DEBUG("创建随机掉落物，等级: {}，魔法寻宝率: {}", level, magicFind);
    Rarity rarity = rollRarity(magicFind);
    entt::entity result;
    if (std::uniform_int_distribution<>(0, 1)(g_rng) == 0) {
        LOG_DEBUG("正在生成武器");
        result = createWeapon(registry, level, rarity);
    } else {
        LOG_DEBUG("正在生成护甲");
        EquipmentSlot slot = (EquipmentSlot)std::uniform_int_distribution<>(3, 8)(g_rng);
        result = createArmor(registry, level, rarity, slot);
    }
    LOG_DEBUG("Created random loot entity: {}", (uint32_t)result);
    return result;
}

entt::entity ItemFactory::createWeapon(entt::registry& registry, int level, Rarity rarity) {
    LOG_DEBUG("Creating weapon with level: {}, rarity: {}", level, static_cast<int>(rarity));
    auto entity = registry.create();
    ItemComponent item;
    item.type = ItemType::Weapon;
    item.slot = EquipmentSlot::MainHand;
    item.rarity = rarity;
    item.id = std::uniform_int_distribution<>(1000, 9999)(g_rng);
    
    const auto& base = selectBaseItem(WEAPON_BASES, level);
    item.name = base.name;
    LOG_DEBUG("Selected base weapon: {}", base.name);
    
    float baseVal = std::uniform_real_distribution<>(base.baseStatMin, base.baseStatMax)(g_rng);
    item.attack = baseVal;
    
    // Implicit
    item.implicits.push_back(createAffix(base.implicitType, 1)); // Implicit usually unscaled or custom? Assume T1 for now
    item.implicits.back().value = std::uniform_real_distribution<>(5.0f, 15.0f)(g_rng) + (level * 0.5f);
    item.implicits.back().tier = 0;
    item.implicits.back().name = "Implicit";

    item.forgingPotential = std::uniform_int_distribution<>(20, 50)(g_rng);
    
    if (rarity == Rarity::Legendary) {
        item.name = "Ancient " + item.name;
        item.legendaryPotential = std::uniform_int_distribution<>(1, 4)(g_rng);
        LOG_DEBUG("Created legendary weapon: {}", item.name);
    } else if (rarity == Rarity::Rare) {
        item.name = "Rare " + item.name;
        LOG_DEBUG("Created rare weapon: {}", item.name);
    } else {
        LOG_DEBUG("Created common/magic weapon: {}", item.name);
    }

    rollAffixes(item, level);
    registry.emplace<ItemComponent>(entity, item);

    // Assign Sprite based on item type/name
    if (item.type == ItemType::Weapon) {
        // Currently we only have one sword texture
        Texture2D tex = AssetLoadingSystem::GetTexture(assets::textures::Weapon_Sword.id);
        if (tex.id > 0) {
            // Weapon textures are usually 1024x1024, scale down to ~40px for UI/World
            registry.emplace<SpriteComponent>(entity, tex, 0.05f);
            LOG_DEBUG("Assigned weapon sprite to entity: {}", (uint32_t)entity);
        }
    }

    LOG_DEBUG("Weapon created with entity ID: {}", (uint32_t)entity);
    return entity;
}

entt::entity ItemFactory::createArmor(entt::registry& registry, int level, Rarity rarity, EquipmentSlot slot) {
    LOG_DEBUG("Creating armor with level: {}, rarity: {}, slot: {}", level, static_cast<int>(rarity), static_cast<int>(slot));
    auto entity = registry.create();
    ItemComponent item;
    item.type = ItemType::Armor;
    item.slot = slot;
    item.rarity = rarity;
    item.id = std::uniform_int_distribution<>(1000, 9999)(g_rng);
    
    const auto& base = selectBaseItem(ARMOR_BASES, level);
    item.name = base.name;
    LOG_DEBUG("Selected base armor: {}", base.name);
    
    float baseVal = std::uniform_real_distribution<>(base.baseStatMin, base.baseStatMax)(g_rng);
    item.defense = baseVal;
    
    item.implicits.push_back(createAffix(base.implicitType, 1));
    item.implicits.back().tier = 0;
    
    item.forgingPotential = std::uniform_int_distribution<>(20, 50)(g_rng);

    if (rarity == Rarity::Legendary) {
        item.name = "Legendary " + item.name;
        item.legendaryPotential = std::uniform_int_distribution<>(1, 3)(g_rng);
        LOG_DEBUG("Created legendary armor: {}", item.name);
    } else {
        LOG_DEBUG("Created common/magic/rare armor: {}", item.name);
    }

    rollAffixes(item, level);
    registry.emplace<ItemComponent>(entity, item);
    LOG_DEBUG("Armor created with entity ID: {}", (uint32_t)entity);
    return entity;
}

} // namespace NoMoreDay
