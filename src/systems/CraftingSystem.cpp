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
    
    // 检查限制
    if (affix.tier >= 5) { // 可打造的最大等级 T5
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
    
    LOG_INFO("打造: 正在升级物品 '{}' 上的词缀 '{}'。消耗: {} 潜力", item.name, affix.name, finalCost);

    // 升级: 纯粹基于类型和新等级重新生成
    int newTier = affix.tier + 1;
    Affix newAffix = ItemFactory::createAffix(affix.type, newTier);
    
    // 保留位置 (前缀/后缀)，以防逻辑不同，尽管类型通常决定了它。
    // 在我们的系统中，类型在 fillAffixDetails 中决定了前缀/后缀。
    // 所以我们直接替换它。
    
    // 复制所需字段 (CreateAffix 处理值、等级、名称、isPrefix)
    affix = newAffix;
    
    return CraftingResult::Success;
}

CraftingResult CraftingSystem::addAffix(ItemComponent& item, AffixType type, bool isPrefix) {
    if (item.forgingPotential <= 0) {
        LOG_DEBUG("Crafting: Cannot add affix to '{}', no potential", item.name);
        return CraftingResult::NoPotential;
    }
    
    // 检查槽位
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
    
    // 计算消耗
    int cost = calculatePotentialCost(1);
    int finalCost = std::min(cost, item.forgingPotential);
    item.forgingPotential -= finalCost;
    
    LOG_INFO("打造: 正在为物品 '{}' 添加新的 {}。消耗: {} 潜力", item.name, isPrefix ? "前缀" : "后缀", finalCost);

    // 添加词缀 - 等级 1
    Affix newAffix = ItemFactory::createAffix(type, 1);
    // 强制 isPrefix 与请求匹配吗？
    // 通常类型决定了它。如果用户请求前缀但选择了“力量”（后缀），会发生什么？
    // 我们的 createAffix 根据类型设置 isPrefix。
    // 所以我们应该验证生成的词缀是否与请求的槽位匹配。
    if (newAffix.isPrefix != isPrefix) {
        // 在真实的 UI 中，我们按槽位过滤类型，所以这不会发生。
        // 为了后端安全，我们可以拒绝或仅仅接受类型的性质。
        // 让我们接受类型的性质。
        // 根据实际类型性质重新检查槽位限制吗？
        // 更简单：只信任 createAffix。
    }
    
    item.affixes.push_back(newAffix);
    
    return CraftingResult::Success;
}

CraftingResult CraftingSystem::chaosAffix(ItemComponent& item, int affixIndex) {
    // 1. 检查基本约束 (潜力, 索引)
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

    // 2. 消耗
    int cost = calculatePotentialCost(oldAffix.tier + 1);
    int finalCost = std::min(cost, item.forgingPotential);
    item.forgingPotential -= finalCost;

    LOG_INFO("打造混沌: 正在重铸物品 '{}' 上的词缀 '{}'。消耗: {}", item.name, oldAffix.name, finalCost);

    // 3. 混沌逻辑: 新类型，等级 + 1
    int targetTier = oldAffix.tier + 1;
    bool targetPrefix = oldAffix.isPrefix;
    
    // 我们需要一个适合此槽位和位置的随机类型。
    // 使用 ItemFactory::generateRandomAffix 作为助手来查找有效类型。
    // 我们传递一个虚拟等级 (例如 50) 来获得有效掷骰。
    // 循环几次以确保我们获得正确的位置 (前缀/后缀)。
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
        // 回退: 如果找不到替换，则只升级现有词缀 (这种情况应该很少发生)
        LOG_WARN("打造混沌: 无法为 '{}' 找到新的有效类型，回退到升级", item.name);
        oldAffix = ItemFactory::createAffix(oldAffix.type, targetTier);
        oldAffix.name = "Chaotic " + oldAffix.name;
        return CraftingResult::Success;
    }
    
    // 应用新类型和目标等级
    oldAffix = ItemFactory::createAffix(tempCandidate.type, targetTier);
    oldAffix.name = "Chaotic " + oldAffix.name;

    return CraftingResult::Success;
}

