#pragma once
#include "game/data/BladeMasteryData.hpp"
#include "raylib.h"

#include <string_view>

namespace NoMoreDay::systems::ui {

class SwordIntentWidget {
public:
    static void Init();
    static int ResolveThresholdTier(BladeResourceKind kind, int currentStacks,
                                    int maxStacks);
    static const char* ResolveThresholdText(BladeResourceKind kind,
                                            int currentStacks, int maxStacks);
    static int ResolveSwordFlowThresholdTier(int currentStacks, int maxStacks);
    static const char* ResolveSwordFlowThresholdText(int currentStacks, int maxStacks);
    static void Draw(int currentStacks, int maxStacks, BladeResourceKind kind,
                     std::string_view label = "Sword Intent",
                     std::string_view detailText = "");
    static void Shutdown();

private:
    static Texture2D swordIcon;
    static Shader shineShader;
    static bool initialized;
    static float glowIntensity;
};

}
