#pragma once
#include "game/components/Common.hpp"
#include <entt/entt.hpp>
#include <cmath>

// AI行为类型枚举
enum class AIType {
    IDLE,           // 闲置
    PATROL,         // 巡逻
    CHASE,          // 追击
    ATTACK,         // 攻击
    FLEE            // 逃跑
};

// AI状态组件
struct AIComponent {
    AIType aiType = AIType::IDLE;
    float detectionRange = 100.0f;     // 检测范围
    float attackRange = 50.0f;        // 攻击范围
    float speed = 80.0f;              // AI移动速度
    float lastDecisionTime = 0.0f;   // 上次决策时间
    float decisionInterval = 0.5f;   // 决策间隔
    
    // 巡逻相关
    Position patrolStart = { 0.0f, 0.0f };
    Position patrolEnd = { 0.0f, 0.0f };
    bool patrolDirection = true;     // true为正向，false为反向
    
    // 追击相关
    entt::entity target = entt::null;      // 当前目标
    
    // 状态相关
    float stateTimer = 0.0f;         // 状态计时器
    bool isAggressive = true;        // 是否具有攻击性
    
    // 帧率无关更新节流
    float updateAccumulator = 0.0f;  // 距离上次AI更新的时间累积
    
    AIComponent() = default;
    AIComponent(AIType type,
                float detRange = 100.0f,
                float attRange = 50.0f,
                float spd = 80.0f,
                float decisionInt = 0.5f)
        : aiType(type), detectionRange(detRange), attackRange(attRange),
          speed(spd), decisionInterval(decisionInt) {}
};

// 标记敌人类实体的组件
struct EnemyTag {};