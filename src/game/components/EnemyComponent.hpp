#pragma once

#include "game/components/Common.hpp"
#include "game/data/TagRegistry.hpp"
#include <cstdint>
#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <vector>


// 敌人种族定义
// 敌人种族数据 (POD) - 静态查找表使用
// 敌人种族数据 (POD) - 静态查找表使用
struct EnemyRaceData {
  float baseHP;
  float baseDamage;
  float baseSpeed;
  float baseXP;
  float baseArmor;
  std::string_view name;        // 显示名称
  NoMoreDay::Tag resistances;             // 使用位掩码替代 vector<string>
  std::string_view texturePath; // 共享视图替代 string 拷贝
};

// 静态常量表：映射 EnemyRace::Type 到具体数据
// 顺序必须与 EnemyRace::Type 枚举一致
static constexpr std::array<EnemyRaceData, 12> kRaceData = {{
    // UNDEAD
    {30.0f, 15.0f, 40.0f, 10.0f, 100.0f, "不死者", NoMoreDay::Tag::Bleeding | NoMoreDay::Tag::Poison, "assets/textures/monster/skeleton_0.png"},
    // DEMON
    {60.0f, 25.0f, 50.0f, 25.0f, 100.0f, "恶魔", NoMoreDay::Tag::Fire | NoMoreDay::Tag::Shadow, "assets/textures/monster/demon_0.png"},
    // CORRUPTED
    {25.0f, 20.0f, 60.0f, 15.0f, 100.0f, "腐蚀兽", NoMoreDay::Tag::Stunned, "assets/textures/monster/warcraft_0.png"},
    // CULTIST
    {35.0f, 20.0f, 45.0f, 12.0f, 100.0f, "邪教徒", NoMoreDay::Tag::Spell, "assets/textures/monster/cultist_0.png"},
    // ElVES
    {25.0f, 15.0f, 55.0f, 12.0f, 80.0f, "堕落精灵", NoMoreDay::Tag::None, "assets/textures/monster/elf_0.png"},
    // BEAST
    {40.0f, 18.0f, 45.0f, 15.0f, 90.0f, "战兽", NoMoreDay::Tag::None, "assets/textures/monster/beast_0.png"},
    // GOBLIN
    {20.0f, 10.0f, 55.0f, 8.0f, 100.0f, "哥布林", NoMoreDay::Tag::None, "assets/textures/monster/goblin_0.png"},
    // DRAGONKIN
    {100.0f, 40.0f, 45.0f, 50.0f, 200.0f, "龙裔", NoMoreDay::Tag::Fire, "assets/textures/monster/dragon_0.png"},
    // MACHINE
    {80.0f, 20.0f, 30.0f, 30.0f, 150.0f, "机械兵", NoMoreDay::Tag::Poison | NoMoreDay::Tag::Bleeding, "assets/textures/monster/mech_0.png"},
    // ELEMENTAL
    {50.0f, 30.0f, 40.0f, 20.0f, 120.0f, "元素精魂", NoMoreDay::Tag::Physical, "assets/textures/monster/elemental_0.png"},
    // SLIME
    {15.0f, 5.0f, 30.0f, 5.0f, 100.0f, "史莱姆", NoMoreDay::Tag::Physical, "assets/textures/monster/slime_0.png"},
    // ANIMAL
    {20.0f, 8.0f, 50.0f, 5.0f, 50.0f, "野兽", NoMoreDay::Tag::None, "assets/textures/monster/animal_0.png"}
}};

// 敌人种族定义
struct EnemyRace {
  enum Type : uint8_t {
    UNDEAD = 0,    // 不死生物
    DEMON = 1,     // 恶魔
    CORRUPTED = 2, // 堕落生物
    CULTIST = 3,   // 邪教徒
    ElVES = 4,     // 精灵
    BEAST = 5,     // 兽人
    GOBLIN = 6,    // 哥布林
    DRAGONKIN = 7, // 龙裔
    MACHINE = 8,   // 机械生命体
    ELEMENTAL = 9, // 元素生物
    SLIME = 10,    // 史莱姆
    ANIMAL = 11    // 野兽
  };
};

