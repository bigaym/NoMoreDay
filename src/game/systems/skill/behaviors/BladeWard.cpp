/**
 * @file BladeWard.cpp
 * @brief 剑气护体 (ID 4) - 防御技能行为实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/Buff.hpp"
#include "game/components/EffectComponent.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "core/logging/Logger.hpp"

namespace NoMoreDay::skills {

struct BladeWard : SkillBehaviorBase<BladeWard> {
    static constexpr uint32_t kSkillId = 4;
    
    static void DoCast(entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto& active_effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
        
        float phys_dr = 10.0f;
        
        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 4) {
                    if (spec.allocated_points.contains(400)) {
                        phys_dr += spec.allocated_points.at(400) * 5.0f;
                    }
                    
                    float elemental_res = 0.0f;
                    if (spec.allocated_points.contains(401)) {
                        elemental_res = spec.allocated_points.at(401) * 3.0f;
                    }
                    
                    float block_inc = 0.0f;
                    if (spec.allocated_points.contains(410)) {
                        block_inc = spec.allocated_points.at(410) * 5.0f;
                    }

                    BuffEffect ward_buff;
                    ward_buff.id = "blade_ward";
                    ward_buff.name = "Blade Ward";
                    ward_buff.type = BuffType::Shield;
                    ward_buff.duration = 10.0f;
                    ward_buff.remaining = 10.0f;
                    
                    ward_buff.modifiers.push_back({StatType::ResistPhysical, ModifierMode::Flat, phys_dr});
                    if (elemental_res > 0.0f) {
                        ward_buff.modifiers.push_back({StatType::ResistFire, ModifierMode::Flat, elemental_res});
                        ward_buff.modifiers.push_back({StatType::ResistCold, ModifierMode::Flat, elemental_res});
                        ward_buff.modifiers.push_back({StatType::ResistLightning, ModifierMode::Flat, elemental_res});
                    }
                    if (block_inc > 0.0f) {
                        ward_buff.modifiers.push_back({StatType::BlockChance, ModifierMode::Flat, block_inc});
                    }
                    
                    active_effects.AddOrRefresh(ward_buff);
                    break;
                }
            }
        }
        
        auto& ward = registry.emplace_or_replace<BladeWardComponent>(owner);
        ward.remaining = 10.0f;
        ward.sword_count = 3;
        ward.is_solidified = false;

        if (auto* active = registry.try_get<ActiveSkillsComponent>(owner)) {
            for (const auto& spec : active->specialized_slots) {
                if (spec.skill_id == 4) {
                    if (spec.allocated_points.contains(122) && spec.allocated_points.at(122) > 0) {
                        ward.is_solidified = true;
                    }
                    break;
                }
            }
        }

        // VFX
        auto* pos = registry.try_get<Position>(owner);
        if (pos) {
            auto& particleSys = systems::GPUParticleSystem::Get();
            for (int s = 0; s < 3; ++s) {
                for (int i = 0; i < 20; ++i) {
                    float t = (float)i / 20.0f;
                    float angle = t * 4.0f * PI + (s * 2.0f * PI / 3);
                    float height = t * 60.0f;
                    float radius = 40.0f * (1.0f - t * 0.5f);
                    
                    Vector2 pPos = { pos->x + cosf(angle) * radius, pos->y + sinf(angle) * radius - height + 30.0f };
                    
                    components::GPUParticle p;
                    p.position = pPos;
                    p.velocity = { 0, -20.0f };
                    p.acceleration = { 0, 0 };
                    p.color = ColorAlpha(SKYBLUE, 0.5f);
                    p.lifetime = 1.0f;
                    p.maxLifetime = 1.0f;
                    p.scale = 1.5f;
                    p.flags = 13;
                    particleSys.Emit(p);
                }
            }
        }

        if (exec.is_empowered) {
            ward.sword_count += 3;
            ward.interception_chance *= 2.0f;
            LOG_INFO("Empowered Blade Ward: +3 swords and 2x interception chance!");
        }
        
        registry.get_or_emplace<StatsDirty>(owner);
        LOG_INFO("Blade Ward activated for entity {}", (uint32_t)owner);
    }
};

REGISTER_SKILL_BEHAVIOR(BladeWard)

void RegisterBladeWard() {}

} // namespace NoMoreDay::skills
