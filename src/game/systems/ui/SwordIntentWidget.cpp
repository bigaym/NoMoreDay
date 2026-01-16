#include "SwordIntentWidget.hpp"
#include <string>
#include <algorithm>
#include <cmath>

namespace NoMoreDay::systems::ui {

Texture2D SwordIntentWidget::swordIcon = { 0 };
bool SwordIntentWidget::initialized = false;

void SwordIntentWidget::Init() {
    if (!initialized) {
        // Assume context is valid for GL loading
        if (IsWindowReady()) {
            swordIcon = LoadTexture("assets/textures/ui/ui_sword_icon.png");
            SetTextureFilter(swordIcon, TEXTURE_FILTER_BILINEAR);
            initialized = true;
        }
    }
}

void SwordIntentWidget::Draw(int currentStacks, int maxStacks) {
    if (!initialized) {
        Init();
        if (!initialized) return;
    }

    // Position: Bottom center, slightly above hotbar
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    
    // Config
    float spacing = 24.0f;
    float iconScale = 0.6f;
    
    float totalWidth = (maxStacks - 1) * spacing;
    float startX = screenW / 2.0f - totalWidth / 2.0f;
    float y = screenH - 160.0f; // Adjust based on HUD layout
    
    for (int i = 0; i < maxStacks; ++i) {
        float x = startX + i * spacing;
        
        bool isActive = i < currentStacks;
        Color color = isActive ? WHITE : Fade(GRAY, 0.3f);
        
        float scale = iconScale;
        if (isActive) {
             // Subtle pulse for active ones
             if (i == currentStacks - 1) {
                 scale *= 1.2f + 0.1f * sinf(GetTime() * 5.0f);
                 color = SKYBLUE;
             }
        }
        
        Rectangle source = { 0, 0, (float)swordIcon.width, (float)swordIcon.height };
        Rectangle dest = { x, y, (float)swordIcon.width * scale, (float)swordIcon.height * scale };
        Vector2 origin = { dest.width / 2.0f, dest.height / 2.0f };
        
        DrawTexturePro(swordIcon, source, dest, origin, 0.0f, color);
        
        // Max Stack Overload Effect
        if (currentStacks >= maxStacks) {
             // Extra glow pass
             Color glowColor = Fade(NoMoreDay::Constants::Visuals::COLOR_BLADE_ASCENDANT, 0.4f + 0.3f * sinf(GetTime() * 10.0f + i * 0.5f));
             DrawTexturePro(swordIcon, source, dest, origin, 0.0f, glowColor);
        }
    }
}

void SwordIntentWidget::Shutdown() {
    if (initialized) {
        UnloadTexture(swordIcon);
        initialized = false;
    }
}

}