// 敌人职业/行为模板
struct EnemyArchetype {
  enum Type : uint8_t {
    FODDER = 0,
    TANK = 1,
    RANGER = 2,
    ASSASSIN = 3,
    SUPPORT = 4 // 支援者：远离玩家并给友军施 Buff
  };
  Type archetypeType;
  std::function<void(entt::registry &, entt::entity, float)> aiBehavior;

  EnemyArchetype(Type type = FODDER) : archetypeType(type) {
    switch (type) {
    case FODDER:
      aiBehavior = &FodderBehavior;
      break;
    case TANK:
      aiBehavior = &TankBehavior;
      break;
    case RANGER:
      aiBehavior = &RangerBehavior;
      break;
    case ASSASSIN:
      aiBehavior = &AssassinBehavior;
      break;
    case SUPPORT:
      aiBehavior = &SupportBehavior;
      break;
    }
  }

  // AI行为实现 - 实际逻辑由 AISystem 和 EnemyAIBehaviors 处理
  static void FodderBehavior(entt::registry &reg, entt::entity entity,
                             float dt) {}
  static void TankBehavior(entt::registry &reg, entt::entity entity, float dt) {
  }
  static void RangerBehavior(entt::registry &reg, entt::entity entity,
                             float dt) {}
  static void AssassinBehavior(entt::registry &reg, entt::entity entity,
                               float dt) {}
  static void SupportBehavior(entt::registry &reg, entt::entity entity,
                              float dt) {}
};

// 敌人状态组件（扩展AIComponent）
struct EnemyStateComponent {
  EnemyRace::Type raceType;
  EnemyArchetype::Type archetypeType;
  float detectionRange;
  float attackRange;
  float speed;
  entt::entity target;
  float stateTimer;
  float activationRange;
  float deactivationRange;
  int level;

  // AI状态
  enum class AIState : uint8_t { IDLE, CHASE, ATTACK, FLEE, PATROL, STUNNED };
  AIState aiState;

  EnemyStateComponent(EnemyRace::Type race = EnemyRace::UNDEAD,
                      EnemyArchetype::Type arch = EnemyArchetype::FODDER)
      : raceType(race), archetypeType(arch), detectionRange(150.0f),
        attackRange(50.0f), speed(100.0f), target(entt::null), stateTimer(0.0f),
        activationRange(250.0f), deactivationRange(500.0f), level(1),
        aiState(AIState::IDLE) {

    // 根据种族和职业设置初始参数
    const auto& raceDef = kRaceData[static_cast<size_t>(race)];
    EnemyArchetype archDef(arch);

    speed = raceDef.baseSpeed;
    switch (arch) {
    case EnemyArchetype::FODDER:
      detectionRange = 80.0f;
      attackRange = 30.0f;
      break;
    case EnemyArchetype::TANK:
      detectionRange = 100.0f;
      attackRange = 40.0f;
      break;
    case EnemyArchetype::RANGER:
      detectionRange = 220.0f;
      attackRange = 250.0f;
      break;
    case EnemyArchetype::ASSASSIN:
      detectionRange = 150.0f;
      attackRange = 35.0f;
      break;
    }
  }
};

// 群聚信息组件
struct ClusterComponent {
  uint8_t clusterID;
  bool isClusterLeader;

  ClusterComponent(uint8_t id = 0, bool isLeader = false)
      : clusterID(id), isClusterLeader(isLeader) {}
};

// 休眠标记组件
struct DormantComponent {
  size_t spawnDataIndex; // 指向EnemySpawnSystem中的数据
  bool isFromSpawnData;  // 标记是否从休眠数据激活

  DormantComponent(size_t index = 0, bool fromData = false)
      : spawnDataIndex(index), isFromSpawnData(fromData) {}
};

struct EnemyRarityComponent {
  enum Rarity : uint8_t { 
    NORMAL = 0,    // 普通
    CHAMPION = 1,  // 冠军 (蓝色，通常成组出现)
    ELITE = 2,     // 精英 (金色，单体强力词缀)
    BOSS = 3,      // 首领 (橙色，关卡核心)
    NEMESIS = 4    // 宿敌 (红色，跨局进化)
  };
  Rarity rarity;
  std::vector<std::string> affixes; // 词缀

  EnemyRarityComponent(Rarity r = NORMAL) : rarity(r) {
  }
};