#pragma once
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/Common.hpp"
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

// 目标最近一次受到的暴击伤害记录（挂被击方）。
// 流云刺 173 霜凝寒骨 "碎裂" 以此为溅射基数（该目标上次暴击伤害 × 30% 冰霜）。
// source 记录来源（原始 attacker，含召唤物），触发时校验来源合法性。
struct LastCritDamageComponent {
    float amount = 0.0f;
    entt::entity source = entt::null;
};

} // namespace NoMoreDay
