#include "SwordIntentWidget.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/ui/UICommon.hpp"
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

void SwordIntentWidget::Draw(int currentStacks, int maxStacks,
                             std::string_view label) {
    if (!initialized) {
        Init();
        if (!initialized) return;
    }

    float dt = GetFrameTime();
    float targetIntensity = (currentStacks >= maxStacks) ? 1.0f : 0.0f;
    glowIntensity = Lerp(glowIntensity, targetIntensity, dt * 3.0f);

    float scale = UISystem::State.scaleFactor;
    
    // Config
    float spacing = 30.0f;  // Increased logic spacing
    float iconScale = 0.75f; // Slightly larger for better visibility
    
    float totalWidth = (maxStacks - 1) * spacing;
    float startX = UI_REF_WIDTH / 2.0f - totalWidth / 2.0f;
    float logicY = UI_REF_HEIGHT - 210.0f; // Above the hotbar

    const std::string labelText(label);
    const int labelWidth = MeasureText(labelText.c_str(), static_cast<int>(18 * scale));
    DrawText(labelText.c_str(),
             static_cast<int>((UI_REF_WIDTH * 0.5f * scale) - (labelWidth * 0.5f)),
             static_cast<int>((logicY - 24.0f) * scale),
             static_cast<int>(18 * scale), Fade(LIGHTGRAY, 0.95f));
    
    // Draw base icons
    for (int i = 0; i < maxStacks; ++i) {
        float lx = startX + i * spacing;
        float ly = logicY;
        
        bool isActive = i < currentStacks;
        Color color = isActive ? WHITE : Fade(GRAY, 0.3f);
        
        float finalScale = iconScale;
        if (isActive && i == currentStacks - 1) {
            finalScale *= 1.2f + 0.1f * sinf(GetTime() * 5.0f);
            color = (currentStacks >= maxStacks) ? GOLD : SKYBLUE;
        }
        
        float sScale = finalScale * scale;
        Rectangle source = { 0, 0, (float)swordIcon.width, (float)swordIcon.height };
        Rectangle dest = { lx * scale, ly * scale, (float)swordIcon.width * sScale, (float)swordIcon.height * sScale };
        Vector2 origin = { dest.width / 2.0f, dest.height / 2.0f };
        
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
