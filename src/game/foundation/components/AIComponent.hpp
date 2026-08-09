#pragma once
#include "game/foundation/components/Common.hpp"
#include <cmath>
#include <entt/entt.hpp>

// namespace NoMoreDay {

// AI行为类型枚举
enum class AIType {
  IDLE,              // 闲置
  PATROL,            // 巡逻
  CHASE,             // 追击
  ATTACK,            // 攻击
  FLEE,              // 逃跑
  NEMESIS_HUNTER,    // 宿敌猎杀模式 (主动巡逻搜索玩家)
  SUPPORT_FLEE_BUFF, // 支援者：远离玩家 + 给友军施 Buff
  ASSASSIN_STEALTH,  // 刺客：潜行等待时机
  TANK_BLOCK         // 坦克：阻挡视线保护远程友军
};

// AI状态组件
struct AIComponent {
  AIType aiType = AIType::IDLE;
  float detectionRange = 100.0f; // 检测范围
  float attackRange = 50.0f;     // 攻击范围
  float speed = 80.0f;           // AI移动速度
  float lastDecisionTime = 0.0f; // 上次决策时间
  float decisionInterval = 0.5f; // 决策间隔

  // 巡逻相关
  Position patrolStart = {0.0f, 0.0f};
  Position patrolEnd = {0.0f, 0.0f};
  bool patrolDirection = true; // true为正向，false为反向

  // 追击相关
  entt::entity target = entt::null; // 当前目标

  // 状态相关
  float stateTimer = 0.0f;  // 状态计时器
  bool isAggressive = true; // 是否具有攻击性

  // 帧率无关更新节流
  float updateAccumulator = 0.0f; // 距离上次AI更新的时间累累积

  // === 支援者 (SUPPORT) 专用 ===
  float buffCooldown = 5.0f;        // Buff 施放冷却时间
  float buffCooldownTimer = 0.0f;   // 当前冷却计时
  float buffRadius = 300.0f;        // Buff 施放范围
  float preferredDistance = 300.0f; // 与玩家保持的首选距离

  // === 刺客 (ASSASSIN) 专用 ===
  bool isStealthed = false;        // 是否处于隐身状态
  float stealthTimer = 0.0f;       // 隐身持续时间
  float backstabMultiplier = 2.5f; // 背刺伤害倍率
  float backstabCooldown = 8.0f;   // 背刺冷却
  float backstabCooldownTimer = 0.0f;

  // === 坦克 (TANK) 专用 ===
  entt::entity protectTarget = entt::null; // 保护的远程友军
  float blockingMass = 5.0f;               // 阻挡质量 (难以被击退)

  AIComponent() = default;
  AIComponent(AIType type, float detRange = 100.0f, float attRange = 50.0f,
              float spd = 80.0f, float decisionInt = 0.5f)
      : aiType(type), detectionRange(detRange), attackRange(attRange),
        speed(spd), decisionInterval(decisionInt) {}
};

// 标记敌人类实体的组件
struct EnemyTag {};

// } // namespace NoMoreDay
