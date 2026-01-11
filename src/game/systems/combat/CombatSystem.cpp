#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/EffectSystem.hpp"
#include "engine/render/RenderSystem.hpp" 
#include "engine/render/GPUParticleSystem.hpp"
#include <cmath>
#include "core/logging/Logger.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Stats.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/world/MovementStanceSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "core/math/PhysicsUtils.hpp"

// Bring CombatEvent types into scope
using NoMoreDay::CombatEvent;
using NoMoreDay::CombatEventType;
using NoMoreDay::CombatEventDispatcher;
namespace CombatEventFactory = NoMoreDay::CombatEventFactory;

// Static member initialization
// (Assuming any static members are here or removed if not needed)

void CombatSystem::update(entt::registry& registry, NoMoreDay::systems::SpatialHashGrid& grid, const Camera2D& camera, float dt) {
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
        float range = 100.0f;
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

                        // --- 命中判定 (Accuracy/Miss Check) ---
                        // 如果命中率 < 1.0，则有几率未命中
                        if (stats && stats->accuracy < 1.0f) {
                            float roll = (float)GetRandomValue(0, 1000) / 1000.0f;
                            if (roll > stats->accuracy) {
                                LOG_DEBUG("Attack missed target {}", (uint32_t)target);
                                return; // 未命中，跳过后续所有判定
                            }
                        }

                        // --- 闪避判定 (Dodge Check) ---
                        bool isDodged = false;
                        if (registry.all_of<NoMoreDay::CombatStats>(target)) {
                            const auto& targetStats = registry.get<NoMoreDay::CombatStats>(target);
                            // 判定是否闪避 (不免疫持续伤害，如果目前持续伤害的机制还没实现则注释预留)
                            // TODO: Future DoT (Damage over Time) logic should bypass this check.
                            if (targetStats.dodge_chance > 0.0f) {
                                // 计算有效闪避率：目标闪避 - (攻击者命中 - 100%)
                                // 例如：目标闪避 30%，攻击者命中 120% -> 有效闪避 10%
                                float attackerAccuracy = stats ? stats->accuracy : 1.0f;
                                float effectiveDodge = targetStats.dodge_chance - (attackerAccuracy - 1.0f);
                                if (effectiveDodge < 0.0f) effectiveDodge = 0.0f;

                                float roll = (float)GetRandomValue(0, 1000) / 1000.0f;
                                if (roll < effectiveDodge) {
                                    isDodged = true;
                                }
                            }
                        }

                        if (isDodged) {
                            LOG_DEBUG("Target {} dodged the attack", (uint32_t)target);
                            
                            // --- Event System: OnDodge ---
                            CombatEvent dodge_evt = CombatEventFactory::CreateOnDodge(target, entity);
                            CombatEventDispatcher::Dispatch(registry, dodge_evt);
                            
                            return; // 闪避成功，跳过击退和伤害计算
                        }

                        // --- 格挡判定 (Block Check) ---
                        bool isBlocked = false;
                        float blockedAmount = 0.0f;
                        if (registry.all_of<NoMoreDay::CombatStats>(target)) {
                            const auto& targetStats = registry.get<NoMoreDay::CombatStats>(target);
                            if (targetStats.block_chance > 0.0f) {
                                float roll = (float)GetRandomValue(0, 1000) / 1000.0f;
                                if (roll < targetStats.block_chance) {
                                    isBlocked = true;
                                    blockedAmount = targetStats.block_amount;
                                    LOG_DEBUG("Target {} blocked attack (Amt: {:.1f})", (uint32_t)target, blockedAmount);
                                    if (registry.all_of<Position>(target)) {
                                        EffectSystem::EmitStatusPopup(registry, {tPos.x, tPos.y}, "格挡", SKYBLUE);
                                    }
                                    
                                    // --- Event System: OnBlock ---
                                    CombatEvent block_evt = CombatEventFactory::CreateOnBlock(target, entity, blockedAmount);
                                    CombatEventDispatcher::Dispatch(registry, block_evt);
                                }
                            }
                        }
                    


                    LOG_DEBUG("Hit confirmed on target {}", (uint32_t)target);
                    // Apply Knockback
                   NoMoreDay::Utils::ApplyKnockback(registry, target, {pos.x, pos.y}, knockback);

                    // 计算最终伤害
                    float totalDamage = 0.0f;
                    bool isCrit = false;

                    // 1. 计算物理伤害 (武器基础 + 附加物理点伤)
                    float physBase = (stats && stats->max_weapon_damage > 0.1f) 
                        ? stats->min_weapon_damage
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

                    // Apply Block Reduction
                    // Formula: Reduction = BlockAmount / (BlockAmount + 100)
                    if (isBlocked) {
                        float blockMitigation = blockedAmount / (blockedAmount + 100.0f);
                        finalDamage *= (1.0f - blockMitigation);
                    }

                    // 3. 暴击判定 (作用于最终总伤害)
                    if (registry.all_of<NoMoreDay::CombatStats>(target)) {
                        float roll = (float)GetRandomValue(0, 1000) / 1000.0f;
                        // 应用暴击率上限
                        float effectiveCrit = std::min(stats->crit_chance, NoMoreDay::GameConstants::CRIT_CHANCE_CAP);
                        if (roll < effectiveCrit) {
                            isCrit = true;
                            finalDamage *= (stats->crit_damage > 0.1f ? stats->crit_damage : 1.5f);
                        }
                    }

                    // Apply Damage
                    if (registry.all_of<HealthComponent>(target)) {
                        // 应用伤害逻辑（这会处理生命值减少和死亡并生成飘字）
                        bool targetDead = ApplyDamage(registry, target, finalDamage, entity, isCrit);
                        LOG_DEBUG("对 {} 造成 {:.1f} 伤害 (暴击: {}, 死亡: {})", (uint32_t)target, finalDamage, isCrit, targetDead);

                        // --- 荆棘伤害 (Thorns) ---
                        if (!targetDead && registry.all_of<NoMoreDay::CombatStats>(target)) {
                            const auto& tStats = registry.get<NoMoreDay::CombatStats>(target);
                            if (tStats.thorns > 0.0f) {
                                // 反伤给攻击者
                                ApplyDamage(registry, entity, tStats.thorns, target, false);
                                LOG_TRACE("Thorns: Entity {} took {:.1f} damage", (uint32_t)entity, tStats.thorns);
                            }
                        }

                        // --- 生命偷取 & 击中回复 ---
                        if (stats && registry.all_of<HealthComponent>(entity)) {
                            float healAmount = 0.0f;
                            if (stats->life_on_hit > 0.0f) healAmount += stats->life_on_hit;
                            if (stats->life_steal > 0.0f) healAmount += finalDamage * stats->life_steal;

                            if (healAmount > 0.0f) {
                                auto& attackerHp = registry.get<HealthComponent>(entity);
                                attackerHp.current += healAmount;
                                if (attackerHp.current > attackerHp.max) attackerHp.current = attackerHp.max;
                                // 可选：在这里添加治疗飘字或特效
                            }
                        }
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

    // --- Enemy Attack Logic ---
    auto enemyView = registry.view<EnemyTag, AIComponent, Position, NoMoreDay::AttackState, NoMoreDay::CombatStats>();
    for (auto enemy : enemyView) {
        auto& ai = enemyView.get<AIComponent>(enemy);
        auto& ePos = enemyView.get<Position>(enemy);
        auto& eAttack = enemyView.get<NoMoreDay::AttackState>(enemy);
        const auto& eStats = enemyView.get<NoMoreDay::CombatStats>(enemy);

        // Update Cooldown
        if (eAttack.cooldownTimer > 0.0f) {
            eAttack.cooldownTimer -= dt;
        }

        // Only attack if in ATTACK state and cooldown is ready
        if (ai.aiType == AIType::ATTACK && eAttack.cooldownTimer <= 0.0f) {
            if (registry.valid(ai.target) && registry.all_of<Position>(ai.target)) {
                const auto& tPos = registry.get<Position>(ai.target);
                float dx = tPos.x - ePos.x;
                float dy = tPos.y - ePos.y;
                float distSq = dx * dx + dy * dy;

                // Precision range check
                if (distSq <= ai.attackRange * ai.attackRange) {
                    // Start Attack
                    float interval = eAttack.baseAttackInterval;
                    if (eStats.attack_speed > 0.01f) interval /= eStats.attack_speed;
                    eAttack.cooldownTimer = interval;

                    // Calculate Damage
                    float basePhys = eStats.min_weapon_damage + (eStats.max_weapon_damage - eStats.min_weapon_damage) * ((float)GetRandomValue(0, 1000) / 1000.0f);
                    
                    // Simple hit check for monsters for now (could be expanded)
                    bool isDodged = false;
                    if (registry.all_of<NoMoreDay::CombatStats>(ai.target)) {
                        const auto& tStats = registry.get<NoMoreDay::CombatStats>(ai.target);
                        float effectiveDodge = tStats.dodge_chance - (eStats.accuracy - 1.0f);
                        if (effectiveDodge > 0.0f) {
                            if ((float)GetRandomValue(0, 1000) / 1000.0f < effectiveDodge) {
                                isDodged = true;
                            }
                        }
                    }

                    if (!isDodged) {
                        float finalDamage = CalculateDamage(eStats, registry.get_or_emplace<NoMoreDay::CombatStats>(ai.target), basePhys, NoMoreDay::DamageType::Physical);
                        
                        // Check for block
                        bool isBlocked = false;
                        float blockedAmount = 0.0f;
                        if (registry.all_of<NoMoreDay::CombatStats>(ai.target)) {
                            const auto& tStats = registry.get<NoMoreDay::CombatStats>(ai.target);
                            if (tStats.block_chance > 0.0f && (float)GetRandomValue(0, 1000) / 1000.0f < tStats.block_chance) {
                                isBlocked = true;
                                blockedAmount = tStats.block_amount;
                                float blockMitigation = blockedAmount / (blockedAmount + 100.0f);
                                finalDamage *= (1.0f - blockMitigation);
                                
                                if (registry.all_of<Position>(ai.target)) {
                                    EffectSystem::EmitStatusPopup(registry, {tPos.x, tPos.y}, "格挡", SKYBLUE);
                                }
                            }
                        }

                        ApplyDamage(registry, ai.target, finalDamage, enemy, false);
                        LOG_DEBUG("Monster {} attacked {} for {:.1f} damage", (uint32_t)enemy, (uint32_t)ai.target, finalDamage);
                    } else {
                        // Show "Dodge" popup
                        if (registry.all_of<Position>(ai.target)) {
                             EffectSystem::EmitStatusPopup(registry, {tPos.x, tPos.y}, "闪避", WHITE);
                        }
                    }
                }
            }
        }
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
        float effective_armor = defender.armor - attacker.armor_pen;
        
        if (effective_armor >= 0) {
            mitigation = 1.0f - (100.0f / (100.0f + effective_armor));
        } else {
            // Negative armor amplification: multiplier = 2 - 100/(100 - eff)
            // We want 'mitigation' to be negative so that damage * (1 - mit) increases.
            // (1 - mitigation) = 2 - 100/(100-eff) -> mit = 1 - (2 - 100/(100-eff)) = 100/(100-eff) - 1
            mitigation = (100.0f / (100.0f - effective_armor)) - 1.0f;
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

    // 4. Global Damage Reduction
    if (defender.damage_reduction > 0.0f) {
        float reduction = defender.damage_reduction;
        if (reduction > 0.90f) reduction = 0.90f; // Hard Cap 90%
        damage *= (1.0f - reduction);
    }

    return std::max(0.0f, damage);
}

bool CombatSystem::ApplyDamage(entt::registry& registry, entt::entity target, float amount, entt::entity attacker, bool isCrit, bool showVFX) {
    if (!registry.valid(target) || !registry.all_of<HealthComponent>(target)) {
        return false;
    }

    // --- Phantom Flash Riposte ---
    if (auto* pf = registry.try_get<NoMoreDay::PhantomFlashComponent>(target)) {
        if (!pf->triggered) {
            pf->triggered = true;
            LOG_INFO("Phantom Flash triggered! Riposte on entity {}", (uint32_t)attacker);
            
            if (registry.valid(attacker) && registry.all_of<Position>(attacker)) {
                const auto& aPos = registry.get<Position>(attacker);
                const auto& tPos = registry.get<Position>(target);
                
                // Teleport behind attacker (approx)
                Vector2 dir = Vector2Normalize(Vector2Subtract({tPos.x, tPos.y}, {aPos.x, aPos.y}));
                Vector2 ripostePos = Vector2Add({aPos.x, aPos.y}, Vector2Scale(dir, 20.0f));
                
                auto& targetPosComp = registry.get<Position>(target);
                targetPosComp.x = ripostePos.x;
                targetPosComp.y = ripostePos.y;

                // Shadow Riposte (Cast Flowing Thrust ID 1 as riposte)
                NoMoreDay::SkillSystem::ShadowCast(registry, target, 1, ripostePos, {aPos.x, aPos.y});
            }
            
            return false; // Damage blocked
        }
    }

    // Interrupt movement stance on damage
    NoMoreDay::MovementStanceSystem::OnTakeDamage(registry, target);

    auto& hp = registry.get<HealthComponent>(target);
    
    // 如果已经打上了死亡标记，直接返回（防止重复结算和回血复活后的逻辑干扰）
    if (registry.all_of<KilledTag>(target)) {
        hp.current = 0.0f; // 强制锁定
        return true;
    }

    hp.current -= amount;

    // --- Unified Damage Popup (Gated by showVFX for performance) ---
    if (showVFX && registry.all_of<Position>(target)) {
        const auto& tPos = registry.get<Position>(target);
        EffectSystem::EmitDamagePopup(registry, {tPos.x, tPos.y}, amount, isCrit);
        
        // Screen Shake for heavy damage
        if (amount > 100.0f) {
             RenderSystem::AddScreenShake(0.15f);
        }
    }

    if (hp.current <= 0) {
        // --- Blade Formation: Immortality (Node 322) ---
        if (auto* formation = registry.try_get<NoMoreDay::BladeFormationComponent>(target)) {
            if (formation->immortality_ready) {
                formation->immortality_ready = false;
                float heal = hp.max * 0.3f;
                hp.current = heal;
                LOG_INFO("Blade Formation Immortality (322) triggered for entity {}! Restored {:.1f} HP", (uint32_t)target, heal);
                
                // Visual Effect for Immortality
                if (registry.all_of<Position>(target)) {
                    const auto& tPos = registry.get<Position>(target);
                    EffectSystem::EmitStatusPopup(registry, {tPos.x, tPos.y}, "不灭剑魂", GOLD);
                    auto& particleSys = NoMoreDay::systems::GPUParticleSystem::Get();
                    auto splash = NoMoreDay::systems::InkEffectHelper::CreateInkSplash({tPos.x, tPos.y}, 20, 15.0f, 200.0f);
                    for(auto& p : splash) { p.color = GOLD; particleSys.Emit(p); }
                    RenderSystem::AddScreenShake(0.3f);
                }
                return false; // Death prevented
            }
        }

        hp.current = 0.0f; // 锁定生命值为0
        
        // --- OPTIMIZATION: Immediate Logical and Visual Removal ---
        // 立即移除战斗标签，使其无法被后续技能搜寻到
        if (registry.all_of<EnemyTag>(target)) registry.remove<EnemyTag>(target);
        if (registry.all_of<AIComponent>(target)) registry.remove<AIComponent>(target);
        if (registry.all_of<SpriteComponent>(target)) registry.remove<SpriteComponent>(target);

        registry.emplace<KilledTag>(target, attacker);
        
        // --- Event System: OnKill ---
        float overkill = -hp.current; // hp.current is 0 or negative after damage
        CombatEvent kill_evt = CombatEventFactory::CreateOnKill(attacker, target, overkill);
        CombatEventDispatcher::Dispatch(registry, kill_evt);

        // 处理击杀奖励 (Moved relevant parts to XPAwardingSystem)
        // Note: Actual item dropping is handled by DropSystem
        if (registry.valid(attacker) && registry.all_of<PlayerStats>(attacker)) {
            auto& playerStats = registry.get<PlayerStats>(attacker);
            playerStats.killCount++;
        }

        return true;
    }

    return false;
}