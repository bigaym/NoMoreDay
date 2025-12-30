#include "ItemFactory.hpp"
#include <random>
#include <algorithm>
#include <map>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../tools/Logger.hpp"
#include "AssetLoadingSystem.hpp"
#include "AssetRegistry.hpp"
#include "../components/Common.hpp"
#include "LootFilter.hpp"

namespace NoMoreDay {

// Simple Random Helper
static std::mt19937 g_rng;

std::map<uint32_t, LootPool> ItemFactory::s_lootPools;
std::vector<AffixDefinition> ItemFactory::s_affixDefinitions;

void ItemFactory::initialize() {
 std::random_device rd;
 g_rng.seed(rd());

 // 加载词缀定义
 loadAffixDefinitions("assets/data/affixes.json");

 // 加载掉落池定义
 loadLootPools("assets/data/loot_tables.json");

 // 加载掉落过滤器
 LootFilter::load("assets/data/loot_filters/default.json");
}

void ItemFactory::loadAffixDefinitions(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("ItemFactory: 无法打开词缀定义文件: {}", path);
        return;
    }

    try {
        nlohmann::json j;
        file >> j;
        s_affixDefinitions = j.get<std::vector<AffixDefinition>>();
        LOG_INFO("ItemFactory: 成功加载了 {} 个词缀定义。", s_affixDefinitions.size());
    } catch (const std::exception& e) {
        LOG_ERROR("ItemFactory: 解析词缀定义文件时出错: {}", e.what());
    }
}

void ItemFactory::loadLootPools(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("ItemFactory: 无法打开掉落池定义文件: {}", path);
        return;
    }

    try {
        nlohmann::json j;
        file >> j;
        std::vector<LootPool> pools = j.get<std::vector<LootPool>>();
        for (auto& pool : pools) {
            pool.calculateTotalWeight();
            s_lootPools[pool.id] = pool;
        }
        LOG_INFO("ItemFactory: 成功加载了 {} 个掉落池定义。", pools.size());
    } catch (const std::exception& e) {
        LOG_ERROR("ItemFactory: 解析掉落池定义文件时出错: {}", e.what());
    }
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

std::pair<float, float> ItemFactory::getAffixRange(AffixType type, int tier) {
    for (const auto& def : s_affixDefinitions) {
        if (def.type == type) {
            for (const auto& t : def.tiers) {
                if (t.tier == tier) {
                    return { t.minValue, t.maxValue };
                }
            }
        }
    }
    
    // Fallback if not found in definitions (maybe it was from fillAffixDetails)
    // For now, return a wide range or zero
    return { 0.0f, 0.0f };
}

std::pair<float, float> ItemFactory::getBaseStatRange(const ItemComponent& item) {
    if (item.type == ItemType::Weapon) {
        for (const auto& base : WEAPON_BASES) {
            if (item.name.find(base.name) != std::string::npos) {
                return { base.baseStatMin, base.baseStatMax };
            }
        }
    } else if (item.type == ItemType::Armor) {
        for (const auto& base : ARMOR_BASES) {
            if (item.name.find(base.name) != std::string::npos) {
                return { base.baseStatMin, base.baseStatMax };
            }
        }
    }
    return { 0.0f, 0.0f };
}

