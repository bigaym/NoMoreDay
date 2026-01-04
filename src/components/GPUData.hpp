#pragma once
#include "raylib.h"

namespace NoMoreDay::components {

// STD430 Layout for Particle
// Note: GLSL vec2 is 8-byte aligned, vec4 is 16-byte aligned.
// Structure itself should be multiple of 16 for best performance and alignment.
struct alignas(16) GPUParticle {
    Vector2 position;  // 8 bytes (Offset 0)
    Vector2 velocity;  // 8 bytes (Offset 8)
    Color color;       // 4 bytes (Offset 16)
    float lifetime;    // 4 bytes (Offset 20)
    float maxLifetime; // 4 bytes (Offset 24)
    float scale;       // 4 bytes (Offset 28)
}; // Total 32 bytes

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
