#include "CombatSystem.hpp"
#include <cmath>
#include "../tools/Logger.hpp"
#include "../components/EffectComponent.hpp"
#include "../components/PlayerState.hpp"
#include "../components/Stats.hpp"
#include "../components/EnemyComponent.hpp"

void CombatSystem::update(entt::registry& registry, systems::SpatialHashGrid& grid, const Camera2D& camera, float dt) {
    // LOG_TRACE("CombatSystem::update: 处理战斗逻辑");

    // 不再强制要求 WeaponComponent
    auto view = registry.view<PlayerTag, InputComponent, Position>();
    
    // 遍历所有玩家（通常只有一个）
    for (auto entity : view) {
        auto& input = view.get<InputComponent>(entity);
        const auto& pos = view.get<Position>(entity);

        // 获取组件指针
        auto* weapon = registry.try_get<WeaponComponent>(entity);
        auto* attackState = registry.try_get<NoMoreDay::AttackState>(entity);
        const auto* stats = registry.try_get<NoMoreDay::CombatStats>(entity);

        // 确定战斗参数
        float currentCooldownTimer = 0.0f;
        float maxCooldown = 1.0f;
        float range = 50.0f;
        float knockback = 0.0f;
        float baseDamage = 0.0f;

        if (attackState) {
            // 新系统路径
            currentCooldownTimer = attackState->cooldownTimer;
            maxCooldown = attackState->baseAttackInterval;
            if (stats) {
                if (stats->attack_speed > 0.01f) maxCooldown /= stats->attack_speed;
                range = (stats->cast_range > 0.1f) ? stats->cast_range : 60.0f; // 默认范围
                knockback = stats->knockback;
            }
        } else if (weapon) {
            // 遗留系统路径
            currentCooldownTimer = weapon->cooldownTimer;
            maxCooldown = weapon->cooldown;
            if (stats && stats->attack_speed > 0.01f) maxCooldown /= stats->attack_speed;
            range = weapon->range;
            knockback = weapon->knockback;
            baseDamage = weapon->damage;
        } else {
            // 无法攻击
            continue;
        }

        // 更新冷却
        if (currentCooldownTimer > 0.0f) currentCooldownTimer -= dt;

        // 2. 处理攻击
        if (input.attack && currentCooldownTimer <= 0.0f) {
            // 重置冷却时间（使用计算出的有效冷却时间）
            LOG_DEBUG("玩家 {} 发起攻击。有效冷却时间: {:.2f}s", (uint32_t)entity, maxCooldown);

            currentCooldownTimer = maxCooldown;

            // 计算瞄准方向（玩家 -> 鼠标）
            Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);
            float dx = mouseWorld.x - pos.x;
            float dy = mouseWorld.y - pos.y;
            float len = std::sqrt(dx*dx + dy*dy);
            
            // 归一化方向
            float dirX = (len > 0) ? dx / len : 1.0f;
            float dirY = (len > 0) ? dy / len : 0.0f;

            // --- 生成攻击特效 (挥剑轨迹) ---
            auto effectEntity = registry.create();
            registry.emplace<Position>(effectEntity, pos.x, pos.y); // 特效跟随玩家位置(或固定在挥剑处)
            float angleDeg = std::atan2(dirY, dirX) * (180.0f / PI);
            
            AttackEffect effect;
            effect.timer = 0.0f;
            effect.lifeTime = 0.2f; // 持续0.2秒
            effect.rotation = angleDeg;
            effect.range = range;
            effect.arcAngle = 120.0f; 
            effect.color = GOLD; // 剑光颜色
            registry.emplace<AttackEffect>(effectEntity, effect);

            // 3. 查询网格中的目标
            // 以玩家为中心，攻击距离为半径进行查询
            bool hitAny = false;
            grid.query({pos.x, pos.y}, range, [&](entt::entity target) {
                if (target == entity) return; // 不要击中自己

                // Validate Target (must have Position)
                if (!registry.valid(target) || !registry.all_of<Position>(target)) return;

                const auto& tPos = registry.get<Position>(target);
                float dx = tPos.x - pos.x;
                float dy = tPos.y - pos.y;
                float distSq = dx * dx + dy * dy;
                
                // 1. 距离检查
                if (distSq <= range * range) {
                    // 2. 扇形角度检查 (120度)
                    float angleToTarget = std::atan2(dy, dx);
                    float aimAngle = std::atan2(dirY, dirX);
                    float angleDiff = std::abs(angleToTarget - aimAngle);
                    if (angleDiff > PI) angleDiff = 2.0f * PI - angleDiff;
                    
                    if (angleDiff <= (120.0f * 0.5f) * (PI / 180.0f)) {
                        // HIT CONFIRMED
                        hitAny = true;
                    
                    LOG_DEBUG("Hit confirmed on target {}", (uint32_t)target);
                    // Apply Knockback
                    if (registry.all_of<Velocity>(target)) {
                        auto& tVel = registry.get<Velocity>(target);
                        tVel.vx += dirX * knockback;
                        tVel.vy += dirY * knockback;
                    }

                    // 计算最终伤害
                    float totalDamage = 0.0f;
                    bool isCrit = false;

                    // 1. 计算物理伤害 (武器基础 + 附加物理点伤)
                    float physBase = (stats && stats->max_weapon_damage > 0.1f) 
                        ? (stats->min_weapon_damage + (stats->max_weapon_damage - stats->min_weapon_damage) * ((float)GetRandomValue(0, 1000) / 1000.0f))
                        : baseDamage;
                    
                    if (stats) physBase += stats->flat_damage[(int)NoMoreDay::DamageType::Physical];
                    totalDamage += CalculateDamage(stats ? *stats : NoMoreDay::CombatStats{}, 
                                                 registry.get_or_emplace<NoMoreDay::CombatStats>(target), 
                                                 physBase, NoMoreDay::DamageType::Physical);

                    LOG_TRACE("Combat: BasePhys={:.1f}, FinalDmg={:.1f}, Target={}", 
                        physBase, totalDamage, (uint32_t)target);

                    // 2. 计算其他元素伤害 (来自装备的附加点伤)
                    if (stats) {
                        for (int i = 1; i < (int)NoMoreDay::DamageType::Count; ++i) {
                            if (stats->flat_damage[i] > 0.01f) {
                                totalDamage += CalculateDamage(*stats, registry.get_or_emplace<NoMoreDay::CombatStats>(target), 
                                                             stats->flat_damage[i], (NoMoreDay::DamageType)i);
                            }
                        }
                    }

                    float finalDamage = totalDamage;

                    // 3. 暴击判定 (作用于最终总伤害)
                    if (registry.all_of<NoMoreDay::CombatStats>(target)) {
                        float roll = (float)GetRandomValue(0, 1000) / 1000.0f;
                        if (roll < stats->crit_chance) {
                            isCrit = true;
                            finalDamage *= (stats->crit_damage > 0.1f ? stats->crit_damage : 1.5f);
                        }
                    }

                    // Apply Damage
                    if (registry.all_of<HealthComponent>(target)) {
                        // Cache position for popup as target might be destroyed
                        float popupX = tPos.x;
                        float popupY = tPos.y - 20.0f; // 弹出位置

                        // 应用伤害逻辑（这会处理生命值减少和死亡）
                        bool targetDead = ApplyDamage(registry, target, finalDamage, entity);
                        LOG_DEBUG("对 {} 造成 {:.1f} 伤害 (暴击: {}, 死亡: {})", (uint32_t)target, finalDamage, isCrit, targetDead);

                        // --- 生成伤害飘字 ---
                        auto popupEntity = registry.create();
                        registry.emplace<Position>(popupEntity, popupX, popupY);
                        
                        DamagePopup popup;
                        popup.damage = finalDamage; // 伤害值
                        popup.timer = 0.0f; // 计时器
                        popup.lifeTime = 0.8f;
                        popup.velX = (float)(GetRandomValue(-20, 20)); // 随机水平漂移
                        popup.velY = -100.0f; // 向上飘
                        
                        // Color coding
                        if (isCrit) {
                            popup.color = RED;
                            popup.lifeTime = 1.0f; // 暴击持续时间更长
                        } else {
                            popup.color = WHITE;
                        }
                        
                        registry.emplace<DamagePopup>(popupEntity, popup);
                    } else { // 如果目标没有生命值组件
                        LOG_LIMITED_WARN(1.0f, "Target {} hit but has no HealthComponent", (uint32_t)target);
                        // For particles/props without health, maybe just destroy or knockback?
                        // For now, let's just knock them back hard.
                    }
                }
            } // Close angleDiff
            });

            if (!hitAny) {
                LOG_TRACE("攻击未命中任何目标（查询半径: {:.1f}）", range);
            }
        }

        // 写回冷却时间
        if (attackState) attackState->cooldownTimer = currentCooldownTimer;
        if (weapon) weapon->cooldownTimer = currentCooldownTimer;
    }
}

