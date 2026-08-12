#include "SwordIntentWidget.hpp"
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UiResourceIds.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace NoMoreDay::systems::ui {

namespace {
// Stable node id for the widget's draw commands (Hud layer).
inline constexpr UiId kSwordIntentWidgetNode =
    static_cast<UiId>(0x53EE5A2Du); // hashed "ui_sword_intent_widget"
inline constexpr UiColor kSwordIntentLabelColor{211, 211, 211, 255}; // LIGHTGRAY
inline constexpr UiColor kSwordIntentDetailColor{211, 211, 211, 255};
inline constexpr UiColor kSwordIntentTier1Color{140, 255, 220, 255};
inline constexpr UiColor kSwordIntentTier2Color{135, 206, 235, 255};  // SKYBLUE
inline constexpr UiColor kSwordIntentTier3Color{255, 215, 0, 255};    // GOLD
inline constexpr UiColor kSwordIntentActiveIconColor{255, 255, 255, 255};
inline constexpr UiColor kSwordIntentInactiveIconColor{128, 128, 128, 77};
inline constexpr UiColor kSwordIntentMaxIconColor{255, 215, 0, 255};
} // namespace

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

void SwordIntentWidget::Update(int currentStacks, int maxStacks,
                               BladeResourceKind kind, std::string_view label,
                               std::string_view detailText, float timeSeconds,
                               float deltaSeconds) {
    m_currentStacks = currentStacks;
    m_maxStacks = maxStacks;
    m_kind = kind;
    m_label = label;
    m_detailText = detailText;
    m_timeSeconds = timeSeconds;
    // Glow lerp (dt*3, same rate as the legacy raylib draw path).
    const float targetIntensity = (currentStacks >= maxStacks) ? 1.0f : 0.0f;
    m_glowIntensity = m_glowIntensity +
                      (targetIntensity - m_glowIntensity) *
                          std::min(1.0f, deltaSeconds * 3.0f);
}

void SwordIntentWidget::Paint(UiDrawList& drawList,
                              const UiViewport& viewport) const {
    if (m_maxStacks <= 0 || m_iconResourceId == kInvalidUiResourceId) {
        return;
    }
    (void)viewport; // Layout is in the fixed 2K reference logical space.

    const float spacing = 30.0f;
    const float iconScale = 0.75f;
    const float totalWidth = (m_maxStacks - 1) * spacing;
    const float startX = UI_REF_WIDTH / 2.0f - totalWidth / 2.0f;
    const float logicY = UI_REF_HEIGHT - 210.0f;

    // Text rows (centered through backend alignment).
    drawList.Text(UiDrawLayer::Hud, kSwordIntentWidgetNode,
                  m_label, {UI_REF_WIDTH * 0.5f, logicY - 24.0f}, 18.0f,
                  kSwordIntentLabelColor, kGlobalFontResourceId,
                  UiTextAlign::Center);

    const char* thresholdText =
        ResolveThresholdText(m_kind, m_currentStacks, m_maxStacks);
    if (thresholdText[0] != '\0') {
        const int tier =
            ResolveThresholdTier(m_kind, m_currentStacks, m_maxStacks);
        const UiColor tierColor = (tier >= 3)
                                      ? kSwordIntentTier3Color
                                      : (tier == 2 ? kSwordIntentTier2Color
                                                   : kSwordIntentTier1Color);
        drawList.Text(UiDrawLayer::Hud, kSwordIntentWidgetNode,
                      thresholdText, {UI_REF_WIDTH * 0.5f, logicY - 44.0f},
                      15.0f, tierColor, kGlobalFontResourceId,
                      UiTextAlign::Center);
    }

    if (!m_detailText.empty()) {
        drawList.Text(UiDrawLayer::Hud, kSwordIntentWidgetNode,
                      m_detailText, {UI_REF_WIDTH * 0.5f, logicY - 62.0f},
                      14.0f, kSwordIntentDetailColor, kGlobalFontResourceId,
                      UiTextAlign::Center);
    }

    // Stack icons: active icons full alpha, inactive faded; the last active
    // icon pulses (1.2 + 0.1*sin(time*5)) and tints gold/full vs blue.
    for (int i = 0; i < m_maxStacks; ++i) {
        const float lx = startX + i * spacing;
        const bool isActive = i < m_currentStacks;
        float finalScale = iconScale;
        UiColor color = isActive ? kSwordIntentActiveIconColor
                                 : kSwordIntentInactiveIconColor;
        if (isActive && i == m_currentStacks - 1) {
            finalScale *= 1.2f + 0.1f * std::sin(m_timeSeconds * 5.0f);
            color = (m_currentStacks >= m_maxStacks)
                        ? kSwordIntentMaxIconColor
                        : kSwordIntentActiveIconColor;
        }
        // Icon texture is 64x64 (ui_sword_icon.png); keep aspect ratio.
        const float iconSize = 64.0f * finalScale;
        UiRect dest;
        dest.origin = {lx - iconSize * 0.5f, logicY - iconSize * 0.5f};
        dest.size = {iconSize, iconSize};
        drawList.Image(UiDrawLayer::Hud, kSwordIntentWidgetNode, dest,
                       m_iconResourceId, color);
    }
}

} // namespace NoMoreDay::systems::ui