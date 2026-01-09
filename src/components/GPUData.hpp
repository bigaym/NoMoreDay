#pragma once
#include "raylib.h"

namespace NoMoreDay::components {

// STD430 Layout for Particle
// Note: GLSL vec2 is 8-byte aligned, vec4 is 16-byte aligned.
// Structure itself should be multiple of 16 for best performance and alignment.
struct alignas(16) GPUParticle {
    Vector2 position;     // 8 bytes (Offset 0)
    Vector2 velocity;     // 8 bytes (Offset 8)
    Vector2 acceleration; // 8 bytes (Offset 16) - New!
    Color color;          // 4 bytes (Offset 24)
    float lifetime;       // 4 bytes (Offset 28)
    float maxLifetime;    // 4 bytes (Offset 32)
    float scale;          // 4 bytes (Offset 36)
    uint32_t flags;       // 4 bytes (Offset 40) - New! (Type/Behavior)
    float growthRate;     // 4 bytes (Offset 44) - Used for scale change over time
}; // Total 48 bytes (Multiple of 16)

// STD430 Layout for Entity Physics
struct alignas(16) GPUEntity {
    Vector2 position;  // 8 bytes (Offset 0)
    Vector2 velocity;  // 8 bytes (Offset 8)
    float radius;      // 4 bytes (Offset 16)
    int type;          // 4 bytes (Offset 20)
    int id;            // 4 bytes (Offset 24)
    float padding;     // 4 bytes (Offset 28)
}; // Total 32 bytes

} // namespace NoMoreDay::components
