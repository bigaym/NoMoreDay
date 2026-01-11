/**
 * @file PhantomFlash.cpp
 * @brief 绝影闪 (ID 9) - 闪避反击技能行为实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/PlayerState.hpp"
#include "game/data/SkillRegistry.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "core/logging/Logger.hpp"
#include "raymath.h"

namespace NoMoreDay::skills {

struct PhantomFlash : SkillBehaviorBase<PhantomFlash> {
    static constexpr uint32_t kSkillId = 9;
    
    static void DoCast(entt::registry& registry, entt::entity owner, SkillExecution& exec) {
        auto* pos = registry.try_get<Position>(owner);
        if (!pos) return;

        const auto* skillData = SkillRegistry::Get().GetSkill(9);
        float dashSpeed = skillData ? skillData->GetParam("dash_speed", 500.0f) : 500.0f;
        float dashDist = skillData ? skillData->GetParam("dash_dist", 50.0f) : 50.0f;

        // Dash backwards
        Vector2 dir = Vector2Normalize(Vector2Subtract({pos->x, pos->y}, exec.target_pos));
        
        if (auto* vel = registry.try_get<Velocity>(owner)) {
            vel->vx = dir.x * dashSpeed;
            vel->vy = dir.y * dashSpeed;
        }
        
        if (auto* dash = registry.try_get<DashComponent>(owner)) {
            dash->isDashing = true;
            dash->dashTimer = dashDist / dashSpeed;
            dash->dirX = dir.x;
            dash->dirY = dir.y;
            dash->dashSpeed = dashSpeed;
        }

        // VFX
        auto& particleSys = systems::GPUParticleSystem::Get();
        Vector2 startPos = { pos->x, pos->y };
        
        auto dashParticles = systems::InkEffectHelper::CreateDashEffect(
            startPos, dir, systems::InkEffectHelper::COLOR_SHADOW_CORE, 
            dashDist, 20);
        particleSys.EmitBatch(dashParticles);
        
        for (int i = 0; i < 8; ++i) {
            Vector2 gVel = { (float)GetRandomValue(-80, 80), (float)GetRandomValue(-80, 80) };
            particleSys.Emit(systems::InkEffectHelper::CreateSpark(
                startPos, gVel, systems::InkEffectHelper::COLOR_GOLD_CORE, 1.5f));
        }

        // Counter State
        auto& pf = registry.emplace_or_replace<PhantomFlashComponent>(owner);
        pf.counter_window = 0.5f;
        pf.triggered = false;

        LOG_INFO("Phantom Flash: Counter state active for entity {}", (uint32_t)owner);
    }
};

REGISTER_SKILL_BEHAVIOR(PhantomFlash)

} // namespace NoMoreDay::skills
