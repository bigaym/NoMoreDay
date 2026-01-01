#pragma once
#include "raylib.h"

namespace NoMoreDay {

struct AstrolabeUIComponent {
    bool isOpen = false;
    float alpha = 0.0f; // For fade in/out animation

    // Camera/View for the infinite canvas
    Vector2 offset = { 0.0f, 0.0f }; // Panning offset (world space center)
    float zoom = 1.0f;               // Zoom level (0.5x to 2.0x usually)
    
    // Interaction
    int hoveredNodeId = -1;
    bool isDragging = false;
    Vector2 lastMousePos = { 0.0f, 0.0f };
};

}
