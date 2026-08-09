#pragma once
#include "game/components/Stats.hpp"
#include "game/components/Common.hpp"
#include <cstdint>
#include <vector>
#include <entt/entity/entity.hpp>

namespace NoMoreDay {

struct DamageEvent {
    entt::entity attacker = entt::null; // 攻击者
    entt::entity target = entt::null;   // 目标
    float amount = 0.0f;          // 伤害量
    DamageType type = DamageType::Physical;       // 伤害类型
    bool is_critical = false;      // 是否暴击
    bool is_hit = true;           // 是否命中 (如果完全闪避/格挡则为false)
};

// 用于排队等待处理的伤害事件组件
struct DamageQueue {
    std::vector<DamageEvent> events;
};

// 攻击运行时状态 (替代 WeaponComponent 的 cooldownTimer)
struct AttackState {
    float cooldownTimer = 0.0f;
    float baseAttackInterval = 1.0f; // NoMoreDay::Constants::Combat::System::DEFAULT_ATTACK_COOLDOWN 基础攻击间隔 (秒)
};

} // namespace NoMoreDay