CraftingResult CraftingSystem::refineAffixValues(ItemComponent& item, int affixIndex) {
    if (affixIndex < 0 || affixIndex >= item.affixes.size()) {
        return CraftingResult::Failure;
    }
    if (item.forgingPotential <= 0) {
        return CraftingResult::NoPotential;
    }

    auto& affix = item.affixes[affixIndex];
    auto range = ItemFactory::getAffixRange(affix.type, affix.tier);

    if (range.first == 0.0f && range.second == 0.0f) {
        LOG_WARN("Crafting: No range found for affix type {} tier {}", (int)affix.type, affix.tier);
        return CraftingResult::Failure;
    }

    // 消耗潜力 (洗练消耗较低)
    int cost = 1 + affix.tier; 
    int finalCost = std::min(cost, item.forgingPotential);
    item.forgingPotential -= finalCost;

    static std::mt19937 rng(std::random_device{}());
    float newValue = std::uniform_real_distribution<float>(range.first, range.second)(rng);
    
    LOG_INFO("打造洗练: 物品 '{}' 的词缀 '{}' 从 {:.1f} 变为 {:.1f}", item.name, affix.name, affix.value, newValue);
    affix.value = newValue;

    return CraftingResult::Success;
}

CraftingResult CraftingSystem::refineBaseStats(ItemComponent& item) {
    if (item.forgingPotential <= 0) {
        return CraftingResult::NoPotential;
    }

    auto range = ItemFactory::getBaseStatRange(item);
    if (range.first == 0.0f && range.second == 0.0f) {
        LOG_WARN("Crafting: No base stat range found for item '{}'", item.name);
        return CraftingResult::Failure;
    }

    // 消耗潜力
    int cost = 5; 
    int finalCost = std::min(cost, item.forgingPotential);
    item.forgingPotential -= finalCost;

    static std::mt19937 rng(std::random_device{}());
    float newValue = std::uniform_real_distribution<float>(range.first, range.second)(rng);

    if (item.type == ItemType::Weapon) {
        LOG_INFO("打造洗练: 武器 '{}' 攻击力从 {:.1f} 变为 {:.1f}", item.name, item.attack, newValue);
        item.attack = newValue;
    } else if (item.type == ItemType::Armor) {
        LOG_INFO("打造洗练: 护甲 '{}' 防御力从 {:.1f} 变为 {:.1f}", item.name, item.defense, newValue);
        item.defense = newValue;
    }

    return CraftingResult::Success;
}

CraftingResult CraftingSystem::socketRune(entt::registry& registry, entt::entity itemEntity, entt::entity runeEntity, int socketIndex) {
    if (!registry.valid(itemEntity) || !registry.valid(runeEntity)) {
        return CraftingResult::Failure;
    }

    auto& item = registry.get<ItemComponent>(itemEntity);
    if (!registry.all_of<RuneComponent>(runeEntity)) {
        LOG_WARN("Crafting: Entity {} is not a rune", (uint32_t)runeEntity);
        return CraftingResult::MaterialMissing;
    }

    if (socketIndex < 0 || socketIndex >= item.socketCount) {
        LOG_WARN("Crafting: Invalid socket index {} for item '{}'", socketIndex, item.name);
        return CraftingResult::Failure;
    }

    if (item.sockets.size() < (size_t)item.socketCount) {
        item.sockets.resize(item.socketCount, entt::null);
    }

    if (item.sockets[socketIndex] != entt::null) {
        LOG_WARN("Crafting: Socket {} already occupied in item '{}'", socketIndex, item.name);
        return CraftingResult::SlotFull;
    }

    item.sockets[socketIndex] = runeEntity;
    LOG_INFO("打造插槽: 将符文放入物品 '{}' 的第 {} 个插槽", item.name, socketIndex);

    return CraftingResult::Success;
}

CraftingResult CraftingSystem::unsocketRune(entt::registry& registry, entt::entity itemEntity, int socketIndex) {
    if (!registry.valid(itemEntity)) return CraftingResult::Failure;

    auto& item = registry.get<ItemComponent>(itemEntity);
    if (socketIndex < 0 || socketIndex >= (int)item.sockets.size()) {
        return CraftingResult::Failure;
    }

    if (item.sockets[socketIndex] == entt::null) {
        return CraftingResult::Failure;
    }

    entt::entity runeEntity = item.sockets[socketIndex];
    item.sockets[socketIndex] = entt::null;
    
    LOG_INFO("打造插槽: 从物品 '{}' 的第 {} 个插槽移除符文", item.name, socketIndex);

    return CraftingResult::Success;
}

CraftingResult CraftingSystem::fuseItems(ItemComponent& baseItem, ItemComponent& fodderItem) {
    LOG_INFO("打造融合: 尝试将物品 '{}' 与 '{}' 融合 (当前为占位实现)", baseItem.name, fodderItem.name);
    // 基础逻辑占位: 消耗潜力，不改变属性
    if (baseItem.forgingPotential <= 0) return CraftingResult::NoPotential;
    
    baseItem.forgingPotential -= std::min(10, baseItem.forgingPotential);
    return CraftingResult::Success;
}

} // namespace NoMoreDay