#pragma once

#include "game/components/Common.hpp"
#include <cstdint>
#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <vector>


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
  Type raceType;
  float baseHP, baseDamage, baseSpeed, baseXP, baseArmor;
  std::vector<std::string> resistances;
  std::string texturePath; // 资源路径

  EnemyRace(Type type = UNDEAD) : raceType(type) {
    baseArmor = 100.0f; // 基础护甲
    switch (type) {
    case UNDEAD:
      baseHP = 30.0f;
      baseDamage = 15.0f;
      baseSpeed = 40.0f; // 接近
      baseXP = 10.0f;
      resistances = {"bleed", "poison"};
      texturePath = "assets/textures/monster/skeleton_0.png";
      break;
    case DEMON:
      baseHP = 60.0f;
      baseDamage = 25.0f;
      baseSpeed = 50.0f; // 目标
      baseXP = 25.0f;
      resistances = {"fire", "dark"};
      texturePath = "assets/textures/monster/demon_0.png";
      break;
    case CORRUPTED:
      baseHP = 25.0f;
      baseDamage = 20.0f;
      baseSpeed = 60.0f; // 稍快
      baseXP = 15.0f;
      resistances = {"slow", "stun"};
      texturePath = "assets/textures/monster/warcraft_0.png";
      break;
    case CULTIST:
      baseHP = 35.0f;
      baseDamage = 20.0f;
      baseSpeed = 45.0f;
      baseXP = 12.0f;
      resistances = {"magic"};
      texturePath = "assets/textures/monster/cultist_0.png";
      break;
    case GOBLIN:
      baseHP = 20.0f;
      baseDamage = 10.0f;
      baseSpeed = 55.0f;
      baseXP = 8.0f;
      resistances = {};
      texturePath = "assets/textures/monster/goblin_0.png";
      break;
    case SLIME:
      baseHP = 15.0f;
      baseDamage = 5.0f;
      baseSpeed = 30.0f; // 史莱姆稍慢
      baseXP = 5.0f;
      resistances = {"physical"};
      texturePath = "assets/textures/monster/slime_0.png";
      break;
    }
  }
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
        activationRange(300.0f), deactivationRange(600.0f), level(1),
        aiState(AIState::IDLE) {

    // 根据种族和职业设置初始参数
    EnemyRace raceDef(race);
    EnemyArchetype archDef(arch);

    speed = raceDef.baseSpeed;
    switch (arch) {
    case EnemyArchetype::FODDER:
      detectionRange = 100.0f;
      attackRange = 30.0f;
      break;
    case EnemyArchetype::TANK:
      detectionRange = 120.0f;
      attackRange = 40.0f;
      break;
    case EnemyArchetype::RANGER:
      detectionRange = 300.0f;
      attackRange = 250.0f;
      break;
    case EnemyArchetype::ASSASSIN:
      detectionRange = 200.0f;
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