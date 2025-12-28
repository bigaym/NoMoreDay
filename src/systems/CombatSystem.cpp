#include "CombatSystem.hpp"
#include <cmath>
#include "../tools/Logger.hpp"
#include "../components/EffectComponent.hpp"
#include "../components/PlayerState.hpp"
#include "../components/Stats.hpp" // Added include

void CombatSystem::update(entt::registry& registry, systems::SpatialHashGrid& grid, const Camera2D& camera, float dt) {
    // LOG_TRACE("CombatSystem::update: Processing combat logic");

    auto view = registry.view<PlayerTag, InputComponent, WeaponComponent, Position>();
    
    // Iterate over all players (usually just one)
    for (auto entity : view) {
        auto& input = view.get<InputComponent>(entity);
        auto& weapon = view.get<WeaponComponent>(entity);
        const auto& pos = view.get<Position>(entity);

        // Get Stats if available
        const NoMoreDay::CombatStats* stats = nullptr;
        if (registry.all_of<NoMoreDay::CombatStats>(entity)) {
            stats = &registry.get<NoMoreDay::CombatStats>(entity);
        }

        // 1. Handle Cooldown
        // If stats are available, cooldown might be affected by attack_speed
        float cooldown = weapon.cooldown;
        if (stats) {
            // Speed 2.0 -> Cooldown / 2.0
            // Guard against 0 or negative speed
            if (stats->attack_speed > 0.01f) {
                cooldown /= stats->attack_speed;
            }
        }

        if (weapon.cooldownTimer > 0.0f) {
            weapon.cooldownTimer -= dt;
        }

        // 2. Process Attack
        if (input.attack && weapon.cooldownTimer <= 0.0f) {
            // Reset Cooldown (using the calculated effective cooldown)
            LOG_DEBUG("Player {} initiating attack. Effective Cooldown: {:.2f}s", (uint32_t)entity, cooldown);

            // Note: weapon.cooldownTimer tracks time remaining. 
            // If we change max cooldown dynamically, it's fine for the reset.
            weapon.cooldownTimer = cooldown;

            // Calculate Aim Direction (Player -> Mouse)
            Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);
            float dx = mouseWorld.x - pos.x;
            float dy = mouseWorld.y - pos.y;
            float len = std::sqrt(dx*dx + dy*dy);
            
            // Normalized Direction
            float dirX = (len > 0) ? dx / len : 1.0f;
            float dirY = (len > 0) ? dy / len : 0.0f;

            // Define Hitbox Center (Slightly in front of player)
            // Use stats range if available, else weapon range
            float range = stats ? stats->cast_range : weapon.range;

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

            // 3. Query Grid for targets
            // 以玩家为圆心，攻击距离为半径进行查询
            grid.query({pos.x, pos.y}, range, [&](entt::entity target) {
                if (target == entity) return; // Don't hit self

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
                    
                    LOG_DEBUG("Hit confirmed on target {}", (uint32_t)target);
                    // Apply Knockback
                    if (registry.all_of<Velocity>(target)) {
                        auto& tVel = registry.get<Velocity>(target);
                        tVel.vx += dirX * weapon.knockback;
                        tVel.vy += dirY * weapon.knockback;
                    }

                    // Calculate Final Damage
                    float finalDamage = weapon.damage;
                    bool isCrit = false;

                    if (stats) {
                        // 1. Base: Random(Min, Max)
                        float minD = stats->min_weapon_damage;
                        float maxD = stats->max_weapon_damage;
                        // Simple Random float
                        float randFactor = (float)GetRandomValue(0, 1000) / 1000.0f;
                        finalDamage = minD + (maxD - minD) * randFactor;
                        
                        // 2. Multipliers (Physical for now)
                        finalDamage *= stats->damage_multipliers[(int)NoMoreDay::DamageType::Physical];
                        
                        // 3. Crit
                        float roll = (float)GetRandomValue(0, 1000) / 1000.0f;
                        if (roll < stats->crit_chance) {
                            isCrit = true;
                            finalDamage *= stats->crit_damage;
                        }
                    }

                    // Apply Damage
                    if (registry.all_of<HealthComponent>(target)) {
                        auto& hp = registry.get<HealthComponent>(target);
                        LOG_DEBUG("Applying {:.1f} damage to {} (Crit: {})", finalDamage, (uint32_t)target, isCrit);
                        hp.current -= finalDamage;
                        
                        // Death Logic
                        if (hp.current <= 0) {
                            LOG_INFO("Entity {} destroyed by player {}", (uint32_t)target, (uint32_t)entity);
                            registry.destroy(target);
                            
                            // Increment Player Kill Count
                            if (registry.all_of<PlayerStats>(entity)) {
                                registry.get<PlayerStats>(entity).killCount++;
                                LOG_TRACE("Player {} kill count: {}", (uint32_t)entity, registry.get<PlayerStats>(entity).killCount);
                            }
                        }

                        // --- 生成伤害飘字 ---
                        auto popupEntity = registry.create();
                        // 从目标位置稍微偏移一点
                        registry.emplace<Position>(popupEntity, tPos.x, tPos.y - 20.0f);
                        
                        DamagePopup popup;
                        popup.damage = finalDamage;
                        popup.timer = 0.0f;
                        popup.lifeTime = 0.8f;
                        popup.velX = (float)(GetRandomValue(-20, 20)); // 随机水平漂移
                        popup.velY = -100.0f; // 向上飘
                        
                        // Color coding
                        if (isCrit) {
                            popup.color = RED;
                            popup.lifeTime = 1.0f; // Crit lingers longer
                            // Could scale text size if supported by RenderSystem
                        } else {
                            popup.color = WHITE;
                        }
                        
                        registry.emplace<DamagePopup>(popupEntity, popup);
                    } else {
                        LOG_LIMITED_WARN(1.0f, "Target {} hit but has no HealthComponent", (uint32_t)target);
                        // For particles/props without health, maybe just destroy or knockback?
                        // For now, let's just knock them back hard.
                    }
                    }
                }
            });
        }
    }
}
