#pragma once
#include <raylib.h>
#include <stdint.h>
#include <type_traits>

namespace NoMoreDay::components {

/**
 * @brief Structure for GPU particles matching SSBO layout.
 * STRICTLY 64 BYTES (16 * 4) to ensure cross-driver std430 compatibility.
 */
struct GPUParticle {
    Vector2 position     = { 0.0f, 0.0f }; // 8
    Vector2 velocity     = { 0.0f, 0.0f }; // 8
    Vector2 acceleration = { 0.0f, 0.0f }; // 8
    Color color          = { 0, 0, 0, 0 }; // 4
    float lifetime       = 0.0f;           // 4
    float maxLifetime    = 0.0f;           // 4
    float scale          = 0.0f;           // 4
    uint32_t flags       = 0;              // 4
    float growthRate     = 0.0f;           // 4
    float padding[4]     = { 0.0f, 0.0f, 0.0f, 0.0f }; // 16

    GPUParticle() = default;
};

// Ensure Stride is exactly 64 bytes
static_assert(sizeof(GPUParticle) == 64, "GPUParticle struct must be exactly 64 bytes for SSBO alignment");

/**
 * @brief Structure for GPU entities (Physics & Sorting).
 * STRICTLY 32 BYTES (16 * 2) to match physics.compute.
 */
struct GPUEntity {
    Vector2 position = { 0.0f, 0.0f }; // 8
    Vector2 velocity = { 0.0f, 0.0f }; // 8
    float radius     = 0.0f;           // 4
    int32_t type     = 0;              // 4
    int32_t id       = 0;              // 4
    float padding    = 0.0f;           // 4

    GPUEntity() = default;
};

// Ensure Stride is exactly 32 bytes
static_assert(sizeof(GPUEntity) == 32, "GPUEntity struct must be exactly 32 bytes for physics SSBO compatibility");

/**
 * @brief Structure for GPU skill effects (SDF Rendering).
 * STRICTLY 64 BYTES (16 * 4) for alignment.
 */
struct GPUSkillEffect {
    Vector2 position     = { 0.0f, 0.0f }; // 8
    Vector2 velocity     = { 0.0f, 0.0f }; // 8
    Vector4 coreColor    = { 1.0f, 1.0f, 1.0f, 1.0f }; // 16
    Vector4 glowColor    = { 1.0f, 1.0f, 1.0f, 1.0f }; // 16
    float radius         = 0.0f;           // 4
    float sectorAngle    = 0.0f;           // 4 (Degrees)
    float softness       = 0.0f;           // 4
    float type           = 0.0f;           // 4 (0=Fan/Sector, 1=Annulus/Circle, 2=Beam)
    
    GPUSkillEffect() = default;
};

// Ensure Stride is exactly 64 bytes
static_assert(sizeof(GPUSkillEffect) == 64, "GPUSkillEffect struct must be exactly 64 bytes for SSBO alignment");

} // namespace NoMoreDay::components