Affix ItemFactory::generateRandomAffix(int level, bool isPrefix, EquipmentSlot slot) {
    if (s_affixDefinitions.empty()) {
        LOG_WARN("ItemFactory: 词缀定义为空，使用回退生成。");
        Affix fallback;
        fallback.type = AffixType::Strength;
        fallback.value = (float)level;
        fallback.tier = 1;
        fallback.isPrefix = isPrefix;
        fallback.name = "Fallback";
        return fallback;
    }

    // 1. 确定槽位对应的标签
    std::vector<std::string> slotTags;
    switch (slot) {
        case EquipmentSlot::MainHand: slotTags = {"weapon"}; break;
        case EquipmentSlot::OffHand:  slotTags = {"weapon", "armor"}; break; // 盾牌或副手
        case EquipmentSlot::Head:
        case EquipmentSlot::Shoulder:
        case EquipmentSlot::Chest:
        case EquipmentSlot::Legs:     slotTags = {"armor"}; break;
        case EquipmentSlot::Hands:    slotTags = {"armor", "gloves"}; break;
        case EquipmentSlot::Feet:     slotTags = {"armor", "boots"}; break;
        case EquipmentSlot::Neck:
        case EquipmentSlot::Ring1:
        case EquipmentSlot::Ring2:    slotTags = {"jewelry"}; break;
        default: slotTags = {"misc"}; break;
    }

    // 2. 筛选符合条件的词缀定义
    std::vector<const AffixDefinition*> candidates;
    for (const auto& def : s_affixDefinitions) {
        if (def.isPrefix != isPrefix) continue;

        // 检查标签是否匹配
        bool tagMatch = false;
        for (const auto& sTag : slotTags) {
            for (const auto& aTag : def.allowedTags) {
                if (sTag == aTag) { tagMatch = true; break; }
            }
            if (tagMatch) break;
        }
        if (!tagMatch) continue;

        // 检查等级是否符合 (至少有 T1 可用)
        if (def.tiers.empty() || def.tiers[0].minLevel > level) continue;

        candidates.push_back(&def);
    }

    if (candidates.empty()) {
        // 如果没有符合标签的词缀，尝试放宽条件或返回基础词缀
        return createAffix(AffixType::Strength, 1);
    }

    // 3. 随机选择一个定义
    std::uniform_int_distribution<> dist(0, candidates.size() - 1);
    const AffixDefinition* selectedDef = candidates[dist(g_rng)];

    // 4. 选择合适的等级 (Tier)
    // 选择 minLevel <= itemLevel 的最高等级
    int bestTierIdx = 0;
    for (int i = 0; i < (int)selectedDef->tiers.size(); ++i) {
        if (selectedDef->tiers[i].minLevel <= level) {
            bestTierIdx = i;
        } else {
            break;
        }
    }
    
    const auto& tier = selectedDef->tiers[bestTierIdx];

    // 5. 生成最终词缀
    Affix result;
    result.type = selectedDef->type;
    result.tier = tier.tier;
    result.isPrefix = selectedDef->isPrefix;
    result.name = selectedDef->nameTemplate;
    
    if (tier.maxValue > tier.minValue) {
        result.value = std::uniform_real_distribution<float>(tier.minValue, tier.maxValue)(g_rng);
    } else {
        result.value = tier.minValue;
    }

    return result;
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

entt::entity ItemFactory::createBag(entt::registry& registry, int level, Rarity rarity) {
    auto entity = registry.create();
    ItemComponent item;
    item.type = ItemType::Bag;
    item.rarity = rarity;
    item.slot = EquipmentSlot::None;
    item.id = std::uniform_int_distribution<>(5000, 5999)(g_rng);
    
    // 基础容量
    // 修改：每个背包现在提供一个完整的页面 (56格)
    int baseCap = 56;
    
    // 稀有度加成 (可选：也许稀有背包提供更多页？目前保持一致)
    // if (rarity >= Rarity::Magic) baseCap += 0;

    item.name = (rarity == Rarity::Common ? "亚麻背包" : "魔法背包");
    item.bagCapacity = baseCap;
    item.description = "增加一个背包页面 (" + std::to_string(baseCap) + " 格)。";

    registry.emplace<ItemComponent>(entity, item);
    // TODO: 添加 SpriteComponent
    return entity;
}

entt::entity ItemFactory::createPotion(entt::registry& registry, int type, int quantity) {
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
    } else {
        item.id = 102; // ID 约定: 102 蓝药水
        item.name = "法力药水";
        item.description = "使用: 恢复 50 点法力值";
        item.value = 10;
        registry.emplace<ColorComponent>(entity, BLUE); // 地面显示蓝色
    }
    
    // TODO: 如果有药水图标，在此处添加 SpriteComponent
    // registry.emplace<SpriteComponent>(entity, potionTexture, 1.0f);

    registry.emplace<ItemComponent>(entity, item);
    return entity;
}

} // namespace NoMoreDay