float CombatSystem::CalculateDamage(const NoMoreDay::CombatStats& attacker, const NoMoreDay::CombatStats& defender, float baseDamage, NoMoreDay::DamageType type) {
    using namespace NoMoreDay;

    // 1. Apply Attacker Multipliers
    // 公式：基础伤害 * (1 + 乘数)
    // 修复：如果倍率为0（未初始化），则视为1.0，防止伤害归零
    float multiplier = attacker.damage_multipliers[(int)type];
    float damage = baseDamage * (multiplier > 0.001f ? multiplier : 1.0f);

    // 2. Mitigation
    float mitigation = 0.0f;
    
    if (type == DamageType::Physical) {
        // Armor Reduction
        // Formula: Reduction = Armor / (Armor + 100)
        // This is a placeholder constant. In real RPGs it scales with level usually.
        float armor = defender.armor;
        // Apply Pen? 
        // armor -= attacker.armor_pen; 
        if (armor < 0) armor = 0;
        
        if (armor > 0) {
            mitigation = armor / (armor + 100.0f);
        }
    } else { // 元素抗性
        // Elemental Resistance
        float res = defender.resistances[(int)type];
        // Hard Cap at 75%
        if (res > 0.75f) res = 0.75f;
        mitigation = res;
    }

    // 3. Final Calculation
    damage *= (1.0f - mitigation);

    return std::max(0.0f, damage);
}

bool CombatSystem::ApplyDamage(entt::registry& registry, entt::entity target, float amount, entt::entity attacker) {
    if (!registry.valid(target) || !registry.all_of<HealthComponent>(target)) {
        return false;
    }

    auto& hp = registry.get<HealthComponent>(target);
    hp.current -= amount;

    if (hp.current <= 0) {
        LOG_INFO("Entity {} destroyed", (uint32_t)target);
        
        // 处理击杀奖励
        if (registry.valid(attacker) && registry.all_of<PlayerStats>(attacker)) {
            auto& playerStats = registry.get<PlayerStats>(attacker);
            playerStats.killCount++;
            LOG_TRACE("Player {} kill count: {}", (uint32_t)attacker, playerStats.killCount);
        }

        // 标记实体为已击杀，用于经验奖励和其他死亡后处理
        registry.emplace<KilledTag>(target, attacker);

        // registry.destroy(target); // Defer actual destruction to XPAwardingSystem or similar
        return true;
    }

    return false;
}
