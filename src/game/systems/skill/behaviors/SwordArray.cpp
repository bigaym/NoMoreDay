/**
 * @file SwordArray.cpp
 * @brief 诛仙剑阵 (ID 6) - 范围技能行为实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "core/logging/Logger.hpp"

namespace NoMoreDay::skills {

struct SwordArray : SkillBehaviorBase<SwordArray> {
    static constexpr uint32_t kSkillId = 6;
    
    static void DoCast(entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto array_ent = registry.create();
        registry.emplace<LocalLevelTag>(array_ent);
        registry.emplace<Position>(array_ent, exec.target_pos.x, exec.target_pos.y);
        registry.emplace<ColorComponent>(array_ent, PURPLE);
        
        auto& array = registry.emplace<SwordArrayComponent>(array_ent);
        array.owner = owner;
        array.duration = 5.0f;
        array.radius = 75.0f;
        array.is_empowered = exec.is_empowered;

        auto& ve = registry.emplace<VisualEffect>(array_ent);
        ve.type = VisualEffectType::AoeArray;
        ve.lifeTime = array.duration;
        ve.color = exec.is_empowered ? GOLD : PURPLE;
        
        auto& ae = registry.emplace<ArrayEffect>(array_ent);
        ae.radius = array.radius;
        ae.thickness = 0.1f;
        ae.color = ve.color;

        auto& particleSys = systems::GPUParticleSystem::Get();
        
        Color coreColor = exec.is_empowered ? systems::InkEffectHelper::COLOR_GOLD_CORE 
                                             : systems::InkEffectHelper::COLOR_SHADOW_CORE;
        Color edgeColor = exec.is_empowered ? systems::InkEffectHelper::COLOR_GOLD_GLOW 
                                             : systems::InkEffectHelper::COLOR_SHADOW_GLOW;
        auto areaParticles = systems::InkEffectHelper::CreateAreaEffect(
            exec.target_pos, array.radius, coreColor, edgeColor, 30, 1.0f);
        particleSys.EmitBatch(areaParticles);
        
        int ringCount = 25;
        for (int i = 0; i < ringCount; ++i) {
            float angle = (float)i / ringCount * 2.0f * PI;
            float r = array.radius + (float)GetRandomValue(-5, 5);
            Vector2 pPos = { exec.target_pos.x + cosf(angle) * r, 
                            exec.target_pos.y + sinf(angle) * r };
            
            Vector2 tangent = { -sinf(angle) * 15.0f, cosf(angle) * 15.0f };
            particleSys.Emit(systems::InkEffectHelper::CreateSpark(
                pPos, tangent, systems::InkEffectHelper::COLOR_INK_LIGHT, 1.0f));
        }

        if (exec.is_empowered) {
            array.radius *= 1.5f;
            array.damage_interval *= 0.6f;
            LOG_INFO("Empowered Sword Array: 1.5x Radius and faster damage pulses!");
            
            for (int i = 0; i < 15; ++i) {
                float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                float r = array.radius * 0.8f + (float)GetRandomValue(0, (int)(array.radius * 0.4f));
                Vector2 pPos = { exec.target_pos.x + cosf(angle) * r, 
                                exec.target_pos.y + sinf(angle) * r };
                particleSys.Emit(systems::InkEffectHelper::CreateSpark(
                    pPos, {0, -30.0f}, systems::InkEffectHelper::COLOR_GOLD_CORE, 2.0f));
            }
        }
        
        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 6) {
                    if (spec.allocated_points.contains(610) && spec.allocated_points.at(610) > 0) {
                        array.has_slow = true;
                    }
                    if (spec.allocated_points.contains(611) && spec.allocated_points.at(611) > 0) {
                        array.has_armor_shred = true;
                    }
                    if (spec.allocated_points.contains(612) && spec.allocated_points.at(612) > 0) {
                        array.has_execute = true;
                    }
                    break;
                }
            }
        }

        registry.emplace<SkillComponent>(array_ent, 6u, owner);
        LOG_INFO("Sword Array summoned at ({}, {}) by entity {}", exec.target_pos.x, exec.target_pos.y, (uint32_t)owner);
    }
};

REGISTER_SKILL_BEHAVIOR(SwordArray)

} // namespace NoMoreDay::skills
