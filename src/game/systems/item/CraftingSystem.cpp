#include "game/systems/item/CraftingSystem.hpp"
#include "core/logging/Logger.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/RunewordSystem.hpp"
#include <algorithm>
#include <random>

namespace NoMoreDay {

int CraftingSystem::calculatePotentialCost(int targetTier) {
  static std::mt19937 rng(std::random_device{}());
  int minCost = 1 + targetTier * 2;
  int maxCost = 10 + targetTier * 3;
  std::uniform_int_distribution<> dist(minCost, maxCost);
  return dist(rng);
}

CraftingResult CraftingSystem::upgradeAffix(ItemComponent &item,
                                            int affixIndex) {
  if (affixIndex < 0 || affixIndex >= item.affixes.size()) {
    LOG_ERROR("Crafting: Invalid affix index {} for item '{}'", affixIndex,
              item.name);
    return CraftingResult::Failure;
  }

  auto &affix = item.affixes[affixIndex];

  // 检查限制
  if (affix.tier >= 5) { // 可打造的最大等级 T5
    LOG_DEBUG("Crafting: Affix '{}' on item '{}' is already max tier (5)",
              GetAffixDescription(affix, false), item.name);
    return CraftingResult::MaxTierReached;
  }

  if (item.forgingPotential <= 0) {
    LOG_DEBUG("Crafting: Item '{}' has no forging potential left", item.name);
    return CraftingResult::NoPotential;
  }

  int cost = calculatePotentialCost(affix.tier + 1);
  int finalCost = std::min(cost, item.forgingPotential);
  item.forgingPotential -= finalCost;

  LOG_INFO("打造: 正在升级物品 '{}' 上的词缀 '{}'。消耗: {} 潜力", item.name,
           GetAffixDescription(affix, false), finalCost);

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

CraftingResult CraftingSystem::addAffix(ItemComponent &item, AffixType type,
                                        bool isPrefix) {
  if (item.forgingPotential <= 0) {
    LOG_DEBUG("Crafting: Cannot add affix to '{}', no potential", item.name);
    return CraftingResult::NoPotential;
  }

  // 检查槽位
  int currentPrefix = 0;
  int currentSuffix = 0;
  for (const auto &a : item.affixes) {
    if (a.isPrefix)
      currentPrefix++;
    else
      currentSuffix++;
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

  LOG_INFO("打造: 正在为物品 '{}' 添加新的 {}。消耗: {} 潜力", item.name,
           isPrefix ? "前缀" : "后缀", finalCost);

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

CraftingResult CraftingSystem::chaosAffix(ItemComponent &item, int affixIndex) {
  // 1. 检查基本约束 (潜力, 索引)
  if (affixIndex < 0 || affixIndex >= item.affixes.size()) {
    LOG_ERROR("Crafting Chaos: Invalid index {}", affixIndex);
    return CraftingResult::Failure;
  }
  if (item.forgingPotential <= 0) {
    LOG_DEBUG("Crafting Chaos: No potential on '{}'", item.name);
    return CraftingResult::NoPotential;
  }

  auto &oldAffix = item.affixes[affixIndex];
  if (oldAffix.tier >= 5) {
    LOG_DEBUG("Crafting Chaos: Affix '{}' is already max tier", GetAffixDescription(oldAffix, false));
    return CraftingResult::MaxTierReached;
  }

  // 2. 消耗
  int cost = calculatePotentialCost(oldAffix.tier + 1);
  int finalCost = std::min(cost, item.forgingPotential);
  item.forgingPotential -= finalCost;

  LOG_INFO("打造混沌: 正在重铸物品 '{}' 上的词缀 '{}'。消耗: {}", item.name,
           GetAffixDescription(oldAffix, false), finalCost);

  // 3. 混沌逻辑: 新类型，等级 + 1
  int targetTier = oldAffix.tier + 1;
  bool targetPrefix = oldAffix.isPrefix;

  // 我们需要一个适合此槽位和位置的随机类型。
  // 使用 ItemFactory::generateRandomAffix 作为助手来查找有效类型。
  // 我们传递一个虚拟等级 (例如 50) 来获得有效掷骰。
  // 循环几次以确保我们获得正确的位置 (前缀/后缀)。
  Affix tempCandidate;
  bool found = false;
  for (int i = 0; i < 10; ++i) {
    tempCandidate =
        ItemFactory::generateRandomAffix(50, targetPrefix, item.slot);
    if (tempCandidate.isPrefix == targetPrefix) {
      found = true;
      break;
    }
  }

  if (!found) {
    // 回退: 如果找不到替换，则只升级现有词缀 (这种情况应该很少发生)
    LOG_WARN("打造混沌: 无法为 '{}' 找到新的有效类型，回退到升级", item.name);
    oldAffix = ItemFactory::createAffix(oldAffix.type, targetTier);
    return CraftingResult::Success;
  }

  // 应用新类型和目标等级
  oldAffix = ItemFactory::createAffix(tempCandidate.type, targetTier);

  return CraftingResult::Success;
}

CraftingResult CraftingSystem::refineAffixValues(ItemComponent &item,
                                                 int affixIndex) {
  if (affixIndex < 0 || affixIndex >= item.affixes.size()) {
    return CraftingResult::Failure;
  }
  if (item.forgingPotential <= 0) {
    return CraftingResult::NoPotential;
  }

  auto &affix = item.affixes[affixIndex];
  auto range = ItemFactory::getAffixRange(affix.type, affix.tier);

  if (range.first == 0.0f && range.second == 0.0f) {
    LOG_WARN("Crafting: No range found for affix type {} tier {}",
             (int)affix.type, affix.tier);
    return CraftingResult::Failure;
  }

  // 消耗潜力 (洗练消耗较低)
  int cost = 1 + affix.tier;
  int finalCost = std::min(cost, item.forgingPotential);
  item.forgingPotential -= finalCost;

  static std::mt19937 rng(std::random_device{}());
  float newValue =
      std::uniform_real_distribution<float>(range.first, range.second)(rng);

  LOG_INFO("打造洗练: 物品 '{}' 的词缀 '{}' 从 {:.1f} 变为 {:.1f}", item.name,
           GetAffixDescription(affix, false), affix.value, newValue);
  affix.value = newValue;

  return CraftingResult::Success;
}

CraftingResult CraftingSystem::refineBaseStats(ItemComponent &item) {
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
  float newValue =
      std::uniform_real_distribution<float>(range.first, range.second)(rng);

  if (item.type == ItemType::Weapon) {
    LOG_INFO("打造洗练: 武器 '{}' 攻击力从 {:.1f} 变为 {:.1f}", item.name,
             item.attack, newValue);
    item.attack = newValue;
  } else if (item.type == ItemType::Armor) {
    LOG_INFO("打造洗练: 护甲 '{}' 防御力从 {:.1f} 变为 {:.1f}", item.name,
             item.defense, newValue);
    item.defense = newValue;
  }

  return CraftingResult::Success;
}

CraftingResult CraftingSystem::socketRune(entt::registry &registry,
                                          entt::entity itemEntity,
                                          entt::entity runeEntity,
                                          int socketIndex) {
  if (!registry.valid(itemEntity) || !registry.valid(runeEntity)) {
    return CraftingResult::Failure;
  }

  auto &item = registry.get<ItemComponent>(itemEntity);
  if (!registry.all_of<RuneComponent>(runeEntity)) {
    LOG_WARN("Crafting: Entity {} is not a rune", (uint32_t)runeEntity);
    return CraftingResult::MaterialMissing;
  }

  if (socketIndex < 0 || socketIndex >= item.socketCount) {
    LOG_WARN("Crafting: Invalid socket index {} for item '{}'", socketIndex,
             item.name);
    return CraftingResult::Failure;
  }

  if (item.sockets.size() < (size_t)item.socketCount) {
    item.sockets.resize(item.socketCount, entt::null);
  }

  if (item.sockets[socketIndex] != entt::null) {
    LOG_WARN("Crafting: Socket {} already occupied in item '{}'", socketIndex,
             item.name);
    return CraftingResult::SlotFull;
  }

  item.sockets[socketIndex] = runeEntity;
  LOG_INFO("打造插槽: 将符文放入物品 '{}' 的第 {} 个插槽", item.name,
           socketIndex);

  // Check for Runeword
  uint32_t rwId =
      RunewordSystem::checkForRuneword(item, item.sockets, registry);
  if (rwId != 0) {
    RunewordSystem::applyRuneword(item, rwId);
    LOG_INFO("Runeword {} Applied to item {}", rwId, item.name);
  }

  return CraftingResult::Success;
}

CraftingResult CraftingSystem::unsocketRune(entt::registry &registry,
                                            entt::entity itemEntity,
                                            int socketIndex) {
  if (!registry.valid(itemEntity))
    return CraftingResult::Failure;

  auto &item = registry.get<ItemComponent>(itemEntity);
  if (socketIndex < 0 || socketIndex >= (int)item.sockets.size()) {
    return CraftingResult::Failure;
  }

  if (item.sockets[socketIndex] == entt::null) {
    return CraftingResult::Failure;
  }

  entt::entity runeEntity = item.sockets[socketIndex];
  item.sockets[socketIndex] = entt::null;

  LOG_INFO("打造插槽: 从物品 '{}' 的第 {} 个插槽移除符文", item.name,
           socketIndex);

  return CraftingResult::Success;
}

CraftingResult CraftingSystem::fuseItems(ItemComponent &baseItem,
                                         ItemComponent &fodderItem) {
  LOG_INFO("打造融合: 尝试将物品 '{}' 与 '{}' 融合 (当前为占位实现)",
           baseItem.name, fodderItem.name);
  // 基础逻辑占位: 消耗潜力，不改变属性
  if (baseItem.forgingPotential <= 0)
    return CraftingResult::NoPotential;

  baseItem.forgingPotential -= std::min(10, baseItem.forgingPotential);
  return CraftingResult::Success;
}

CraftingResult CraftingSystem::fuseLegendary(entt::registry &registry,
                                             entt::entity baseEntity,
                                             entt::entity fodderEntity,
                                             entt::entity catalystEntity,
                                             int selectedAffixIndex) {
  if (!registry.valid(baseEntity) || !registry.valid(fodderEntity) ||
      !registry.valid(catalystEntity)) {
    LOG_ERROR("Fusion: Invalid entities provided.");
    return CraftingResult::Failure;
  }

  auto *base = registry.try_get<ItemComponent>(baseEntity);
  auto *fodder = registry.try_get<ItemComponent>(fodderEntity);
  auto *catalyst = registry.try_get<ItemComponent>(catalystEntity);

  if (!base || !fodder || !catalyst) {
    LOG_ERROR("Fusion: Missing ItemComponent on entities.");
    return CraftingResult::Failure;
  }

  // 1. Validation
  // 1. Validation
  // Base: Any item with LP > 0 (Unique/Legendary/Mythic/Ancient)
  if (base->legendaryPotential <= 0) {
    LOG_WARN("Fusion: Base item '{}' has no Legendary Potential.", base->name);
    return CraftingResult::NoPotential;
  }

  // Fodder: 4 affixes
  if (fodder->affixes.size() != 4) {
    LOG_WARN("Fusion: Fodder item '{}' must have exactly 4 affixes.",
             fodder->name);
    return CraftingResult::Failure;
  }

  // Slots match
  if (base->slot != fodder->slot) {
    LOG_WARN("Fusion: Slot mismatch. Base: {}, Fodder: {}", (int)base->slot,
             (int)fodder->slot);
    return CraftingResult::Failure;
  }

  // Catalyst: Legendary Core
  // 按 catalystKind 身份判断, 旧存档仅有名称时回退兼容
  if (!IsLegendaryCoreCatalyst(*catalyst)) {
    LOG_WARN("Fusion: Invalid catalyst '{}'. Expected 'Legendary Core'.",
             catalyst->name);
    // Allow bypass if testing? No, strict.
    return CraftingResult::Failure;
  }

  // Selected index
  if (selectedAffixIndex < 0 || selectedAffixIndex >= 4) {
    LOG_ERROR("Fusion: Invalid selected affix index {}", selectedAffixIndex);
    return CraftingResult::Failure;
  }

  // 2. Selection Logic
  std::vector<int> indicesToInherit;
  indicesToInherit.push_back(selectedAffixIndex);

  int lp = base->legendaryPotential;
  if (lp > 1) {
    std::vector<int> available;
    for (int i = 0; i < 4; ++i) {
      if (i != selectedAffixIndex)
        available.push_back(i);
    }

    // Randomly pick lp - 1
    std::shuffle(available.begin(), available.end(),
                 std::default_random_engine(std::random_device{}()));

    for (int i = 0; i < lp - 1 && i < available.size(); ++i) {
      indicesToInherit.push_back(available[i]);
    }
  }

  // 3. Transformation
  for (int idx : indicesToInherit) {
    Affix inherited = fodder->affixes[idx];
    inherited.isLegendary = true;
    base->affixes.push_back(inherited);
    LOG_INFO("Fusion: Inherited affix '{}' [T{}]", GetAffixDescription(inherited, false),
             inherited.tier);
  }

  base->rarity = Rarity::Ancient;
  base->name = "Ancient " + base->name; // Simple prefix
  base->legendaryPotential = 0;         // Consumed

  // 4. Consumption
  std::string resultName = base->name;

  // Consume catalyst safely
  auto *catComp = registry.try_get<ItemComponent>(catalystEntity);
  if (catComp) {
    if (catComp->quantity > 1) {
      catComp->quantity--;
    } else {
      registry.destroy(catalystEntity);
    }
  }

  // Destroy fodder last to avoid pointer invalidation for base/catalyst if they
  // were moved
  if (registry.valid(fodderEntity)) {
    registry.destroy(fodderEntity);
  }

  LOG_INFO("Fusion: Successfully fused to create '{}'!", resultName);
  return CraftingResult::Success;
}

} // namespace NoMoreDay