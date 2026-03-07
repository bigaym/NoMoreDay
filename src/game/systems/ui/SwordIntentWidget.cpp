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

int SwordIntentWidget::ResolveThresholdTier(BladeResourceKind kind,
                                            int currentStacks, int maxStacks) {
    if (kind == BladeResourceKind::SwordIntent || maxStacks <= 0) {
        return 0;
    }
    if (currentStacks >= maxStacks) {
        return 3;
    }
    const int secondThreshold =
        (kind == BladeResourceKind::SpiritBladeTier) ? 7 : 8;
    if (currentStacks >= secondThreshold) {
        return 2;
    }
    if (currentStacks >= 5) {
        return 1;
    }
    return 0;
}

const char* SwordIntentWidget::ResolveThresholdText(BladeResourceKind kind,
                                                    int currentStacks,
                                                    int maxStacks) {
    switch (kind) {
    case BladeResourceKind::SwordFlow:
        return ResolveSwordFlowThresholdText(currentStacks, maxStacks);
    case BladeResourceKind::SpiritBladeTier:
        switch (ResolveThresholdTier(kind, currentStacks, maxStacks)) {
        case 1:
            return "灵剑成阵";
        case 2:
            return "万剑齐鸣";
        case 3:
            return "天剑待发";
        default:
            return "";
        }
    case BladeResourceKind::Bloodthirst:
        switch (ResolveThresholdTier(kind, currentStacks, maxStacks)) {
        case 1:
            return "血意沸腾";
        case 2:
            return "危险线";
        case 3:
            return "血海临界";
        default:
            return "";
        }
    case BladeResourceKind::SwordIntent:
    case BladeResourceKind::None:
    default:
        return "";
    }
}

int SwordIntentWidget::ResolveSwordFlowThresholdTier(int currentStacks, int maxStacks) {
    return ResolveThresholdTier(BladeResourceKind::SwordFlow, currentStacks,
                                maxStacks);
}

const char* SwordIntentWidget::ResolveSwordFlowThresholdText(int currentStacks, int maxStacks) {
    switch (ResolveSwordFlowThresholdTier(currentStacks, maxStacks)) {
    case 1:
        return "剑流·启";
    case 2:
        return "剑流·盛";
    case 3:
        return "满流";
    default:
        return "";
    }
}

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
                             BladeResourceKind kind, std::string_view label,
                             std::string_view detailText) {
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

    const char* thresholdText = ResolveThresholdText(kind, currentStacks, maxStacks);
    if (thresholdText[0] != '\0') {
        const int tier = ResolveThresholdTier(kind, currentStacks, maxStacks);
        const Color tierColor = (tier >= 3) ? GOLD : (tier == 2 ? SKYBLUE : Color{140, 255, 220, 255});
        DrawText(thresholdText,
                 static_cast<int>((UI_REF_WIDTH * 0.5f * scale) - (MeasureText(thresholdText, static_cast<int>(15 * scale)) * 0.5f)),
                 static_cast<int>((logicY - 44.0f) * scale),
                 static_cast<int>(15 * scale), Fade(tierColor, 0.95f));
    }

    if (!detailText.empty()) {
        const std::string detail(detailText);
        DrawText(detail.c_str(),
                 static_cast<int>((UI_REF_WIDTH * 0.5f * scale) -
                                  (MeasureText(detail.c_str(), static_cast<int>(14 * scale)) * 0.5f)),
                 static_cast<int>((logicY - 62.0f) * scale),
                 static_cast<int>(14 * scale), Fade(LIGHTGRAY, 0.9f));
    }
    
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
