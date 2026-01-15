/**
 * @file RendingWave.cpp
 * @brief 裂空斩 (ID 2) - 投射物技能行为实现
 * 
 * 发射扇形剑气波，可穿透敌人。
 * 
 * 天赋分支:
 * - 210 多重剑气: 额外投射物
 * - 230 回旋劲: 回旋镖效果
 * - 250 灵力转化: 物理转虚空
 * - 252 剑意爆发: 满层爆发
 * - 253 剑意汲取: 命中回剑意
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/Projectile.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "core/logging/Logger.hpp"
#include "raymath.h"
#include "game/components/SkillDefs.hpp"

namespace NoMoreDay::skills {

struct RendingWave : SkillBehaviorBase<RendingWave> {
    static constexpr uint32_t kSkillId = 2;
    
    static void DoCast(entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto* pos = registry.try_get<Position>(owner);
        auto* stats = registry.try_get<CombatStats>(owner);
        if (!pos || !stats) return;

        const auto* skillData = SkillRegistry::Get().GetSkill(exec.skill_id);
        Tag skillTags = skillData ? skillData->tags : Tag::None;
        float baseSpeed = skillData ? skillData->GetParam("speed", 300.0f) : 300.0f;
        float baseRadius = skillData ? skillData->GetParam("radius", 35.0f) : 35.0f;
        float baseLifetime = skillData ? skillData->GetParam("lifetime", 1.2f) : 1.2f;

        // Apply Stats (Area of Effect, Projectile Speed)
        // GetStatWithTags returns values like 100.0f for 1.0 (100%)
        float areaStat = StatsSystem::GetStatWithTags(registry, owner, StatType::AreaScale, skillTags, exec.skill_id);
        float speedStat = StatsSystem::GetStatWithTags(registry, owner, StatType::ProjectileSpeed, skillTags, exec.skill_id);
        
        float areaScale = (areaStat > 0.1f) ? areaStat / 100.0f : 1.0f;
        float speedScale = (speedStat > 0.1f) ? speedStat / 100.0f : 1.0f;
        
        // Clamp scales to sane values to prevent "all screen" or "frozen" projectiles
        areaScale = std::clamp(areaScale, 0.1f, 5.0f);
        speedScale = std::clamp(speedScale, 0.1f, 10.0f);
        
        baseRadius *= areaScale;
        baseSpeed *= speedScale;
        if (baseSpeed < 1.0f) baseSpeed = 1.0f; // Prevent division by zero

        // Spirit Sword adjustment
        if (registry.any_of<SpiritSwordTag>(owner)) {
            baseRadius *= 0.5f;
            baseLifetime *= 0.75f;
            LOG_INFO("Spirit Sword Rending Wave: Radius halved, Lifetime reduced to 75%.");
        }

        Vector2 baseDir = Vector2Normalize(Vector2Subtract(exec.target_pos, {pos->x, pos->y}));

        // --- TALENT BRANCH LOGIC ---
        int extraWaves = 0;
        float damagePenalty = 1.0f;
        bool boomerang = false;
        bool isVoid = false;
        float talentWidthBonus = 0.0f;

        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == kSkillId) {
                    // Talent: Jian Qi Zong Heng (剑气纵横) - ID 200
                    // Increases width and range (radius)
                    if (spec.allocated_points.contains(200)) {
                        int pts = spec.allocated_points.at(200);
                        baseRadius *= (1.0f + pts * 0.1f);
                        talentWidthBonus = pts * 10.0f; // +10 deg per point
                    }

                    // Talent: Duo Zhong Jian Qi (多重剑气) - ID 210
                    if (spec.allocated_points.contains(210)) {
                        int pts = spec.allocated_points.at(210);
                        extraWaves = pts;
                        static constexpr float penalties[] = { 1.0f, 0.75f, 0.8f, 0.85f };
                        damagePenalty = penalties[std::min(pts, 3)];
                    }

                    // Talent: Hui Xuan Jin (回旋劲) - ID 230
                    if (spec.allocated_points.contains(230) && spec.allocated_points.at(230) > 0) {
                        boomerang = true;
                    }

                    // Talent: Ling Li Zhuan Hua (灵力转化) - ID 250
                    if (spec.allocated_points.contains(250) && spec.allocated_points.at(250) > 0) {
                        isVoid = true;
                    }

                    // Talent: Jian Yi Bao Fa (剑意爆发) - ID 252
                    if (spec.allocated_points.contains(252) && spec.allocated_points.at(252) > 0) {
                        if (auto* intent = registry.try_get<SwordIntentComponent>(owner)) {
                            if (intent->stacks >= 10) {
                                exec.is_empowered = true;
                                intent->stacks = 0;
                                LOG_INFO("Sword Intent Burst (252) triggered for Rending Wave!");

                                // Dispatch Event for Legendary Affixes (e.g. Blade Resonance)
                                CombatEventDispatcher::Dispatch(registry, CombatEventFactory::CreateResourceConsumed(
                                    owner, Tag::SwordSkill, 10.0f, kSkillId));
                            }
                        }
                    }
                    break;
                }
            }
        }

        int totalCount = (int)StatsSystem::GetStatWithTags(registry, owner, StatType::ProjectileCount, skillTags, exec.skill_id);
        if (totalCount < 1) totalCount = 1;
        totalCount += extraWaves;

        if (exec.is_empowered) {
            totalCount *= 2;
            LOG_INFO("Empowered Rending Wave: Double projectiles!");
            RenderSystem::AddScreenShake(0.2f);
        }

        float spread = 0.4f + (totalCount * 0.05f);
        float startAngle = (totalCount > 1) ? -spread / 2.0f : 0.0f;
        float angleStep = totalCount > 1 ? spread / (totalCount - 1) : 0.0f;

        // Fix UAF: Copy component data to local variables before creating new entities
        Position ownerPos = *pos;
        CombatStats ownerStats = *stats;

        for (int i = 0; i < totalCount; ++i) {
            float angle = startAngle + i * angleStep;
            Vector2 dir = Vector2Rotate(baseDir, angle);
            
            // --- VISUAL EFFECTS ---
            auto& particleSys = systems::GPUParticleSystem::Get();
            Color coreColor = exec.is_empowered ? systems::InkEffectHelper::COLOR_GOLD_CORE 
                                                : systems::InkEffectHelper::COLOR_SWORD_QI;
            Color glowColor = exec.is_empowered ? systems::InkEffectHelper::COLOR_GOLD_GLOW 
                                                : (isVoid ? PURPLE : systems::InkEffectHelper::COLOR_FROST_LIGHT);
            
            // Optimization: Use thread-local buffer to avoid heap allocation per projectile
            static thread_local std::vector<components::GPUParticle> s_trailBuffer;
            s_trailBuffer.clear();
            s_trailBuffer.reserve(32); // Pre-reserve enough space
            
            systems::InkEffectHelper::AppendProjectileTrail(
                s_trailBuffer, {ownerPos.x, ownerPos.y}, dir, coreColor, glowColor, 25.0f, 4);
            particleSys.EmitBatch(s_trailBuffer);

            auto proj_ent = registry.create();
            registry.emplace<LocalLevelTag>(proj_ent);
            registry.emplace<Position>(proj_ent, ownerPos.x, ownerPos.y);
            registry.emplace<Velocity>(proj_ent, dir.x * baseSpeed, dir.y * baseSpeed);
            registry.emplace<ColorComponent>(proj_ent, isVoid ? PURPLE : (exec.is_empowered ? GOLD : WHITE));
            
            auto& proj = registry.emplace<Projectile>(proj_ent);
            proj.owner = owner;
            proj.cast_id = exec.cast_id;
            proj.speed = baseSpeed;
            proj.lifeTime = boomerang ? (400.0f / baseSpeed) * 2.0f + 0.5f : baseLifetime; // Travel 400 yards and back
            proj.radius = exec.is_empowered ? baseRadius * 1.7f : baseRadius;
            proj.arcWidth = 60.0f + talentWidthBonus;
            if (exec.is_empowered) proj.arcWidth *= 1.3f;
            
            proj.visualType = 0; // Fan/Sector
            
            proj.pierce = true;
            proj.pierceCount = 99;
            proj.snapshot = ownerStats;

            // Apply Penalties and Empowerment
            for (auto& mult : proj.snapshot.damage_multipliers) {
                mult *= damagePenalty;
                if (exec.is_empowered) mult *= 2.0f; // 252: Guaranteed crit handled in pipeline, but we add more dmg here
            }

            if (exec.is_empowered) {
                proj.snapshot.crit_chance += 100.0f; // Force Crit
                proj.snapshot.crit_damage += 1.0f; // +100% Crit Damage
            }

            registry.emplace<CombatStats>(proj_ent, proj.snapshot);
            registry.emplace<SkillComponent>(proj_ent, exec.skill_id, owner);

            if (isVoid) {
                auto& mods = registry.emplace<SkillModifierComponent>(proj_ent);
                mods.damage_modifiers.push_back({Tag::Physical, Tag::Void, 1.0f, ModifierType::Convert});
            }

            if (boomerang) {
                auto& bc = registry.emplace<BoomerangComponent>(proj_ent);
                bc.owner = owner;
                bc.returnTimer = 400.0f / baseSpeed; // Turn back after 400 yards
                bc.phase = BoomerangComponent::Outward;
                bc.returnSpeed = proj.speed * 1.2f;
            }
        }

        LOG_INFO("Rending Wave fired {} projectiles from entity {}", totalCount, (uint32_t)owner);
    }

    static void DoHit(entt::registry& registry, entt::entity attacker, entt::entity target, Tag hit_tags, bool is_crit) {
        if (auto* active = registry.try_get<ActiveSkillsComponent>(attacker)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == kSkillId) {
                    // Talent: Jian Yi Ji Qu (剑意汲取) - ID 253
                    if (spec.allocated_points.contains(253)) {
                        int pts = spec.allocated_points.at(253);
                        if (pts > 0 && GetRandomValue(0, 100) < 10 * pts) {
                             if (auto* intent = registry.try_get<SwordIntentComponent>(attacker)) {
                                 if (intent->stacks < intent->max_stacks) {
                                     intent->stacks++;
                                     intent->time_since_last_gain = 0.0f;
                                     intent->decay_tick_timer = 0.0f;
                                     LOG_DEBUG("Rending Wave (253): Gained Intent via Hit");
                                 }
                             }
                        }
                    }
                    break;
                }
            }
        }
    }
};

REGISTER_SKILL_BEHAVIOR(RendingWave)

void RegisterRendingWave() {}

} // namespace NoMoreDay::skills
