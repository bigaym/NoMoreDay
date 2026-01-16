#include "game/systems/vfx/TrailSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/vfx/MotionTrailComponent.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "raymath.h"
#include "rlgl.h"

namespace NoMoreDay::systems {

void TrailSystem::Update(entt::registry &registry, float dt) {
  auto view = registry.view<components::MotionTrail, Position>();

  for (auto entity : view) {
    auto &trail = view.get<components::MotionTrail>(entity);
    const auto &pos = view.get<Position>(entity);

    // 1. Update existing points (Mesh Only)
    // Particle trails don't use 'points', particles manage their own lifetime.
    if (!trail.useParticles) {
        for (auto it = trail.points.begin(); it != trail.points.end();) {
        it->timeAlive += dt;
        // Simple alpha fade based on lifetime
        it->alpha = 1.0f - (it->timeAlive / trail.lifetime);

        if (it->timeAlive >= trail.lifetime) {
            it = trail.points.erase(it);
        } else {
            ++it;
        }
        }
    }

    // 2. Add new point or emit particles
    if (trail.isActive) {
      if (trail.useParticles) {
        // --- Particle Flow Trail Logic ---
        trail.emitTimer += dt;
        auto& particleSys = GPUParticleSystem::Get();
        
        while (trail.emitTimer >= trail.emitInterval) {
            trail.emitTimer -= trail.emitInterval;
            
            // Interpolate position based on velocity could be better, but use current pos for simplicity
            // Apply Scatter (Width) - Random offset within [-Width/2, Width/2]
            float halfWidth = trail.maxWidth * 0.5f;
            Vector2 emitPos = { 
                pos.x + (float)GetRandomValue(-(int)halfWidth, (int)halfWidth), 
                pos.y + (float)GetRandomValue(-(int)halfWidth, (int)halfWidth) 
            };
            
            // Random offset for size/pos
            float sizeVar = trail.particleSize * GetRandomValue(-20, 20) / 100.0f;
            float lifeVar = trail.lifetime * GetRandomValue(-20, 20) / 100.0f;
            
            // 1. Core Particle (Bright Center)
            components::GPUParticle core;
            core.position = emitPos;
            core.velocity = { (float)GetRandomValue(-10, 10), (float)GetRandomValue(-10, 10) }; // Slight drift
            core.color = trail.coreColor;
            core.lifetime = trail.lifetime + lifeVar;
            core.maxLifetime = core.lifetime;
            core.scale = (trail.particleSize * 0.6f) + sizeVar; // Smaller core
            core.flags = 2; // Additive/Glow
            particleSys.Emit(core);
            
            // 2. Edge/Glow Particle (Dim Edge)
            components::GPUParticle glow;
            glow.position = emitPos;
            glow.velocity = { (float)GetRandomValue(-5, 5), (float)GetRandomValue(-5, 5) };
            glow.color = trail.color; // Base color (usually darker/faint)
            glow.lifetime = (trail.lifetime + lifeVar) * 1.2f; // Lasts slightly longer
            glow.maxLifetime = glow.lifetime;
            glow.scale = (trail.particleSize * 1.5f) + sizeVar; // Larger Glow
            glow.growthRate = -1.0f * glow.scale; // Shrink
            glow.flags = 2; 
            particleSys.Emit(glow);
        }
      } else {
        // --- Mesh Trail Logic ---
        bool shouldAdd = false;
        if (trail.points.empty()) {
            shouldAdd = true;
        } else {
            float dist =
                Vector2Distance(trail.points.back().position, {pos.x, pos.y});
            if (dist >= trail.minDistance) {
            shouldAdd = true;
            }
        }

        if (shouldAdd) {
            // Calculate angle based on movement if possible
            float angle = 0.0f;
            if (!trail.points.empty()) {
            Vector2 dir = Vector2Normalize(
                Vector2Subtract({pos.x, pos.y}, trail.points.back().position));
            angle = atan2f(dir.y, dir.x);
            } else {
            // Fallback angle if first point
            angle = 0.0f;
            }

            trail.points.push_back({{pos.x, pos.y}, 1.0f, 0.0f, angle});
        }
      }
    }
  }
}

void TrailSystem::Render(entt::registry &registry, Shader trailShader) {
  auto view = registry.view<const components::MotionTrail>();

  for (auto entity : view) {
    const auto &trail = view.get<const components::MotionTrail>(entity);
    if (trail.useParticles || trail.points.size() < 2)
      continue;

    BeginShaderMode(trailShader);

    // We use RL_TRIANGLES because RL_TRIANGLE_STRIP might not be
    // defined/supported in all rlgl versions
    rlBegin(RL_TRIANGLES);
    for (size_t i = 0; i < trail.points.size() - 1; ++i) {
      const auto &pA = trail.points[i];
      const auto &pB = trail.points[i + 1];

      float progressA = 1.0f - (pA.timeAlive / trail.lifetime);
      float progressB = 1.0f - (pB.timeAlive / trail.lifetime);

      float alphaA = pA.alpha;
      float alphaB = pB.alpha;

      float widthA = trail.maxWidth * progressA;
      float widthB = trail.maxWidth * progressB;

      Vector2 dirA = {cosf(pA.angle), sinf(pA.angle)};
      Vector2 normalA = {-dirA.y, dirA.x};
      Vector2 dirB = {cosf(pB.angle), sinf(pB.angle)};
      Vector2 normalB = {-dirB.y, dirB.x};

      Vector2 vL_A = {pA.position.x + normalA.x * widthA * 0.5f,
                      pA.position.y + normalA.y * widthA * 0.5f};
      Vector2 vR_A = {pA.position.x - normalA.x * widthA * 0.5f,
                      pA.position.y - normalA.y * widthA * 0.5f};
      Vector2 vL_B = {pB.position.x + normalB.x * widthB * 0.5f,
                      pB.position.y + normalB.y * widthB * 0.5f};
      Vector2 vR_B = {pB.position.x - normalB.x * widthB * 0.5f,
                      pB.position.y - normalB.y * widthB * 0.5f};

      float texU_A = (float)i / (trail.points.size() - 1);
      float texU_B = (float)(i + 1) / (trail.points.size() - 1);

      // Modulate alpha by trail color's original alpha to respect transparency
      float baseAlpha = (float)trail.color.a / 255.0f;
      unsigned char finalAlphaA = (unsigned char)(255.0f * baseAlpha * alphaA);
      unsigned char finalAlphaB = (unsigned char)(255.0f * baseAlpha * alphaB);

      // Triangle 1
      rlColor4ub(trail.color.r, trail.color.g, trail.color.b, finalAlphaA);
      rlTexCoord2f(texU_A, 0.0f);
      rlVertex2f(vL_A.x, vL_A.y);
      
      rlColor4ub(trail.color.r, trail.color.g, trail.color.b, finalAlphaA);
      rlTexCoord2f(texU_A, 1.0f);
      rlVertex2f(vR_A.x, vR_A.y);
      
      rlColor4ub(trail.color.r, trail.color.g, trail.color.b, finalAlphaB);
      rlTexCoord2f(texU_B, 0.0f);
      rlVertex2f(vL_B.x, vL_B.y);

      // Triangle 2
      rlColor4ub(trail.color.r, trail.color.g, trail.color.b, finalAlphaA);
      rlTexCoord2f(texU_A, 1.0f);
      rlVertex2f(vR_A.x, vR_A.y);
      
      rlColor4ub(trail.color.r, trail.color.g, trail.color.b, finalAlphaB);
      rlTexCoord2f(texU_B, 1.0f);
      rlVertex2f(vR_B.x, vR_B.y);
      
      rlColor4ub(trail.color.r, trail.color.g, trail.color.b, finalAlphaB);
      rlTexCoord2f(texU_B, 0.0f);
      rlVertex2f(vL_B.x, vL_B.y);
    }
    rlEnd();

    EndShaderMode();
  }
}

} // namespace NoMoreDay::systems
