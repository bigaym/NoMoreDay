#pragma once
#include "../components/Common.hpp"
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
    AIType aiType;
    float detectionRange;     // 检测范围
    float attackRange;        // 攻击范围
    float speed;              // AI移动速度
    float lastDecisionTime;   // 上次决策时间
    float decisionInterval;   // 决策间隔
    
    // 巡逻相关
    Position patrolStart;
    Position patrolEnd;
    bool patrolDirection;     // true为正向，false为反向
    
    // 追击相关
    entt::entity target;      // 当前目标
    
    // 状态相关
    float stateTimer;         // 状态计时器
    bool isAggressive;        // 是否具有攻击性
    
    AIComponent(AIType type = AIType::IDLE,
                float detRange = 100.0f,
                float attRange = 50.0f,
                float spd = 80.0f,
                float decisionInt = 0.5f)
        : aiType(type), detectionRange(detRange), attackRange(attRange),
          speed(spd), lastDecisionTime(0.0f), decisionInterval(decisionInt),
          patrolStart{0.0f, 0.0f}, patrolEnd{0.0f, 0.0f}, patrolDirection(true),
          target(entt::null), stateTimer(0.0f), isAggressive(true) {}
};

// 标记敌人类实体的组件
struct EnemyTag {};