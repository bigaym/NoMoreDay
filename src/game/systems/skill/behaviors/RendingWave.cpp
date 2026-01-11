/**
 * @file RendingWave.cpp
 * @brief 裂空斩 (ID 2) - 投射物技能行为实现
 * 
 * 发射扇形剑气波，可穿透敌人。
 * 
 * 天赋分支:
 * - 210 分海: 额外投射物
 * - 220 翻天: 回旋镖效果
 * - 230 剑意增幅: 剑意层数增伤
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/Projectile.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "core/logging/Logger.hpp"
#include "raymath.h"

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

        // Spirit Sword adjustment
        if (registry.any_of<SpiritSwordTag>(owner)) {
            baseRadius *= 0.5f;
            baseLifetime *= 0.75f;
            LOG_INFO("Spirit Sword Rending Wave: Radius halved, Lifetime reduced to 75%.");
        }

        Vector2 baseDir = Vector2Normalize(Vector2Subtract(exec.target_pos, {pos->x, pos->y}));

        // --- TALENT BRANCH LOGIC ---
        int extraWaves = 0;
        bool boomerang = false;
        float moreDamageFromIntent = 0.0f;

        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 2) {
                    // Talent: Fen Hai (分海) - ID 210
                    if (spec.allocated_points.contains(210)) {
                        extraWaves = spec.allocated_points.at(210);
                    }

                    // Talent: Fan Tian (翻天) - ID 220
                    if (spec.allocated_points.contains(220) && spec.allocated_points.at(220) > 0) {
                        boomerang = true;
                    }

                    // Talent: Sword Intent Scaling - ID 230
                    if (spec.allocated_points.contains(230)) {
                        if (auto* intent = registry.try_get<SwordIntentComponent>(owner)) {
                            moreDamageFromIntent = intent->stacks * 0.05f * spec.allocated_points.at(230);
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

        for (int i = 0; i < totalCount; ++i) {
            float angle = startAngle + i * angleStep;
            Vector2 dir = Vector2Rotate(baseDir, angle);
            
            // --- VISUAL EFFECTS ---
            auto& particleSys = systems::GPUParticleSystem::Get();
            Color coreColor = exec.is_empowered ? systems::InkEffectHelper::COLOR_GOLD_CORE 
                                                : systems::InkEffectHelper::COLOR_SWORD_QI;
            Color glowColor = exec.is_empowered ? systems::InkEffectHelper::COLOR_GOLD_GLOW 
                                                : systems::InkEffectHelper::COLOR_FROST_LIGHT;
            auto trailParticles = systems::InkEffectHelper::CreateProjectileTrail(
                {pos->x, pos->y}, dir, coreColor, glowColor, 25.0f, 4);
            particleSys.EmitBatch(trailParticles);

            auto proj_ent = registry.create();
            registry.emplace<LocalLevelTag>(proj_ent);
            registry.emplace<Position>(proj_ent, pos->x, pos->y);
            registry.emplace<Velocity>(proj_ent, dir.x * baseSpeed, dir.y * baseSpeed);
            registry.emplace<ColorComponent>(proj_ent, exec.is_empowered ? GOLD : WHITE);
            
            auto& proj = registry.emplace<Projectile>(proj_ent);
            proj.owner = owner;
            proj.speed = baseSpeed;
            proj.lifeTime = boomerang ? 2.0f : baseLifetime;
            proj.radius = exec.is_empowered ? baseRadius * 1.7f : baseRadius;
            proj.pierce = true;
            proj.pierceCount = 99;
            proj.snapshot = *stats;

            // Apply Intent Scaling
            if (moreDamageFromIntent > 0.0f) {
                for (auto& m : proj.snapshot.damage_multipliers) m *= (1.0f + moreDamageFromIntent);
            }

            if (exec.is_empowered) {
                for (auto& mult : proj.snapshot.damage_multipliers) mult *= 1.5f;
            }
            registry.emplace<CombatStats>(proj_ent, proj.snapshot);
            registry.emplace<SkillComponent>(proj_ent, exec.skill_id, owner);

            if (boomerang) {
                auto& bc = registry.emplace<BoomerangComponent>(proj_ent);
                bc.owner = owner;
                bc.returnTimer = 0.5f;
                bc.phase = BoomerangComponent::Outward;
                bc.returnSpeed = proj.speed * 1.2f;
            }
        }

        LOG_INFO("Rending Wave fired {} projectiles from entity {}", totalCount, (uint32_t)owner);
    }
};

REGISTER_SKILL_BEHAVIOR(RendingWave)

} // namespace NoMoreDay::skills
