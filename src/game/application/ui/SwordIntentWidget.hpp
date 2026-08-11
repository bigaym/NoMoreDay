#pragma once
#include "game/foundation/data/BladeMasteryData.hpp"
#include "raylib.h"

#include <string_view>

namespace NoMoreDay::systems::ui {

// Instance widget for the blade-resource (sword intent) HUD widget.
//
// U7 cleanup: the legacy static mutable state (swordIcon, shineShader,
// glowIntensity) was migrated to instance members so the UI no longer keeps
// any static mutable rendering state (design invariant 4). The Resolve* pure
// helpers stay static: they are stateless and side-effect free.
class SwordIntentWidget {
public:
    SwordIntentWidget() = default;
    ~SwordIntentWidget() { Shutdown(); }

    SwordIntentWidget(const SwordIntentWidget&) = delete;
    SwordIntentWidget& operator=(const SwordIntentWidget&) = delete;

    static int ResolveThresholdTier(BladeResourceKind kind, int currentStacks,
                                    int maxStacks);
    static const char* ResolveThresholdText(BladeResourceKind kind,
                                            int currentStacks, int maxStacks);
    static int ResolveSwordFlowThresholdTier(int currentStacks, int maxStacks);
    static const char* ResolveSwordFlowThresholdText(int currentStacks, int maxStacks);

    void Draw(int currentStacks, int maxStacks, BladeResourceKind kind,
              std::string_view label = "Sword Intent",
              std::string_view detailText = "");

private:
    void Init();
    void Shutdown();

    Texture2D m_swordIcon = { 0 };
    Shader m_shineShader = { 0 };
    bool m_initialized = false;
    float m_glowIntensity = 0.0f;
};

}
