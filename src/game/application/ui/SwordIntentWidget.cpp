#include "SwordIntentWidget.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UICommon.hpp"
#include <string>
#include <algorithm>
#include <cmath>

extern "C" {
  #include "rlgl.h"
}
#include "engine/render/GPUUtils.hpp"

namespace NoMoreDay::systems::ui {

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
    if (!m_initialized) {
        if (IsWindowReady()) {
            m_swordIcon = LoadTexture("assets/textures/ui/ui_sword_icon.png");
            SetTextureFilter(m_swordIcon, TEXTURE_FILTER_BILINEAR);
            
            if (FileExists("assets/shaders/vfx/ui_shine.fs")) {
                m_shineShader = NoMoreDay::utils::GPUUtils::LoadShaderLabeled(
                    "assets/shaders/vfx/ui_shine.vs", "assets/shaders/vfx/ui_shine.fs");
            }
            
            m_initialized = true;
        }
    }
}

void SwordIntentWidget::Draw(int currentStacks, int maxStacks,
                             BladeResourceKind kind, std::string_view label,
                             std::string_view detailText) {
    if (!m_initialized) {
        Init();
        if (!m_initialized) return;
    }

    float dt = GetFrameTime();
    float targetIntensity = (currentStacks >= maxStacks) ? 1.0f : 0.0f;
    m_glowIntensity = Lerp(m_glowIntensity, targetIntensity, dt * 3.0f);

    float scale = UISystem::GetScaleFactor();
    
    // Config
    float spacing = 30.0f;  // Increased logic spacing
    float iconScale = 0.75f; // Slightly larger for better visibility
    
    float totalWidth = (maxStacks - 1) * spacing;
    float startX = UI_REF_WIDTH / 2.0f - totalWidth / 2.0f;
    float logicY = UI_REF_HEIGHT - 210.0f; // Above the hotbar

    const Font uiFont = UISystem::GetFont();
    const auto measureTextWidth = [&](std::string_view text,
                                      const float fontSize) -> float {
        if (text.empty()) {
            return 0.0f;
        }
        if (IsFontValid(uiFont)) {
            return MeasureTextEx(uiFont, text.data(), fontSize * scale,
                                 1.0f * scale).x / scale;
        }
        return static_cast<float>(MeasureText(text.data(), static_cast<int>(fontSize * scale))) / scale;
    };

    const std::string labelText(label);
    const float labelWidth = measureTextWidth(labelText, 18.0f);
    UISystem::DrawTextUI(labelText.c_str(),
                         UI_REF_WIDTH * 0.5f - labelWidth * 0.5f,
                         logicY - 24.0f, 18.0f, LIGHTGRAY, 0.95f);

    const char* thresholdText = ResolveThresholdText(kind, currentStacks, maxStacks);
    if (thresholdText[0] != '\0') {
        const int tier = ResolveThresholdTier(kind, currentStacks, maxStacks);
        const Color tierColor = (tier >= 3) ? GOLD : (tier == 2 ? SKYBLUE : Color{140, 255, 220, 255});
        const float thresholdWidth = measureTextWidth(thresholdText, 15.0f);
        UISystem::DrawTextUI(thresholdText,
                             UI_REF_WIDTH * 0.5f - thresholdWidth * 0.5f,
                             logicY - 44.0f, 15.0f, tierColor, 0.95f);
    }

    if (!detailText.empty()) {
        const std::string detail(detailText);
        const float detailWidth = measureTextWidth(detail, 14.0f);
        UISystem::DrawTextUI(detail.c_str(),
                             UI_REF_WIDTH * 0.5f - detailWidth * 0.5f,
                             logicY - 62.0f, 14.0f, LIGHTGRAY, 0.9f);
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
        Rectangle source = { 0, 0, (float)m_swordIcon.width, (float)m_swordIcon.height };
        Rectangle dest = { lx * scale, ly * scale, (float)m_swordIcon.width * sScale, (float)m_swordIcon.height * sScale };
        Vector2 origin = { dest.width / 2.0f, dest.height / 2.0f };
        
        if (isActive && m_glowIntensity > 0.01f && m_shineShader.id != 0) {
            float time = (float)GetTime();
            int timeLoc = GetShaderLocation(m_shineShader, "time");
            int intentLoc = GetShaderLocation(m_shineShader, "intensity");
            
            SetShaderValue(m_shineShader, timeLoc, &time, SHADER_UNIFORM_FLOAT);
            SetShaderValue(m_shineShader, intentLoc, &m_glowIntensity, SHADER_UNIFORM_FLOAT);
            
            BeginShaderMode(m_shineShader);
            DrawTexturePro(m_swordIcon, source, dest, origin, 0.0f, color);
            EndShaderMode();
        } else {
            DrawTexturePro(m_swordIcon, source, dest, origin, 0.0f, color);
        }
    }
}

void SwordIntentWidget::Shutdown() {
    if (m_initialized) {
        UnloadTexture(m_swordIcon);
        if (m_shineShader.id != 0) UnloadShader(m_shineShader);
        m_initialized = false;
    }
}

}
