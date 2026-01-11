#include "SwordArray.hpp"
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Stats.hpp"
#include "game/components/AIComponent.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "core/logging/Logger.hpp"
#include "core/utils/FrameRateUtils.hpp"
#include "engine/physics/SpatialGrid.hpp"

namespace NoMoreDay::skills {

void SwordArray::Update(entt::registry& registry, entt::entity entity, SwordArrayComponent& array, float dt, const systems::SpatialHashGrid& grid) {
    const auto* pos = registry.try_get<Position>(entity);
    if (!pos) return;

    array.duration -= dt;
    if (array.duration <= 0.0f) {
        registry.destroy(entity);
        return;
    }

    // --- Continuous VFX: Sword Rain ---
    auto& particleSys = systems::GPUParticleSystem::Get();
    if (utils::FrameRateUtils::ShouldTrigger(15.0f, dt)) { 
        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        float dist = sqrtf((float)GetRandomValue(0, 1000) / 1000.0f) * array.radius;
        Vector2 dropPos = { pos->x + cosf(angle) * dist, pos->y + sinf(angle) * dist };
        
        components::GPUParticle p;
        p.position = { dropPos.x, dropPos.y - 100.0f }; 
        p.velocity = { 0, 800.0f }; 
        p.acceleration = { 0, 0 };
        p.color = array.is_empowered ? GOLD : ColorAlpha(SKYBLUE, 0.7f);
        p.lifetime = 0.125f; 
        p.maxLifetime = 0.125f;
        p.scale = 2.0f;
        p.flags = 2; 
        particleSys.Emit(p);

        auto splash = systems::InkEffectHelper::CreateInkSplash(dropPos, 4, 5.0f, 40.0f);
        for(auto& sp : splash) {
            sp.color = p.color;
            particleSys.Emit(sp);
        }
    }

    array.damage_timer -= dt;
    if (array.damage_timer <= 0.0f) {
        array.damage_timer = array.damage_interval; // Reset timer

        std::vector<components::GPUParticle> particles;
        int pCount = 60;
        
        for(int i=0; i<pCount; ++i) {
            float angle = (float)i / pCount * 2.0f * PI;
            Vector2 offset = { cosf(angle) * (array.radius * 0.9f), sinf(angle) * (array.radius * 0.9f) };
            Vector2 pPos = { pos->x + offset.x, pos->y + offset.y };
            
            components::GPUParticle p;
            p.position = pPos;
            p.velocity = { offset.x * 2.0f, offset.y * 2.0f }; 
            p.acceleration = { 0, 0 };
            p.color = systems::InkEffectHelper::COLOR_INK_DARK;
            p.lifetime = 0.4f;
            p.maxLifetime = 0.4f;
            p.scale = 1.5f; 
            p.flags = 13; 
            particles.push_back(p);
        }
        
         if (array.radius > 50.0f) { 
            for(int i=0; i<10; ++i) {
                float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                float dist = (float)GetRandomValue(0, (int)array.radius);
                Vector2 pPos = { pos->x + cosf(angle) * dist, pos->y + sinf(angle) * dist };
                
                if (array.is_empowered) {
                    auto p = systems::InkEffectHelper::CreateGoldParticle(pPos, {0,0}, 0.8f);
                    p.lifetime = 0.5f;
                    particleSys.Emit(p);
                } else {
                    particleSys.Emit(systems::InkEffectHelper::CreateInkTrail(pPos, {0,0}, 0.8f, 0.5f));
                }
            }
         }

        particleSys.EmitBatch(particles);

        std::vector<entt::entity> targets;
        grid.query(*pos, array.radius, [&](entt::entity target) {
            if (target == array.owner || target == entity) return;
            if (!registry.valid(target) || registry.all_of<::KilledTag>(target) || !registry.all_of<::EnemyTag, ::HealthComponent, ::Position>(target)) return;

            const auto& tPos = registry.get<::Position>(target);
            float dx = tPos.x - pos->x;
            float dy = tPos.y - pos->y;
            if (dx*dx + dy*dy <= array.radius * array.radius) {
                targets.push_back(target);
            }
        });

        if (!targets.empty()) {
            std::sort(targets.begin(), targets.end());
            targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

            DamagePool base;
            base.Add(Tag::Physical, 20.0f); // Default for ID 6

            DamagePipeline::CalculateBatch(registry, array.owner, targets, 6, base, Tag::Area | Tag::SwordSkill | Tag::Hit, entity);

            for (auto target_ent : targets) {
                if (array.has_slow) {
                    auto& effects = registry.get_or_emplace<ActiveEffectsComponent>(target_ent);
                    BuffEffect slow;
                    slow.id = "array_slow";
                    slow.name = "Sword Array Slow";
                    slow.type = BuffType::SpeedDown;
                    slow.duration = 1.0f;
                    slow.remaining = 1.0f;
                    slow.is_debuff = true;
                    
                    StatModifier m;
                    m.type = StatType::MoveSpeed;
                    m.mode = ModifierMode::PercentAdd;
                    m.value = -10.0f;
                    m.required_tags = Tag::None;
                    m.source = ModifierSource::Buff;
                    slow.modifiers.push_back(m);
                    
                    effects.AddOrRefresh(slow);
                }
                if (array.has_armor_shred) {
                    auto& effects = registry.get_or_emplace<ActiveEffectsComponent>(target_ent);
                    BuffEffect shred;
                    shred.id = "array_armor_shred";
                    shred.name = "Armor Shred";
                    shred.type = BuffType::DefenseDown;
                    shred.duration = 1.0f;
                    shred.remaining = 1.0f;
                    shred.is_debuff = true;
                    
                    StatModifier m;
                    m.type = StatType::Armor;
                    m.mode = ModifierMode::PercentAdd;
                    m.value = -5.0f;
                    m.required_tags = Tag::None;
                    m.source = ModifierSource::Buff;
                    shred.modifiers.push_back(m);
                    
                    effects.AddOrRefresh(shred);
                }
                if (array.has_execute) {
                    if (auto* hp = registry.try_get<HealthComponent>(target_ent)) {
                        if (hp->current / hp->max < 0.15f) {
                            CombatSystem::ApplyDamage(registry, target_ent, hp->max * 0.1f, array.owner, false, true);
                        }
                    }
                }
            }
        }
    }
}

void SwordArray::DoCast(entt::registry& registry, entt::entity owner, SkillExecution& exec) {
    auto array_ent = registry.create();
    registry.emplace<LocalLevelTag>(array_ent);
    registry.emplace<Position>(array_ent, exec.target_pos.x, exec.target_pos.y);
    registry.emplace<ColorComponent>(array_ent, PURPLE);
    
    auto& array = registry.emplace<SwordArrayComponent>(array_ent);
    array.owner = owner;
    array.duration = 5.0f;
    array.radius = 75.0f;
    array.is_empowered = exec.is_empowered;
    array.cast_id = exec.cast_id;

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

REGISTER_SKILL_BEHAVIOR(SwordArray)

void RegisterSwordArray() {}

} // namespace NoMoreDay::skills
