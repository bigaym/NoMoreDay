#include "game/systems/item/FragmentDropSystem.hpp"
#include "core/logging/Logger.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/CombatEvents.hpp"
#include "game/systems/world/LevelManager.hpp"
#include <random>

namespace NoMoreDay {

// 静态成员初始化
uint32_t FragmentDropSystem::s_killHandlerId = 0;
bool FragmentDropSystem::s_initialized = false;
::LevelManager* FragmentDropSystem::s_levelManager = nullptr;
std::vector<FragmentDropSystem::DropRequest> FragmentDropSystem::s_pendingRequests;
std::mutex FragmentDropSystem::s_requestMutex;

// 随机数生成器
static thread_local std::mt19937 s_fragmentRng(std::random_device{}());

void FragmentDropSystem::Init() {
  if (s_initialized)
    return;

  // 清空残余请求
  {
    std::lock_guard<std::mutex> lock(s_requestMutex);
    s_pendingRequests.clear();
  }

  // 注册击杀事件处理器
  s_killHandlerId = CombatEventDispatcher::Register(
      CombatEventType::OnKill,
      [](entt::registry &registry, const CombatEvent &evt) {
        FragmentDropSystem::OnEnemyKilled(registry, evt.source, evt.target);
      },
      40 // 中等优先级，在经验奖励之后
  );

  s_initialized = true;
  LOG_INFO("FragmentDropSystem: Initialized");
}

void FragmentDropSystem::Shutdown() {
  if (!s_initialized)
    return;

  if (s_killHandlerId != 0) {
    CombatEventDispatcher::Unregister(CombatEventType::OnKill, s_killHandlerId);
    s_killHandlerId = 0;
  }

  {
    std::lock_guard<std::mutex> lock(s_requestMutex);
    s_pendingRequests.clear();
  }

  s_initialized = false;
  LOG_INFO("FragmentDropSystem: Shutdown");
}

void FragmentDropSystem::Update(entt::registry &registry) {
  std::vector<DropRequest> requests;
  
  // 快速交换队列，尽量减少锁持有时间
  {
    std::lock_guard<std::mutex> lock(s_requestMutex);
    if (s_pendingRequests.empty()) return;
    requests.swap(s_pendingRequests);
  }

  // 在主线程同步处理掉落 (线程安全)
  for (const auto& req : requests) {
    entt::entity fragment = CreateRandomFragment(registry, req.areaLevel, req.magicFind, req.areaElement);
    if (fragment != entt::null) {
      registry.emplace<Position>(fragment, req.posX, req.posY);
      registry.emplace<LocalLevelTag>(fragment); // Clean up on map transition
      LOG_DEBUG("Fragment created at ({}, {}) from deferred request", req.posX, req.posY);
    }
  }
}

void FragmentDropSystem::OnEnemyKilled(entt::registry &registry,
                                       entt::entity killer,
                                       entt::entity victim) {
  // 检查受害者是否是有效实体 (敌人或已死亡的敌人)
  // CombatSystem 可能会在派发事件前移除 EnemyTag，所以我们需要检查 EnemyStateComponent 或 KilledTag
  bool validEnemy = registry.valid(victim) && 
      (registry.any_of<EnemyTag>(victim) || 
       registry.any_of<EnemyStateComponent>(victim) ||
       registry.any_of<KilledTag>(victim));

  if (!validEnemy) {
    return;
  }

  // 获取敌人信息 (只读操作)
  bool isElite = false;
  bool isBoss = false;
  int victimLevel = 1;
  float posX = 0.0f;
  float posY = 0.0f;

  if (auto *stateComp = registry.try_get<EnemyStateComponent>(victim)) {
    victimLevel = stateComp->level;
  }

  if (auto *rarityComp = registry.try_get<EnemyRarityComponent>(victim)) {
    isElite = (rarityComp->rarity == EnemyRarityComponent::ELITE);
    isBoss = (rarityComp->rarity == EnemyRarityComponent::BOSS);
  }

  if (auto *pos = registry.try_get<Position>(victim)) {
    posX = pos->x;
    posY = pos->y;
  }

  // 计算掉落概率
  float dropChance = GetFragmentDropChance(victimLevel, isElite, isBoss);

  // 获取玩家魔法寻宝率
  float magicFind = 0.0f;
  if (registry.valid(killer)) {
    if (auto *stats = registry.try_get<CombatStats>(killer)) {
      magicFind = stats->magic_find;
    }
  }

  // 魔法寻宝率影响掉落概率
  dropChance *= (1.0f + magicFind * 0.01f);

  // 掷骰
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  if (dist(s_fragmentRng) > dropChance) {
    return; // 没有掉落
  }

  // 获取当前区域元素 (如果有关卡管理器)
  FragmentElement areaElement = FragmentElement::None;
  if (s_levelManager) {
    areaElement = s_levelManager->getCurrentResonance().dominantElement;
  }

  // 将请求加入队列而非直接创建 (线程安全)
  {
    std::lock_guard<std::mutex> lock(s_requestMutex);
    s_pendingRequests.push_back({victimLevel, magicFind, posX, posY, areaElement});
  }
}

entt::entity FragmentDropSystem::CreateRandomFragment(entt::registry &registry,
                                                      int areaLevel,
                                                      float magicFind,
                                                      FragmentElement areaElement) {
  FragmentType type = RollFragmentType(magicFind * 0.01f);
  FragmentElement element = RollFragmentElement(areaElement);
  Rarity rarity = RollFragmentRarity(magicFind, false, false);

  return CreateFragment(registry, type, element, rarity);
}

entt::entity FragmentDropSystem::CreateFragment(entt::registry &registry,
                                                FragmentType type,
                                                FragmentElement element,
                                                Rarity rarity) {
  entt::entity entity = registry.create();

  // 添加物品组件 (使碎片可被拾取和存储)
  auto &item = registry.emplace<ItemComponent>(entity);
  // 修正: 碎片具有随机属性 (MapFragmentComponent)，不能作为简单的堆叠材料存储在 MaterialBank 中。
  // 因此将其设为 Consumable (或 Quest)，使其进入主背包并保留组件数据。
  item.type = ItemType::Consumable; 
  item.rarity = rarity;
  item.maxStack = 1; // 碎片不可堆叠
  item.quantity = 1;

  // 根据类型设置名称和ID (参考 materials.json)
  switch (type) {
  case FragmentType::Terrain:
    item.name = "地形碎片";
    item.id = 2001; // Dimension Fragment
    break;
  case FragmentType::Affix:
    item.name = "词缀碎片";
    item.id = 0; // 暂时没有对应的单一材料ID，保持 0 或自定义
    break;
  case FragmentType::Unique:
    item.name = "特殊碎片";
    item.id = 2002; // Void Essence
    break;
  }

  // 根据元素添加前缀
  if (element != FragmentElement::None) {
    item.name = std::string(GetFragmentElementName(element)) + item.name;
  }

  // 添加碎片组件
  auto &fragment = registry.emplace<MapFragmentComponent>(entity);
  fragment.type = type;
  fragment.element = element;
  fragment.rarity = rarity;

  // 根据稀有度生成属性
  RollFragmentStats(fragment);

  // 添加标签
  registry.emplace<MapFragmentTag>(entity);

  item.description = fragment.GetDescription();

  LOG_DEBUG("Created fragment: type={}, element={}, rarity={}",
            static_cast<int>(type), static_cast<int>(element),
            static_cast<int>(rarity));

  return entity;
}

float FragmentDropSystem::GetFragmentDropChance(int victimLevel, bool isElite,
                                                bool isBoss) {
  // 基础掉落率
  float baseChance = 0.05f; // 5% 基础

  // 等级调整 (每10级+1%)
  float levelBonus = victimLevel * 0.001f;

  // 精英/Boss 加成
  float rarityBonus = 0.0f;
  if (isBoss) {
    rarityBonus = 0.50f; // Boss 50% 额外
  } else if (isElite) {
    rarityBonus = 0.20f; // 精英 20% 额外
  }

  return std::min(baseChance + levelBonus + rarityBonus, 1.0f);
}

FragmentType FragmentDropSystem::RollFragmentType(float luck) {
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  float roll = dist(s_fragmentRng);

  // 类型权重：Affix 70%, Terrain 25%, Unique 5% (+luck)
  float uniqueChance = 0.05f + luck * 0.02f;
  float terrainChance = 0.25f;

  if (roll < uniqueChance) {
    return FragmentType::Unique;
  } else if (roll < uniqueChance + terrainChance) {
    return FragmentType::Terrain;
  } else {
    return FragmentType::Affix;
  }
}

FragmentElement
FragmentDropSystem::RollFragmentElement(FragmentElement areaElement) {
  std::uniform_int_distribution<int> dist(0, 5);
  int roll = dist(s_fragmentRng);

  // 基于区域偏向某元素
  if (areaElement != FragmentElement::None) {
    if (areaElement == FragmentElement::Fire && roll < 2)
      return FragmentElement::Fire;
    if (areaElement == FragmentElement::Cold && roll < 2)
      return FragmentElement::Cold;
    if (areaElement == FragmentElement::Lightning && roll < 2)
      return FragmentElement::Lightning;
    if (areaElement == FragmentElement::Shadow && roll < 2)
      return FragmentElement::Shadow;
  }

  // 随机选择
  switch (roll) {
  case 0:
    return FragmentElement::None;
  case 1:
    return FragmentElement::Fire;
  case 2:
    return FragmentElement::Cold;
  case 3:
    return FragmentElement::Lightning;
  case 4:
    return FragmentElement::Shadow;
  case 5:
    return FragmentElement::Chaos;
  default:
    return FragmentElement::None;
  }
}

Rarity FragmentDropSystem::RollFragmentRarity(float magicFind, bool isElite,
                                              bool isBoss) {
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  float roll = dist(s_fragmentRng);

  // 魔法寻宝调整
  float mfBonus = magicFind * 0.003f; // 每1%MF = 0.3%更好稀有度

  // Boss/精英加成
  if (isBoss)
    roll -= 0.30f;
  else if (isElite)
    roll -= 0.15f;

  roll -= mfBonus;

  // 稀有度分布: Common 50%, Magic 30%, Rare 15%, Epic 4%, Legendary 1%
  if (roll < 0.01f)
    return Rarity::Legendary;
  if (roll < 0.05f)
    return Rarity::Epic;
  if (roll < 0.20f)
    return Rarity::Rare;
  if (roll < 0.50f)
    return Rarity::Magic;
  return Rarity::Common;
}

void FragmentDropSystem::RollFragmentStats(MapFragmentComponent &fragment) {
  std::uniform_real_distribution<float> smallDist(0.9f, 1.1f);
  std::uniform_real_distribution<float> mediumDist(1.0f, 1.5f);
  std::uniform_real_distribution<float> largeDist(1.2f, 2.0f);
  std::uniform_int_distribution<int> levelDist(-2, 5);

  // 根据稀有度调整属性范围
  float rarityMultiplier = 1.0f;
  switch (fragment.rarity) {
  case Rarity::Magic:
    rarityMultiplier = 1.2f;
    break;
  case Rarity::Rare:
    rarityMultiplier = 1.4f;
    break;
  case Rarity::Epic:
    rarityMultiplier = 1.7f;
    break;
  case Rarity::Legendary:
    rarityMultiplier = 2.0f;
    break;
  default:
    break;
  }

  // 生成属性
  switch (fragment.type) {
  case FragmentType::Terrain:
    // 地形碎片主要影响敌人密度
    fragment.enemyDensityMod = mediumDist(s_fragmentRng) * rarityMultiplier;
    fragment.dropRateMod = smallDist(s_fragmentRng);
    break;

  case FragmentType::Affix:
    // 词缀碎片平衡影响密度和掉落
    fragment.enemyDensityMod = smallDist(s_fragmentRng) * rarityMultiplier;
    fragment.dropRateMod = mediumDist(s_fragmentRng) * rarityMultiplier;
    fragment.monsterLevelMod = levelDist(s_fragmentRng);
    break;

  case FragmentType::Unique:
    // 特殊碎片有特殊效果
    fragment.enemyDensityMod = 1.0f;
    fragment.dropRateMod = largeDist(s_fragmentRng) * rarityMultiplier;

    // 随机选择一个特殊效果
    std::uniform_int_distribution<int> specialDist(0, 2);
    switch (specialDist(s_fragmentRng)) {
    case 0:
      fragment.hasBoss = true;
      break;
    case 1:
      fragment.hasMerchant = true;
      break;
    case 2:
      fragment.hasTreasure = true;
      break;
    }
    break;
  }

  // 生成图标ID
  fragment.iconId =
      "fragment_" + std::to_string(static_cast<int>(fragment.element));
}

} // namespace NoMoreDay
