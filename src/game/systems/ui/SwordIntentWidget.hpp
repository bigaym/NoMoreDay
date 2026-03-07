#pragma once
#include "raylib.h"

#include <string_view>

namespace NoMoreDay::systems::ui {

class SwordIntentWidget {
public:
    static void Init();
    static int ResolveSwordFlowThresholdTier(int currentStacks, int maxStacks);
    static const char* ResolveSwordFlowThresholdText(int currentStacks, int maxStacks);
    static void Draw(int currentStacks, int maxStacks,
                     std::string_view label = "Sword Intent");
    static void Shutdown();

private:
    static Texture2D swordIcon;
    static Shader shineShader;
    static bool initialized;
    static float glowIntensity;
};

}
