#pragma once
#include "raylib.h"

namespace NoMoreDay::components {

struct VisualGhost {
    Texture2D texture;
    Rectangle source;
    float alpha = 1.0f;
    float fadeSpeed = 3.0f;
    Color color = WHITE;
    float scale = 1.0f;
    float rotation = 0.0f;
};

} // namespace NoMoreDay::components
