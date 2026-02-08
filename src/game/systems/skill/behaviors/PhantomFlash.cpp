#include "PhantomFlash.hpp"
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/PlayerState.hpp"
#include "game/data/SkillRegistry.hpp"
#include "raymath.h"

namespace NoMoreDay::skills {

void PhantomFlash::DoCast(entt::registry &registry, entt::entity owner,
                          SkillExecution &exec) {
  auto *pos = registry.try_get<Position>(owner);
  if (!pos)
    return;

  const auto *skillData = SkillRegistry::Get().GetSkill(9);
  float dashSpeed =
      skillData ? skillData->GetParam("dash_speed", 500.0f) : 500.0f;
  float dashDist = skillData ? skillData->GetParam("dash_dist", 50.0f) : 50.0f;

  // Dash backwards
  Vector2 dir =
      Vector2Normalize(Vector2Subtract({pos->x, pos->y}, exec.target_pos));

  if (auto *vel = registry.try_get<Velocity>(owner)) {
    vel->vx = dir.x * dashSpeed;
    vel->vy = dir.y * dashSpeed;
  }

  if (auto *dash = registry.try_get<DashComponent>(owner)) {
    dash->isDashing = true;
    dash->dashTimer = dashDist / dashSpeed;
    dash->dirX = dir.x;
    dash->dirY = dir.y;
    dash->dashSpeed = dashSpeed;
  }

  // VFX
  auto &particleSys = systems::GPUParticleSystem::Get();
  Vector2 startPos = {pos->x, pos->y};

  auto dashParticles = systems::InkEffectHelper::CreateDashEffect(
      startPos, dir, systems::InkEffectHelper::COLOR_SHADOW_CORE, dashDist, 20);
  particleSys.EmitBatch(dashParticles);

  for (int i = 0; i < 8; ++i) {
    Vector2 gVel = {(float)GetRandomValue(-80, 80),
                    (float)GetRandomValue(-80, 80)};
    particleSys.Emit(systems::InkEffectHelper::CreateSpark(
        startPos, gVel, systems::InkEffectHelper::COLOR_GOLD_CORE, 1.5f));
  }

  // Counter State
  auto &pf = registry.emplace_or_replace<PhantomFlashComponent>(owner);
  pf.counter_window = 0.5f;
  pf.triggered = false;

  LOG_INFO("Phantom Flash: Counter state active for entity {}",
           (uint32_t)owner);
}

bool PhantomFlash::Update(entt::registry &registry, entt::entity entity,
                          PhantomFlashComponent &pf, float dt) {
  pf.counter_window -= dt;
  if (pf.counter_window <= 0.0f || pf.triggered) {
    return true;
  }

  // Optional: Visual effect for "Counter Ready" state?
  if (GetRandomValue(0, 10) == 0) { // Low freq
    if (registry.all_of<Position>(entity)) {
      const auto &pos = registry.get<Position>(entity);
      auto &particleSys = systems::GPUParticleSystem::Get();
      components::GPUParticle p;
      p.position = {pos.x + GetRandomValue(-10, 10),
                    pos.y + GetRandomValue(-20, 0)};
      p.velocity = {0, -10};
      p.color = ColorAlpha(GRAY, 0.5f);
      p.lifetime = 0.3f;
      p.maxLifetime = 0.3f;
      p.scale = 1.0f;
      p.flags = 0;
      particleSys.Emit(p);
    }
  }

  return false;
}

REGISTER_SKILL_BEHAVIOR(PhantomFlash)

void RegisterPhantomFlash() {}

} // namespace NoMoreDay::skills
