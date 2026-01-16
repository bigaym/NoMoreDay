#include "game/systems/vfx/SwordIntentVisualSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/vfx/SwordIntentVisualComponent.hpp"
#include "raymath.h"
#include <algorithm>
#include <string>

namespace NoMoreDay::systems {

void SwordIntentVisualSystem::Update(entt::registry &registry, float dt) {
  auto view = registry.view<SwordIntentComponent, Position>();

  for (auto entity : view) {
    const auto &intent = view.get<SwordIntentComponent>(entity);
    const auto &pos = view.get<Position>(entity);

    // Ensure visual component exists
    auto &visual =
        registry.get_or_emplace<components::SwordIntentVisual>(entity);

    // Sync level
    visual.currentLevel = intent.stacks;

    // Calculate target intensity based on level
    float targetIntensity = (float)intent.stacks / (float)intent.max_stacks;

    // Smoothly interp intensity
    visual.intensity = Lerp(visual.intensity, targetIntensity, dt * 5.0f);

    // Update pulse
    visual.pulseTime += dt * visual.pulseSpeed;
    float pulse = (sinf(visual.pulseTime) + 1.0f) * 0.5f; // [0, 1]

    // Emit particles if level > 0
    if (intent.stacks > 0 && visual.showAura) {
      auto &particleSys = GPUParticleSystem::Get();

      // Base aura density
      float density = 2.0f + intent.stacks * 3.0f;

      // We can't use ShouldTrigger here easily without a timer,
      // but we can spawn based on probability or a local timer.
      // For now, let's just do a simple probability check per frame.
      if ((float)GetRandomValue(0, 1000) < density * dt * 1000.0f) {
        components::GPUParticle p;

        // Spawn in a small area around feet
        p.position = {pos.x + (float)GetRandomValue(-15, 15),
                      pos.y + (float)GetRandomValue(-10, 5)};

        // Float upwards
        p.velocity = {(float)GetRandomValue(-10, 10),
                      -30.0f - (intent.stacks * 5.0f)};

        p.acceleration = {0, 5.0f}; // Slight gravity/air resistance

        // Color gets more intense/vibrant at higher stacks
        // Use central definition
        Color baseColor = NoMoreDay::Constants::Visuals::COLOR_BLADE_ASCENDANT;
        float alpha = 0.2f + (visual.intensity * 0.4f) + (pulse * 0.1f);
        p.color = ColorAlpha(baseColor, alpha);

        p.lifetime = 0.8f + (visual.intensity * 0.4f);
        p.maxLifetime = p.lifetime;
        p.scale = 1.0f + (visual.intensity * 2.0f);
        p.flags = 0; // Standard fade
        p.growthRate = -0.05f;

        particleSys.Emit(p);
      }

      // Max Stacks Gold Sparks
      if (intent.stacks >= intent.max_stacks) {
        if ((float)GetRandomValue(0, 1000) < 15.0f * dt * 1000.0f) {
          components::GPUParticle p;
          p.position = {pos.x + (float)GetRandomValue(-20, 20), pos.y - 15.0f};
          p.velocity = {(float)GetRandomValue(-40, 40),
                        (float)GetRandomValue(-80, -40)};
          p.color = GOLD;
          p.lifetime = 0.4f;
          p.maxLifetime = 0.4f;
          p.scale = 2.5f;
          p.flags = 2; // Spark/Glow type
          particleSys.Emit(p);
        }
      }
    }
  }
}

void SwordIntentVisualSystem::Render(entt::registry& registry) {
    static Shader auraShader = {0};
    static Texture2D auraNoise = {0};
    
    if (auraShader.id == 0) {
        if (FileExists("assets/shaders/vfx/aura.fs")) {
            auraShader = LoadShader("assets/shaders/vfx/aura.vs", "assets/shaders/vfx/aura.fs");
            auraNoise = LoadTexture("assets/textures/vfx/vfx_aura_noise.png");
            SetTextureFilter(auraNoise, TEXTURE_FILTER_BILINEAR);
        }
    }

    if (auraShader.id == 0) return;

    // Use Visual Component for smooth parameters
    auto view = registry.view<Position, components::SwordIntentVisual>();

    BeginShaderMode(auraShader);
    
    int timeLoc = GetShaderLocation(auraShader, "time");
    int intentLoc = GetShaderLocation(auraShader, "intensity");
    int colorLoc = GetShaderLocation(auraShader, "auraColor");
    int noiseLoc = GetShaderLocation(auraShader, "noiseTexture");
    
    SetShaderValueTexture(auraShader, noiseLoc, auraNoise);
    
    float time = (float)GetTime();
    SetShaderValue(auraShader, timeLoc, &time, SHADER_UNIFORM_FLOAT);
    
    for (auto entity : view) {
        const auto& pos = view.get<Position>(entity);
        const auto& visual = view.get<components::SwordIntentVisual>(entity);
        
        if (visual.intensity > 0.01f && visual.showAura) {
             float intensity = visual.intensity;
             // Overload effect logic
             if (visual.currentLevel >= 10) intensity = 1.5f; // Boost for max stacks

             SetShaderValue(auraShader, intentLoc, &intensity, SHADER_UNIFORM_FLOAT);
             
             // Use Constants::Visuals::COLOR_BLADE_ASCENDANT
             Vector4 colVec = ColorNormalize(NoMoreDay::Constants::Visuals::COLOR_BLADE_ASCENDANT);
             SetShaderValue(auraShader, colorLoc, &colVec, SHADER_UNIFORM_VEC4);
             
             // Draw Quad
             float size = 120.0f + intensity * 40.0f;
             Rectangle src = {0, 0, (float)auraNoise.width, (float)auraNoise.height};
             Rectangle dest = {pos.x, pos.y, size, size};
             Vector2 origin = {size/2, size/2};
             
             DrawTexturePro(auraNoise, src, dest, origin, 0.0f, WHITE);
        }
    }
    
    EndShaderMode();
}

} // namespace NoMoreDay::systems