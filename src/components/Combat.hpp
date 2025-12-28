#pragma once
#include "Stats.hpp"
#include <cstdint>
#include <vector>
#include <entt/entity/entity.hpp>

namespace NoMoreDay {

struct DamageEvent {
    entt::entity attacker; // 攻击者
    entt::entity target;   // 目标
    float amount;          // 伤害量
    DamageType type;       // 伤害类型
    bool is_critical;      // 是否暴击
    bool is_hit;           // 是否命中 (如果完全闪避/格挡则为false)
};

// 用于排队等待处理的伤害事件组件
struct DamageQueue {
    std::vector<DamageEvent> events;
};

// 攻击运行时状态 (替代 WeaponComponent 的 cooldownTimer)
struct AttackState {
    float cooldownTimer = 0.0f;
    float baseAttackInterval = 0.6f; // 基础攻击间隔 (秒)
};

} // namespace NoMoreDay
