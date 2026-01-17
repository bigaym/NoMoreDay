#include "SwordIntentWidget.hpp"
#include <string>
#include <algorithm>
#include <cmath>

extern "C" {
#include "rlgl.h"
}

namespace NoMoreDay::systems::ui {

Texture2D SwordIntentWidget::swordIcon = { 0 };
Shader SwordIntentWidget::shineShader = { 0 };
bool SwordIntentWidget::initialized = false;
float SwordIntentWidget::glowIntensity = 0.0f;

void SwordIntentWidget::Init() {
    if (!initialized) {
        if (IsWindowReady()) {
            swordIcon = LoadTexture("assets/textures/ui/ui_sword_icon.png");
            SetTextureFilter(swordIcon, TEXTURE_FILTER_BILINEAR);
            
            if (FileExists("assets/shaders/vfx/ui_shine.fs")) {
                shineShader = LoadShader("assets/shaders/vfx/ui_shine.vs", "assets/shaders/vfx/ui_shine.fs");
            }
            
            initialized = true;
        }
    }
}

void SwordIntentWidget::Draw(int currentStacks, int maxStacks) {
    if (!initialized) {
        Init();
        if (!initialized) return;
    }

    float dt = GetFrameTime();
    float targetIntensity = (currentStacks >= maxStacks) ? 1.0f : 0.0f;
    glowIntensity = Lerp(glowIntensity, targetIntensity, dt * 3.0f);

    // Position: Bottom center, slightly above hotbar
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    
    // Config
    float spacing = 24.0f;
    float iconScale = 0.6f;
    
    float totalWidth = (maxStacks - 1) * spacing;
    float startX = screenW / 2.0f - totalWidth / 2.0f;
    float y = screenH - 160.0f; 
    
    // Draw base icons
    for (int i = 0; i < maxStacks; ++i) {
        float x = startX + i * spacing;
        bool isActive = i < currentStacks;
        Color color = isActive ? WHITE : Fade(GRAY, 0.3f);
        
        float scale = iconScale;
        if (isActive && i == currentStacks - 1) {
            scale *= 1.2f + 0.1f * sinf(GetTime() * 5.0f);
            color = (currentStacks >= maxStacks) ? GOLD : SKYBLUE;
        }
        
        Rectangle source = { 0, 0, (float)swordIcon.width, (float)swordIcon.height };
        Rectangle dest = { x, y, (float)swordIcon.width * scale, (float)swordIcon.height * scale };
        Vector2 origin = { dest.width / 2.0f, dest.height / 2.0f };
        
        // If at max stacks, we might want to use the shine shader for all active ones or just specific ones
        // Apply shine shader for the golden flow
        if (isActive && glowIntensity > 0.01f && shineShader.id != 0) {
            float time = (float)GetTime();
            int timeLoc = GetShaderLocation(shineShader, "time");
            int intentLoc = GetShaderLocation(shineShader, "intensity");
            
            SetShaderValue(shineShader, timeLoc, &time, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shineShader, intentLoc, &glowIntensity, SHADER_UNIFORM_FLOAT);
            
            BeginShaderMode(shineShader);
            DrawTexturePro(swordIcon, source, dest, origin, 0.0f, color);
            EndShaderMode();
        } else {
            DrawTexturePro(swordIcon, source, dest, origin, 0.0f, color);
        }
    }
}

void SwordIntentWidget::Shutdown() {
    if (initialized) {
        UnloadTexture(swordIcon);
        if (shineShader.id != 0) UnloadShader(shineShader);
        initialized = false;
    }
}

}
